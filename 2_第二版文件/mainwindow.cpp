#include "mainwindow.h"
#include "GameEngine.h"
#include "Database.h"
#include "ResourceManager.h"
#include "StartupDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QInputDialog>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QEvent>
#include <QApplication>
#include <QPainter>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), currentViewedOrgan("")
{
    // 提前初始化数据库，确保后续 UI 构建能读取到药品
    Database::getInstance().init();
    ResourceManager::getInstance().init();

    engine = new GameEngine(this);

    // 连接引擎信号
    connect(engine, &GameEngine::healthChanged, this, [this](double h){ healthBar->setValue((int)h); });
    connect(engine, &GameEngine::immunityChanged, this, [this](double im){ immuneBar->setValue((int)im); });
    connect(engine, &GameEngine::ageChanged, this, [this](int a){ 
        ageLabel->setText(QString("年龄：%1 岁").arg(a));
        ageStageLabel->setText(getAgeStageText(a));
    });
    connect(engine, &GameEngine::moneyChanged, this, [this](int m){
        moneyLabel->setText(QString("金币：%1").arg(m));
    });
    connect(engine, &GameEngine::researchChanged, this, [this](int res){
        researchLabel->setText(QString("科研点数：%1").arg(res));
    });
    connect(engine, &GameEngine::vitalsChanged, this, [this](double sys, double dia, double sugar, double lipids){
        // 为各个指标单独判断颜色
        QString bpColor = (sys > 140 || dia > 90) ? "red" : "black";
        QString sugarColor = (sugar > 7.0) ? "red" : "black";
        QString lipidsColor = (lipids > 2.0) ? "red" : "black";
        
        // 使用富文本 (HTML) 分别对不同部分进行着色
        vitalsLabel->setText(QString("血压: <font color='%1'>%2/%3</font> | "
                                     "血糖: <font color='%4'>%5</font> | "
                                     "血脂: <font color='%6'>%7</font>")
                             .arg(bpColor).arg(sys, 0, 'f', 0).arg(dia, 0, 'f', 0)
                             .arg(sugarColor).arg(sugar, 0, 'f', 1)
                             .arg(lipidsColor).arg(lipids, 0, 'f', 1));
    });
    connect(engine, &GameEngine::organStateChanged, this, &MainWindow::updateOrganButtonState);
    connect(engine, &GameEngine::gameOver, this, &MainWindow::showGameOver);
    connect(engine, &GameEngine::gameMessage, this, &MainWindow::appendLogMessage);
    connect(engine, &GameEngine::diseaseTriggered, this, [this](){
        // 触发病变时，时间流速降至 0.01 倍 (子弹时间)，让疾病的扣血机制有更充足的操作时间
        timeSlider->setValue(0); 
        updateTimerInterval();
        flashScreen();
    });

    // 绝症与长寿事件处理
    connect(engine, &GameEngine::checkupEvent, this, [this](int age) {
        timeSlider->setValue(0); 
        updateTimerInterval();
        int ret = QMessageBox::question(this, "例行体检", 
            QString("你今年 %1 岁了。按规定可以进行每3年一次的全身深度体检，费用 30,000 金币。\n体检可以及早发现隐藏的恶性疾病，避免其恶化为晚期。\n是否花费 30,000 金币进行体检？（当前金币：%2）").arg(age).arg(engine->getMoney()),
            QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            engine->performCheckup();
        } else {
            engine->gameMessage("【系统】你拒绝了今年的全身深度体检。如果有隐藏的重疾，它可能会在未来突然爆发！");
        }
        
        timeSlider->setValue(20); 
        updateTimerInterval();
    });
    connect(engine, &GameEngine::level6DiseaseEvent, this, [this](bool success, const QString& diseaseName) {
        timeSlider->setValue(0); 
        updateTimerInterval();
        if (success) {
            bool ok;
            QString techName = QInputDialog::getText(this, "科研奇迹！", 
                QString("80岁大限将至，你患上了绝症【%1】！\n但凭借强大的科研能力，你成功研发了攻克该绝症的技术！\n请为你的技术命名：").arg(diseaseName),
                QLineEdit::Normal, "基因修复疗法", &ok);
            if (!ok || techName.isEmpty()) techName = "未知靶向药";
            
            QMessageBox::information(this, "绝症攻克", QString("你发明的【%1】成功治愈了绝症！\n你的寿命上限延长至 100 岁！").arg(techName));
            engine->extendMaxAge(20); // 80 -> 100
            
            // 恢复时间流速
            timeSlider->setValue(20); 
            updateTimerInterval();
        } else {
            showGameOver(QString("【讣告】\n%1 在 %2 岁时突发绝症【%3】。\n由于科研能力不足，未能突破技术瓶颈，遗憾离世。")
                .arg(engine->getPlayerName()).arg(engine->getAge()).arg(diseaseName));
        }
    });

    connect(engine, &GameEngine::lifeExtensionEvent, this, [this](bool success) {
        timeSlider->setValue(0); 
        updateTimerInterval();
        if (success) {
            bool ok;
            QString techName = QInputDialog::getText(this, "长寿突破！", 
                "100岁大限已到！\n你成功研发了长寿技术！\n请为该技术命名：",
                QLineEdit::Normal, "端粒延长术", &ok);
            if (!ok || techName.isEmpty()) techName = "神秘长寿术";
            
            engine->setLifeExtensionTechName(techName);
            
            int ret = QMessageBox::question(this, "使用长寿技术", 
                QString("是否花费 100万 金币使用【%1】延长 10 年寿命？\n（当前金币：%2）").arg(techName).arg(engine->getMoney()),
                QMessageBox::Yes | QMessageBox::No);
            
            if (ret == QMessageBox::Yes && engine->payForLifeExtension()) {
                QMessageBox::information(this, "重获新生", "寿命成功延长 10 年！");
                timeSlider->setValue(20); 
                updateTimerInterval();
            } else {
                showGameOver(QString("【讣告】\n%1 在 %2 岁时大限已到。\n虽然掌握了长寿技术，但因资金不足或放弃使用，安详离世。")
                    .arg(engine->getPlayerName()).arg(engine->getAge()));
            }
        } else {
            showGameOver(QString("【讣告】\n%1 在 %2 岁时身体机能彻底衰竭。\n科研未能突破寿命极限，安详离世。")
                .arg(engine->getPlayerName()).arg(engine->getAge()));
        }
    });

    connect(engine, &GameEngine::requireLifeExtensionPayment, this, [this](const QString& techName) {
        timeSlider->setValue(0); 
        updateTimerInterval();
        int ret = QMessageBox::question(this, "大限将至", 
            QString("寿命极限再次到来，是否花费 100万 金币使用【%1】延长 10 年寿命？\n（当前金币：%2）").arg(techName).arg(engine->getMoney()),
            QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes && engine->payForLifeExtension()) {
            QMessageBox::information(this, "重获新生", "寿命成功延长 10 年！");
            timeSlider->setValue(20); 
            updateTimerInterval();
        } else {
            showGameOver(QString("【讣告】\n%1 在 %2 岁时大限已到。\n放弃延续生命，安详离世。")
                .arg(engine->getPlayerName()).arg(engine->getAge()));
        }
    });

    connect(engine, &GameEngine::organDetailUpdated, this, [this](const QString& organName) {
        if (currentViewedOrgan == organName) {
            showOrganDetail(organName);
        }
    });

    flashOverlay = new QWidget(this);
    flashOverlay->setStyleSheet("background-color: rgba(255, 0, 0, 100);");
    flashOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    flashOverlay->hide();

    setupUi();
    // 启动游戏并请求玩家姓名
    engine->initGame();
    
    QTimer::singleShot(100, this, [this]() {
        StartupDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            engine->setPlayerAttributes(dialog.getPlayerName(), dialog.getWealth(), dialog.getLuck(), dialog.getConstitution(), dialog.getResearch());
        } else {
            engine->setPlayerAttributes("无名氏", 0, 0, 0, 0); // 默认值
        }
        
        // 开始游戏循环
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, engine, &GameEngine::tick);
        updateTimerInterval(); // 根据 Slider 启动 timer
    
        // 真实时间计时器，用于在子弹时间 (0.01x) 期间持续扣血
        realTimeTimer = new QTimer(this);
        connect(realTimeTimer, &QTimer::timeout, this, [this](){
            if (timeSlider->value() == 0) {
                engine->applyRealTimePenalty();
            }
        });
        realTimeTimer->start(1000); // 每秒触发一次
    });
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    this->resize(1200, 800);
    this->setWindowTitle("医疗科普模拟系统 - 深度体验版");

    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // ============================================
    // 1. 顶部：状态栏 (含免疫系统与时间流速控制)
    // ============================================
    QHBoxLayout *topLayout = new QHBoxLayout();
    
    ageLabel = new QLabel("年龄：0 岁", this);
    ageLabel->setFont(QFont("Arial", 14, QFont::Bold));
    ageStageLabel = new QLabel("婴儿期", this);
    ageStageLabel->setFont(QFont("Arial", 12, QFont::Bold));
    ageStageLabel->setStyleSheet("color: #1976D2;");
    
    // 新增：金币与体征显示
    QVBoxLayout *statsLayout = new QVBoxLayout();
    moneyLabel = new QLabel("金币：0", this);
    moneyLabel->setFont(QFont("Arial", 12, QFont::Bold));
    moneyLabel->setStyleSheet("color: #FF9800;");
    
    // 新增科研点数显示与购买
    QHBoxLayout *researchLayout = new QHBoxLayout();
    researchLabel = new QLabel("科研点数：0", this);
    researchLabel->setFont(QFont("Arial", 12, QFont::Bold));
    researchLabel->setStyleSheet("color: #9C27B0;");
    buyResearchBtn = new QPushButton("提升科研 (20万)", this);
    buyResearchBtn->setStyleSheet("background-color: #E1BEE7; border-radius: 3px; padding: 2px;");
    connect(buyResearchBtn, &QPushButton::clicked, this, [this](){
        if(engine->buyResearchPoint()) {
            QMessageBox::information(this, "科研突破", "成功花费 20万金币，科研点数 +1！\n你的研究能力增强了。");
        } else {
            QMessageBox::warning(this, "资金不足", "购买科研点数需要 20万 金币！");
        }
    });
    researchLayout->addWidget(researchLabel);
    researchLayout->addWidget(buyResearchBtn);
    researchLayout->addStretch();
    
    vitalsLabel = new QLabel("血压: 120/80 | 血糖: 5.0 | 血脂: 1.5", this);
    vitalsLabel->setFont(QFont("Arial", 10));
    statsLayout->addWidget(moneyLabel);
    statsLayout->addLayout(researchLayout);
    statsLayout->addWidget(vitalsLabel);
    
    QVBoxLayout *barsLayout = new QVBoxLayout();
    healthBar = new QProgressBar(this);
    healthBar->setRange(0, 100);
    healthBar->setValue(100);
    healthBar->setFormat("生命值：%p / 100");
    healthBar->setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }");
    
    immuneBar = new QProgressBar(this);
    immuneBar->setRange(0, 100);
    immuneBar->setValue(50);
    immuneBar->setFormat("免疫力：%p / 100");
    immuneBar->setStyleSheet("QProgressBar::chunk { background-color: #2196F3; }");
    barsLayout->addWidget(healthBar);
    barsLayout->addWidget(immuneBar);

    // 时间控制区
    QVBoxLayout *timeLayout = new QVBoxLayout();
    timeSpeedLabel = new QLabel("流速: 2.0x", this);
    timeSlider = new QSlider(Qt::Horizontal, this);
    timeSlider->setRange(0, 40); // 允许从 0 (完全暂停) 到 4.0x (极快)
    timeSlider->setValue(20); // 默认 2.0x
    connect(timeSlider, &QSlider::valueChanged, this, &MainWindow::updateTimerInterval);
    timeLayout->addWidget(timeSpeedLabel);
    timeLayout->addWidget(timeSlider);

    QPushButton* saveBtn = new QPushButton("保存", this);
    QPushButton* loadBtn = new QPushButton("读取", this);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveGame);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadGame);

    topLayout->addWidget(ageLabel);
    topLayout->addWidget(ageStageLabel);
    topLayout->addLayout(statsLayout); // 插入金币与体征布局
    topLayout->addLayout(barsLayout, 2);
    topLayout->addLayout(timeLayout, 1);
    topLayout->addWidget(saveBtn);
    topLayout->addWidget(loadBtn);
    
    mainLayout->addLayout(topLayout);

    // ============================================
    // 2. 中间：多页面切换
    // ============================================
    stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(stackedWidget, 3); 

    // --- 页面1：主界面 ---
    bodyPage = new QWidget();
    QVBoxLayout *bodyLayout = new QVBoxLayout(bodyPage);
    
    // 自定义一个可点击的 QLabel（通过事件过滤器实现）或者直接用按钮做背景
    bodyBackgroundLabel = new QLabel(bodyPage);
    bodyBackgroundLabel->setAlignment(Qt::AlignCenter);
    bodyBackgroundLabel->setMinimumHeight(400);
    bodyBackgroundLabel->setStyleSheet("border: 2px solid #ccc; background-color: #f5f5f5;");
    bodyBackgroundLabel->setCursor(Qt::PointingHandCursor);
    bodyBackgroundLabel->setToolTip("点击查看皮肤详情");
    
    // 初始化皮肤为健康状态
    QPixmap skinPixmap = ResourceManager::getInstance().getTexture("healthy_skin.png");
    bodyBackgroundLabel->setPixmap(skinPixmap.scaled(600, 600, Qt::KeepAspectRatio));

    // 为了支持绝对定位和点击事件，我们将 bodyBackgroundLabel 设置为事件过滤器
    bodyBackgroundLabel->installEventFilter(this);

    // 新增：疾病等级颜色图例 (Legend)
    QWidget *legendWidget = new QWidget(bodyPage);
    QHBoxLayout *legendLayout = new QHBoxLayout(legendWidget);
    legendLayout->setContentsMargins(5, 5, 5, 5);
    legendLayout->setSpacing(10);
    
    auto createLegendItem = [](const QString& text, const QString& colorStr) -> QWidget* {
        QWidget *w = new QWidget();
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        QLabel *colorBox = new QLabel();
        colorBox->setFixedSize(16, 16);
        colorBox->setStyleSheet(QString("background-color: %1; border: 1px solid #999;").arg(colorStr));
        QLabel *label = new QLabel(text);
        label->setFont(QFont("Arial", 10, QFont::Bold));
        l->addWidget(colorBox);
        l->addWidget(label);
        return w;
    };
    
    legendLayout->addWidget(new QLabel("<b>疾病严重程度：</b>"));
    legendLayout->addWidget(createLegendItem("1级(轻微)", "#4CAF50")); // 绿色
    legendLayout->addWidget(createLegendItem("2级(普通)", "#CDDC39")); // 较深的黄绿色 (Lime)
    legendLayout->addWidget(createLegendItem("3级(中度)", "#FF9800")); // 橙色
    legendLayout->addWidget(createLegendItem("4级(严重)", "#F44336")); // 红色
    legendLayout->addWidget(createLegendItem("5级(致命)", "#9C27B0")); // 紫色
    legendLayout->addStretch();
    legendWidget->setStyleSheet("background-color: rgba(255, 255, 255, 220); border-radius: 5px;");
    
    bodyLayout->addWidget(legendWidget, 0, Qt::AlignTop);
    bodyLayout->addWidget(bodyBackgroundLabel, 1);

    QStringList organNames = {"心脏", "肺部", "胃部", "肝脏"};
    for(const QString& name : organNames) {
        // 按钮父组件设为 bodyBackgroundLabel，使其悬浮在背景上
        QPushButton *btn = new QPushButton(name, bodyBackgroundLabel);
        btn->setFixedSize(80, 80);
        btn->setStyleSheet("background-color: rgba(224, 224, 224, 200); font-size: 14px; font-weight: bold; border-radius: 40px;");
        organButtons[name] = btn;
        connect(btn, &QPushButton::clicked, this, [this, name](){ showOrganDetail(name); });
    }

    stackedWidget->addWidget(bodyPage);

    // --- 页面2：详情视图 ---
    detailPage = new QWidget();
    QVBoxLayout *detailLayout = new QVBoxLayout(detailPage);
    
    QHBoxLayout *detailTopLayout = new QHBoxLayout();
    returnBtn = new QPushButton("← 返回人体视图", detailPage);
    diseaseNameLabel = new QLabel("健康", detailPage);
    diseaseNameLabel->setFont(QFont("Arial", 18, QFont::Bold));
    diseaseNameLabel->setAlignment(Qt::AlignCenter);
    
    // 诊疗指南按钮
    guideBtn = new QPushButton("💡 查看诊疗指南", detailPage);
    guideBtn->setStyleSheet("background-color: #FFC107; font-weight: bold; border-radius: 5px; padding: 5px;");
    guideBtn->hide(); // 默认隐藏，生病时才显示
    connect(guideBtn, &QPushButton::clicked, this, &MainWindow::showTreatmentGuide);

    detailTopLayout->addWidget(returnBtn);
    detailTopLayout->addWidget(diseaseNameLabel, 1);
    detailTopLayout->addWidget(guideBtn);
    
    organDetailImageLabel = new QLabel("器官高清图 (从本地 assets/textures 加载)", detailPage);
    organDetailImageLabel->setAlignment(Qt::AlignCenter);
    organDetailImageLabel->setMinimumHeight(300);
    organDetailImageLabel->setStyleSheet("background-color: #E3F2FD; font-size: 16px; border: 2px solid #90CAF9;");
    
    symptomTextEdit = new QTextEdit(detailPage);
    symptomTextEdit->setReadOnly(true);
    symptomTextEdit->setFont(QFont("Arial", 12));
    
    detailLayout->addLayout(detailTopLayout);
    detailLayout->addWidget(organDetailImageLabel, 2);
    detailLayout->addWidget(symptomTextEdit, 1);
    stackedWidget->addWidget(detailPage);

    // ============================================
    // 3. 底部：日志与药箱
    // ============================================
    bottomWidget = new QWidget(this);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);

    QGroupBox *logGroup = new QGroupBox("系统日志", this);
    QVBoxLayout *logGroupLayout = new QVBoxLayout(logGroup);
    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);
    logGroupLayout->addWidget(logTextEdit);
    bottomLayout->addWidget(logGroup, 1);

    QGroupBox *medicineBoxGroup = new QGroupBox("医疗药箱", this);
    QVBoxLayout *medMainLayout = new QVBoxLayout(medicineBoxGroup); // 改用 VBox，上方搜索，下方网格

    // 搜索框
    medicineSearchBar = new QLineEdit(medicineBoxGroup);
    medicineSearchBar->setPlaceholderText("🔍 输入药品名称或适应症拼音进行快速搜索...");
    medicineSearchBar->setClearButtonEnabled(true);
    connect(medicineSearchBar, &QLineEdit::textChanged, this, &MainWindow::filterMedicines);
    medMainLayout->addWidget(medicineSearchBar);

    QTabWidget *medicineTabWidget = new QTabWidget(medicineBoxGroup);
    medicineTabWidget->setStyleSheet("QTabBar::tab { height: 30px; font-weight: bold; }");
    
    QVector<Medicine> allMeds = Database::getInstance().getAllMedicines();
    
    auto createTab = [&](const QString& title, const QString& typeFilter) {
        QScrollArea *scrollArea = new QScrollArea(this);
        QWidget *scrollWidget = new QWidget(scrollArea);
        QGridLayout *scrollLayout = new QGridLayout(scrollWidget);
        
        int row = 0, col = 0;
        for(const Medicine& med : allMeds) {
            if (med.type != typeFilter) continue;
            
            // 显示药品名称、类型和价格
            QString btnText = QString("%1\n¥%2").arg(med.name).arg(med.price);
            QPushButton *btn = new QPushButton(btnText, scrollWidget);
            QString tooltip = QString("适应症: %1\n安全剂量: %2\n副作用扣血: %3\n冲突药品数量: %4\n类型: %5\n价格: %6")
                                .arg(med.indication).arg(med.safeDosage).arg(med.sideEffectData.hpPenalty).arg(med.incompatibleMeds.size()).arg(med.type).arg(med.price);
            btn->setToolTip(tooltip);
            btn->setMinimumHeight(50);
            btn->setProperty("medId", med.id);
            btn->setProperty("indication", med.indication); // 存入适应症，方便搜索
            connect(btn, &QPushButton::clicked, this, &MainWindow::useMedicine);
            
            medicineButtons[med.id] = btn;
            scrollLayout->addWidget(btn, row, col);
            
            col++;
            if(col > 3) { col = 0; row++; }
        }
        
        // 添加一个弹簧填充底部空间，避免按钮高度被拉伸
        scrollLayout->setRowStretch(row + 1, 1);
        
        scrollWidget->setLayout(scrollLayout);
        scrollArea->setWidget(scrollWidget);
        scrollArea->setWidgetResizable(true);
        medicineTabWidget->addTab(scrollArea, title);
    };

    createTab("💊 药剂", "药剂");
    createTab("🔪 手术", "手术");
    createTab("🌿 保健品", "保健品");
    
    medMainLayout->addWidget(medicineTabWidget);
    bottomLayout->addWidget(medicineBoxGroup, 2);
    mainLayout->addWidget(bottomWidget, 1);

    connect(returnBtn, &QPushButton::clicked, this, &MainWindow::returnToBodyView);
}

void MainWindow::updateTimerInterval() {
    double mult = timeSlider->value() / 10.0;
    
    // 如果滑块拉到 0，我们将其视为 0.01x (治病时的极慢速/子弹时间)
    if (timeSlider->value() == 0) {
        mult = 0.01;
    }
    
    timeSpeedLabel->setText(QString("流速: %1x").arg(mult, 0, 'f', 2));
    engine->setTimeMultiplier(mult);
    
    int baseInterval = 1000;
    int newInterval = static_cast<int>(baseInterval / mult);
    
    gameTimer->start(newInterval);
}

QString MainWindow::getAgeStageText(int age) {
    if (age <= 5) return "婴儿期";
    if (age <= 12) return "儿童期";
    if (age <= 18) return "青年期"; // 新增：13-18岁为青年期
    if (age <= 60) return "成年期";
    return "老年期";
}

bool MainWindow::areAllOrgansHealthy() {
    QMap<QString, OrganState> organs = engine->getOrgans();
    for (const auto& organName : organs.keys()) {
        if (organs[organName].isDiseased) {
            return false;
        }
    }
    return true;
}

void MainWindow::updateOrganButtonState(const QString& organName, bool isDiseased) {
    OrganState state = engine->getOrgans()[organName];
    
    // 根据 severity 决定颜色
    QString severityColor = "red"; // 默认红色
    QString borderColor = "red";
    QString textColor = "white";
    if (isDiseased) {
        switch (state.currentDisease.severity) {
            case 1: severityColor = "#4CAF50"; borderColor = "#388E3C"; break; // 绿
            case 2: severityColor = "#CDDC39"; borderColor = "#AFB42B"; textColor = "black"; break; // 较深的黄绿色 (Lime)
            case 3: severityColor = "#FF9800"; borderColor = "#F57C00"; break; // 橙
            case 4: severityColor = "#F44336"; borderColor = "#D32F2F"; break; // 红
            case 5: severityColor = "#9C27B0"; borderColor = "#7B1FA2"; break; // 紫
            default: severityColor = "#F44336"; borderColor = "#D32F2F"; break;
        }
    } else {
        // 如果是健康状态，也要确保颜色重置
        severityColor = "rgba(224, 224, 224, 200)";
        textColor = "black";
    }

    if (organName == "皮肤") {
        // 特殊处理皮肤：它是背景图片，不是按钮
        QString fileName = isDiseased ? "disease_skin.png" : "healthy_skin.png";
        QPixmap skinPixmap = ResourceManager::getInstance().getTexture(fileName);
        
        QSize bgSize = bodyBackgroundLabel->size();
        if (bgSize.isEmpty() || bgSize.width() < 100) {
            bgSize = QSize(600, 600);
        }
        bodyBackgroundLabel->setPixmap(skinPixmap.scaled(bgSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        
        if (isDiseased) {
            bodyBackgroundLabel->setStyleSheet(QString("border: 4px solid %1; background-color: #ffebee;").arg(borderColor));
        } else {
            bodyBackgroundLabel->setStyleSheet("border: 2px solid #ccc; background-color: #f5f5f5;");
        }
    } else if (organButtons.contains(organName)) {
        QPushButton *btn = organButtons[organName];
        if (isDiseased) {
            btn->setStyleSheet(QString("background-color: %1; color: %2; font-size: 14px; font-weight: bold; border-radius: 40px; border: 4px solid %3;")
                               .arg(severityColor, textColor, borderColor));
            btn->setText(organName + "\n(病变!)");
        } else {
            btn->setStyleSheet("background-color: rgba(224, 224, 224, 200); color: black; font-size: 14px; font-weight: bold; border-radius: 40px; border: none;");
            btn->setText(organName);
        }
    }
    
    if (currentViewedOrgan == organName) showOrganDetail(organName);
    
    // 如果器官刚刚治愈，并且此时全身都已经健康，自动恢复时间流速到默认的 2.0x
    if (!isDiseased && areAllOrgansHealthy()) {
        if (timeSlider->value() < 20) { // 只有在流速低于 2.0 时才恢复
            timeSlider->setValue(20);
            updateTimerInterval();
        }
    }
}

void MainWindow::crossFadeImage(QLabel* label, const QPixmap& newPixmap) {
    if (!label) return;

    // 如果想做淡入淡出动画，可以使用 QGraphicsOpacityEffect
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(label);
    label->setGraphicsEffect(effect);
    
    QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(300); // 300ms 动画
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    
    label->setPixmap(newPixmap.scaled(512, 512, Qt::KeepAspectRatio));
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QString MainWindow::getOrganFileName(const QString& organName) {
    if (organName == "心脏") return "heart";
    if (organName == "肺部") return "lungs";
    if (organName == "胃部") return "stomach";
    if (organName == "肝脏") return "liver";
    if (organName == "皮肤") return "skin";
    return "unknown";
}

void MainWindow::showOrganDetail(const QString &organName) {
    currentViewedOrgan = organName;
    OrganState state = engine->getOrgans()[organName];

    // 从本地加载资源，使用英文文件名避免中文路径在 Qt 构建时出现乱码或找不到的问题
    QString organFileBase = getOrganFileName(organName);
    QString fileName = state.isDiseased ? "disease_" + organFileBase + ".png" : "healthy_" + organFileBase + ".png";
    QPixmap pixmap = ResourceManager::getInstance().getTexture(fileName);
    
    // 平滑切换贴图
    crossFadeImage(organDetailImageLabel, pixmap);

    if (state.isDiseased) {
        QString severityColorStr;
        QString severityText;
        switch (state.currentDisease.severity) {
            case 1: severityColorStr = "#4CAF50"; severityText = "1级(轻微)"; break;
            case 2: severityColorStr = "#AFB42B"; severityText = "2级(普通)"; break; // 在白色背景下，详情页用稍深的颜色
            case 3: severityColorStr = "#FF9800"; severityText = "3级(中度)"; break;
            case 4: severityColorStr = "#F44336"; severityText = "4级(严重)"; break;
            case 5: severityColorStr = "#9C27B0"; severityText = "5级(致命)"; break;
            default: severityColorStr = "red"; severityText = QString("%1级").arg(state.currentDisease.severity); break;
        }

        // 针对5级分阶段治疗的 UI 进度提示
        QString stageInfo = "";
        if (state.currentDisease.severity == 5 && !state.currentDisease.treatmentStages.isEmpty()) {
            int total = state.currentDisease.treatmentStages.size();
            int current = state.currentTreatmentStage;
            QString diseasePeriod;
            if (state.diseaseStage == 1) diseasePeriod = "早期";
            else if (state.diseaseStage == 2) diseasePeriod = "中期";
            else if (state.diseaseStage == 3) diseasePeriod = "晚期";
            
            stageInfo = QString("<br><br><b><font color='#9C27B0'>【病情阶段】: %1</font></b><br><b><font color='#9C27B0'>【治疗进度】: 阶段 %2 / %3</font></b><br><i>需要连续进行多次手术/疗程才能彻底治愈！请查看诊疗指南！</i>")
                            .arg(diseasePeriod).arg(current).arg(total);
        }

        diseaseNameLabel->setText(QString("<font color='%1'>病变: %2</font>").arg(severityColorStr, state.currentDisease.name));
        QString desc = QString("<b>疾病名称:</b> %1<br><b>严重程度:</b> <font color='%2'>%3</font><br><b>临床症状:</b> %4<br><b>自愈预期:</b> %5%6")
                               .arg(state.currentDisease.name).arg(severityColorStr).arg(severityText).arg(state.currentDisease.symptoms)
                               .arg(state.autoHealTimer > 0 ? QString("存在自愈可能 (剩余 %1 Ticks)").arg(state.autoHealTimer) : "需药物干预")
                               .arg(stageInfo);
        symptomTextEdit->setHtml(desc);
        guideBtn->show(); // 生病时显示提示按钮
    } else {
        diseaseNameLabel->setText("<font color='green'>健康状态</font>");
        symptomTextEdit->setHtml("<b>状态:</b> 器官功能正常，未发现病变。");
        guideBtn->hide(); // 健康时隐藏提示按钮
    }
    stackedWidget->setCurrentIndex(1);
}

void MainWindow::showTreatmentGuide() {
    if (currentViewedOrgan.isEmpty()) return;
    OrganState state = engine->getOrgans()[currentViewedOrgan];
    if (!state.isDiseased) return;

    QString guide = state.currentDisease.treatmentGuide;
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("专家诊疗指南");
    msgBox.setText("<b>" + state.currentDisease.name + " 治疗指南</b>");
    msgBox.setInformativeText(guide);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet("QLabel{min-width: 400px;}");
    msgBox.exec();
}

void MainWindow::appendLogMessage(const QString& msg) {
    logTextEdit->append(QString("[%1岁] %2").arg(engine->getAge()).arg(msg));
    logTextEdit->verticalScrollBar()->setValue(logTextEdit->verticalScrollBar()->maximum());
}

void MainWindow::returnToBodyView() {
    currentViewedOrgan = "";
    stackedWidget->setCurrentIndex(0);
}

void MainWindow::useMedicine() {
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString medId = btn->property("medId").toString();
    Medicine med = Database::getInstance().getMedicineById(medId);

    // 保健品等全局药品可以直接使用，不需要进入器官详情页
    if (med.type != "保健品" && currentViewedOrgan.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先点击一个器官进入详情页，再使用药物或进行手术！");
        return;
    }

    // 1. 冲突检查
    QString conflictName;
    if (engine->checkConflict(medId, conflictName)) {
        int ret = QMessageBox::critical(this, "严重用药冲突", 
            QString("警告！该药物与您当前正在承受副作用的药物 [%1] 存在严重冲突，可能导致致命后果！是否坚持服用？").arg(conflictName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }

    // 2. 剂量输入
    bool ok;
    int dosage = QInputDialog::getInt(this, "用药剂量/手术次数", "请输入使用数量 (1为标准):", 1, 1, 10, 1, &ok);
    if (ok) {
        engine->applyMedicine(currentViewedOrgan, medId, dosage);
        
        // 用药后可能会改变健康状态，立刻刷新一下当前页面的贴图
        if (!currentViewedOrgan.isEmpty()) {
            showOrganDetail(currentViewedOrgan);
        }
    }
}

void MainWindow::showGameOver(const QString& reason) {
    if (gameTimer) gameTimer->stop();
    if (realTimeTimer) realTimeTimer->stop();
    
    QMessageBox::critical(this, "Game Over", reason);
    close();
}

void MainWindow::filterMedicines(const QString& searchText) {
    // 根据搜索框文本过滤药品按钮显示
    for (auto it = medicineButtons.begin(); it != medicineButtons.end(); ++it) {
        QPushButton *btn = it.value();
        QString medName = btn->text();
        QString indication = btn->property("indication").toString();
        
        // 如果搜索文本为空，或者药品名称包含搜索文本，或者适应症(疾病ID)包含搜索文本，则显示，否则隐藏
        if (searchText.isEmpty() || 
            medName.contains(searchText, Qt::CaseInsensitive) ||
            indication.contains(searchText, Qt::CaseInsensitive)) {
            btn->show();
        } else {
            btn->hide();
        }
    }
}

void MainWindow::saveGame() {}
void MainWindow::loadGame() {}

void MainWindow::flashScreen() {
    flashOverlay->setGeometry(this->rect());
    flashOverlay->raise();
    flashOverlay->show();

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(flashOverlay);
    flashOverlay->setGraphicsEffect(effect);
    
    QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(500);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, flashOverlay, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == bodyBackgroundLabel) {
        if (event->type() == QEvent::Resize) {
            // 当背景标签调整大小时，重新计算并绝对定位器官按钮
            int w = bodyBackgroundLabel->width();
            int h = bodyBackgroundLabel->height();
            
            // 每次 Resize 时也保持皮肤图片的比例缩放
            OrganState skinState = engine->getOrgans()["皮肤"];
            QString fileName = skinState.isDiseased ? "disease_skin.png" : "healthy_skin.png";
            QPixmap skinPixmap = ResourceManager::getInstance().getTexture(fileName);
            bodyBackgroundLabel->setPixmap(skinPixmap.scaled(w, h, Qt::KeepAspectRatio));

            // 基于中心的相对偏移量排布（可根据实际人体背景图调整比例）
            if (organButtons.contains("心脏")) organButtons["心脏"]->move(w/2 - 10, h/2 - 80);
            if (organButtons.contains("肺部")) organButtons["肺部"]->move(w/2 - 40, h/2 - 120);
            if (organButtons.contains("胃部")) organButtons["胃部"]->move(w/2 + 20, h/2 - 10);
            if (organButtons.contains("肝脏")) organButtons["肝脏"]->move(w/2 - 50, h/2 - 10);
        } else if (event->type() == QEvent::MouseButtonPress) {
            // 点击背景区域（皮肤），显示皮肤详情
            showOrganDetail("皮肤");
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
