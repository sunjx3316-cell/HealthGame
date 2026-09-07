#include "GameEngine.h"
#include "Database.h"
#include "DiseaseRules.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QScopedValueRollback>
#include <cmath>
#include <sstream>
#include <algorithm>

GameEngine::GameEngine(QObject *parent) : QObject(parent), rng(QRandomGenerator::global()->generate()) {
    initGame();
}
double GameEngine::random() { return std::generate_canonical<double, 53>(rng); }
int GameEngine::randomInt(int upper) { return std::uniform_int_distribution<int>(0, upper - 1)(rng); }

void GameEngine::initGame() {
    Database::getInstance().init();
    health = 100; immunity = 50; age = 0; currentTick = 0; tickAccumulator = 0;
    familyWealth = luck = constitution = research = 0; money = 10000;
    bpSys = 120; bpDia = 80; bloodSugar = 5; bloodLipids = 1.5;
    maxAge = 80; lastCheckupAge = -3; playerName = "无名氏";
    hasTriggeredLevel6 = hasLifeExtensionTech = pendingMilestone = milestoneResearchSucceeded = false;
    finished = paused = false; timeMultiplier = 2;
    lifeExtensionTechName.clear(); error.clear();
    organs.clear(); activeSideEffects.clear(); medicationCooldowns.clear();
    diseaseHistory.clear(); lastMajorOnsetTick = lastMajorRecoveryTick = -1000;
    lastResearchAge = -3; trainedResearchPoints = immuneSupplementReadyTick = 0; attributesSet = false;
    legacyTalentBudget = false;
    for (const auto& name : {"心脏", "肺部", "胃部", "肝脏", "皮肤"})
        organs[QString::fromUtf8(name)] = OrganState(QString::fromUtf8(name));
    publish();
}
void GameEngine::setPlayerAttributes(const QString& name, int wealth, int lck, int consti, int res) {
    if (attributesSet || age != 0 || currentTick != 0 || wealth < 0 || wealth > 15 || lck < 0 || lck > 15
        || consti < 0 || consti > 15 || res < 0 || res > 15 || wealth + lck + consti + res > 30) return;
    attributesSet = true;
    playerName = name.trimmed().left(40);
    if (playerName.isEmpty()) playerName = "无名氏";
    familyWealth = wealth; luck = lck; constitution = consti; research = res;
    money = 30000 + wealth * 60000;
    immunity = 45 + constitution * 1.5 - research * 0.5;
    bpSys = 110 + randomInt(10); bpDia = 70 + randomInt(10);
    bloodSugar = 4 + randomInt(15) / 10.0; bloodLipids = 1 + randomInt(5) / 10.0;
    publish();
}
void GameEngine::publish() {
    emit healthChanged(health); emit immunityChanged(immunity); emit ageChanged(age);
    emit moneyChanged(money); emit researchChanged(research);
    emit vitalsChanged(bpSys, bpDia, bloodSugar, bloodLipids); emit stateChanged();
}
void GameEngine::setTimeMultiplier(double m) {
    if (std::isfinite(m)) timeMultiplier = std::clamp(m, 0.1, 4.0);
}
void GameEngine::advanceTime(double realSeconds) {
    if (paused || finished || pendingMilestone || !std::isfinite(realSeconds) || realSeconds <= 0) return;
    // One clock for aging, immunity, disease evolution, damage and medication.
    // Discard suspension/backlogged wall time instead of charging it when a dialog closes.
    tickAccumulator += std::min(realSeconds, 0.25) * timeMultiplier;
    if (tickAccumulator + 1e-9 >= 1.0) {
        tickAccumulator = std::max(0.0, tickAccumulator - 1.0);
        tick();
    }
}
void GameEngine::tick() {
    if (paused || finished || pendingMilestone || inTick) return;
    QScopedValueRollback<bool> guard(inTick, true);
    ++currentTick;
    if (currentTick >= ticksPerYear) {
        currentTick = 0; ++age;
        updateYear();
        if (checkDeath()) { publish(); return; }
        checkMilestone();
        if (finished || pendingMilestone) { publish(); return; }
    }
    updateImmunityCurve();
    applyDamageAndHeal();
    if (checkDeath()) { publish(); return; }
    updateSideEffects();
    if (checkDeath("药物副作用或身体机能衰竭")) { publish(); return; }
    generateDiseases();
    publish();
}
void GameEngine::updateYear() {
    int income = age < 18 ? 1000 : getAnnualIncome();
    money = int(std::min<qint64>(1000000000, qint64(money) + income));
    if (organs["肝脏"].isTransplanted) {
        if (money >= 4000) money -= 4000;
        else { health -= 2; emit gameMessage("移植后年度维护资金不足，生命 -2。"); }
    }
    double ageDrift = age < 30 ? 0.05 : (age < 50 ? 0.5 : 1.0);
    double drift = ageDrift * (1.0 - constitution * 0.025) * (1.0 + research * 0.025);
    bpSys = std::min(300.0, bpSys + (1.5 + randomInt(10) / 10.0) * drift);
    bpDia = std::min(bpSys - 10, bpDia + (0.8 + randomInt(5) / 10.0) * drift);
    bloodSugar = std::min(40.0, bloodSugar + (0.1 + randomInt(5) / 100.0) * drift);
    bloodLipids = std::min(15.0, bloodLipids + (0.05 + randomInt(5) / 100.0) * drift);
    if (bpSys > 140 || bpDia > 90 || bloodSugar > 7 || bloodLipids > 2)
        emit gameMessage("体征偏高，请关注血压、血糖和血脂，按需控制指标。");
    if (age >= 40 && age - lastCheckupAge >= getCheckupInterval() && age < maxAge)
        emit checkupEvent(age);
}
void GameEngine::checkMilestone() {
    if (age < maxAge) return;
    if (maxAge >= 110) {
        forceDeath(QString("%1在 110 岁寿终正寝，达成最高寿命成就。").arg(playerName));
        return;
    }
    pendingMilestone = true;
    // 所有流派都可用资金筹备寿命项目；科研提供折扣而非随机生死硬门槛。
    milestoneResearchSucceeded = true;
    if (maxAge == 80) hasTriggeredLevel6 = true;
    if (maxAge == 80) emit level6DiseaseEvent(true, "全身性器官衰竭");
    else if (maxAge == 100 && !hasLifeExtensionTech) emit lifeExtensionEvent(true);
    else emit requireLifeExtensionPayment(lifeExtensionTechName.isEmpty() ? "续命特效药" : lifeExtensionTechName);
}
void GameEngine::setLifeExtensionTechName(const QString& name) {
    if (!pendingMilestone || !milestoneResearchSucceeded) return;
    lifeExtensionTechName = name.trimmed().left(60);
    if (lifeExtensionTechName.isEmpty()) lifeExtensionTechName = "基因修复疗法";
    if (maxAge == 100) hasLifeExtensionTech = true;
}
bool GameEngine::payForLifeExtension(int cost, int years) {
    if (cost == -1) cost = getLifeExtensionCost();
    // UI parameters cannot be used to obtain free, premature or unlimited extensions.
    if (finished || !pendingMilestone || !milestoneResearchSucceeded || cost != getLifeExtensionCost()
        || years != 10 || money < cost || maxAge >= 110) return false;
    money -= cost; maxAge += years; pendingMilestone = false;
    if (maxAge == 110) hasLifeExtensionTech = true;
    emit gameMessage(QString("支付 %1 金币，寿命上限延长至 %2 岁。").arg(cost).arg(maxAge));
    publish(); return true;
}
void GameEngine::declineLifeExtension() {
    if (pendingMilestone && !finished)
        forceDeath(QString("%1在 %2 岁到达寿命大限，未接受续命治疗。").arg(playerName).arg(age));
}
int GameEngine::getAnnualIncome() const { return 32000 + familyWealth * 1200 + research * 1500; }
int GameEngine::getResearchPointCost() const { return 60000 + research * 8000; }
int GameEngine::getLifeExtensionCost() const { return 1200000 - research * 25000; }
int GameEngine::getCheckupCost() const { return 30000 - research * 1000; }
int GameEngine::getCheckupInterval() const { return std::max(1, 3 - research / 6); }
bool GameEngine::canBuyResearchPoint() const {
    return !finished && !pendingMilestone && research < 17 && trainedResearchPoints < 5
        && age >= 18 && age - lastResearchAge >= 3 && money >= getResearchPointCost();
}
bool GameEngine::buyResearchPoint() {
    if (!canBuyResearchPoint()) return false;
    money -= getResearchPointCost(); ++research; ++trainedResearchPoints; lastResearchAge = age;
    publish(); return true;
}
void GameEngine::updateImmunityCurve() {
    double target = (age < 18 ? 45.0 + age * 0.9 : 62.0 - std::max(0, age - 50) * 0.35)
        + constitution * 1.5 - research * 0.5;
    // 临时补剂回归体质基线，不能靠廉价连点永久替代健康天赋。
    immunity = std::clamp(immunity + (target - immunity) * 0.08, 0.0, 100.0);
}
double GameEngine::diseaseDamage(const OrganState& o) const {
    double reduction = immunity / 100 * 0.15 + constitution / 15.0 * 0.35;
    double factor = o.currentDisease.severity == 1 ? 0.2 : o.currentDisease.severity == 2 ? 0.4 : 1.0;
    if (o.currentDisease.severity == 5 && o.diseaseStage > 0)
        factor = o.diseaseStage == 1 ? 0.3 : o.diseaseStage == 2 ? 0.6 : 1.5;
    if (absoluteTick() < o.controlledUntilTick) factor *= 0.25;
    return o.currentDisease.damagePerTick * (1 - reduction) * factor / ticksPerYear;
}
void GameEngine::revealDisease(OrganState& o) {
    o.currentDisease = o.hiddenDisease; o.hiddenDisease = Disease();
    o.hasHiddenDisease = false; o.isDiseased = true; o.autoHealTimer = 0;
    o.currentTreatmentStage = 0; o.diseasedDays = 0; o.hiddenTicks = 0;
    o.currentDisease.treatmentStages = o.diseaseStage == 1 ? o.currentDisease.earlyTreatment :
        o.diseaseStage == 2 ? o.currentDisease.midTreatment : o.currentDisease.lateTreatment;
    emit organStateChanged(o.name, true); emit organDetailUpdated(o.name);
    emit diseaseTriggered();
}
void GameEngine::normalizeOrgan(OrganState& o) {
    // Flatten the organ's concurrent conditions. Anatomy belongs to the parent, not a diagnosis.
    QList<OrganState> cases = o.coexisting;
    if (o.isDiseased || o.hasHiddenDisease) {
        OrganState primary = o; primary.coexisting.clear(); cases.prepend(primary);
    }
    const auto removed = o.removedDiseases; const bool transplanted = o.isTransplanted;
    o = OrganState(o.name);
    if (!cases.isEmpty()) {
        std::stable_sort(cases.begin(), cases.end(), [](const auto& a, const auto& b) {
            int pa = a.isDiseased ? a.currentDisease.severity : 0;
            int pb = b.isDiseased ? b.currentDisease.severity : 0;
            return pa > pb;
        });
        o = cases.takeFirst(); o.coexisting = cases;
    }
    o.removedDiseases = removed; o.isTransplanted = transplanted;
    for (auto& c : o.coexisting) { c.removedDiseases.clear(); c.isTransplanted = false; c.coexisting.clear(); }
}
void GameEngine::cure(OrganState& o, double recovery) {
    const auto disease = o.currentDisease;
    if (!disease.id.isEmpty()) {
        auto& h = diseaseHistory[disease.id];
        if (!h.episodes) { h.episodes = 1; h.lastOnsetTick = absoluteTick(); }
        h.lastRecoveryTick = absoluteTick();
        if (DiseaseRules::major(disease)) lastMajorRecoveryTick = absoluteTick();
    }
    // Only refund a fraction of this episode's damage; never heal aging or unrelated injuries.
    health = std::min(100.0, health + std::min(recovery, o.damageTaken * (0.6 + constitution * 0.02)));
    const auto removed = o.removedDiseases; const bool transplanted = o.isTransplanted;
    const auto cases = o.coexisting;
    o = OrganState(o.name); o.removedDiseases = removed; o.isTransplanted = transplanted; o.coexisting = cases;
    emit organStateChanged(o.name, false); emit organDetailUpdated(o.name);
}
void GameEngine::updateDisease(OrganState& o) {
    if (o.hasHiddenDisease) {
        ++o.hiddenTicks;
        if (o.diseaseStage == 1 && o.hiddenTicks >= 10) {
            o.diseaseStage = 2; o.hiddenTicks = 0;
        } else if (o.diseaseStage == 2 && o.hiddenTicks >= 35) {
            o.diseaseStage = 3; revealDisease(o);
            emit gameMessage(QString("%1出现症状，确诊 %2（晚期）。").arg(o.name, o.currentDisease.name));
        }
    }
    if (!o.isDiseased) return;
    ++o.diseasedDays;
    if (!o.currentDisease.earlyTreatment.isEmpty() && o.diseaseStage < 3) {
        // Acute cardiac events are visible immediately and worsen much faster than tumors.
        int threshold = DiseaseRules::latentProgressive(o.currentDisease) ? (o.diseaseStage == 1 ? 10 : 35) : 5;
        if (o.diseasedDays >= threshold) {
            ++o.diseaseStage; o.diseasedDays = 0; o.currentTreatmentStage = 0;
            o.currentDisease.treatmentStages = o.diseaseStage == 2 ? o.currentDisease.midTreatment : o.currentDisease.lateTreatment;
            emit gameMessage(QString("%1的%2病情升级，治疗方案已更新。").arg(o.name, o.currentDisease.name));
            emit organDetailUpdated(o.name); emit diseaseTriggered();
        }
    }
    if (o.autoHealTimer > 0 && --o.autoHealTimer == 0) {
        emit gameMessage(QString("%1的%2已自愈。").arg(o.name, o.currentDisease.name));
        cure(o, o.damageTaken); return;
    }
    const double damage = diseaseDamage(o);
    o.damageTaken = std::min(100.0, o.damageTaken + std::min(std::max(0.0, health), damage));
    health -= damage;
}
void GameEngine::applyDamageAndHeal() {
    double decay = (age <= 12 ? 0.1 : age <= 60 ? 0.3 : 0.65) * (1 - constitution * 0.015);
    if (bpSys > 160 || bpDia > 100) decay += 0.5;
    if (bloodSugar > 8) decay += 0.5;
    if (bloodLipids > 2.5) decay += 0.5;
    if (bpSys < 85 || bpDia < 55) decay += 0.8;
    if (bloodSugar < 3.5) decay += 1.0;
    if (bloodLipids < 0.3) decay += 0.3;
    health -= decay / ticksPerYear;
    if (health <= 0) return;
    for (auto& o : organs) {
        updateDisease(o);
        if (health <= 0) return;
        for (auto& c : o.coexisting) { updateDisease(c); if (health <= 0) return; }
        for (int i = o.coexisting.size() - 1; i >= 0; --i)
            if (!o.coexisting[i].isDiseased && !o.coexisting[i].hasHiddenDisease) o.coexisting.removeAt(i);
        normalizeOrgan(o);
    }
}
void GameEngine::updateSideEffects() {
    for (auto it = medicationCooldowns.begin(); it != medicationCooldowns.end();) {
        if (--it.value() <= 0) it = medicationCooldowns.erase(it); else ++it;
    }
    for (int i = activeSideEffects.size() - 1; i >= 0; --i) {
        health -= activeSideEffects[i].hpPenaltyPerTick;
        if (--activeSideEffects[i].remainingTicks <= 0) activeSideEffects.removeAt(i);
    }
}
bool GameEngine::diseaseEligible(const Disease& d) const {
    if (d.id.isEmpty() || age < d.minAge || age > d.maxAge) return false;
    const auto& organ = organs[d.organ];
    if (organ.isRemoved) return false;
    const auto structure = DiseaseRules::requiredStructure(d);
    if ((structure == "appendix" && organ.removedDiseases.contains("D29"))
        || (structure == "gallbladder" && organ.removedDiseases.contains("D37"))) return false;
    const int now = absoluteTick();
    const auto h = diseaseHistory.value(d.id);
    if (DiseaseRules::congenital(d) && h.episodes > 0) return false;
    if (h.lastRecoveryTick >= 0 && now - h.lastRecoveryTick < DiseaseRules::recoveryTicks(d)) return false;
    if (d.id == "D28" && bpSys <= 140 && bpDia <= 90) return false;
    if (d.id == "D51" && bloodSugar <= 7) return false;
    if (d.id == "D49" && activeSideEffects.isEmpty()) return false;
    if (d.id == "D50") {
        bool history = false;
        for (auto it = diseaseHistory.cbegin(); it != diseaseHistory.cend(); ++it)
            history |= DiseaseRules::isChronicLung(it.key()) && it.value().episodes > 0;
        if (!history) return false;
    }
    bool activeMajor = false;
    for (const auto& o : organs) {
        QList<OrganState> cases = o.coexisting; cases.append(o);
        for (const auto& c : cases) {
            const auto& active = c.isDiseased ? c.currentDisease : c.hiddenDisease;
            if (active.id == d.id) return false;
            activeMajor |= !active.id.isEmpty() && DiseaseRules::major(active);
        }
    }
    if (DiseaseRules::major(d) && (activeMajor || now - lastMajorOnsetTick < 30 || now - lastMajorRecoveryTick < 10)) return false;
    // Reserve a slot for a major illness: deliberately retaining two small illnesses is not a shield.
    int count = int(organ.isDiseased || organ.hasHiddenDisease) + organ.coexisting.size();
    return count < (DiseaseRules::major(d) ? 3 : 2);
}
double GameEngine::riskWeight(const Disease& d) const {
    double vital = 1;
    if (d.organ == "心脏") {
        vital += std::clamp((bpSys - 135) / 60.0, 0.0, 1.0);
        vital += std::clamp((bloodLipids - 2) / 2.0, 0.0, 0.5);
    }
    if (d.organ == "肝脏") vital += std::clamp((bloodLipids - 2) / 1.5, 0.0, 1.0);
    vital += std::clamp((bloodSugar - 7) / 8.0, 0.0, 0.5);
    const auto h = diseaseHistory.value(d.id);
    // Gradual return after protection, not an accumulated overdue roll.
    const double recoveryRamp = h.lastRecoveryTick < 0 ? 1.0 :
        std::clamp(0.35 + (absoluteTick() - h.lastRecoveryTick - DiseaseRules::recoveryTicks(d)) / 30.0, 0.35, 1.0);
    return std::max(0.001, d.probability) * std::min(3.0, vital) * recoveryRamp;
}
void GameEngine::startDisease(const Disease& d) {
    auto& h = diseaseHistory[d.id]; ++h.episodes; h.lastOnsetTick = absoluteTick();
    if (DiseaseRules::major(d)) lastMajorOnsetTick = absoluteTick();
    OrganState c(d.organ);
    if (DiseaseRules::latentProgressive(d)) {
        c.hasHiddenDisease = true; c.hiddenDisease = d; c.diseaseStage = 1;
    } else {
        c.isDiseased = true; c.currentDisease = d;
        if (!d.earlyTreatment.isEmpty()) { c.diseaseStage = 1; c.currentDisease.treatmentStages = d.earlyTreatment; }
        if (d.severity <= 2) c.autoHealTimer = std::max(2, (d.severity == 1 ? 4 : 9) - constitution / 5);
    }
    auto& o = organs[d.organ]; o.coexisting.append(c); normalizeOrgan(o);
    if (c.isDiseased) {
        emit organStateChanged(o.name, true); emit organDetailUpdated(o.name);
        emit gameMessage(QString("%1患上%2，已降低时间流速。").arg(o.name, d.name));
        emit diseaseTriggered();
    }
}
void GameEngine::generateDiseases() {
    // Bounded, category-level annual hazards. Database size does not multiply event frequency.
    // First roll a category event, then select an eligible disease by normalized weights.
    for (int category = 2; category >= 0; --category) {
        QVector<Disease> candidates;
        double totalWeight = 0, weightedVital = 0, baseWeight = 0;
        for (const auto& d : Database::getInstance().getAllDiseases()) {
            int group = DiseaseRules::major(d) ? 2 : d.severity == 3 ? 1 : 0;
            if (group != category || !diseaseEligible(d)) continue;
            candidates.append(d); totalWeight += riskWeight(d);
            baseWeight += std::max(0.001, d.probability);
            weightedVital += riskWeight(d);
        }
        if (candidates.isEmpty()) continue;
        double hazard;
        if (category == 0)
            hazard = (age < 18 ? 1.35 : 1.0) * (1.35 - immunity * 0.01) * (1.15 - constitution * 0.035);
        else if (category == 1)
            hazard = (0.12 + std::max(0, age - 30) * 0.002) * (1.15 - luck * 0.025);
        else
            hazard = (0.025 + std::max(0, age - 30) * 0.0017) * (1.15 - luck * 0.035);
        hazard *= std::clamp(weightedVital / baseWeight, 0.35, 2.5);
        if (category == 2 && lastMajorOnsetTick >= 0)
            hazard *= std::clamp(0.4 + (absoluteTick() - lastMajorOnsetTick - 30) / 50.0, 0.4, 1.0);
        hazard = std::min(hazard, category == 0 ? 1.8 : category == 1 ? 0.5 : 0.22);
        if (random() >= -std::expm1(-hazard / ticksPerYear)) continue;
        double choice = random() * totalWeight;
        for (const auto& d : candidates) {
            choice -= riskWeight(d);
            if (choice <= 0) { startDisease(d); break; }
        }
    }
}
bool GameEngine::medicineMatches(const OrganState& o, const Medicine& m) const {
    if (!o.isDiseased || m.id.isEmpty() || m.type == "保健品") return false;
    if (!o.currentDisease.treatmentStages.isEmpty()) {
        return o.currentTreatmentStage >= 0 && o.currentTreatmentStage < o.currentDisease.treatmentStages.size()
            && o.currentDisease.treatmentStages[o.currentTreatmentStage] == m.id;
    }
    // Stage-specific treatments must never become broad-spectrum cures through ALL_DISEASES.
    if (m.id.startsWith("M_CANCER_") || m.id.startsWith("M_HEART_")) return false;
    if (m.indication.split('|').contains(o.currentDisease.id)) return true;
    if (o.currentDisease.severity >= 4 || DiseaseRules::requiresProcedure(o.currentDisease)) return false;
    if (m.id == "M_BROAD_SUPER") return true;
    const QMap<QString, QString> broad{{"心脏","HEART_ALL"},{"肺部","LUNGS_ALL"},{"胃部","STOMACH_ALL"},{"肝脏","LIVER_ALL"},{"皮肤","SKIN_ALL"}};
    return m.indication == broad.value(o.name);
}
bool GameEngine::checkConflict(const QString& id, QString& other) const {
    const auto m = Database::getInstance().getMedicineById(id);
    for (auto it = medicationCooldowns.cbegin(); it != medicationCooldowns.cend(); ++it) {
        const auto active = Database::getInstance().getMedicineById(it.key());
        if (m.incompatibleMeds.contains(it.key()) || active.incompatibleMeds.contains(id)) {
            other = active.name; return true;
        }
    }
    return false;
}
bool GameEngine::applyMedicine(const QString& organName, const QString& medicineId, int dosage) {
    if (finished || pendingMilestone || dosage < 1 || dosage > 10) return false;
    const auto med = Database::getInstance().getMedicineById(medicineId);
    if (med.id.isEmpty() || (med.type != "保健品" && !organs.contains(organName))) return false;
    if (medicineId == "M52" && organs[organName].controlledUntilTick > absoluteTick()) {
        emit gameMessage("当前支持治疗仍有效，无需重复购买。"); return false;
    }
    if (med.type != "保健品" && organs[organName].isDiseased
        && absoluteTick() < organs[organName].treatmentReadyTick) {
        emit gameMessage("上一阶段治疗仍在结算，请等待一个游戏时间步。"); return false;
    }
    if (med.type == "手术" && dosage != 1) {
        emit gameMessage("手术一次只能进行一个阶段。"); return false;
    }
    QString conflict;
    if (checkConflict(medicineId, conflict)) {
        emit gameMessage(QString("用药被阻止：与仍在作用期的%1冲突。").arg(conflict)); return false;
    }
    const qint64 cost = qint64(med.price) * dosage;
    if (cost <= 0 || money < cost) { emit gameMessage("金币不足，无法进行这次治疗。"); return false; }
    if (med.type == "保健品") {
        if (med.indication == "SUPP_IMMUNE" && (dosage != 1 || absoluteTick() < immuneSupplementReadyTick)) {
            emit gameMessage("免疫支持每次限一份，所有品类共享 1 游戏年间隔。"); return false;
        }
        // Permit a low-value penalty, but never negative vitals or endless immune purchases at 100.
        if ((med.indication == "SUPP_BP" && (bpSys - 15 * dosage < 70 || bpDia - 8 * dosage < 40))
            || (med.indication == "SUPP_SUGAR" && bloodSugar - 1.5 * dosage < 2)
            || (med.indication == "SUPP_LIPID" && bloodLipids - 0.5 * dosage < 0.15)
            || (med.indication == "SUPP_IMMUNE" && immunity >= 100)) {
            emit gameMessage("本次数量会使指标越过安全边界，或当前免疫力已满。请减少数量。"); return false;
        }
        money -= int(cost);
        if (med.indication == "SUPP_BP") { bpSys -= 15 * dosage; bpDia -= 8 * dosage; }
        else if (med.indication == "SUPP_SUGAR") bloodSugar -= 1.5 * dosage;
        else if (med.indication == "SUPP_LIPID") bloodLipids -= 0.5 * dosage;
        else if (med.indication == "SUPP_IMMUNE") {
            double boost = medicineId == "M_SUPP_01" ? 2 : medicineId == "M_SUPP_05" ? 6 : medicineId == "M_SUPP_06" ? 12 : 18;
            immunity = std::min(100.0, immunity + boost * dosage);
            immuneSupplementReadyTick = absoluteTick() + 10;
        }
        emit gameMessage(QString("使用%1 × %2，体征已更新。").arg(med.name).arg(dosage));
        publish(); return true;
    }
    money -= int(cost);
    if (dosage > med.safeDosage) health -= (dosage - med.safeDosage) * med.penaltyCoefficient * 0.5;
    if (checkDeath("过量用药")) { publish(); return false; }
    medicationCooldowns[medicineId] = std::max(5, med.sideEffectData.durationTicks);
    if (random() < med.sideEffectData.probability * (1 - immunity / 200)) {
        // The database penalty is the total burden of the course, distributed across ticks.
        activeSideEffects.append({med.name, med.sideEffectData.durationTicks,
            double(med.sideEffectData.hpPenalty) * 0.25 / std::max(1, med.sideEffectData.durationTicks), med.id});
        emit gameMessage(QString("%1触发副作用，将按游戏时间逐步结算。").arg(med.name));
    }
    OrganState& o = organs[organName];
    if (!medicineMatches(o, med)) {
        health -= 10;
        emit gameMessage(QString("%1不符合当前所选器官的治疗方案，生命 -10。").arg(med.name));
        checkDeath("错误用药"); publish(); return false;
    }
    o.treatmentReadyTick = absoluteTick() + 1;
    if (medicineId == "M52") {
        o.controlledUntilTick = absoluteTick() + 5;
        emit gameMessage("支持疗法暂时降低肝衰竭损伤，未根治；需进一步完成移植方案。");
        publish(); return true;
    }
    const bool staged = !o.currentDisease.treatmentStages.isEmpty();
    double success = staged ? std::clamp(0.80 + luck * 0.006 + research * 0.004 +
        (o.diseaseStage == 1 ? 0.08 : o.diseaseStage == 3 ? -0.10 : 0) + o.treatmentFailures * 0.10, 0.7, 0.99) : 1.0;
    if (med.id.startsWith("M_BROAD_")) { success = std::min(0.99, 0.75 + luck * 0.006 + o.treatmentFailures * 0.1); health -= 1; }
    if (checkDeath("高风险治疗负担")) { publish(); return false; }
    if (random() >= success) {
        ++o.treatmentFailures;
        health -= 4; emit gameMessage("本阶段治疗未成功，生命 -4；下一时间步可重试，重试成功率提高。");
        checkDeath("治疗失败"); publish(); return false;
    }
    const QString diseaseName = o.currentDisease.name;
    o.treatmentFailures = 0;
    if (staged) ++o.currentTreatmentStage;
    // Explicit anatomical outcomes, never inference from a medicine's display name.
    const auto structure = DiseaseRules::removedStructure(med.id);
    if (!structure.isEmpty()) {
        const QString marker = structure == "appendix" ? "D29" : "D37";
        if (!o.removedDiseases.contains(marker)) o.removedDiseases.append(marker);
        for (auto& c : o.coexisting)
            if (DiseaseRules::requiredStructure(c.currentDisease) == structure) cure(c, c.damageTaken);
    }
    if (med.id == "M53") o.isTransplanted = true;
    if (o.currentDisease.id == "D28") { bpSys = std::min(bpSys, 135.0); bpDia = std::min(bpDia, 85.0); }
    if (o.currentDisease.id == "D51") bloodSugar = std::min(bloodSugar, 6.5);
    if (!staged || o.currentTreatmentStage >= o.currentDisease.treatmentStages.size()) {
        cure(o, o.damageTaken);
        emit gameMessage(QString("%1的%2已完成治疗。").arg(organName, diseaseName));
    } else {
        emit gameMessage(QString("%1治疗阶段 %2 / %3 完成。").arg(diseaseName).arg(o.currentTreatmentStage).arg(o.currentDisease.treatmentStages.size()));
        emit organDetailUpdated(o.name);
    }
    for (int i = o.coexisting.size() - 1; i >= 0; --i)
        if (!o.coexisting[i].isDiseased && !o.coexisting[i].hasHiddenDisease) o.coexisting.removeAt(i);
    normalizeOrgan(o);
    publish(); return true;
}
bool GameEngine::performCheckup() {
    if (finished || pendingMilestone || money < getCheckupCost()) { emit gameMessage(QString("体检需要 %1 金币。").arg(getCheckupCost())); return false; }
    if (age - lastCheckupAge < getCheckupInterval()) { emit gameMessage("尚未到下一次体检时间。"); return false; }
    money -= getCheckupCost(); lastCheckupAge = age;
    int count = 0;
    auto screen = [&](OrganState& o) { if (o.hasHiddenDisease) {
        // Preserve elapsed time in this stage: screening cannot reset disease progression.
        int elapsed = o.hiddenTicks;
        revealDisease(o); o.diseasedDays = elapsed;
        emit gameMessage(QString("体检发现：%1，%2。").arg(o.name, o.currentDisease.name)); ++count;
    }};
    for (auto& o : organs) { screen(o); for (auto& c : o.coexisting) screen(c); normalizeOrgan(o); }
    emit gameMessage(count ? QString("体检发现 %1 处隐性病变，请查看对应器官。").arg(count) : "体检未发现隐性疾病。");
    publish(); return true;
}
void GameEngine::forceDeath(const QString& reason) {
    if (finished) return;
    health = 0; finished = true; pendingMilestone = false;
    emit healthChanged(0); emit stateChanged(); emit gameOver(reason);
}
bool GameEngine::checkDeath(const QString& cause) {
    if (health > 0) return false;
    QString reason = cause;
    if (reason.isEmpty()) {
        int severity = 0;
        for (const auto& o : organs) if (o.isDiseased && o.currentDisease.severity > severity) {
            severity = o.currentDisease.severity; reason = o.currentDisease.name;
        }
        if (reason.isEmpty()) reason = "自然衰老或体征失衡";
    }
    forceDeath(QString("%1在 %2 岁因%3离世。").arg(playerName).arg(age).arg(reason));
    return true;
}

bool GameEngine::saveGame(const QString& path) {
    error.clear();
    if (pendingMilestone || inTick) { error = "请先完成当前寿命事件。"; return false; }
    QJsonObject root{{"format","HealthGame"},{"version",3},{"health",health},{"immunity",immunity},
        {"age",age},{"playerName",playerName},{"wealth",familyWealth},{"luck",luck},{"constitution",constitution},
        {"research",research},{"money",money},{"maxAge",maxAge},{"lastCheckupAge",lastCheckupAge},
        {"level6",hasTriggeredLevel6},{"lifeTech",hasLifeExtensionTech},{"techName",lifeExtensionTechName},
        {"bpSys",bpSys},{"bpDia",bpDia},{"sugar",bloodSugar},{"lipids",bloodLipids},
        {"tick",currentTick},{"fraction",tickAccumulator},{"speed",timeMultiplier},{"finished",finished},
        {"attributesSet",attributesSet},{"legacyTalentBudget",legacyTalentBudget},{"trainedResearch",trainedResearchPoints},{"lastResearchAge",lastResearchAge},
        {"immuneReady",immuneSupplementReadyTick},{"majorOnset",lastMajorOnsetTick},{"majorRecovery",lastMajorRecoveryTick}};
    std::ostringstream rngState; rngState << rng; root["rng"] = QString::fromStdString(rngState.str());
    auto encode = [](const OrganState& o) {
        QJsonArray removed; for (const auto& id : o.removedDiseases) removed.append(id);
        return QJsonObject{{"name",o.name},{"disease",o.isDiseased ? o.currentDisease.id : ""},
            {"hidden",o.hasHiddenDisease ? o.hiddenDisease.id : ""},{"stage",o.diseaseStage},
            {"elapsed",o.diseasedDays},{"latentTicks",o.hiddenTicks},{"autoHeal",o.autoHealTimer},
            {"treatment",o.currentTreatmentStage},{"removed",removed},{"transplanted",o.isTransplanted},
            {"damageTaken",o.damageTaken},{"failures",o.treatmentFailures},{"ready",o.treatmentReadyTick},
            {"controlled",o.controlledUntilTick}};
    };
    QJsonArray states;
    for (const auto& o : organs) {
        auto entry = encode(o); QJsonArray concurrent;
        for (const auto& c : o.coexisting) concurrent.append(encode(c));
        entry["coexisting"] = concurrent; states.append(entry);
    }
    root["organs"] = states;
    QJsonObject history;
    for (auto it = diseaseHistory.cbegin(); it != diseaseHistory.cend(); ++it)
        history[it.key()] = QJsonObject{{"episodes",it->episodes},{"onset",it->lastOnsetTick},{"recovery",it->lastRecoveryTick}};
    root["history"] = history;
    QJsonArray effects;
    for (const auto& e : activeSideEffects) effects.append(QJsonObject{
        {"id",e.medicineId},{"ticks",e.remainingTicks},{"damage",e.hpPenaltyPerTick}});
    root["effects"] = effects;
    QJsonObject cooldowns;
    for (auto it = medicationCooldowns.cbegin(); it != medicationCooldowns.cend(); ++it) cooldowns[it.key()] = it.value();
    root["cooldowns"] = cooldowns;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
    const auto bytes = QJsonDocument(root).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}
bool GameEngine::loadGame(const QString& path) {
    error.clear();
    if (inTick || pendingMilestone) { error = "请先完成当前事件。"; return false; }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    if (file.size() > 1024 * 1024) { error = "存档大小超出限制。"; return false; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto r = doc.object(); const int version = r["version"].toInt();
    auto fail = [&]() { error = "存档格式或数值无效，当前游戏未改变。"; return false; };
    if (parse.error != QJsonParseError::NoError || !doc.isObject()
        || r["format"] != "HealthGame" || (version != 2 && version != 3)) return fail();
    auto number = [](const QJsonObject& obj, const QString& key, double lo, double hi, bool integer = false) {
        if (!obj[key].isDouble()) return false;
        const double v = obj[key].toDouble();
        return std::isfinite(v) && v >= lo && v <= hi && (!integer || std::floor(v) == v);
    };
    for (const auto& key : {"level6","lifeTech","finished"}) if (!r[key].isBool()) return fail();
    for (const auto& key : {"playerName","techName","rng"}) if (!r[key].isString()) return fail();
    if (!number(r,"health",0,100) || !number(r,"immunity",0,100) || !number(r,"age",0,110,true)
        || !number(r,"wealth",0,15,true) || !number(r,"luck",0,15,true) || !number(r,"constitution",0,15,true)
        || !number(r,"research",0,17,true) || !number(r,"money",0,1000000000,true)
        || !number(r,"maxAge",80,110,true) || !number(r,"lastCheckupAge",-3,110,true)
        || !number(r,"bpSys",70,300) || !number(r,"bpDia",40,290) || !number(r,"sugar",2,40)
        || !number(r,"lipids",0.15,15) || !number(r,"tick",0,9,true)
        || !number(r,"fraction",0,0.999999999) || !number(r,"speed",0.1,4)) return fail();
    int savedAge = r["age"].toInt(), savedMax = r["maxAge"].toInt(), now = savedAge * 10 + r["tick"].toInt();
    if (savedMax % 10 != 0 || savedAge > savedMax || r["lastCheckupAge"].toInt() > savedAge
        || r["bpDia"].toDouble() >= r["bpSys"].toDouble()
        || r["finished"].toBool() != (r["health"].toDouble() == 0)
        || (!r["finished"].toBool() && savedAge >= savedMax)
        || (savedMax > 80 && !r["level6"].toBool())
        || (savedMax == 110 && !r["lifeTech"].toBool())
        || r["playerName"].toString().size() > 40 || r["techName"].toString().size() > 60) return fail();
    int trained = version == 3 ? r["trainedResearch"].toInt() :
        std::max(0, r["wealth"].toInt()+r["luck"].toInt()+r["constitution"].toInt()+r["research"].toInt()-30);
    const bool legacyTalents = version == 2 || r["legacyTalentBudget"].toBool();
    if (version == 3 && (!r["attributesSet"].isBool() || !r["legacyTalentBudget"].isBool() || !number(r,"trainedResearch",0,legacyTalents ? 17 : 5,true)
        || !number(r,"lastResearchAge",-3,savedAge,true) || !number(r,"immuneReady",0,now+10,true)
        || !number(r,"majorOnset",-1000,now,true) || !number(r,"majorRecovery",-1000,now,true)
        || !r["history"].isObject())) return fail();
    if (trained > (legacyTalents ? 17 : 5) || trained > r["research"].toInt()
        || r["wealth"].toInt()+r["luck"].toInt()+r["constitution"].toInt()+r["research"].toInt()-trained > 30) return fail();
    if (!r["organs"].isArray() || r["organs"].toArray().size() != 5
        || !r["effects"].isArray() || r["effects"].toArray().size() > 1000 || !r["cooldowns"].isObject()) return fail();
    QMap<QString, DiseaseHistory> histories;
    int majorOnset = -1000, majorRecovery = -1000;
    if (version == 3) {
        const auto history = r["history"].toObject();
        for (auto it = history.begin(); it != history.end(); ++it) {
            const auto d = Database::getInstance().getDiseaseById(it.key()); const auto h = it.value().toObject();
            if (d.id.isEmpty() || !it.value().isObject() || !number(h,"episodes",1,1100,true)
                || !number(h,"onset",0,now,true) || !number(h,"recovery",-1000,now,true)
                || (h["recovery"].toInt() < 0 && h["recovery"].toInt() != -1000)
                || (DiseaseRules::congenital(d) && h["episodes"].toInt() != 1)) return fail();
            histories[it.key()] = {h["episodes"].toInt(),h["onset"].toInt(),h["recovery"].toInt()};
            if (DiseaseRules::major(d)) {
                majorOnset = std::max(majorOnset, h["onset"].toInt());
                majorRecovery = std::max(majorRecovery, h["recovery"].toInt());
            }
        }
        if (majorOnset != r["majorOnset"].toInt() || majorRecovery != r["majorRecovery"].toInt()) return fail();
    }
    QStringList activeIds;
    auto decode = [&](const QJsonObject& o, const QString& name, bool child, OrganState& s) {
        if (o["name"] != name || !o["disease"].isString() || !o["hidden"].isString()
            || !o["transplanted"].isBool() || !o["removed"].isArray()
            || !number(o,"stage",0,3,true) || !number(o,"elapsed",0,1100,true)
            || !number(o,"latentTicks",0,34,true) || !number(o,"autoHeal",0,10,true)
            || !number(o,"treatment",0,3,true)) return false;
        s = OrganState(name);
        const auto id = o["disease"].toString(), hidden = o["hidden"].toString();
        if (!id.isEmpty() && !hidden.isEmpty()) return false;
        s.isDiseased = !id.isEmpty(); s.hasHiddenDisease = !hidden.isEmpty();
        s.diseaseStage = o["stage"].toInt(); s.diseasedDays = o["elapsed"].toInt();
        s.hiddenTicks = o["latentTicks"].toInt(); s.autoHealTimer = o["autoHeal"].toInt();
        s.currentTreatmentStage = o["treatment"].toInt(); s.isTransplanted = o["transplanted"].toBool();
        if (s.isTransplanted && (name != "肝脏" || child)) return false;
        if (version == 3) {
            if (!number(o,"damageTaken",0,100) || !number(o,"failures",0,1100,true)
                || !number(o,"ready",0,now+1,true) || !number(o,"controlled",0,now+5,true)) return false;
            s.damageTaken = o["damageTaken"].toDouble(); s.treatmentFailures = o["failures"].toInt();
            s.treatmentReadyTick = o["ready"].toInt(); s.controlledUntilTick = o["controlled"].toInt();
        }
        for (const auto& removed : o["removed"].toArray()) {
            const auto d = Database::getInstance().getDiseaseById(removed.toString());
            if (!removed.isString() || d.id.isEmpty() || d.organ != name || child) return false;
            if (d.id == "D29" || d.id == "D37") {
                if (s.removedDiseases.contains(d.id)) return false;
                s.removedDiseases.append(d.id);
            } else if (version == 3) return false;
            // V2's name-based tumor immunity is intentionally migrated into recovery history.
            if (version == 2 && !histories.contains(d.id)) {
                histories[d.id] = {1,now,now};
                if (DiseaseRules::major(d)) majorOnset = majorRecovery = now;
            }
        }
        if (s.isDiseased || s.hasHiddenDisease) {
            auto d = Database::getInstance().getDiseaseById(s.isDiseased ? id : hidden);
            if (d.id.isEmpty() || d.organ != name || activeIds.contains(d.id)) return false;
            activeIds.append(d.id);
            if (s.hasHiddenDisease && !DiseaseRules::latentProgressive(d)) {
                if (version == 3) return false;
                s.isDiseased = true; s.hasHiddenDisease = false; s.diseasedDays = 0; s.hiddenTicks = 0;
            }
            if (!d.earlyTreatment.isEmpty()) {
                if (s.diseaseStage < 1 || (s.hasHiddenDisease && s.diseaseStage > 2)) return false;
                d.treatmentStages = s.diseaseStage == 1 ? d.earlyTreatment : s.diseaseStage == 2 ? d.midTreatment : d.lateTreatment;
                if (s.currentTreatmentStage >= d.treatmentStages.size() || s.autoHealTimer != 0) return false;
            } else if (s.diseaseStage != 0 || s.currentTreatmentStage != 0 || s.hasHiddenDisease) return false;
            if ((s.hasHiddenDisease && (s.currentTreatmentStage || s.diseasedDays || s.damageTaken))
                || (s.isDiseased && s.hiddenTicks) || (d.severity > 2 && s.autoHealTimer)) return false;
            if (version == 3 && !histories.contains(d.id)) return false;
            if (version == 2) {
                histories[d.id] = {1,now,-1000};
                if (DiseaseRules::major(d)) majorOnset = now;
            }
            if (s.hasHiddenDisease) s.hiddenDisease = d; else s.currentDisease = d;
        } else if (s.diseaseStage || s.currentTreatmentStage || s.autoHealTimer || s.hiddenTicks
                   || s.diseasedDays || s.damageTaken || s.treatmentFailures || s.treatmentReadyTick || s.controlledUntilTick) return false;
        return true;
    };
    QMap<QString, OrganState> loaded;
    for (const auto& entry : r["organs"].toArray()) {
        if (!entry.isObject()) return fail();
        const auto o = entry.toObject(); const auto name = o["name"].toString();
        if (!organs.contains(name) || loaded.contains(name)) return fail();
        OrganState s;
        if (!decode(o,name,false,s)) return fail();
        if (version == 3) {
            if (!o["coexisting"].isArray() || o["coexisting"].toArray().size() > 2) return fail();
            for (const auto& concurrent : o["coexisting"].toArray()) {
                OrganState c;
                if (!concurrent.isObject() || !decode(concurrent.toObject(),name,true,c)
                    || (!c.isDiseased && !c.hasHiddenDisease) || concurrent.toObject().contains("coexisting")) return fail();
                s.coexisting.append(c);
            }
        }
        QList<OrganState> cases = s.coexisting; cases.append(s);
        for (const auto& c : cases) {
            auto d = c.isDiseased ? c.currentDisease : c.hiddenDisease;
            auto structure = DiseaseRules::requiredStructure(d);
            if ((structure == "appendix" && s.removedDiseases.contains("D29"))
                || (structure == "gallbladder" && s.removedDiseases.contains("D37"))) {
                if (version == 3) return fail();
                // An old engine bug could infect an already removed gallbladder. Repair only
                // that impossible diagnosis; preserve money, age, anatomy and all other organs.
                histories[d.id].lastRecoveryTick = now;
                activeIds.removeAll(d.id);
                const auto removed = s.removedDiseases; const bool transplanted = s.isTransplanted;
                s = OrganState(name); s.removedDiseases = removed; s.isTransplanted = transplanted;
            }
        }
        normalizeOrgan(s); loaded[name] = s;
    }
    for (auto it = histories.cbegin(); it != histories.cend(); ++it) {
        if (activeIds.contains(it.key())) {
            if (it->lastRecoveryTick >= it->lastOnsetTick) return fail();
        } else if (it->lastRecoveryTick < it->lastOnsetTick) return fail();
    }
    QList<ActiveSideEffect> effects;
    for (const auto& entry : r["effects"].toArray()) {
        const auto e = entry.toObject(); const auto med = Database::getInstance().getMedicineById(e["id"].toString());
        if (med.id.isEmpty() || !number(e,"ticks",1,100,true) || !number(e,"damage",0,20)) return fail();
        effects.append({med.name,e["ticks"].toInt(),e["damage"].toDouble(),med.id});
    }
    QMap<QString,int> cooldowns;
    const auto cd = r["cooldowns"].toObject();
    for (auto it = cd.begin(); it != cd.end(); ++it) {
        if (Database::getInstance().getMedicineById(it.key()).id.isEmpty() || !number(cd,it.key(),1,100,true)) return fail();
        cooldowns[it.key()] = it.value().toInt();
    }
    std::mt19937 restoredRng;
    if (version == 2) {
        majorOnset = majorRecovery = -1000;
        for (auto it = histories.cbegin(); it != histories.cend(); ++it)
            if (DiseaseRules::major(Database::getInstance().getDiseaseById(it.key()))) {
                majorOnset = std::max(majorOnset, it->lastOnsetTick);
                majorRecovery = std::max(majorRecovery, it->lastRecoveryTick);
            }
    }
    if (r["rng"].toString().size() > 20000) return fail();
    std::istringstream stream(r["rng"].toString().toStdString());
    if (!(stream >> restoredRng)) return fail();
    stream >> std::ws; if (!stream.eof()) return fail();
    // Commit only after every field and relationship has passed validation.
    health = r["health"].toDouble(); immunity = r["immunity"].toDouble(); age = savedAge;
    playerName = r["playerName"].toString(); familyWealth = r["wealth"].toInt(); luck = r["luck"].toInt();
    constitution = r["constitution"].toInt(); research = r["research"].toInt(); money = r["money"].toInt();
    maxAge = savedMax; lastCheckupAge = r["lastCheckupAge"].toInt(); hasTriggeredLevel6 = r["level6"].toBool();
    hasLifeExtensionTech = r["lifeTech"].toBool(); lifeExtensionTechName = r["techName"].toString();
    bpSys = r["bpSys"].toDouble(); bpDia = r["bpDia"].toDouble(); bloodSugar = r["sugar"].toDouble(); bloodLipids = r["lipids"].toDouble();
    currentTick = r["tick"].toInt(); tickAccumulator = r["fraction"].toDouble(); timeMultiplier = r["speed"].toDouble();
    finished = r["finished"].toBool(); pendingMilestone = false; milestoneResearchSucceeded = false;
    organs = loaded; activeSideEffects = effects; medicationCooldowns = cooldowns; rng = restoredRng;
    diseaseHistory = histories; lastMajorOnsetTick = majorOnset; lastMajorRecoveryTick = majorRecovery;
    trainedResearchPoints = trained; lastResearchAge = version == 3 ? r["lastResearchAge"].toInt() : savedAge;
    immuneSupplementReadyTick = version == 3 ? r["immuneReady"].toInt() : now + 10;
    attributesSet = version == 3 ? r["attributesSet"].toBool() : true;
    legacyTalentBudget = legacyTalents;
    paused = true;
    if (version == 2) emit gameMessage("旧存档已迁移：保留结构切除与科研投入，重建病史保护期，修复已切除部位再发病，取消肿瘤终身免疫；原文件未改写。");
    for (const auto& o : organs) { emit organStateChanged(o.name,o.isDiseased); emit organDetailUpdated(o.name); }
    publish(); return true;
}
