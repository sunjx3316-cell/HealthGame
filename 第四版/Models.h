#pragma once

#include <QString>
#include <QList>

// 疾病模型
struct Disease {
    QString id;
    QString name;
    QString organ;
    int minAge = 0;
    int maxAge = 110;
    double probability = 0;
    int severity = 1;
    QString symptoms;
    int damagePerTick = 0;
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
    int hpPenalty = 0;
    int durationTicks = 0;
    double probability = 0;
};

// 药品模型
struct Medicine {
    QString id;
    QString name;
    QString indication; // 对应疾病ID
    QString usage;
    QString sideEffectsDesc;
    int effectPercentage = 0;
    
    // 深度建模新增字段
    int safeDosage = 1;
    double penaltyCoefficient = 5;
    SideEffect sideEffectData;// 副作用具体数值
    QList<QString> incompatibleMeds; // 不可混用药物列表
    
    // 新增属性
    int price = 500;          // 默认价格
    QString type = "药剂";    // 类别: "药剂", "手术", "保健品"
};

// 当前服用的副作用状态
struct ActiveSideEffect {
    QString medicineName;
    int remainingTicks = 0;
    double hpPenaltyPerTick = 0;
    QString medicineId;
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
    
    // 新增：临床器官不可逆状态标记
    bool isRemoved; // 是否已被完全切除（如阑尾、胆囊），切除后器官不可再生任何病
    bool isTransplanted; // 是否已进行过移植（如心脏、肝脏）
    QList<QString> removedDiseases; // 兼容存档的结构标记：仅 D29=阑尾、D37=胆囊；肿瘤切除不产生终身免疫
    double damageTaken = 0; // 本次病程实际扣血；治疗不能借小病恢复其他来源的损伤
    int treatmentFailures = 0;
    int treatmentReadyTick = 0;
    int controlledUntilTick = 0;
    QList<OrganState> coexisting; // 同器官合并病；只保存一层，优先展示/治疗较严重的已确诊病
    
    OrganState() : name(""), isDiseased(false), diseasedDays(0), autoHealTimer(0), currentTreatmentStage(0), hasHiddenDisease(false), diseaseStage(0), hiddenTicks(0), isRemoved(false), isTransplanted(false) {}
    OrganState(QString n) : name(n), isDiseased(false), diseasedDays(0), autoHealTimer(0), currentTreatmentStage(0), hasHiddenDisease(false), diseaseStage(0), hiddenTicks(0), isRemoved(false), isTransplanted(false) {}
};

struct DiseaseHistory {
    int episodes = 0;
    int lastOnsetTick = -1000;
    int lastRecoveryTick = -1000;
};
