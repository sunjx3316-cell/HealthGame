#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QSet>
#include <algorithm>
#include <cmath>
#include "GameEngine.h"
#include "Database.h"
#include "DiseaseRules.h"

// A reproducible player-policy simulation, not an epidemiological model.
// Policy reads only publicly diagnosed conditions, vitals, age and funds.
// Private history is observed AFTER a tick exclusively for validation/reporting.
class BalanceSimulation : public QObject {
    Q_OBJECT
    struct Build {
        QString id;
        int wealth, luck, constitution, research;
    };
    struct Trial {
        int seed = 0;
        double lifespan = 0;
        double healthBeforeFinalTick = 0;
        bool won = false, unaffordableExtension = false;
        int totalOnsets = 0, primaryOnsets = 0, concurrentOnsets = 0, majorOnsets = 0;
        int treatments = 0, checkups = 0, vitalActions = 0, finalMoney = 0;
        qint64 treatmentSpend = 0, checkupSpend = 0, vitalSpend = 0, extensionSpend = 0;
        int minimumMajorGap = 10000;
        QString error;
    };
    const QList<Build> builds{
        {"wealth", 15, 5, 5, 5},
        {"luck", 5, 15, 5, 5},
        {"constitution", 5, 5, 15, 5},
        {"research", 5, 5, 5, 15},
        {"balanced", 8, 7, 8, 7}
    };

    QString activeId(const OrganState& o) const {
        if (o.isDiseased) return o.currentDisease.id;
        if (o.hasHiddenDisease) return o.hiddenDisease.id;
        return {};
    }

    bool observe(const GameEngine& e, const QMap<QString, DiseaseHistory>& before,
                 int previousMajorOnset, int previousMajorRecovery, Trial& result) const {
        const int now = e.absoluteTick();
        int newMajors = 0;
        for (auto it = e.diseaseHistory.cbegin(); it != e.diseaseHistory.cend(); ++it) {
            const auto old = before.value(it.key());
            const int delta = it.value().episodes - old.episodes;
            if (delta < 0 || delta > 1) {
                result.error = "Episode history changed non-monotonically: " + it.key(); return false;
            }
            const auto d = Database::getInstance().getDiseaseById(it.key());
            if (DiseaseRules::congenital(d) && it.value().episodes > 1) {
                result.error = "A congenital primary event occurred twice: " + it.key(); return false;
            }
            if (!delta) continue;
            if (it.value().lastOnsetTick != now) {
                result.error = "Onset timestamp differs from the simulation clock"; return false;
            }
            if (old.lastRecoveryTick >= 0 && now - old.lastRecoveryTick < DiseaseRules::recoveryTicks(d)) {
                result.error = "Disease-specific recovery protection violated: " + it.key(); return false;
            }
            ++result.totalOnsets;
            const auto organ = e.organs.value(d.organ);
            if (activeId(organ) == d.id) ++result.primaryOnsets;
            else ++result.concurrentOnsets;
            if (DiseaseRules::major(d)) {
                ++result.majorOnsets; ++newMajors;
                if (previousMajorOnset >= 0) {
                    const int gap = now - previousMajorOnset;
                    result.minimumMajorGap = std::min(result.minimumMajorGap, gap);
                    if (gap < 30) {
                        result.error = QString("Major onset spacing %1 < 30 ticks at %2").arg(gap).arg(now);
                        return false;
                    }
                }
                const int latestRecovery = std::max(previousMajorRecovery, e.lastMajorRecoveryTick);
                if (latestRecovery >= 0 && now - latestRecovery < 10) {
                    result.error = "Global post-major recovery protection violated"; return false;
                }
            }
        }
        if (newMajors > 1) {
            result.error = "Multiple major onsets in one tick"; return false;
        }
        return validateStates(e, result);
    }

    bool validateStates(const GameEngine& e, Trial& result) const {
        int majorCases = 0;
        QSet<QString> active;
        for (const auto& organ : e.organs) {
            if (organ.coexisting.size() > 2) {
                result.error = "Organ concurrency exceeds three conditions"; return false;
            }
            for (const auto& marker : organ.removedDiseases) {
                // Regression: a local tumour excision must NOT confer lifetime immunity.
                if (marker != "D29" && marker != "D37") {
                    result.error = "Non-anatomical permanent immunity marker: " + marker; return false;
                }
            }
            QList<OrganState> cases = organ.coexisting;
            OrganState primary = organ; primary.coexisting.clear(); cases.prepend(primary);
            for (const auto& c : cases) {
                if (!c.coexisting.isEmpty() || (c.isDiseased && c.hasHiddenDisease)) {
                    result.error = "Nested or contradictory disease state"; return false;
                }
                const auto id = activeId(c);
                if (id.isEmpty()) continue;
                if (active.contains(id)) {
                    result.error = "Duplicate active disease: " + id; return false;
                }
                active.insert(id);
                const auto d = Database::getInstance().getDiseaseById(id);
                majorCases += DiseaseRules::major(d);
                if (c.hasHiddenDisease && !DiseaseRules::latentProgressive(d)) {
                    result.error = "An acute/non-latent disease was hidden: " + id; return false;
                }
                if ((id == "D29" && organ.removedDiseases.contains("D29"))
                    || ((id == "D18" || id == "D37") && organ.removedDiseases.contains("D37"))) {
                    result.error = "Disease in a removed anatomical structure: " + id; return false;
                }
            }
        }
        if (majorCases > 1) {
            result.error = "Multiple simultaneous major conditions"; return false;
        }
        // When every legitimate restriction has expired, prior cancer alone
        // cannot leave a permanent lockout. This check never informs the policy.
        const int now = e.absoluteTick();
        if (majorCases == 0 && now - e.lastMajorOnsetTick >= 30 && now - e.lastMajorRecoveryTick >= 10) {
            for (const auto& id : {"D34", "D35", "D44", "D45"}) {
                const auto h = e.diseaseHistory.value(id);
                const auto d = Database::getInstance().getDiseaseById(id);
                const auto organ = e.organs.value(d.organ);
                if (h.lastRecoveryTick >= 0 && now - h.lastRecoveryTick >= DiseaseRules::recoveryTicks(d)
                    && e.getAge() >= d.minAge && e.getAge() <= d.maxAge
                    && !active.contains(id) && organ.coexisting.size() < 2 && !organ.isRemoved
                    && !e.diseaseEligible(d)) {
                    result.error = "Recovered cancer remained permanently ineligible: " + QString(id); return false;
                }
            }
        }
        if (!std::isfinite(e.getHealth()) || !std::isfinite(e.getImmunity())
            || e.getHealth() < 0 || e.getHealth() > 100 || e.getImmunity() < 0 || e.getImmunity() > 100
            || e.getMoney() < 0) {
            result.error = "Invalid vital/resource bounds during normal play"; return false;
        }
        return true;
    }

    void buyVitalSupport(GameEngine& e, const QString& id, Trial& result) const {
        if (e.isFinished()) return;
        const int moneyBefore = e.getMoney();
        if (e.applyMedicine("", id, 1)) ++result.vitalActions;
        result.vitalSpend += std::max(0, moneyBefore - e.getMoney());
    }

    void managePublicState(GameEngine& e, int elapsedTicks, QMap<QString, int>& supportUse,
                           Trial& result) const {
        if (e.isFinished()) return;
        if (e.hasPendingMilestone()) {
            if (e.getMoney() < e.getLifeExtensionCost()) {
                result.unaffordableExtension = true;
                e.declineLifeExtension();
                return;
            }
            e.setLifeExtensionTechName("模拟策略·寿命项目");
            const int before = e.getMoney();
            if (!e.payForLifeExtension()) {
                result.error = "An affordable valid life extension was rejected"; return;
            }
            result.extensionSpend += before - e.getMoney();
        }
        // Screening is scheduled from age and the displayed cooldown, never from hidden state.
        if (e.getAge() >= 30 && e.getAge() - e.getLastCheckupAge() >= e.getCheckupInterval()
            && e.getMoney() >= e.getCheckupCost()) {
            const int before = e.getMoney();
            if (e.performCheckup()) ++result.checkups;
            result.checkupSpend += std::max(0, before - e.getMoney());
        }

        QString selectedOrgan;
        Medicine selectedMedicine;
        int selectedSeverity = -1;
        const auto visibleOrgans = e.getOrgans();
        for (auto it = visibleOrgans.cbegin(); it != visibleOrgans.cend(); ++it) {
            const auto& organ = it.value();
            if (!organ.isDiseased) continue; // Do not inspect its hidden/coexisting latent fields.
            QVector<Medicine> options = Database::getInstance().getMedicinesForDisease(organ.currentDisease.id);
            std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) {
                if (a.price != b.price) return a.price < b.price;
                return a.id < b.id;
            });
            if (organ.currentDisease.id == "D52") {
                // Definitive transplant when affordable; otherwise support no more than once
                // per five ticks, tracked from our OWN publicly successful purchases.
                options.clear();
                if (e.getMoney() >= Database::getInstance().getMedicineById("M53").price)
                    options.append(Database::getInstance().getMedicineById("M53"));
                else if (elapsedTicks - supportUse.value(it.key(), -1000) >= 5)
                    options.append(Database::getInstance().getMedicineById("M52"));
            }
            for (const auto& med : options) {
                QString conflict;
                if (med.price > e.getMoney() || !e.medicineMatches(organ, med)
                    || e.checkConflict(med.id, conflict)) continue;
                if (organ.currentDisease.severity > selectedSeverity
                    || (organ.currentDisease.severity == selectedSeverity
                        && (selectedMedicine.id.isEmpty() || med.price < selectedMedicine.price))) {
                    selectedSeverity = organ.currentDisease.severity;
                    selectedOrgan = it.key(); selectedMedicine = med;
                }
                break;
            }
        }
        // At most one disease-treatment dose per real simulation tick, including staged care.
        if (!selectedMedicine.id.isEmpty()) {
            const int before = e.getMoney();
            const bool success = e.applyMedicine(selectedOrgan, selectedMedicine.id, 1);
            const int paid = std::max(0, before - e.getMoney());
            if (paid > 0) ++result.treatments;
            result.treatmentSpend += paid;
            if (success && selectedMedicine.id == "M52") supportUse[selectedOrgan] = elapsedTicks;
        }
        if (e.getSystolic() > 145 || e.getDiastolic() > 90) buyVitalSupport(e, "M_SUPP_02", result);
        if (e.getBloodSugar() > 7) buyVitalSupport(e, "M_SUPP_04", result);
        if (e.getBloodLipids() > 2.3) buyVitalSupport(e, "M_SUPP_03", result);
        // No research training or immunity purchases: isolate the initial talent allocation.
    }

    Trial runTrial(const Build& build, int seed) const {
        GameEngine e;
        e.rng.seed(static_cast<unsigned int>(seed)); // The only direct mutation of engine state.
        e.setPlayerAttributes(build.id, build.wealth, build.luck, build.constitution, build.research);
        Trial result; result.seed = seed;
        QMap<QString, int> supportUse;
        int elapsed = 0;
        while (!e.isFinished() && elapsed < 1110) {
            const auto historyBefore = e.diseaseHistory;
            const int priorMajorOnset = e.lastMajorOnsetTick, priorMajorRecovery = e.lastMajorRecoveryTick;
            result.healthBeforeFinalTick = e.getHealth();
            e.tick(); ++elapsed;
            if (!observe(e, historyBefore, priorMajorOnset, priorMajorRecovery, result)) break;
            managePublicState(e, elapsed, supportUse, result);
            if (!result.error.isEmpty() || !validateStates(e, result)) break;
        }
        if (result.error.isEmpty() && !e.isFinished()) result.error = "Simulation did not terminate by 1110 ticks";
        result.lifespan = e.getAge() + e.currentTick / 10.0;
        result.won = e.isFinished() && e.getAge() == 110;
        result.finalMoney = e.getMoney();
        if (result.primaryOnsets + result.concurrentOnsets != result.totalOnsets)
            result.error = "Primary/concurrent onset accounting mismatch";
        return result;
    }

    bool writeReport(const QString& path, const QString& report, QString& error) const {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            error = "Cannot create report directory: " + path; return false;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
        const auto bytes = report.toUtf8();
        if (file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
        return true;
    }

private slots:
    void metadataAndColdStartRegressions() {
        auto& db = Database::getInstance();
        db.init();
        for (const auto& id : {"D34", "D35", "D44", "D45"}) {
            QVERIFY(DiseaseRules::latentProgressive(db.getDiseaseById(id)));
            QVERIFY(!DiseaseRules::congenital(db.getDiseaseById(id)));
            QCOMPARE(DiseaseRules::recoveryTicks(db.getDiseaseById(id)), 50);
        }
        for (const auto& id : {"D32", "D42", "D50", "D53"})
            QVERIFY(!DiseaseRules::latentProgressive(db.getDiseaseById(id)));
        QVERIFY(DiseaseRules::removedStructure("M_CANCER_EARLY_OP").isEmpty());
        QVERIFY(DiseaseRules::removedStructure("M_CANCER_MID_OP").isEmpty());
        QCOMPARE(DiseaseRules::removedStructure("M29"), QString("appendix"));
        QCOMPARE(DiseaseRules::requiredStructure(db.getDiseaseById("D18")), QString("gallbladder"));
        QCOMPARE(DiseaseRules::requiredStructure(db.getDiseaseById("D37")), QString("gallbladder"));
        for (const auto& build : builds) {
            GameEngine e; e.rng.seed(1);
            e.setPlayerAttributes(build.id, build.wealth, build.luck, build.constitution, build.research);
            QCOMPARE(e.getAge(), 0); QCOMPARE(e.getResearch(), build.research);
            QCOMPARE(e.getMoney(), 30000 + build.wealth * 60000);
            QCOMPARE(e.getImmunity(), 45.0 + build.constitution * 1.5 - build.research * 0.5);
            const int money = e.getMoney();
            e.setPlayerAttributes("reroll", 15, 5, 5, 5);
            QCOMPARE(e.getPlayerName(), build.id); QCOMPARE(e.getMoney(), money);
            QVERIFY(e.diseaseHistory.isEmpty());
        }
    }

    void sameSeedSamePolicyIsDeterministic() {
        const auto a = runTrial(builds.last(), 417);
        const auto b = runTrial(builds.last(), 417);
        QVERIFY2(a.error.isEmpty(), qPrintable(a.error));
        QVERIFY2(b.error.isEmpty(), qPrintable(b.error));
        QCOMPARE(a.lifespan, b.lifespan);
        QCOMPARE(a.won, b.won);
        QCOMPARE(a.totalOnsets, b.totalOnsets);
        QCOMPARE(a.majorOnsets, b.majorOnsets);
        QCOMPARE(a.concurrentOnsets, b.concurrentOnsets);
        QCOMPARE(a.treatmentSpend, b.treatmentSpend);
        QCOMPARE(a.checkupSpend, b.checkupSpend);
        QCOMPARE(a.extensionSpend, b.extensionSpend);
        QCOMPARE(a.finalMoney, b.finalMoney);
        QCOMPARE(a.healthBeforeFinalTick, b.healthBeforeFinalTick);
    }

    void multiSeedTalentComparison() {
        bool valid = true;
        const QString seedSetting = qEnvironmentVariable("HEALTHGAME_BALANCE_SEEDS");
        const int seeds = seedSetting.isEmpty() ? 200 : seedSetting.toInt(&valid);
        QVERIFY2(valid && seeds >= 1 && seeds <= 2000, "HEALTHGAME_BALANCE_SEEDS must be between 1 and 2000.");
        QString aggregate = "build,seeds,mean_age,p10_age,median_age,p90_age,win_110_rate,unaffordable_extension_rate,"
            "mean_onsets,mean_primary_onsets,mean_concurrent_onsets,mean_major_onsets,"
            "mean_treatment_spend,mean_vital_spend,mean_checkup_spend,mean_extension_spend,"
            "mean_checkups,mean_treatments,mean_final_money,minimum_major_gap_ticks,mean_health_before_final_tick\n";
        QString details = "build,seed,age,win_110,unaffordable_extension,onsets,primary_onsets,concurrent_onsets,"
            "major_onsets,treatment_spend,vital_spend,checkup_spend,extension_spend,checkups,treatments,final_money,health_before_final_tick\n";
        qInfo().noquote() << QString("Fixed policy: %1 seeds per build; one treatment/tick; no training; screening age >= 30.").arg(seeds);
        for (const auto& build : builds) {
            QList<Trial> trials;
            QVector<double> ages;
            double ageSum = 0, wins = 0, fundingFailures = 0, onsets = 0, primary = 0, concurrent = 0, major = 0;
            double treatmentSpend = 0, vitalSpend = 0, checkupSpend = 0, extensionSpend = 0;
            double checkups = 0, treatments = 0, money = 0, healthBeforeFinalTick = 0;
            int minimumGap = 10000;
            for (int seed = 1; seed <= seeds; ++seed) {
                const auto result = runTrial(build, seed);
                QVERIFY2(result.error.isEmpty(), qPrintable(QString("%1 seed %2: %3").arg(build.id).arg(seed).arg(result.error)));
                trials.append(result); ages.append(result.lifespan); ageSum += result.lifespan;
                wins += result.won; fundingFailures += result.unaffordableExtension;
                onsets += result.totalOnsets; primary += result.primaryOnsets;
                concurrent += result.concurrentOnsets; major += result.majorOnsets;
                treatmentSpend += result.treatmentSpend; vitalSpend += result.vitalSpend;
                checkupSpend += result.checkupSpend; extensionSpend += result.extensionSpend;
                checkups += result.checkups; treatments += result.treatments; money += result.finalMoney;
                healthBeforeFinalTick += result.healthBeforeFinalTick;
                minimumGap = std::min(minimumGap, result.minimumMajorGap);
                const QStringList row{build.id, QString::number(seed), QString::number(result.lifespan, 'f', 1),
                    QString::number(result.won), QString::number(result.unaffordableExtension),
                    QString::number(result.totalOnsets), QString::number(result.primaryOnsets),
                    QString::number(result.concurrentOnsets), QString::number(result.majorOnsets),
                    QString::number(result.treatmentSpend), QString::number(result.vitalSpend),
                    QString::number(result.checkupSpend), QString::number(result.extensionSpend),
                    QString::number(result.checkups), QString::number(result.treatments), QString::number(result.finalMoney),
                    QString::number(result.healthBeforeFinalTick, 'f', 3)};
                details += row.join(',') + '\n';
            }
            std::sort(ages.begin(), ages.end());
            auto number = [](double n) { return QString::number(n, 'f', 3); };
            auto quantile = [&](double q) { return ages[int(std::floor(q * (ages.size() - 1)))]; };
            const QStringList row{build.id, QString::number(seeds), number(ageSum / seeds),
                number(quantile(0.1)), number(quantile(0.5)), number(quantile(0.9)), number(wins / seeds),
                number(fundingFailures / seeds), number(onsets / seeds), number(primary / seeds),
                number(concurrent / seeds), number(major / seeds), number(treatmentSpend / seeds),
                number(vitalSpend / seeds), number(checkupSpend / seeds), number(extensionSpend / seeds),
                number(checkups / seeds), number(treatments / seeds), number(money / seeds),
                minimumGap == 10000 ? QString("NA") : QString::number(minimumGap), number(healthBeforeFinalTick / seeds)};
            aggregate += row.join(',') + '\n';
            qInfo().noquote() << QString("%1: mean age %2; 110 wins %3/%4; money-blocked %5; major events %6/run; min major gap %7 ticks.")
                .arg(build.id).arg(ageSum / seeds, 0, 'f', 2).arg(int(wins)).arg(seeds)
                .arg(int(fundingFailures)).arg(major / seeds, 0, 'f', 2)
                .arg(minimumGap == 10000 ? "NA" : QString::number(minimumGap));
        }
        QString output = qEnvironmentVariable("HEALTHGAME_BALANCE_OUTPUT");
        if (output.isEmpty())
            output = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../verification/balance-simulation.csv");
        QString error;
        QVERIFY2(writeReport(output, aggregate, error), qPrintable(error));
        const QString detailsOutput = qEnvironmentVariable("HEALTHGAME_BALANCE_TRIALS_OUTPUT");
        if (!detailsOutput.isEmpty()) QVERIFY2(writeReport(detailsOutput, details, error), qPrintable(error));
        qInfo().noquote() << "Aggregate results:" << QDir::toNativeSeparators(QFileInfo(output).absoluteFilePath());
        qInfo().noquote() << "These are gameplay-policy measurements, not clinical probabilities or proof of optimal strategy.";
    }
};
QTEST_GUILESS_MAIN(BalanceSimulation)
#include "BalanceSimulation.moc"
