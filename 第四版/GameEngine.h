#pragma once
#include <QObject>
#include <QMap>
#include <random>
#include "Models.h"

class GameEngine : public QObject {
    Q_OBJECT
public:
    explicit GameEngine(QObject *parent = nullptr);
    void initGame();
    void tick();
    void advanceTime(double realSeconds);
    void setPaused(bool value) { paused = value; }
    bool isPaused() const { return paused; }
    bool isFinished() const { return finished; }
    bool hasPendingMilestone() const { return pendingMilestone; }
    void setTimeMultiplier(double m);
    double getTimeMultiplier() const { return timeMultiplier; }
    void setPlayerAttributes(const QString& name, int wealth, int lck, int consti, int res);
    QString getPlayerName() const { return playerName; }
    int getMoney() const { return money; }
    int getResearch() const { return research; }
    int getMaxAge() const { return maxAge; }
    int getLastCheckupAge() const { return lastCheckupAge; }
    int getAnnualIncome() const;
    int getResearchPointCost() const;
    int getLifeExtensionCost() const;
    int getCheckupCost() const;
    int getCheckupInterval() const;
    bool canBuyResearchPoint() const;
    bool buyResearchPoint();
    bool performCheckup();
    bool payForLifeExtension(int cost = -1, int years = 10);
    void setLifeExtensionTechName(const QString& name);
    void declineLifeExtension();
    bool saveGame(const QString& filePath);
    bool loadGame(const QString& filePath);
    QString lastError() const { return error; }
    bool applyMedicine(const QString& organName, const QString& medicineId, int dosage);
    bool checkConflict(const QString& medicineId, QString& conflictMedName) const;
    bool medicineMatches(const OrganState& organ, const Medicine& medicine) const;
    double getHealth() const { return health; }
    double getImmunity() const { return immunity; }
    int getAge() const { return age; }
    float visualAge() const { return float(age+(currentTick+tickAccumulator)/ticksPerYear); }
    QMap<QString, OrganState> getOrgans() const { return organs; }
    double getSystolic() const { return bpSys; }
    double getDiastolic() const { return bpDia; }
    double getBloodSugar() const { return bloodSugar; }
    double getBloodLipids() const { return bloodLipids; }

signals:
    void healthChanged(double);
    void immunityChanged(double);
    void ageChanged(int);
    void moneyChanged(int);
    void researchChanged(int);
    void vitalsChanged(double, double, double, double);
    void organStateChanged(const QString&, bool);
    void organDetailUpdated(const QString&);
    void stateChanged();
    void gameOver(const QString&);
    void gameMessage(const QString&);
    void diseaseTriggered();
    void level6DiseaseEvent(bool, const QString&);
    void lifeExtensionEvent(bool);
    void requireLifeExtensionPayment(const QString&);
    void checkupEvent(int);

private:
    friend class GameEngineTests;
    friend class BalanceSimulation;
    void generateDiseases();
    void updateYear();
    void checkMilestone();
    void applyDamageAndHeal();
    void updateSideEffects();
    void updateImmunityCurve();
    void updateDisease(OrganState&);
    bool diseaseEligible(const Disease&) const;
    void startDisease(const Disease&);
    void normalizeOrgan(OrganState&);
    double riskWeight(const Disease&) const;
    int absoluteTick() const { return age * ticksPerYear + currentTick; }
    double diseaseDamage(const OrganState&) const;
    void revealDisease(OrganState&);
    void cure(OrganState&, double recovery);
    void forceDeath(const QString&);
    bool checkDeath(const QString& cause = {});
    void publish();
    double random();
    int randomInt(int upper);

    double health = 100, immunity = 50;
    int age = 0;
    QString playerName = "无名氏";
    int familyWealth = 0, luck = 0, constitution = 0, research = 0, money = 10000;
    int maxAge = 80, lastCheckupAge = -3;
    bool hasTriggeredLevel6 = false, hasLifeExtensionTech = false;
    bool pendingMilestone = false, milestoneResearchSucceeded = false;
    QString lifeExtensionTechName;
    double bpSys = 120, bpDia = 80, bloodSugar = 5, bloodLipids = 1.5;
    QMap<QString, OrganState> organs;
    QList<ActiveSideEffect> activeSideEffects;
    QMap<QString, int> medicationCooldowns;
    QMap<QString, DiseaseHistory> diseaseHistory;
    int lastMajorOnsetTick = -1000, lastMajorRecoveryTick = -1000;
    int lastResearchAge = -3, trainedResearchPoints = 0, immuneSupplementReadyTick = 0;
    bool attributesSet = false;
    bool legacyTalentBudget = false;
    double timeMultiplier = 2.0, tickAccumulator = 0;
    static constexpr int ticksPerYear = 10;
    int currentTick = 0;
    bool paused = false, finished = false, inTick = false;
    QString error;
    std::mt19937 rng;
};
