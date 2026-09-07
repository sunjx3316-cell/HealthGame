#include "DiseaseVisual.h"
#include <QMap>
#include <algorithm>
namespace {
struct Entry { int pattern; const char* title; const char* description; QVector3D focus; bool systemic; };
const QMap<QString,Entry>& catalog() {
    static const QMap<QString,Entry> entries{
        {"D01",{13,"先天结构异常","心脏剖面标示室间隔异常区域；外形与正常组织作对照。",{0,-0.05f,0.6f},false}},
        {"D02",{1,"咽部炎症","肺部视图包含上呼吸道，红色区域定位于咽部。",{0,1.6f,0},false}},
        {"D03",{1,"胃肠黏膜炎症","胃壁及相邻肠道出现弥漫性充血、肿胀。",{0,-0.2f,0.9f},true}},
        {"D04",{11,"黄染","肝脏视图呈胆色素相关的黄色变化。",{0,0,0.6f},true}},
        {"D05",{6,"手足口皮疹","皮肤局部放大呈红色斑疹与小疱，人体视图提示手足区域。",{0.1f,0,0.6f},false}},
        {"D06",{6,"湿疹","皮肤表面斑片、发红与不均匀隆起。",{-0.2f,0.1f,0.6f},false}},
        {"D07",{12,"白色黏膜斑膜","消化系统黏膜以白色斑块示意念珠菌相关变化。",{0,0.4f,0.9f},false}},
        {"D08",{8,"胆道闭锁","肝脏下方胆道高亮闭塞点，胆管呈局部扩张。",{-0.25f,-0.5f,0.55f},false}},
        {"D09",{1,"气道炎症","支气管树充血，沿气道分布病变。",{0,0.7f,0.35f},false}},
        {"D10",{12,"呼吸道感染","上呼吸道及支气管出现散在炎症灶。",{0,0.8f,0.45f},true}},
        {"D11",{8,"气道狭窄","支气管壁肿胀，分支周围以橙色环标示气流受限。",{0.5f,0.3f,0.6f},false}},
        {"D12",{1,"胃肠黏膜充血","内壁充血伴斑片状肿胀；可打开剖视检查胃皱襞。",{0.1f,0,0.9f},true}},
        {"D13",{6,"簇集水疱","皮肤表面出现成簇的小型半透明疱状隆起。",{-0.2f,0.1f,0.65f},false}},
        {"D14",{1,"局灶心肌炎症","心肌局部红色充血区，心搏略加快。",{0.3f,0.1f,0.7f},false}},
        {"D15",{4,"脂肪沉积","肝表面黄色脂肪纹理与体积变化；为代谢负担的游戏示意。",{0,0,0.7f},true}},
        {"D16",{6,"过敏性红斑","皮肤斑片发红，表面轻度隆起。",{0,0.1f,0.7f},false}},
        {"D17",{12,"肺叶实变","肺叶出现乳白色实变与充血区域。",{-0.65f,-0.1f,0.6f},false}},
        {"D18",{1,"胆囊炎症","肝下胆囊周围发红、肿胀。",{-0.3f,-0.55f,0.5f},false}},
        {"D19",{6,"风团","皮肤表面出现散在隆起的风团。",{0.1f,0,0.65f},true}},
        {"D20",{12,"肠道局部病变","相邻肠道突出局部异常，消化道黏膜出现炎症。",{0.45f,-0.7f,0.45f},false}},
        {"D21",{5,"冠脉供血受限","冠状动脉旁暗红色缺血区域，心搏幅度减弱。",{0.15f,-0.25f,0.8f},false}},
        {"D22",{1,"胃黏膜炎症","胃内壁可见红色充血纹理与局部水肿。",{0.2f,0.15f,0.8f},true}},
        {"D23",{4,"脂肪肝","肝脏增大、色泽偏黄，表面显示脂肪斑点。",{0,0,0.7f},true}},
        {"D24",{1,"肝组织炎症","肝脏弥漫性充血、轻度肿大。",{-0.2f,0,0.65f},true}},
        {"D25",{7,"胃溃疡","胃黏膜上显示边缘充血、中央凹陷的溃疡灶。",{0.3f,-0.2f,0.78f},false}},
        {"D26",{12,"肺部感染结节","肺尖及肺叶呈散在结节和斑片状病变。",{-0.65f,0.75f,0.6f},false}},
        {"D27",{1,"心肌炎症","心肌壁充血与局部肿胀，心搏变化。",{0.3f,-0.1f,0.7f},true}},
        {"D28",{13,"心肌肥厚","左心室外壁增厚，剖视中与内腔形成对照。",{0.3f,-0.35f,0.75f},false}},
        {"D29",{1,"阑尾炎症","胃部详情同时展示相邻肠道和阑尾，标出红肿的阑尾末端。",{0.75f,-1.1f,0.35f},false}},
        {"D30",{1,"肝炎","肝脏表面呈弥漫性炎症与斑驳色泽。",{0,0,0.7f},true}},
        {"D31",{3,"气道重塑","支气管增厚、纤维纹理增加，肺部呼吸运动受限。",{0.6f,0.1f,0.6f},true}},
        {"D32",{10,"心脏扩大","心室扩大、表面淤血，心搏幅度降低；病期越晚越明显。",{0,-0.2f,0.7f},true}},
        {"D33",{3,"肝硬化结节","肝表面出现细密结节和纤维条索，轮廓不再平滑。",{0,0,0.75f},true}},
        {"D34",{2,"胃部肿瘤","不规则局部肿块；随早、中、晚期增加体积及周围病灶。",{0.3f,0,0.8f},false}},
        {"D35",{2,"肺部肿瘤","肺叶显示不规则实体结节，晚期增加邻近卫星灶。",{-0.75f,0.2f,0.6f},false}},
        {"D36",{9,"节律异常","心脏外观变化较轻，以节律波形和不规则搏动表现功能异常。",{0,0.5f,0.65f},true}},
        {"D37",{8,"胆囊结石","胆囊内显示结石颗粒，囊壁充血。",{-0.3f,-0.55f,0.6f},false}},
        {"D38",{7,"溃疡出血","胃内壁凹陷灶周围显示暗红色出血斑。",{0.35f,-0.2f,0.8f},false}},
        {"D39",{13,"气胸","一侧肺体积收缩，外侧轮廓提示胸膜腔空间。",{-0.8f,0.2f,0.6f},false}},
        {"D40",{1,"胰腺炎症","胃后方的胰腺呈红肿变化，剖视可看到其位置。",{0.35f,-0.55f,0.15f},false}},
        {"D41",{10,"肺气肿","肺部膨大、局部大疱，呼吸起伏减弱。",{0.6f,0.5f,0.6f},true}},
        {"D42",{5,"心肌缺血坏死","冠状动脉供血区出现暗色损伤，病期越晚面积越大。",{0.25f,-0.25f,0.85f},false}},
        {"D43",{13,"瓣膜异常","心脏剖视显示瓣膜增厚与钙化样结节。",{0,0.4f,0.3f},false}},
        {"D44",{2,"肝部肿瘤","肝表面及组织内显示分期变化的不规则肿块。",{-0.4f,0.15f,0.7f},false}},
        {"D45",{2,"色素性肿瘤","皮肤表面出现边界不规则、颜色不均匀的色素病灶。",{0.15f,0.1f,0.65f},false}},
        {"D46",{12,"肺部重症感染","两肺出现较大范围炎症与实变，呼吸运动受限。",{0.5f,-0.15f,0.65f},true}},
        {"D47",{9,"不规则心搏","心房节律紊乱以不规则搏动和波形呈现。",{0,0.5f,0.6f},true}},
        {"D48",{3,"黏膜萎缩","胃壁皱襞变浅，黏膜色泽变淡，纤维纹理增多。",{0,0,0.8f},true}},
        {"D49",{11,"肝损伤","肝脏呈斑驳、黄染与局部暗色损伤。",{-0.2f,0.1f,0.6f},true}},
        {"D50",{10,"右心负担增大","右心扩大、淤血，搏动幅度随病期减弱。",{-0.25f,0,0.7f},false}},
        {"D51",{4,"代谢异常","肝脏以脂肪纹理表示代谢负担；血糖数值在体征栏独立呈现。",{0,0,0.7f},true}},
        {"D52",{3,"肝功能衰竭","肝表面显著斑驳、纤维化与结节变化。",{-0.2f,0,0.7f},true}},
        {"D53",{9,"心脏骤停","心脏停止搏动，器官色泽转暗，节律线近乎平直。",{0,-0.1f,0.7f},true}},
        {"D54",{9,"室性心律失常","心室出现快速异常搏动，节律波形明显紊乱。",{0,-0.2f,0.7f},true}}
    }; return entries;
}
}
bool DiseaseVisuals::contains(const QString& id) { return catalog().contains(id); }
DiseaseVisual DiseaseVisuals::forState(const OrganState& state) {
    // Intentionally ignore hiddenDisease, diseaseStage and hiddenTicks unless diagnosed.
    return state.isDiseased ? forDisease(state.currentDisease.id, state.diseaseStage,
        state.currentTreatmentStage, state.currentDisease.treatmentStages.size()) : DiseaseVisual{};
}
DiseaseVisual DiseaseVisuals::forDisease(const QString& id, int stage, int completed, int total) {
    DiseaseVisual v;
    if (!catalog().contains(id)) return v;
    const auto e = catalog().value(id);
    v.id = id; v.pattern = e.pattern; v.title = QString::fromUtf8(e.title);
    v.description = QString::fromUtf8(e.description); v.focus = e.focus; v.systemic = e.systemic;
    // Bind pathology to source-atlas structures, never just the coarse game slot.
    static const QMap<QString,QString> targets{
        {"D01","Left ventricle|Right ventricle"},{"D02","Oropharynx|Palatine tonsil"},
        {"D03","Mucosa of stomach|Jejunum|colon"},{"D04","Liver"},
        {"D05","forearm|digits|dorsum|palm|sole|Tongue"},{"D06","forearm"},
        {"D07","Tongue"},{"D08","Bile duct"},{"D09","bronch|Trachea"},
        {"D10","Oropharynx|bronch|Trachea"},{"D11","bronch"},
        {"D12","Mucosa of stomach|Jejunum|colon"},{"D13","forearm"},
        {"D14","ventricle"},{"D15","Liver"},{"D16","forearm"},
        {"D17","Inferior lobe of right lung"},{"D18","Gallbladder"},
        {"D19","forearm"},{"D20","Jejunum"},{"D21","Left ventricle|coronary artery"},
        {"D22","Mucosa of stomach"},{"D23","Liver"},{"D24","Liver"},
        {"D25","Mucosa of stomach"},{"D26","Superior lobe of right lung"},
        {"D27","ventricle|atrium"},{"D28","Left ventricle"},{"D29","Vermiform appendix"},
        {"D30","Liver"},{"D31","bronch"},{"D32","ventricle"},
        {"D33","Liver|segment of liver"},{"D34","Stomach"},
        {"D35","Superior lobe of right lung"},{"D36","atrium|ventricle"},
        {"D37","Gallbladder"},{"D38","Mucosa of stomach"},{"D39","lobe of left lung"},
        {"D40","Pancreas"},{"D41","lobe of"},{"D42","Left ventricle"},
        {"D43","leaflet"},{"D44","Liver"},{"D45","forearm"},
        {"D46","lobe of"},{"D47","atrium"},{"D48","Mucosa of stomach"},
        {"D49","Liver"},{"D50","Right ventricle|Right atrium"},
        {"D51","Liver"},{"D52","Liver|segment of liver"},
        {"D53","ventricle|atrium"},{"D54","ventricle"}
    };
    v.target=targets.value(id);
    v.amount = stage == 1 ? 0.34f : stage == 2 ? 0.62f : stage == 3 ? 1.0f : 0.6f;
    if (total > 0) v.amount *= 1.0f - 0.42f * std::clamp(float(completed) / total,0.0f,1.0f);
    switch (v.pattern) {
    case 1: v.color = QColor("#ee483d"); v.expansion = 0.04f; v.pulse = 1.2f; break;
    case 2: v.color = id == "D45" ? QColor("#3a1d29") : QColor("#bb6890"); break;
    case 3: v.color = QColor("#c8a79c"); v.pulse = 0.6f; break;
    case 4: v.color = QColor("#e1b567"); v.expansion = 0.1f; break;
    case 5: v.color = QColor("#522136"); v.pulse = 0.6f; break;
    case 6: v.color = QColor("#db4e69"); break;
    case 7: v.color = QColor("#711f37"); break;
    case 8: v.color = QColor("#e9b45f"); break;
    case 9: v.color = QColor("#bc5e75"); v.pulse = id == "D53" ? 0 : id == "D54" ? 2.6f : 1.6f; break;
    case 10: v.color = QColor("#89709c"); v.expansion = 0.14f; v.pulse = 0.45f; break;
    case 11: v.color = QColor("#cdb568"); break;
    case 12: v.color = QColor("#e1c9a7"); break;
    case 13: v.color = QColor("#b38c86"); v.expansion = id == "D39" ? -0.16f : 0.09f; break;
    }
    return v;
}
