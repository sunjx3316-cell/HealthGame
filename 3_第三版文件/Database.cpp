#include "Database.h"

Database& Database::getInstance() {
    static Database instance;
    return instance;
}

void Database::init() {
    if (!diseases.isEmpty()) return; // 已经初始化过了

    // 辅助宏用于添加疾病
    #define ADD_DISEASE(id, name, organ, minAge, maxAge, prob, severity, symptoms, damage, guide) \
        diseases[id] = {id, name, organ, minAge, maxAge, prob, severity, symptoms, damage, guide, {}}

    // 辅助宏用于添加药物 (修正初始化列表，补全所有字段)
    // 默认 effectPercentage=100, safeDosage=1, penaltyCoefficient=5.0, 空副作用, 空不兼容列表
    #define ADD_MEDICINE(id, name, indication, usage, sideEffects, priceVal, typeVal) \
        medicines[id] = {id, name, indication, usage, sideEffects, 100, 1, 5.0, SideEffect{0, 0, 0.0}, {}, priceVal, typeVal}

    // ==========================================
    // 疾病库 (50种) 严格限定器官: 心脏, 肺部, 胃部, 肝脏, 皮肤
    // 婴儿期 (0-5)
    ADD_DISEASE("D01", "先天性心脏病", "心脏", 0, 5, 0.01, 5, "心杂音，呼吸急促，喂养困难", 30, "【诊疗指南】先天性心脏病是胎儿时期心脏血管发育异常所致。建议尽早采用【先心病手术包】进行外科干预，标准用量：1。");
    ADD_DISEASE("D02", "婴儿肺炎", "肺部", 0, 5, 0.05, 4, "咳嗽，发烧，呼吸急促", 15, "【诊疗指南】婴幼儿肺炎多由细菌或病毒引起，导致肺泡充血水肿。推荐使用【阿莫西林干混悬剂】抗感染，标准用量：1。");
    ADD_DISEASE("D03", "轮状病毒肠炎", "胃部", 0, 5, 0.08, 3, "腹泻，呕吐，发烧，脱水", 15, "【诊疗指南】轮状病毒极易引起婴幼儿严重腹泻。首要治疗是防脱水，建议使用【口服补液盐】补充电解质，标准用量：1。");
    ADD_DISEASE("D04", "婴儿黄疸", "肝脏", 0, 1, 0.10, 2, "皮肤和眼白发黄", 5, "【诊疗指南】由于新生儿胆红素代谢异常导致。建议使用【蓝光照射治疗仪】加速胆红素分解，标准用量：1。");
    ADD_DISEASE("D05", "手足口病", "皮肤", 0, 5, 0.06, 2, "发烧，口腔溃疡，手足皮疹", 5, "【诊疗指南】由肠道病毒引起的传染病，主要表现在皮肤和口腔。推荐使用【利巴韦林颗粒】抗病毒治疗，标准用量：1。");
    ADD_DISEASE("D06", "婴儿湿疹", "皮肤", 0, 5, 0.15, 1, "皮肤红斑，瘙痒", 5, "【诊疗指南】特应性皮炎，与遗传和过敏有关。建议外涂【氢化可的松软膏】消炎止痒，标准用量：1。");
    ADD_DISEASE("D07", "鹅口疮", "胃部", 0, 2, 0.04, 1, "口腔内白色斑块", 5, "【诊疗指南】由白色念珠菌感染引起的口腔黏膜炎症（归属于消化道前端）。建议使用【制霉菌素涂剂】，标准用量：1。");
    ADD_DISEASE("D08", "先天性胆道闭锁", "肝脏", 0, 1, 0.005, 5, "黄疸持续，灰白便", 30, "【诊疗指南】胆管发育异常导致胆汁无法排出，严重损害肝脏。唯一有效方法是【葛西手术包】进行胆道重建，标准用量：1。");
    ADD_DISEASE("D09", "支气管炎", "肺部", 0, 5, 0.10, 2, "咳嗽，喘息", 5, "【诊疗指南】支气管黏膜发炎。初期可使用【小儿止咳糖浆】缓解症状，标准用量：1。");
    ADD_DISEASE("D10", "小儿感冒", "肺部", 0, 5, 0.30, 1, "流涕，轻度发烧", 5, "【诊疗指南】上呼吸道病毒感染。推荐【小儿氨酚黄那敏颗粒】缓解感冒症状，标准用量：1。");

    // 青年期 (13-18)
    ADD_DISEASE("D11", "青少年哮喘", "肺部", 13, 18, 0.08, 3, "反复喘息，咳嗽，胸闷", 15, "【诊疗指南】气道慢性炎症导致的呼吸道高反应。急性发作期需使用【沙丁胺醇气雾剂】迅速扩张支气管，标准用量：1-2。");
    ADD_DISEASE("D12", "急性胃肠炎", "胃部", 13, 18, 0.12, 2, "腹痛，恶心，呕吐，腹泻", 5, "【诊疗指南】饮食不洁引起的胃肠道急性炎症。建议服用【蒙脱石散】保护胃肠黏膜并止泻，标准用量：1。");
    ADD_DISEASE("D13", "青春痘(重度痤疮)", "皮肤", 13, 18, 0.15, 2, "面部红肿硬结，囊肿", 5, "【诊疗指南】青春期激素分泌旺盛导致的毛囊皮脂腺炎症。局部可涂抹【阿昔洛韦乳膏】（暂代抗炎药）抗感染，标准用量：1。");
    ADD_DISEASE("D14", "流行性腮腺炎", "胃部", 13, 18, 0.03, 2, "腮腺肿痛，发热", 5, "【诊疗指南】由腮腺炎病毒引起的急性呼吸道传染病（影响消化腺）。发热期可服用【布洛芬混悬液】退热镇痛，标准用量：1。");
    ADD_DISEASE("D15", "青少年肥胖症", "肝脏", 13, 18, 0.15, 1, "体重超标，活动气促", 5, "【诊疗指南】长期热量摄入超标，极易导致脂肪肝。重度肥胖需遵医嘱使用【奥利司他】控制脂肪吸收，标准用量：1。");
    ADD_DISEASE("D16", "过敏性鼻炎", "肺部", 13, 18, 0.20, 1, "打喷嚏，流清涕", 5, "【诊疗指南】呼吸道对过敏原的高敏反应。推荐使用抗组胺药物【氯雷他定片】，标准用量：1。");
    ADD_DISEASE("D17", "风湿热", "心脏", 13, 18, 0.01, 4, "发热，关节痛，心脏炎", 15, "【诊疗指南】链球菌感染后引发的全身性结缔组织病，常侵犯心脏瓣膜。需立即静脉滴注【青霉素G注射液】清除链球菌，标准用量：1。");
    ADD_DISEASE("D18", "大叶性肺炎", "肺部", 13, 18, 0.02, 4, "高热，寒战，胸痛，铁锈色痰", 15, "【诊疗指南】肺炎链球菌引起的肺大叶急性炎症。首选强效抗生素【头孢曲松钠】静脉注射，标准用量：1。");
    ADD_DISEASE("D19", "甲型肝炎", "肝脏", 13, 18, 0.01, 3, "食欲减退，恶心，黄疸", 15, "【诊疗指南】甲肝病毒引起的急性肝脏炎症。使用【复方甘草酸苷片】进行保肝降酶治疗，标准用量：1。");
    ADD_DISEASE("D20", "蛔虫病", "胃部", 13, 18, 0.04, 1, "脐周阵发性疼痛，食欲不振", 5, "【诊疗指南】最常见的肠道寄生虫病。服用驱虫药【阿苯达唑片】即可有效杀灭成虫，标准用量：1。");

    // 成年期 (18-60)
    ADD_DISEASE("D21", "冠心病", "心脏", 18, 60, 0.05, 4, "心前区疼痛，胸闷，心悸", 15, "【诊疗指南】冠状动脉粥样硬化导致心肌缺血。心绞痛发作时立即舌下含服【硝酸甘油片】，标准用量：1。");
    ADD_DISEASE("D22", "慢性胃炎", "胃部", 18, 60, 0.25, 2, "上腹痛，饱胀感，反酸", 5, "【诊疗指南】胃黏膜的慢性炎症。服用质子泵抑制剂【奥美拉唑肠溶胶囊】抑制胃酸分泌，标准用量：1。");
    ADD_DISEASE("D23", "脂肪肝", "肝脏", 18, 60, 0.20, 2, "乏力，右上腹隐痛", 5, "【诊疗指南】肝细胞内脂肪过度蓄积。建议服用【多烯磷脂酰胆碱】修复肝细胞膜，标准用量：1。");
    ADD_DISEASE("D24", "酒精性肝炎", "肝脏", 18, 60, 0.08, 3, "恶心，黄疸，肝区痛", 15, "【诊疗指南】长期大量饮酒导致的肝脏炎症。戒酒是基础，辅以【美他多辛】加速酒精代谢，标准用量：1。");
    ADD_DISEASE("D25", "胃溃疡", "胃部", 18, 60, 0.10, 3, "餐后规律性上腹痛", 15, "【诊疗指南】胃黏膜被胃酸/胃蛋白酶自身消化而形成的溃疡。推荐使用【雷贝拉唑+阿莫西林+克拉霉素+铋剂】四联疗法根除幽门螺杆菌，标准用量：1。");
    ADD_DISEASE("D26", "肺结核", "肺部", 18, 60, 0.02, 4, "低热，盗汗，咯血", 15, "【诊疗指南】结核分枝杆菌引起的慢性传染病。必须坚持服用抗结核药物【异烟肼+利福平】，标准用量：1。");
    ADD_DISEASE("D27", "心肌炎", "心脏", 18, 60, 0.01, 4, "心悸，胸痛，疲乏", 15, "【诊疗指南】心肌发生局限性或弥漫性炎症。建议服用【辅酶Q10】营养心肌，标准用量：1。");
    ADD_DISEASE("D28", "高血压性心脏病", "心脏", 30, 60, 0.15, 3, "头晕，心悸，劳力性呼吸困难", 15, "【诊疗指南】长期血压偏高导致左心室肥厚扩大。使用【硝苯地平控释片】平稳降压，标准用量：1。");
    ADD_DISEASE("D29", "急性阑尾炎", "胃部", 18, 60, 0.05, 3, "转移性右下腹痛，恶心", 15, "【诊疗指南】阑尾管腔阻塞或继发细菌感染。一经确诊应尽早实施【阑尾切除术】，标准用量：1。");
    ADD_DISEASE("D30", "乙型肝炎", "肝脏", 18, 60, 0.05, 4, "乏力，食欲减退，肝区痛", 15, "【诊疗指南】乙肝病毒(HBV)引起的肝脏病变。需长期服用核苷类抗病毒药【恩替卡韦】，标准用量：1。");
    ADD_DISEASE("D31", "慢性阻塞性肺病(COPD)", "肺部", 40, 60, 0.05, 4, "慢性咳嗽，咳痰，气短", 15, "【诊疗指南】气流受限不完全可逆的慢性肺部疾病。推荐吸入【噻托溴铵粉吸入剂】维持气道扩张，标准用量：1。");
    ADD_DISEASE("D32", "重度心力衰竭", "心脏", 40, 100, 0.02, 5, "呼吸困难，水肿，极度乏力", 30, "【诊疗指南】心脏泵血功能显著降低的晚期表现。需分早中晚期进行治疗干预。");
    ADD_DISEASE("D33", "肝硬化", "肝脏", 40, 60, 0.02, 4, "腹水，黄疸，出血倾向", 30, "【诊疗指南】肝组织弥漫性纤维化及假小叶形成。重度腹水时需静脉滴注【白蛋白注射液】提高血浆渗透压，标准用量：1。");
    ADD_DISEASE("D34", "胃部恶性肿瘤", "胃部", 40, 100, 0.01, 5, "进行性消瘦，黑便，剧烈腹痛", 30, "【诊疗指南】胃黏膜上皮恶性肿瘤。发现越早治愈率越高，晚期预后极差。");
    ADD_DISEASE("D35", "肺部恶性肿瘤", "肺部", 40, 100, 0.01, 5, "刺激性干咳，咯血，胸痛", 30, "【诊疗指南】支气管黏膜恶变。早中期可手术切除，晚期依赖靶向与化疗。");
    ADD_DISEASE("D36", "心律失常", "心脏", 18, 60, 0.10, 2, "心悸，漏搏感", 5, "【诊疗指南】心脏电传导系统异常。可口服抗心律失常药物【胺碘酮片】控制，标准用量：1。");
    ADD_DISEASE("D37", "胆结石", "肝脏", 18, 60, 0.10, 3, "右上腹绞痛，放射至肩背", 15, "【诊疗指南】胆汁淤积形成的结石，归于肝胆系统。症状频发时建议行【腹腔镜胆囊切除术】，标准用量：1。");
    ADD_DISEASE("D38", "反流性食管炎", "胃部", 18, 60, 0.12, 2, "烧心，反酸，胸骨后痛", 5, "【诊疗指南】胃酸反流腐蚀食管黏膜。需服用强效抑酸药【潘多拉唑钠肠溶片】，标准用量：1。");
    ADD_DISEASE("D39", "自发性气胸", "肺部", 18, 40, 0.01, 4, "突发单侧胸痛，呼吸困难", 15, "【诊疗指南】肺大疱破裂导致气体进入胸膜腔。需紧急进行【胸腔闭式引流】排气减压，标准用量：1。");
    ADD_DISEASE("D40", "急性胰腺炎", "胃部", 18, 60, 0.02, 4, "剧烈上腹痛，呕吐，发热", 15, "【诊疗指南】胰酶在胰腺内被激活引起的组织自身消化（消化系统急症）。需使用【奥曲肽注射液】强效抑制胰液分泌，标准用量：1。");

    // 老年期 (60+)
    ADD_DISEASE("D41", "老年性肺气肿", "肺部", 60, 100, 0.20, 3, "桶状胸，进行性呼吸困难", 15, "【诊疗指南】终末细支气管远端气腔弹性减退、过度膨胀。推荐长期使用【布地奈德福莫特罗粉吸入剂】抗炎平喘，标准用量：1。");
    ADD_DISEASE("D42", "急性心肌梗死", "心脏", 40, 100, 0.08, 5, "剧烈胸骨后疼痛，大汗，濒死感", 30, "【诊疗指南】冠状动脉急性闭塞导致心肌坏死。必须在黄金时间窗内进行溶栓或支架植入，不同发病阶段方案不同。");
    ADD_DISEASE("D43", "老年瓣膜性心脏病", "心脏", 60, 100, 0.10, 4, "心悸，气促，心绞痛", 15, "【诊疗指南】心脏瓣膜随年龄增长发生退行性钙化。重度狭窄需进行【瓣膜置换术】，标准用量：1。");
    ADD_DISEASE("D44", "肝脏恶性肿瘤", "肝脏", 40, 100, 0.02, 5, "肝区剧痛，消瘦，严重黄疸", 30, "【诊疗指南】肝细胞或肝内胆管上皮细胞恶性肿瘤。早中期可手术，晚期依赖分子靶向药物。");
    ADD_DISEASE("D45", "黑色素瘤", "皮肤", 40, 100, 0.03, 5, "皮肤色素痣突然增大，边缘不规则", 30, "【诊疗指南】高度恶性的皮肤肿瘤。早期扩大切除预后较好，晚期极易转移。");
    ADD_DISEASE("D46", "肺部感染(重症)", "肺部", 60, 100, 0.15, 4, "发热，浓痰，意识障碍", 15, "【诊疗指南】老年人免疫力低下易引发重症肺炎。需使用广谱抗生素【哌拉西林他唑巴坦】静脉滴注，标准用量：1。");
    ADD_DISEASE("D47", "心房颤动", "心脏", 60, 100, 0.12, 3, "心跳极度不规则，心悸", 15, "【诊疗指南】心房失去规则收缩，易形成血栓。需使用【美托洛尔缓释片】控制心室率，标准用量：1。");
    ADD_DISEASE("D48", "萎缩性胃炎", "胃部", 60, 100, 0.30, 2, "食欲减退，贫血，上腹胀", 5, "【诊疗指南】胃黏膜固有腺体萎缩。可服用【胃复春片】等中成药改善胃黏膜血液循环，标准用量：1。");
    ADD_DISEASE("D49", "药物性肝损伤", "肝脏", 60, 100, 0.05, 3, "乏力，皮疹，黄疸", 15, "【诊疗指南】老年人常多药同服，极易加重肝脏代谢负担。需立即停用可疑药物并静滴【异甘草酸镁注射液】保肝，标准用量：1。");
    ADD_DISEASE("D50", "重症肺心病", "心脏", 40, 100, 0.08, 5, "持续严重缺氧，极度紫绀，右心衰竭", 30, "【诊疗指南】慢性肺部疾病导致右心室严重肥厚、心力衰竭晚期。需多阶段综合抢救。");

    // ==========================================
    // 药物库
    // 针对各种疾病的药物，简写一部分通用药物
    ADD_MEDICINE("M01", "先心病手术包", "D01", "手术治疗", "创伤，感染风险", 50000, "手术");
    ADD_MEDICINE("M02", "阿莫西林干混悬剂", "D02", "口服，每日3次", "胃肠道反应，皮疹", 35, "药剂");
    ADD_MEDICINE("M03", "口服补液盐", "D03", "随水冲服", "无明显副作用", 10, "药剂");
    ADD_MEDICINE("M04", "蓝光照射治疗仪", "D04", "物理治疗", "发热，皮疹", 500, "药剂");
    ADD_MEDICINE("M05", "利巴韦林颗粒", "D05", "口服", "贫血，乏力", 25, "药剂");
    ADD_MEDICINE("M06", "氢化可的松软膏", "D06", "外用涂抹", "皮肤萎缩", 15, "药剂");
    ADD_MEDICINE("M07", "制霉菌素涂剂", "D07", "口腔涂抹", "局部刺激", 20, "药剂");
    ADD_MEDICINE("M08", "葛西手术包", "D08", "手术治疗", "胆管炎并发症", 80000, "手术");
    ADD_MEDICINE("M09", "小儿止咳糖浆", "D09", "口服", "嗜睡", 18, "药剂");
    ADD_MEDICINE("M10", "小儿氨酚黄那敏颗粒", "D10", "口服", "轻度嗜睡", 22, "药剂");

    ADD_MEDICINE("M11", "沙丁胺醇气雾剂", "D11", "吸入", "心悸，手抖", 45, "药剂");
    ADD_MEDICINE("M12", "蒙脱石散", "D12", "口服", "便秘", 30, "药剂");
    ADD_MEDICINE("M13", "阿昔洛韦乳膏", "D13", "外用", "局部灼热", 15, "药剂");
    ADD_MEDICINE("M14", "布洛芬混悬液", "D14", "口服退热", "胃肠不适", 28, "药剂");
    ADD_MEDICINE("M15", "奥利司他", "D15", "口服", "脂肪泻", 150, "药剂");
    ADD_MEDICINE("M16", "氯雷他定片", "D16", "口服", "头痛，嗜睡", 35, "药剂");
    ADD_MEDICINE("M17", "青霉素G注射液", "D17", "静脉滴注", "过敏性休克风险", 80, "药剂");
    ADD_MEDICINE("M18", "头孢曲松钠", "D18", "静脉注射", "皮疹，腹泻", 120, "药剂");
    ADD_MEDICINE("M19", "复方甘草酸苷片", "D19", "口服", "低血钾，血压升高", 60, "药剂");
    ADD_MEDICINE("M20", "阿苯达唑片", "D20", "口服", "恶心，腹痛", 12, "药剂");

    ADD_MEDICINE("M21", "硝酸甘油片", "D21", "舌下含服", "头痛，面部潮红", 25, "药剂");
    ADD_MEDICINE("M22", "奥美拉唑肠溶胶囊", "D22", "口服", "口干，恶心", 40, "药剂");
    ADD_MEDICINE("M23", "多烯磷脂酰胆碱", "D23", "口服", "胃肠道不适", 80, "药剂");
    ADD_MEDICINE("M24", "美他多辛", "D24", "口服", "极少不良反应", 90, "药剂");
    ADD_MEDICINE("M25", "雷贝拉唑+阿莫西林+克拉霉素+铋剂", "D25", "四联疗法口服", "肠道菌群失调", 200, "药剂");
    ADD_MEDICINE("M26", "异烟肼+利福平", "D26", "长期口服", "肝损伤，周围神经炎", 150, "药剂");
    ADD_MEDICINE("M27", "辅酶Q10", "D27", "口服", "胃部不适", 120, "药剂");
    ADD_MEDICINE("M28", "硝苯地平控释片", "D28", "口服", "下肢水肿，面红", 50, "药剂");
    ADD_MEDICINE("M29", "阑尾切除术", "D29", "微创手术", "切口感染", 8000, "手术");
    ADD_MEDICINE("M30", "恩替卡韦", "D30", "口服", "头痛，疲劳", 180, "药剂");
    ADD_MEDICINE("M31", "噻托溴铵粉吸入剂", "D31", "吸入", "口干", 160, "药剂");
    ADD_MEDICINE("M32", "呋塞米片", "D32", "口服利尿", "电解质紊乱", 10, "药剂");
    ADD_MEDICINE("M33", "白蛋白注射液", "D33", "静脉滴注", "发热，寒战", 400, "药剂");
    ADD_MEDICINE("M34", "胃癌早期内镜切除", "D34", "手术", "出血，穿孔", 15000, "手术");
    ADD_MEDICINE("M35", "肺癌早期胸腔镜手术", "D35", "手术", "出血，感染", 30000, "手术");
    ADD_MEDICINE("M36", "胺碘酮片", "D36", "口服", "甲状腺功能异常", 45, "药剂");
    ADD_MEDICINE("M37", "腹腔镜胆囊切除术", "D37", "手术", "胆管损伤", 10000, "手术");
    ADD_MEDICINE("M38", "潘多拉唑钠肠溶片", "D38", "口服", "头痛，腹泻", 45, "药剂");
    ADD_MEDICINE("M39", "胸腔闭式引流", "D39", "微创操作", "复张性肺水肿", 1500, "手术");
    ADD_MEDICINE("M40", "奥曲肽注射液", "D40", "皮下注射", "注射部位疼痛", 300, "药剂");

    ADD_MEDICINE("M41", "布地奈德福莫特罗粉吸入剂", "D41", "吸入", "鹅口疮", 220, "药剂");
    ADD_MEDICINE("M42", "阿替普酶溶栓治疗", "D42", "静脉注射", "出血危险", 5000, "药剂");
    ADD_MEDICINE("M43", "瓣膜置换术", "D43", "开胸手术", "抗凝并发症", 100000, "手术");
    ADD_MEDICINE("M44", "索拉非尼", "D44", "靶向口服", "手足综合征", 12000, "药剂");
    ADD_MEDICINE("M45", "氟尿嘧啶化疗", "D45", "静脉化疗", "骨髓抑制，脱发", 3000, "药剂");
    ADD_MEDICINE("M46", "哌拉西林他唑巴坦", "D46", "静脉滴注", "过敏反应", 150, "药剂");
    ADD_MEDICINE("M47", "美托洛尔缓释片", "D47", "口服", "心动过缓", 30, "药剂");
    ADD_MEDICINE("M48", "胃复春片", "D48", "口服中成药", "偶有口干", 40, "药剂");
    ADD_MEDICINE("M49", "异甘草酸镁注射液", "D49", "静脉滴注", "血压轻度升高", 120, "药剂");
    ADD_MEDICINE("M50", "低流量吸氧+抗感染综合治疗", "D50", "综合治疗", "氧中毒风险", 1000, "药剂");
    
    // ==========================================
    // 新增：针对5级疾病的早、中、晚期分阶段特效药物/手术
    // ==========================================
    ADD_MEDICINE("M_CANCER_EARLY_OP", "【早期】内镜微创切除术", "ALL_DISEASES", "微创手术", "出血", 20000, "手术");
    ADD_MEDICINE("M_CANCER_MID_OP", "【中期】器官部分切除术", "ALL_DISEASES", "开腹手术", "重度创伤", 60000, "手术");
    ADD_MEDICINE("M_CANCER_MID_CHEMO", "【中期】辅助化疗", "ALL_DISEASES", "静脉注射", "骨髓抑制", 30000, "药剂");
    ADD_MEDICINE("M_CANCER_LATE_TARGET", "【晚期】分子靶向药", "ALL_DISEASES", "口服", "剧烈呕吐", 80000, "药剂");
    ADD_MEDICINE("M_CANCER_LATE_IMMUNE", "【晚期】免疫抑制剂", "ALL_DISEASES", "静脉滴注", "免疫风暴", 100000, "药剂");
    
    ADD_MEDICINE("M_HEART_EARLY_STENT", "【早期】冠脉支架植入术", "ALL_DISEASES", "微创介入", "血栓", 30000, "手术");
    ADD_MEDICINE("M_HEART_MID_BYPASS", "【中期】冠脉搭桥术", "ALL_DISEASES", "开胸大手术", "心衰", 80000, "手术");
    ADD_MEDICINE("M_HEART_LATE_ECMO", "【晚期】ECMO体外生命支持", "ALL_DISEASES", "生命支持", "全身感染", 150000, "手术");
    
    // 添加通用药物，不绑定特定疾病，用于玩家选错药的选项
    ADD_MEDICINE("M_GEN_01", "维生素C含片", "ALL", "口服", "胃酸过多", 15, "保健品");
    ADD_MEDICINE("M_GEN_02", "板蓝根颗粒", "ALL", "口服", "无", 20, "药剂");
    
    // 【新增万金油方案】可以广谱治疗特定器官的所有疾病，但副作用巨大且效果打折
    ADD_MEDICINE("M_BROAD_HEART", "【万金油】强效强心剂", "HEART_ALL", "静脉推注", "严重心律失常", 2000, "药剂");
    ADD_MEDICINE("M_BROAD_LUNGS", "【万金油】广谱抗生素(肺)", "LUNGS_ALL", "静脉滴注", "重度肝肾损伤", 2000, "药剂");
    ADD_MEDICINE("M_BROAD_STOMACH", "【万金油】全能胃肠康", "STOMACH_ALL", "口服", "严重便秘", 2000, "药剂");
    ADD_MEDICINE("M_BROAD_LIVER", "【万金油】顶级保肝药", "LIVER_ALL", "静脉滴注", "低血糖昏迷", 2000, "药剂");
    ADD_MEDICINE("M_BROAD_SKIN", "【万金油】超级激素软膏", "SKIN_ALL", "外敷", "重度皮肤萎缩", 2000, "药剂");
    ADD_MEDICINE("M_BROAD_SUPER", "【终极神药】九转还魂丹", "ALL_DISEASES", "口服", "剧毒", 99999, "药剂");

    // 【新增保健品】用于控制血压、血糖、血脂
    ADD_MEDICINE("M_SUPP_01", "维生素C含片(微效)", "SUPP_IMMUNE", "口服", "无明显副作用", 50, "保健品");
    ADD_MEDICINE("M_SUPP_02", "降压药片(保健)", "SUPP_BP", "口服", "低血压风险", 500, "保健品");
    ADD_MEDICINE("M_SUPP_03", "深海鱼油(降脂)", "SUPP_LIPID", "口服", "无", 800, "保健品");
    ADD_MEDICINE("M_SUPP_04", "二甲双胍(降糖)", "SUPP_SUGAR", "口服", "胃肠道不适", 300, "保健品");
    
    // 新增：高级免疫力保健品
    ADD_MEDICINE("M_SUPP_05", "野生极品人参(中效)", "SUPP_IMMUNE", "口服", "上火流鼻血", 5000, "保健品");
    ADD_MEDICINE("M_SUPP_06", "极草冬虫夏草(强效)", "SUPP_IMMUNE", "口服", "无", 25000, "保健品");
    ADD_MEDICINE("M_SUPP_07", "胸腺肽肠溶片(特效)", "SUPP_IMMUNE", "口服", "免疫紊乱风险", 80000, "保健品");

    // ==========================================
    // 深度建模扩展：为5级疾病配置早中晚期分阶段治疗方案
    // ==========================================
    
    // D32: 重度心力衰竭
    diseases["D32"].earlyTreatment = {"M_HEART_EARLY_STENT"}; // 早期 1阶段
    diseases["D32"].midTreatment = {"M_HEART_MID_BYPASS", "M32"}; // 中期 2阶段
    diseases["D32"].lateTreatment = {"M_HEART_LATE_ECMO", "M32", "M_HEART_MID_BYPASS"}; // 晚期 3阶段
    diseases["D32"].treatmentGuide += "\n【治疗提示】不同时期的治疗方案不同，晚期成功率极低且费用高昂！";
    
    // D34: 胃部恶性肿瘤
    diseases["D34"].earlyTreatment = {"M_CANCER_EARLY_OP"};
    diseases["D34"].midTreatment = {"M_CANCER_MID_OP", "M_CANCER_MID_CHEMO"};
    diseases["D34"].lateTreatment = {"M_CANCER_LATE_TARGET", "M_CANCER_LATE_IMMUNE", "M_CANCER_LATE_TARGET"};
    diseases["D34"].treatmentGuide += "\n【治疗提示】该疾病分为早中晚期，建议定期体检早发现早治疗！";
    
    // D35: 肺部恶性肿瘤
    diseases["D35"].earlyTreatment = {"M_CANCER_EARLY_OP"};
    diseases["D35"].midTreatment = {"M_CANCER_MID_OP", "M_CANCER_MID_CHEMO"};
    diseases["D35"].lateTreatment = {"M_CANCER_LATE_TARGET", "M_CANCER_LATE_IMMUNE", "M_CANCER_LATE_TARGET"};
    diseases["D35"].treatmentGuide += "\n【治疗提示】不同时期的治疗方案不同，晚期成功率极低且费用高昂！";
    
    // D42: 急性心肌梗死
    diseases["D42"].earlyTreatment = {"M_HEART_EARLY_STENT"};
    diseases["D42"].midTreatment = {"M_HEART_MID_BYPASS", "M42"};
    diseases["D42"].lateTreatment = {"M_HEART_LATE_ECMO", "M42", "M_HEART_MID_BYPASS"};
    diseases["D42"].treatmentGuide += "\n【治疗提示】该疾病分为早中晚期，建议定期体检早发现早治疗！";
    
    // D44: 肝脏恶性肿瘤
    diseases["D44"].earlyTreatment = {"M_CANCER_EARLY_OP"};
    diseases["D44"].midTreatment = {"M_CANCER_MID_OP", "M_CANCER_MID_CHEMO"};
    diseases["D44"].lateTreatment = {"M_CANCER_LATE_TARGET", "M_CANCER_LATE_IMMUNE", "M_CANCER_LATE_TARGET", "M_CANCER_LATE_IMMUNE"}; // 晚期 4阶段
    diseases["D44"].treatmentGuide += "\n【治疗提示】该疾病分为早中晚期，建议定期体检早发现早治疗！";
    
    // D45: 黑色素瘤
    diseases["D45"].earlyTreatment = {"M_CANCER_EARLY_OP"};
    diseases["D45"].midTreatment = {"M_CANCER_MID_OP", "M_CANCER_LATE_IMMUNE"};
    diseases["D45"].lateTreatment = {"M_CANCER_LATE_TARGET", "M_CANCER_LATE_IMMUNE", "M_CANCER_LATE_TARGET"};
    diseases["D45"].treatmentGuide += "\n【治疗提示】不同时期的治疗方案不同，晚期成功率极低且费用高昂！";
    
    // D50: 重症肺心病
    diseases["D50"].earlyTreatment = {"M50"};
    diseases["D50"].midTreatment = {"M50", "M32"};
    diseases["D50"].lateTreatment = {"M_HEART_LATE_ECMO", "M50", "M32"};
    diseases["D50"].treatmentGuide += "\n【治疗提示】该疾病分为早中晚期，建议定期体检早发现早治疗！";
    
    // 为了兼容旧版或者不想加早中晚期的5级病（如先天性胆道闭锁，我们保持原逻辑或设为空）
    for (auto it = medicines.begin(); it != medicines.end(); ++it) {
        Medicine& med = it.value();
        // 默认设置：安全剂量为 1，超量惩罚系数 5.0
        med.safeDosage = 1;
        med.penaltyCoefficient = 5.0;
        
        // 默认副作用配置：扣减 2 点生命，持续 5 Ticks，触发概率 10%
        med.sideEffectData.hpPenalty = 2;
        med.sideEffectData.durationTicks = 5;
        med.sideEffectData.probability = 0.1;
    }

    // 手动配置部分典型药物的深度属性 (冲突与高危副作用)
    medicines["M02"].incompatibleMeds.append("M18"); // 阿莫西林 与 头孢类 模拟冲突
    medicines["M18"].incompatibleMeds.append("M02");
    
    medicines["M21"].incompatibleMeds.append("M42"); // 硝酸甘油 与 溶栓药 冲突
    medicines["M42"].incompatibleMeds.append("M21");

    medicines["M11"].safeDosage = 2; // 沙丁胺醇喷雾可以喷两次
    medicines["M11"].sideEffectData = {10, 3, 0.3}; // 心悸副作用较强
    
    medicines["M45"].sideEffectData = {20, 10, 0.8}; // 化疗药副作用极强
}

QVector<Disease> Database::getDiseasesByAgeAndOrgan(int age, const QString& organ) {
    QVector<Disease> result;
    for (const auto& disease : diseases) {
        if (disease.organ == organ && age >= disease.minAge && age <= disease.maxAge) {
            result.push_back(disease);
        }
    }
    return result;
}

Disease Database::getDiseaseById(const QString& id) {
    return diseases.value(id);
}

QVector<Medicine> Database::getMedicinesForDisease(const QString& diseaseId) {
    QVector<Medicine> result;
    for (const auto& med : medicines) {
        if (med.indication == diseaseId) {
            result.push_back(med);
        }
    }
    return result;
}

QVector<Medicine> Database::getAllMedicines() {
    QVector<Medicine> result;
    for (const auto& med : medicines) {
        result.push_back(med);
    }
    return result;
}

Medicine Database::getMedicineById(const QString& id) {
    return medicines.value(id);
}
