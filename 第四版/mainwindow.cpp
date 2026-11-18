#include "mainwindow.h"
#include "StartupDialog.h"
#include "Database.h"
#include "DiseaseVisual.h"
#include "AnatomyLab.h"
#include "SaveRepository.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QSplitter>
#include <QScrollArea>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QFileInfo>
#include <QCloseEvent>
#include <QTime>
#include <QSignalBlocker>
#include <QTextDocument>
#include <QStatusBar>

namespace {
QLabel* label(const QString& text,const QString& name,QWidget* parent=nullptr){
    auto* l=new QLabel(text,parent);l->setObjectName(name);return l;
}
QFrame* card(const QString& name){auto* f=new QFrame;f->setObjectName(name);return f;}
QString stageName(int stage){return stage==1?"早期":stage==2?"中期":stage==3?"晚期":"";}
const QStringList organOrder{"心脏","肺部","肝脏","胃部","皮肤"};
QString severityColor(int n){return QStringList{"#9dccc0","#88c99b","#c2cb7c","#e4b078","#e18f84","#ca9ac4"}.value(n,"#a6c1bb");}
QStringList diagnosedComorbidities(const OrganState& organ){
    QStringList result;
    for(const auto& other:organ.coexisting)
        if(other.isDiseased)result.append(other.currentDisease.name);
    return result;
}
// Nested dialogs pause the engine and restore the caller's prior state.
struct PauseScope {
    GameEngine* engine;
    QElapsedTimer& elapsed;
    bool before;
    PauseScope(GameEngine* e,QElapsedTimer& t):engine(e),elapsed(t),before(e->isPaused()){engine->setPaused(true);}
    ~PauseScope(){engine->setPaused(before);elapsed.restart();}
};
}
MainWindow::MainWindow(QWidget* parent,bool useDialogs,const QString& saveDirectory):QMainWindow(parent),interactive(useDialogs),saveRoot(saveDirectory){
    engine=new GameEngine(this);engine->setObjectName("gameEngine");
    setupUi();
    connect(engine,&GameEngine::stateChanged,this,&MainWindow::refresh);
    connect(engine,&GameEngine::stateChanged,this,[this]{
        // Loading, initialization and mere navigation must never overwrite a saved life.
        if(interactive&&hasCurrentSession&&!suppressSaveTracking){saveDirty=true;autosaveArmed=true;}
    });
    connect(engine,&GameEngine::gameMessage,this,&MainWindow::log);
    connect(engine,&GameEngine::diseaseTriggered,this,[this]{speed->setValue(1);},Qt::QueuedConnection);
    connect(engine,&GameEngine::checkupEvent,this,[this](int age){
        log(QString("例行体检提醒：%1 岁，可以用右侧「全身体检」发现隐性疾病。").arg(age));
    },Qt::QueuedConnection);
    connect(engine,&GameEngine::level6DiseaseEvent,this,[this](bool,const QString&){handleMilestone(80);},Qt::QueuedConnection);
    connect(engine,&GameEngine::lifeExtensionEvent,this,[this](bool){handleMilestone(100);},Qt::QueuedConnection);
    connect(engine,&GameEngine::requireLifeExtensionPayment,this,[this](const QString&){handleMilestone(90);},Qt::QueuedConnection);
    connect(engine,&GameEngine::gameOver,this,&MainWindow::showGameOver,Qt::QueuedConnection);
    connect(&timer,&QTimer::timeout,this,[this]{
        double seconds=elapsed.restart()/1000.0;
        engine->advanceTime(seconds);
        anatomy->setAge(engine->visualAge());
    });
    elapsed.start();timer.start(50);refresh();setPaused(true);
    connect(&autosaveTimer,&QTimer::timeout,this,[this]{writeAutosave();});
    if(interactive){
        autosaveTimer.start(10000);
        QTimer::singleShot(0,this,[this]{
            if(saveEntries().isEmpty())newGame();
            else showSaveManager(true);
        });
    }
}
void MainWindow::setupUi(){
    setWindowTitle("器官保卫战 · Anatomy 3D");
    resize(1480,960);setMinimumSize(1140,760);
    qApp->setFont(QFont("Microsoft YaHei UI",10));
    setStyleSheet(R"(
        QMainWindow,QDialog { background:#101c23; color:#dde7e2; }
        QWidget { color:#d9e4df; font-family:"Microsoft YaHei UI"; font-size:12px; }
        QFrame#card,QFrame#sidebar,QFrame#inspector,QFrame#pharmacy {background:#17272e;border:1px solid #2a3c42;border-radius:10px;}
        QLabel#eyebrow{color:#79b7ae;font-size:10px;letter-spacing:2px;}
        QLabel#heading{font-size:23px;font-weight:600;color:#eef3e9;}
        QLabel#section{font-size:15px;font-weight:600;color:#e4ece4;}
        QLabel#muted{color:#8ca6a7;}
        QLabel#largeValue{font-size:20px;font-weight:600;color:#e2eddf;}
        QPushButton{background:#22363c;border:1px solid #354b50;border-radius:6px;padding:8px 12px;color:#cbded9;}
        QPushButton:hover{background:#2c464b;border-color:#73b9ae;}
        QPushButton:pressed{background:#34564f;}
        QPushButton:checked{background:#2b4f4b;border:1px solid #7bb7a6;color:#daf2e3;}
        QPushButton:disabled{color:#667c7e;border-color:#27383d;background:#1a2a30;}
        QPushButton#primary{background:#bbd0b4;color:#142b29;font-weight:600;border:0;}
        QPushButton#primary:hover{background:#d5e6c9;}
        QPushButton#organCard{text-align:left;padding:13px;font-size:13px;border-radius:8px;}
        QProgressBar{border:0;border-radius:3px;background:#2b3c40;height:8px;text-align:center;font-size:10px;}
        QProgressBar::chunk{border-radius:3px;background:#9bbc98;}
        QProgressBar#immune::chunk{background:#7dacae;}
        QLineEdit,QSpinBox,QComboBox{background:#102129;border:1px solid #344b50;padding:7px;border-radius:5px;color:#dce7e1;}
        QSpinBox::up-button,QSpinBox::down-button{width:18px;background:#30464b;}
        QComboBox QAbstractItemView{background:#182e35;color:#e1ebe3;selection-background-color:#386257;}
        QTextBrowser{background:transparent;border:0;color:#bfd0ca;}
        QListWidget{background:#112129;border:1px solid #293d43;border-radius:5px;outline:0;}
        QListWidget::item{padding:8px 11px;border-bottom:1px solid #24363d;}
        QListWidget::item:selected{background:#294e47;color:#e3f1dd;}
        QListWidget::item:hover{background:#233b40;}
        QScrollBar:vertical{background:#16272d;width:7px;border:0;}
        QScrollBar::handle:vertical{background:#45615f;border-radius:3px;min-height:20px;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}
        QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:none;}
        QSlider::groove:horizontal{height:4px;background:#304449;border-radius:2px;}
        QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#b8d4b9;border-radius:6px;}
        QToolTip{background:#24413f;color:#e9efdf;border:1px solid #5a8178;padding:6px;}
        QStatusBar{background:#101c23;color:#799795;font-size:10px;}
        QMessageBox{background:#17272e;}
    )");
    auto* central=new QWidget;setCentralWidget(central);
    auto* root=new QVBoxLayout(central);root->setContentsMargins(20,16,20,8);root->setSpacing(12);
    auto* header=new QHBoxLayout;
    auto* brand=new QVBoxLayout;brand->addWidget(label("HEALTHGAME / 生命观察站","eyebrow"));
    brand->addWidget(label("器官保卫战","heading"));header->addLayout(brand);header->addSpacing(28);
    auto* identity=new QVBoxLayout;ageLabel=label("0 岁 · 婴儿期","largeValue");
    identity->addWidget(ageLabel);moneyLabel=label("金币 10,000","muted");identity->addWidget(moneyLabel);header->addLayout(identity);header->addSpacing(20);
    auto* vitals=new QVBoxLayout;vitalsLabel=label("","");vitalsLabel->setTextFormat(Qt::RichText);vitals->addWidget(vitalsLabel);
    auto* bars=new QHBoxLayout;healthBar=new QProgressBar;immuneBar=new QProgressBar;immuneBar->setObjectName("immune");
    healthBar->setFormat("生命 %p%");immuneBar->setFormat("免疫 %p%");healthBar->setRange(0,100);immuneBar->setRange(0,100);
    bars->addWidget(healthBar);bars->addWidget(immuneBar);vitals->addLayout(bars);header->addLayout(vitals,1);
    auto* timeBox=new QVBoxLayout;speedLabel=label("时间 2.0×","muted");timeBox->addWidget(speedLabel);
    speed=new QSlider(Qt::Horizontal);speed->setObjectName("timeSpeed");speed->setRange(1,40);speed->setValue(20);speed->setMaximumWidth(150);
    connect(speed,&QSlider::valueChanged,this,[this](int v){engine->setTimeMultiplier(v/10.0);speedLabel->setText(QString("时间 %1× · 10 Tick / 年").arg(v/10.0,0,'f',1));});
    timeBox->addWidget(speed);header->addLayout(timeBox);
    pauseButton=new QPushButton("暂停");pauseButton->setObjectName("pauseButton");
    connect(pauseButton,&QPushButton::clicked,this,[this]{setPaused(!manualPause);});header->addWidget(pauseButton);
    auto* save=new QPushButton("快速存档");auto* load=new QPushButton("存档管理");auto* restart=new QPushButton("新人生");
    save->setObjectName("saveButton");load->setObjectName("loadButton");restart->setObjectName("newGameButton");
    connect(save,&QPushButton::clicked,this,&MainWindow::saveGame);connect(load,&QPushButton::clicked,this,&MainWindow::loadGame);
    connect(restart,&QPushButton::clicked,this,&MainWindow::newGame);
    header->addWidget(save);header->addWidget(load);header->addWidget(restart);root->addLayout(header);

    auto* center=new QHBoxLayout;center->setSpacing(12);
    auto* sidebar=card("sidebar");sidebar->setMinimumWidth(172);
    auto* nav=new QVBoxLayout(sidebar);nav->setContentsMargins(14,18,14,14);nav->setSpacing(10);
    nav->setSizeConstraint(QLayout::SetMinimumSize);
    nav->addWidget(label("ORGAN SYSTEM","eyebrow"));nav->addWidget(label("器官导航","section"));nav->addSpacing(6);
    bodyButton=new QPushButton("◎  全身概览");bodyButton->setObjectName("bodyButton");bodyButton->setCheckable(true);
    connect(bodyButton,&QPushButton::clicked,this,[this]{selectOrgan("");});nav->addWidget(bodyButton);
    const QStringList en{"HEART","LUNGS","LIVER","STOMACH","SKIN"};
    for(int i=0;i<organOrder.size();++i){
        auto* btn=new QPushButton(organOrder[i]+"\n"+en[i]);btn->setObjectName("organCard");btn->setProperty("organ",organOrder[i]);
        btn->setCheckable(true);btn->setMinimumHeight(61);organButtons[organOrder[i]]=btn;
        connect(btn,&QPushButton::clicked,this,[this,i]{selectOrgan(organOrder[i]);});nav->addWidget(btn);
    }
    nav->addStretch();
    auto* atlasButton=new QPushButton("年龄 / 疾病图谱");atlasButton->setObjectName("atlasButton");
    connect(atlasButton,&QPushButton::clicked,this,[this]{PauseScope pause(engine,elapsed);AnatomyLab lab(engine->getAge(),this);lab.exec();});
    nav->addWidget(atlasButton);
    auto* hint=label("观察身体的每一次变化\n点击模型或器官卡片进入","muted");hint->setWordWrap(true);nav->addWidget(hint);
    auto* note=label("病灶为游戏可视化示意","muted");note->setStyleSheet("font-size:10px;color:#658783;");nav->addWidget(note);
    auto* navigationScroll=new QScrollArea;navigationScroll->setObjectName("organNavigationScroll");
    navigationScroll->setWidgetResizable(true);navigationScroll->setFrameShape(QFrame::NoFrame);
    navigationScroll->setStyleSheet("QScrollArea#organNavigationScroll{background:#101c23;border:0;}");
    navigationScroll->viewport()->setAutoFillBackground(false);
    navigationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigationScroll->setFixedWidth(186);navigationScroll->setWidget(sidebar);center->addWidget(navigationScroll);

    auto* canvas=new QVBoxLayout;canvas->setSpacing(8);
    auto* toolbar=new QHBoxLayout;
    titleLabel=label("全身概览","section");toolbar->addWidget(titleLabel);toolbar->addStretch();
    skinButton=new QPushButton("胸腹透视");skinButton->setCheckable(true);
    cutButton=new QPushButton("内部剖视");cutButton->setCheckable(true);cutButton->setObjectName("cutawayButton");
    auto* rotate=new QPushButton("自动旋转");rotate->setCheckable(true);
    auto* reset=new QPushButton("视角复位");
    toolbar->addWidget(skinButton);toolbar->addWidget(cutButton);toolbar->addWidget(rotate);toolbar->addWidget(reset);
    canvas->addLayout(toolbar);
    anatomy=new AnatomyView;
    connect(anatomy,&AnatomyView::organSelected,this,&MainWindow::selectOrgan);
    connect(skinButton,&QPushButton::toggled,anatomy,&AnatomyView::setDissectionWindow);
    connect(cutButton,&QPushButton::toggled,anatomy,&AnatomyView::setCutaway);
    connect(rotate,&QPushButton::toggled,anatomy,&AnatomyView::setAutoRotate);
    connect(reset,&QPushButton::clicked,anatomy,&AnatomyView::resetCamera);
    canvas->addWidget(anatomy,1);center->addLayout(canvas,1);

    auto* inspector=card("inspector");inspector->setFixedWidth(274);
    auto* info=new QVBoxLayout(inspector);info->setContentsMargins(17,18,17,16);info->setSpacing(10);
    info->addWidget(label("CLINICAL OBSERVATION","eyebrow"));
    organStatus=label("生命档案","section");info->addWidget(organStatus);
    diseaseLabel=label("未见明显异常","");diseaseLabel->setWordWrap(true);info->addWidget(diseaseLabel);
    details=new QTextBrowser;details->setObjectName("organDetails");details->setOpenExternalLinks(false);info->addWidget(details,1);
    checkupLabel=label("","muted");checkupLabel->setWordWrap(true);info->addWidget(checkupLabel);
    checkupButton=new QPushButton("全身体检");checkupButton->setObjectName("checkupButton");
    connect(checkupButton,&QPushButton::clicked,this,&MainWindow::runCheckup);info->addWidget(checkupButton);
    researchLabel=label("科研 0 / 17","section");info->addWidget(researchLabel);
    researchInfo=label("","muted");researchInfo->setWordWrap(true);info->addWidget(researchInfo);
    researchButton=new QPushButton("科研训练");researchButton->setObjectName("researchButton");
    researchButton->setToolTip("18 岁起开放，需满足资金、3 年训练间隔、每局最多提升 5 点及科研总上限 17 的条件。");
    connect(researchButton,&QPushButton::clicked,this,[this]{
        if(engine->buyResearchPoint())log("科研提升 1 点：收入增加，体检与续命成本降低；下一次训练需间隔 3 年。");
    });info->addWidget(researchButton);center->addWidget(inspector);
    root->addLayout(center,1);

    auto* bottom=new QHBoxLayout;bottom->setSpacing(12);
    auto* pharmacy=card("pharmacy");auto* pharmacyLayout=new QVBoxLayout(pharmacy);pharmacyLayout->setContentsMargins(14,12,14,12);
    auto* filter=new QHBoxLayout;filter->addWidget(label("治疗中心","section"));
    category=new QComboBox;category->addItems({"推荐治疗","全部","药剂","手术","保健品"});category->setMinimumWidth(105);filter->addWidget(category);
    search=new QLineEdit;search->setObjectName("medicineSearch");search->setPlaceholderText("搜索药品、疾病名称或编号");search->setClearButtonEnabled(true);filter->addWidget(search,1);
    pharmacyLayout->addLayout(filter);
    auto* treatment=new QHBoxLayout;medicines=new QListWidget;medicines->setObjectName("medicineList");medicines->setMinimumWidth(310);
    treatment->addWidget(medicines,3);
    auto* selectedMed=new QVBoxLayout;medicineInfo=label("选择一种治疗方式","");medicineInfo->setWordWrap(true);selectedMed->addWidget(medicineInfo,1);
    auto* action=new QHBoxLayout;dosage=new QSpinBox;dosage->setRange(1,10);dosage->setPrefix("数量 ");dosage->setMaximumWidth(100);
    treatButton=new QPushButton("进行治疗");treatButton->setObjectName("primary");treatButton->setMinimumWidth(130);
    treatButton->setToolTip("优先处理选中器官的当前主病。治疗失败后需推进至少 1 个 Tick 才能重试。");
    action->addWidget(dosage);action->addWidget(treatButton,1);selectedMed->addLayout(action);treatment->addLayout(selectedMed,4);pharmacyLayout->addLayout(treatment,1);
    pharmacy->setFixedHeight(212);bottom->addWidget(pharmacy,3);
    auto* logs=card("card");auto* logLayout=new QVBoxLayout(logs);logLayout->setContentsMargins(14,12,14,10);
    logLayout->addWidget(label("生命记录","section"));journal=new QTextBrowser;journal->setObjectName("journal");journal->document()->setMaximumBlockCount(300);
    logLayout->addWidget(journal);logs->setFixedHeight(212);bottom->addWidget(logs,1);root->addLayout(bottom);
    connect(search,&QLineEdit::textChanged,this,&MainWindow::refreshMedicines);
    connect(category,&QComboBox::currentIndexChanged,this,&MainWindow::refreshMedicines);
    connect(medicines,&QListWidget::currentRowChanged,this,&MainWindow::refreshMedicineDetail);
    connect(dosage,&QSpinBox::valueChanged,this,&MainWindow::refreshMedicineDetail);
    connect(treatButton,&QPushButton::clicked,this,&MainWindow::treat);
    statusBar()->showMessage("三维器官实时观察  /  有进展时每 10 秒自动保存  /  Ctrl+S 独立手动存档，空格暂停");
    auto* saveAction=new QAction(this);saveAction->setShortcut(QKeySequence::Save);addAction(saveAction);connect(saveAction,&QAction::triggered,this,&MainWindow::saveGame);
    auto* pauseAction=new QAction(this);pauseAction->setShortcut(Qt::Key_Space);addAction(pauseAction);connect(pauseAction,&QAction::triggered,this,[this]{setPaused(!manualPause);});
}
void MainWindow::setPaused(bool value){
    if(!value&&interactive&&!hasCurrentSession)value=true;
    manualPause=value;engine->setPaused(value);pauseButton->setText(value?"继续":"暂停");elapsed.restart();
}
void MainWindow::selectOrgan(const QString& name){
    selected=name;anatomy->setOrgan(name);
    const bool section=AnatomyGeometry::prefersSection(name,DiseaseVisuals::forState(engine->getOrgans().value(name)));
    QSignalBlocker blocker(cutButton);cutButton->setChecked(section);anatomy->setCutaway(section);
    refresh();
}
void MainWindow::refresh(){
    if(refreshing)return;
    refreshing=true;
    const int age=engine->getAge();
    QString stage=age<=5?"婴儿期":age<=12?"儿童期":age<=18?"青年期":age<=60?"成年期":"老年期";
    ageLabel->setText(QString("%1 岁 · %2").arg(age).arg(stage));
    moneyLabel->setText(QString("金币 %1").arg(QLocale().toString(engine->getMoney())));
    healthBar->setValue(int(engine->getHealth()));immuneBar->setValue(int(engine->getImmunity()));
    auto val=[](double n,bool bad,int dec){return QString("<span style='color:%1'>%2</span>").arg(bad?"#ee9c86":"#cadbd0").arg(n,0,'f',dec);};
    vitalsLabel->setText(QString("血压 %1/%2　 血糖 %3　 血脂 %4")
        .arg(val(engine->getSystolic(),engine->getSystolic()>140||engine->getSystolic()<90,0),
             val(engine->getDiastolic(),engine->getDiastolic()>90||engine->getDiastolic()<60,0),
             val(engine->getBloodSugar(),engine->getBloodSugar()>7||engine->getBloodSugar()<3.9,1),
             val(engine->getBloodLipids(),engine->getBloodLipids()>2||engine->getBloodLipids()<0.5,1)));
    const auto states=engine->getOrgans();anatomy->setAge(engine->visualAge());anatomy->setStates(states);
    bodyButton->setChecked(selected.isEmpty());
    int count=0;
    for(const auto& name:organOrder){
        const auto o=states.value(name);auto* btn=organButtons[name];btn->setChecked(name==selected);
        const auto coexisting=diagnosedComorbidities(o);
        QString status=o.isDiseased?QString("%1级 · %2").arg(o.currentDisease.severity).arg(stageName(o.diseaseStage)):"未见明显异常";
        if(!coexisting.isEmpty())status+=QString(" · 合并 %1 病").arg(coexisting.size());
        if(o.isDiseased||!coexisting.isEmpty())++count;
        btn->setText(name+"\n"+status);
        btn->setStyleSheet(QString("color:%1;").arg(severityColor(o.isDiseased?o.currentDisease.severity:0)));
    }
    titleLabel->setText(selected.isEmpty()?"全身概览":selected+" · 组织观察");
    cutButton->setEnabled(!selected.isEmpty());skinButton->setEnabled(selected.isEmpty());
    if(selected.isEmpty()){
        organStatus->setText("生命档案");
        diseaseLabel->setText(count?QString("%1 个器官存在已确诊病变").arg(count):"未见明显异常");
        QString html="<p style='color:#97bdb2'>观察 · 发现 · 干预</p><p>旋转人体观察器官位置，点击器官进入详细的三维解剖视图。</p>"
            "<p>内部剖视可查看血管、气道和组织层。病灶表现随疾病、病期和治疗进度变化。</p>"
            "<p>隐性疾病在体检或症状出现前不显示。合并疾病会同时影响身体，治疗中心优先处理当前主病。</p>";
        html+=QString("<p>当前寿命上限：<b>%1 岁</b><br>目标：110 岁</p>").arg(engine->getMaxAge());
        details->setHtml(html);
    }else{
        const auto o=states.value(selected);organStatus->setText(selected+"观察");
        if(o.isDiseased){
            auto visual=DiseaseVisuals::forState(o);diseaseLabel->setText(o.currentDisease.name);
            diseaseLabel->setStyleSheet("color:"+severityColor(o.currentDisease.severity)+";font-size:16px;font-weight:600;");
            QString html=QString("<p><b>%1 级%2</b></p><p>%3</p><p style='color:#91bdb3'>可视变化</p><p>%4</p>")
                .arg(o.currentDisease.severity).arg(o.diseaseStage?" · "+stageName(o.diseaseStage):"")
                .arg(o.currentDisease.symptoms.toHtmlEscaped(),visual.description.toHtmlEscaped());
            if(!o.currentDisease.treatmentStages.isEmpty()){
                html+=QString("<p style='color:#91bdb3'>治疗进度 %1 / %2</p>").arg(o.currentTreatmentStage).arg(o.currentDisease.treatmentStages.size());
                for(int i=0;i<o.currentDisease.treatmentStages.size();++i){
                    QString med=Database::getInstance().getMedicineById(o.currentDisease.treatmentStages[i]).name.toHtmlEscaped();
                    html+=QString("<p style='color:%1'>%2 %3</p>").arg(i<o.currentTreatmentStage?"#688f84":i==o.currentTreatmentStage?"#ecd2a4":"#9badab")
                        .arg(i<o.currentTreatmentStage?"✓":QString::number(i+1)+".").arg(med);
                }
            }else html+="<p>在下方「推荐治疗」中选择对症药物。</p>";
            if(o.autoHealTimer>0)html+=QString("<p>预计自愈：剩余 %1 个 Tick</p>").arg(o.autoHealTimer);
            const auto coexisting=diagnosedComorbidities(o);
            if(!coexisting.isEmpty()){
                html+="<p style='color:#ecd2a4'>已确诊合并病</p>";
                for(const auto& name:coexisting)html+="<p>• "+name.toHtmlEscaped()+"</p>";
                html+="<p>当前优先治疗主病；主病治愈后自动切换至下一病情。合并病仍需关注。</p>";
            }
            details->setHtml(html);
        }else{
            diseaseLabel->setText("未发现可见病变");diseaseLabel->setStyleSheet("color:#a6c9aa;font-size:15px;");
            QMap<QString,QString> desc{
                {"心脏","观察心室、心房、主动脉弓、冠状血管。打开剖视可见心腔、瓣膜与腱索。"},
                {"肺部","观察左右肺轮廓、肺叶分界、气管软骨环。打开剖视可见分支气道。"},
                {"肝脏","观察肝叶、胆囊及胆管。打开剖视可见组织内分支血管。"},
                {"胃部","观察胃、食管、十二指肠及相邻结构。打开剖视可见胃黏膜皱襞与胰腺。"},
                {"皮肤","观察立体皮肤组织样本。剖视可见表皮、真皮、皮下脂肪、血管及毛囊。"}};
            QString html="<p>"+desc.value(selected)+"</p><p>拖动模型可以从不同方向观察，滚轮调整远近。</p><p>外观正常不代表没有隐性疾病；定期体检可以发现早期病变。</p>";
            const auto coexisting=diagnosedComorbidities(o);
            if(!coexisting.isEmpty()){
                diseaseLabel->setText("存在已确诊合并病");
                html+="<p style='color:#ecd2a4'>已确诊合并病</p>";
                for(const auto& name:coexisting)html+="<p>• "+name.toHtmlEscaped()+"</p>";
            }
            details->setHtml(html);
        }
    }
    const int checkupInterval=engine->getCheckupInterval(),checkupCost=engine->getCheckupCost();
    int remaining=std::max(0,checkupInterval-(age-engine->getLastCheckupAge()));
    checkupLabel->setText(remaining?QString("下次体检：%1 岁（还需 %2 年）").arg(age+remaining).arg(remaining):QString("当前可以体检 · 每 %1 年一次").arg(checkupInterval));
    checkupButton->setText(QString("全身体检 · %1").arg(QLocale().toString(checkupCost)));
    checkupButton->setEnabled(!engine->isFinished()&&!engine->hasPendingMilestone()&&remaining==0&&engine->getMoney()>=checkupCost);
    researchLabel->setText(QString("科研 %1 / 17").arg(engine->getResearch()));
    researchInfo->setText(QString("当前年收入 %1 金币\n每次续命 %2 金币 · 支付后成功\n18 岁起训练，每 3 年一次，最多 +5 点")
        .arg(QLocale().toString(age<18?1000:engine->getAnnualIncome()),QLocale().toString(engine->getLifeExtensionCost())));
    researchButton->setText(QString("科研训练 · %1").arg(QLocale().toString(engine->getResearchPointCost())));
    researchButton->setEnabled(!engine->isFinished()&&!engine->hasPendingMilestone()&&engine->canBuyResearchPoint()&&engine->getMoney()>=engine->getResearchPointCost());
    pauseButton->setEnabled((!interactive||hasCurrentSession)&&!engine->isFinished()&&!engine->hasPendingMilestone());
    refreshMedicines();refreshing=false;
}
void MainWindow::refreshMedicines(){
    const QString previous=medicines->currentItem()?medicines->currentItem()->data(Qt::UserRole).toString():"";
    const auto state=engine->getOrgans().value(selected);const QString filter=search->text().trimmed(),type=category->currentText();
    // Both the database and the displayed rows are ordered by medicine ID.
    // Reconcile only changed rows so a simulation tick cannot disturb selection or hover.
    QSignalBlocker blocker(medicines);
    int row=0;
    bool previousStillAvailable=false;
    for(const auto& med:Database::getInstance().getAllMedicines()){
        if(type=="推荐治疗"){
            if(selected.isEmpty()||!state.isDiseased){if(med.type!="保健品")continue;}
            else if(!engine->medicineMatches(state,med))continue;
        }else if(type!="全部"&&type!=med.type)continue;
        QString text=med.name+" "+med.indication;
        for(const auto& id:med.indication.split('|'))text+=" "+Database::getInstance().getDiseaseById(id).name;
        if(!filter.isEmpty()&&!text.contains(filter,Qt::CaseInsensitive))continue;
        while(row<medicines->count()&&medicines->item(row)->data(Qt::UserRole).toString()<med.id)
            delete medicines->takeItem(row);
        if(row==medicines->count()||medicines->item(row)->data(Qt::UserRole).toString()!=med.id){
            auto* item=new QListWidgetItem(QString("%1   ·   %2").arg(med.name,QLocale().toString(med.price)));
            item->setData(Qt::UserRole,med.id);item->setToolTip(med.usage+"\n"+med.sideEffectsDesc);
            medicines->insertItem(row,item);
        }
        if(previous==med.id)previousStillAvailable=true;
        ++row;
    }
    while(row<medicines->count())delete medicines->takeItem(row);
    if(row&&!previousStillAvailable)medicines->setCurrentRow(0);
    refreshMedicineDetail();
}
void MainWindow::refreshMedicineDetail(){
    if(!medicines->currentItem()){medicineInfo->setText("没有符合条件的治疗方式。");treatButton->setEnabled(false);return;}
    const auto med=Database::getInstance().getMedicineById(medicines->currentItem()->data(Qt::UserRole).toString());
    QSignalBlocker blocker(dosage);
    const int maximum=med.type=="手术"?1:10;
    if(dosage->maximum()!=maximum)dosage->setMaximum(maximum);
    QString target=med.type=="保健品"?"全身体征":selected.isEmpty()?"请先选择器官":selected;
    QString description=QString("<b>%1</b><br><span style='color:#8da7a3'>%2 · %3<br>副作用：%4<br>目标：%5　安全数量：%6</span>")
        .arg(med.name.toHtmlEscaped(),med.type,med.usage.toHtmlEscaped(),med.sideEffectsDesc.toHtmlEscaped(),target).arg(med.safeDosage);
    if(med.id=="M52")description+="<br><span style='color:#ecd2a4'>支持治疗：仅暂时控制病情，不会直接治愈。</span>";
    medicineInfo->setText(description);
    int cost=med.price*dosage->value();treatButton->setText(QString("使用 · %1").arg(QLocale().toString(cost)));
    treatButton->setEnabled(!engine->isFinished()&&!engine->hasPendingMilestone()&&engine->getMoney()>=cost
        &&(med.type=="保健品"||!selected.isEmpty()));
}
void MainWindow::treat(){
    if(!medicines->currentItem())return;
    const QString id=medicines->currentItem()->data(Qt::UserRole).toString();
    const auto med=Database::getInstance().getMedicineById(id);
    QString conflict;
    if(engine->checkConflict(id,conflict)){log("当前用药与"+conflict+"冲突，等待药效消退后再试。");return;}
    if(interactive&&(med.type=="手术"||med.price*dosage->value()>=20000)){
        PauseScope pause(engine,elapsed);
        if(QMessageBox::question(this,"确认治疗",QString("在%1使用%2，费用 %3 金币。\n%4\n是否继续？")
            .arg(med.type=="保健品"?"全身":selected,med.name,QLocale().toString(med.price*dosage->value()),
                med.id=="M52"?"这是支持治疗：只暂时控制病情，不会直接治愈。":med.type=="手术"?"分期治疗可能失败；失败仍会消耗费用并损失生命，需等待至少 1 个 Tick 才能重试。":"治疗效果与当前病情及方案有关。"),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    }
    int oldMoney=engine->getMoney();
    bool ok=engine->applyMedicine(selected,id,dosage->value());
    if(oldMoney!=engine->getMoney())anatomy->playTreatment(selected.isEmpty()?"心脏":selected,id,ok);
    refresh();
}
void MainWindow::log(const QString& message){
    journal->append(QString("<span style='color:#7a9f97'>%1 岁</span>　%2").arg(engine->getAge()).arg(message.toHtmlEscaped()));
}
void MainWindow::newGame(){
    PauseScope pause(engine,elapsed);
    StartupDialog dialog(this);
    if(dialog.exec()!=QDialog::Accepted){setPaused(true);pause.before=true;return;}
    // A new life gets a new history: retain the previous automatic checkpoint.
    const auto previousAuto=savePath("autosave");
    if(!previousAuto.isEmpty()&&QFileInfo::exists(previousAuto)){
        const auto backup=savePath(SaveRepository::createManualSlot());
        if(backup.isEmpty()||!QFile::copy(previousAuto,backup)){
            QMessageBox::warning(this,"未能保留上一人生","旧自动档无法备份，因此没有开始新人生。请确认存档位置有可用空间与写入权限。");
            return;
        }
    }
    if(hasCurrentSession&&saveDirty&&!writeManualSave())return;
    suppressSaveTracking=true;
    engine->initGame();engine->setPlayerAttributes(dialog.getPlayerName(),dialog.getWealth(),dialog.getLuck(),dialog.getConstitution(),dialog.getResearch());
    suppressSaveTracking=false;hasCurrentSession=true;saveDirty=false;autosaveArmed=false;
    journal->clear();log("新的人生开始。观察体征、关注器官变化，尝试活到 110 岁。");
    speed->setValue(20);selectOrgan("");manualPause=false;pause.before=false;pauseButton->setText("暂停");refresh();
}
QList<SaveEntry> MainWindow::saveEntries()const{
    return saveRoot.isEmpty()?SaveRepository::entries():SaveRepository::Store(saveRoot).entries();
}
QString MainWindow::savePath(const QString& slot)const{
    return saveRoot.isEmpty()?SaveRepository::pathFor(slot):SaveRepository::Store(saveRoot).pathFor(slot);
}
bool MainWindow::writeManualSave(){
    if(!hasCurrentSession){log("请先开始或载入一段人生。");return false;}
    const auto path=savePath(SaveRepository::createManualSlot());
    if(path.isEmpty()){
        QMessageBox::warning(this,"保存失败","无法建立可写的存档位置；已有存档未改变。");return false;
    }
    if(!engine->saveGame(path)){
        QMessageBox::warning(this,"保存失败",engine->lastError());return false;
    }
    log("已创建独立手动存档，可在「存档管理」中继续；其他存档未覆盖。");
    statusBar()->showMessage("手动存档已保存 · 在存档管理中查看",6000);
    return true;
}
bool MainWindow::writeAutosave(bool showFailure){
    if(!interactive||!hasCurrentSession||!autosaveArmed||!saveDirty||suppressSaveTracking)return true;
    if(engine->hasPendingMilestone())return false;
    const auto path=savePath("autosave");
    if(path.isEmpty()||!engine->saveGame(path)){
        const auto message=path.isEmpty()?QString("无法建立可写的存档位置。"):engine->lastError();
        statusBar()->showMessage("自动保存失败："+message,10000);
        if(showFailure)QMessageBox::warning(this,"自动保存失败",message+"\n已有存档未改变。");
        return false;
    }
    saveDirty=false;
    statusBar()->showMessage("已自动保存 · "+QString::number(engine->getAge())+" 岁 · "+QTime::currentTime().toString("HH:mm:ss"),4000);
    return true;
}
void MainWindow::saveGame(){PauseScope pause(engine,elapsed);writeManualSave();}
void MainWindow::loadGame(){showSaveManager();}
void MainWindow::showSaveManager(bool startup){
    bool startAnotherLife=false;
    {
        PauseScope pause(engine,elapsed);
        QDialog dialog(this);dialog.setObjectName("saveManager");dialog.setWindowTitle("人生存档");dialog.resize(710,520);
        auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,22,24,22);layout->setSpacing(12);
        layout->addWidget(label(startup?"继续你的人生":"人生存档","heading"));
        auto* explanation=label("自动档记录最近进展；手动档独立保留。载入后暂停，点击「继续」才推进时间。","muted");
        explanation->setWordWrap(true);layout->addWidget(explanation);
        auto* list=new QListWidget;list->setObjectName("saveSlotList");layout->addWidget(list,1);
        auto* note=label("","muted");note->setWordWrap(true);note->setTextFormat(Qt::PlainText);layout->addWidget(note);
        const auto folder=saveRoot.isEmpty()?SaveRepository::directory():saveRoot;
        auto* location=label(folder.isEmpty()?"没有可写的存档位置：请检查磁盘空间与权限。":"存档位置："+folder,"muted");
        location->setTextFormat(Qt::PlainText);location->setWordWrap(true);
        location->setToolTip("优先保存在游戏旁的 saves 文件夹；不可写时自动使用当前用户的应用数据目录。");layout->addWidget(location);
        auto* actions=new QHBoxLayout;
        auto* create=new QPushButton("新建手动档");create->setObjectName("createManualSave");
        auto* resume=new QPushButton("继续此存档");resume->setObjectName("resumeSave");
        auto* fresh=new QPushButton("新人生");fresh->setObjectName("startNewLife");
        auto* cancel=new QPushButton("返回");
        actions->addWidget(create);actions->addStretch();actions->addWidget(fresh);actions->addWidget(resume);actions->addWidget(cancel);layout->addLayout(actions);
        QList<SaveEntry> entries;
        auto populate=[&]{
            entries=saveEntries();list->clear();
            for(const auto& entry:entries){
                QString text=QString("%1  ·  %2\n%3 岁  ·  %4  ·  %5")
                    .arg(entry.slot=="autosave"?"自动存档":"手动存档",entry.name)
                    .arg(entry.age).arg(entry.modified.toString("yyyy-MM-dd HH:mm:ss"),entry.valid?(entry.finished?"人生已结束 · 可回看":"可继续"):"无法读取 · 原文件保留");
                auto* item=new QListWidgetItem(text,list);item->setData(Qt::UserRole,entry.slot);
                if(!entry.valid)item->setForeground(QColor("#ca9a8b"));
            }
            if(!entries.isEmpty())list->setCurrentRow(0);
            else note->setText("还没有存档。开始新人生后会自动保存，也可以随时创建独立手动档。");
            create->setEnabled(hasCurrentSession&&!engine->hasPendingMilestone());
            resume->setEnabled(!entries.isEmpty()&&entries.first().valid&&!engine->hasPendingMilestone());
        };
        connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row){
            const bool found=row>=0&&row<entries.size();
            resume->setEnabled(found&&entries[row].valid&&!engine->hasPendingMilestone());
            resume->setText(found&&entries[row].finished?"查看此人生":"继续此存档");
            if(found)note->setText(entries[row].valid?"每次载入都会完整校验；如校验失败，当前人生不会改变。":"此存档无法读取，文件已保留，不会自动删除或覆盖。请选择其他存档。");
        });
        connect(create,&QPushButton::clicked,&dialog,[&]{if(writeManualSave())populate();});
        auto restore=[&]{
            const int row=list->currentRow();if(row<0||row>=entries.size()||!entries[row].valid)return;
            const auto path=savePath(entries[row].slot);
            if(path.isEmpty()){QMessageBox::warning(&dialog,"读取失败","存档位置不可用，当前人生未改变。");return;}
            if(hasCurrentSession&&saveDirty&&!writeManualSave())return;
            suppressSaveTracking=true;
            const bool loaded=engine->loadGame(path);suppressSaveTracking=false;
            if(!loaded){QMessageBox::warning(&dialog,"读取失败",engine->lastError());return;}
            hasCurrentSession=true;saveDirty=false;autosaveArmed=false;
            manualPause=true;pause.before=true;pauseButton->setText(engine->isFinished()?"已结束":"继续");
            speed->setValue(qRound(engine->getTimeMultiplier()*10));
            log(engine->isFinished()?"已载入结束的人生，可继续观察记录与身体。":"存档已恢复；暂停期间不会覆盖自动档，点击「继续」推进时间。");
            refresh();dialog.accept();
        };
        connect(resume,&QPushButton::clicked,&dialog,restore);
        connect(list,&QListWidget::itemDoubleClicked,&dialog,[&](QListWidgetItem*){if(resume->isEnabled())restore();});
        connect(fresh,&QPushButton::clicked,&dialog,[&]{startAnotherLife=true;dialog.accept();});
        connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
        populate();dialog.exec();
    }
    if(startAnotherLife)newGame();
}
void MainWindow::closeEvent(QCloseEvent* event){
    PauseScope pause(engine,elapsed);
    if(engine->hasPendingMilestone()){
        if(interactive)QMessageBox::information(this,"请先完成寿命事件","当前寿命事件尚未完成，暂不能保存。完成选择后再关闭，已有存档会保留。");
        event->ignore();return;
    }
    if(!writeAutosave(true)){
        if(QMessageBox::question(this,"进度尚未保存","自动保存失败。仍要关闭吗？未保存的进度会丢失。",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){event->ignore();return;}
    }
    QMainWindow::closeEvent(event);
}
void MainWindow::runCheckup(){
    if(interactive){
        PauseScope pause(engine,elapsed);
        if(QMessageBox::question(this,"全身体检",QString("支付 %1 金币进行全身体检？\n当前科研对应体检间隔为 %2 年。")
            .arg(QLocale().toString(engine->getCheckupCost())).arg(engine->getCheckupInterval()),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)!=QMessageBox::Yes)return;
    }
    engine->performCheckup();refresh();
}
void MainWindow::handleMilestone(int type){
    if(!interactive||engine->isFinished()||!engine->hasPendingMilestone())return;
    PauseScope pause(engine,elapsed);
    if(type==80||type==100){
        bool ok;
        QString name=QInputDialog::getText(this,"寿命管理",QString("%1 岁：已解锁寿命延长项目。\n为这项技术命名（仍需支付项目费用）：").arg(type),
            QLineEdit::Normal,type==80?"基因修复疗法":"端粒修复技术",&ok);
        engine->setLifeExtensionTechName(ok?name:"基因修复疗法");
    }
    bool accepts=QMessageBox::question(this,"寿命大限",
        QString("当前 %1 岁。\n支付 %2 金币延长 10 年寿命？支付后确定成功。\n当前金币：%3\n不支付将结束本次人生。")
            .arg(engine->getAge()).arg(QLocale().toString(engine->getLifeExtensionCost()),QLocale().toString(engine->getMoney())),QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes;
    if(!accepts||!engine->payForLifeExtension())engine->declineLifeExtension();
    refresh();
}
void MainWindow::showGameOver(const QString& reason){
    if(!engine->isFinished())return;
    manualPause=true;engine->setPaused(true);pauseButton->setText("已结束");
    log(reason);
    if(interactive)QMessageBox::information(this,engine->getAge()>=110?"漫长的一生":"人生结束",
        reason+"\n\n可以继续观察身体，读取存档，或点击「新人生」重新开始。");
    refresh();
}
