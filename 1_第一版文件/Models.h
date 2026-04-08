#pragma once

#include <QString>
#include <QList>

// 疾病模型
struct Disease {
    QString id;
    QString name;
    QString organ;
    int minAge;
    int maxAge;
    double probability; // 0.0 - 1.0
    int severity;       // 1-5
    QString symptoms;
    int damagePerTick;  // 根据 severity 设定: 1,2级-5; 3,4级-15; 5级-30
    QString treatmentGuide; // 【新增】专业的治病科普指南
    
    // 5级绝症的分阶段治疗方案 (存储每一阶段需要的药物/手术 ID)
    QList<QString> treatmentStages; 
    
    // 5级疾病的早、中、晚期治疗方案
    QList<QString> earlyTreatment;
    QList<QString> midTreatment;
    QList<QString> lateTreatment;
};

// 药品副作用数据结构
struct SideEffect {
    int hpPenalty;        // 生命值扣减值
    int durationTicks;    // 持续时间
    double probability;   // 触发概率 (0.0-1.0)
};

// 药品模型
struct Medicine {
    QString id;
    QString name;
    QString indication; // 对应疾病ID
    QString usage;
    QString sideEffectsDesc;
    int effectPercentage; // 0-100 (治疗效果值)
    
    // 深度建模新增字段
    int safeDosage;           // 安全剂量
    double penaltyCoefficient;// 过量服用时的单位惩罚系数
    SideEffect sideEffectData;// 副作用具体数值
    QList<QString> incompatibleMeds; // 不可混用药物列表
    
    // 新增属性
    int price = 500;          // 默认价格
    QString type = "药剂";    // 类别: "药剂", "手术", "保健品"
};

// 当前服用的副作用状态
struct ActiveSideEffect {
    QString medicineName;
    int remainingTicks;
    int hpPenaltyPerTick;
};

// 器官状态模型
struct OrganState {
    QString name;
    bool isDiseased;
    Disease currentDisease;
    int diseasedDays;
    int autoHealTimer; // 自愈倒计时 (针对轻度疾病)
    int currentTreatmentStage; // 针对分阶段治疗的当前进度
    
    // 隐藏疾病（用于5级绝症的早中期演化）
    bool hasHiddenDisease;
    Disease hiddenDisease;
    int diseaseStage; // 1:早期, 2:中期, 3:晚期
    int hiddenTicks;
    
    OrganState() : name(""), isDiseased(false), diseasedDays(0), autoHealTimer(0), currentTreatmentStage(0), hasHiddenDisease(false), diseaseStage(0), hiddenTicks(0) {}
    OrganState(QString n) : name(n), isDiseased(false), diseasedDays(0), autoHealTimer(0), currentTreatmentStage(0), hasHiddenDisease(false), diseaseStage(0), hiddenTicks(0) {}
};
