#pragma once

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include "Models.h"

class GameEngine : public QObject {
    Q_OBJECT
public:
    explicit GameEngine(QObject *parent = nullptr);

    void initGame();
    void tick();
    void applyRealTimePenalty(); // 真实时间扣血（子弹时间专用）
    
    // 时间控制
    void setTimeMultiplier(double m);
    double getTimeMultiplier() const { return timeMultiplier; }

    // 玩家姓名与属性设置
    void setPlayerAttributes(const QString& name, int wealth, int lck, int consti, int res);
    QString getPlayerName() const { return playerName; }
    int getMoney() const { return money; }
    int getResearch() const { return research; }
    
    // 购买科研点
    bool buyResearchPoint();
    
    // 体检机制
    void performCheckup();
    
    // 绝症与长寿机制
    void extendMaxAge(int years);
    void setLifeExtensionTechName(const QString& name) { lifeExtensionTechName = name; hasLifeExtensionTech = true; }
    bool payForLifeExtension();
    
    // 存读档
    bool saveGame(const QString& filePath);
    bool loadGame(const QString& filePath);

    // 治疗动作：增加了剂量参数
    bool applyMedicine(const QString& organName, const QString& medicineId, int dosage);
    
    // 冲突检查
    bool checkConflict(const QString& medicineId, QString& conflictMedName) const;

    // Getters
    double getHealth() const { return health; }
    double getImmunity() const { return immunity; }
    int getAge() const { return age; }
    QMap<QString, OrganState> getOrgans() const { return organs; }

signals:
    void healthChanged(double newHealth);
    void immunityChanged(double newImmunity);
    void ageChanged(int newAge);
    void moneyChanged(int newMoney);
    void researchChanged(int newRes); // 新增
    void vitalsChanged(double sys, double dia, double sugar, double lipids);
    void organStateChanged(const QString& organName, bool isDiseased);
    void organDetailUpdated(const QString& organName); // 新增：用于通知 UI 刷新器官详情（如治疗进度）
    void gameOver(const QString& reason);
    void gameMessage(const QString& msg);
    void diseaseTriggered(); // 用于触发UI降速
    
    // 绝症与长寿事件
    void level6DiseaseEvent(bool success, const QString& diseaseName);
    void lifeExtensionEvent(bool success);
    void requireLifeExtensionPayment(const QString& techName);
    
    void checkupEvent(int age); // 新增：体检事件

private:
    void generateDiseases();
    void applyDamageAndHeal();
    void updateSideEffects();
    void updateImmunityCurve();
    double getNaturalDecayRate() const;
    void forceDeath(const QString& reason); // 辅助函数

    double health;
    double immunity; // 免疫系统 0-100
    int age; // 单位：岁
    QString playerName; // 玩家姓名
    
    // 玩家天赋与经济
    int familyWealth; // 家境
    int luck;         // 气运
    int constitution; // 健康
    int research;     // 科研
    int money;        // 金币
    
    // 绝症与长寿状态
    int maxAge;
    bool hasTriggeredLevel6;
    bool hasLifeExtensionTech;
    QString lifeExtensionTechName;

    // 实时生理指标
    double bpSys; // 收缩压 (高压)
    double bpDia; // 舒张压 (低压)
    double bloodSugar; // 血糖
    double bloodLipids; // 血脂

    QMap<QString, OrganState> organs;
    QList<ActiveSideEffect> activeSideEffects; // 当前记录的副作用历史
    
    double timeMultiplier; // 时间流速倍率
    int ticksPerYear;
    int currentTick;
};
