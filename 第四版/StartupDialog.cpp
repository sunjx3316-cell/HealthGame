#include "StartupDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFrame>

StartupDialog::StartupDialog(QWidget* parent):QDialog(parent){
    setWindowTitle("开始新的人生");setMinimumWidth(510);resize(540,560);setModal(true);
    auto* main=new QVBoxLayout(this);main->setContentsMargins(26,25,26,24);main->setSpacing(13);
    auto* eyebrow=new QLabel("HEALTHGAME / ANATOMY 3D");eyebrow->setStyleSheet("color:#8dbbae;font-size:11px;");main->addWidget(eyebrow);
    auto* title=new QLabel("你的下一次人生");title->setStyleSheet("font-size:25px;color:#e7eddd;font-weight:600;");main->addWidget(title);
    auto* intro=new QLabel("分配 30 点天赋，从出生开始管理身体。\n观察三维器官，控制体征，发现并治疗疾病，目标是 110 岁。");intro->setWordWrap(true);intro->setStyleSheet("color:#a4bbb3;");main->addWidget(intro);
    nameEdit=new QLineEdit("无名氏");nameEdit->setMaxLength(40);nameEdit->setPlaceholderText("角色姓名");main->addWidget(nameEdit);
    pointsLabel=new QLabel;main->addWidget(pointsLabel);
    auto* grid=new QGridLayout;
    // Construct all controls before connecting valueChanged callbacks.
    wealthSpin=new QSpinBox;luckSpin=new QSpinBox;constiSpin=new QSpinBox;researchSpin=new QSpinBox;
    QList<QSpinBox*> spins{wealthSpin,luckSpin,constiSpin,researchSpin};
    const QStringList names{"家境","气运","健康","科研"};
    const QStringList descriptions{"储备资金 / 稳定收入","重病风险 / 治疗成功率","免疫恢复 / 自愈 / 减伤","收入 / 体检效率 / 续命成本"};
    for(int i=0;i<4;++i){
        spins[i]->setRange(0,15);spins[i]->setValue(i==2?9:7);
        auto* text=new QLabel(names[i]+"\n"+descriptions[i]);grid->addWidget(text,i,0);
        grid->addWidget(spins[i],i,1);
        auto* help=new QPushButton("?");help->setFixedWidth(32);
        connect(help,&QPushButton::clicked,this,[this,i]{showHelp(i);});grid->addWidget(help,i,2);
    }
    for(auto* spin:spins)connect(spin,QOverload<int>::of(&QSpinBox::valueChanged),this,[this,spin](int value){
        int used=wealthSpin->value()+luckSpin->value()+constiSpin->value()+researchSpin->value();
        if(used>totalPoints)spin->setValue(value-(used-totalPoints));
        updatePoints();
    });
    main->addLayout(grid);
    auto* hint=new QLabel("家境走资金储备，健康走耐久自愈，气运降低风险，科研重视筛查与长期投入。\n天赋相互补足：高科研有小幅身体负担；各路线都要留出治疗与续命预算。");hint->setWordWrap(true);hint->setStyleSheet("color:#89a49b;");main->addWidget(hint);
    main->addStretch();startBtn=new QPushButton("开始人生");startBtn->setObjectName("primary");
    connect(startBtn,&QPushButton::clicked,this,&QDialog::accept);main->addWidget(startBtn);updatePoints();
}
QString StartupDialog::getPlayerName()const{return nameEdit->text();}
int StartupDialog::getWealth()const{return wealthSpin->value();}
int StartupDialog::getLuck()const{return luckSpin->value();}
int StartupDialog::getConstitution()const{return constiSpin->value();}
int StartupDialog::getResearch()const{return researchSpin->value();}
void StartupDialog::updatePoints(){
    int left=totalPoints-getWealth()-getLuck()-getConstitution()-getResearch();
    pointsLabel->setText(QString("剩余天赋点　%1 / 30").arg(left));
    pointsLabel->setStyleSheet("color:#cadbab;font-weight:600;");
    startBtn->setEnabled(left>=0);
}
void StartupDialog::showHelp(int type){
    const QStringList messages{
        "资金储备打法：更早负担检查与治疗，保留应急资金。\n初始金币 = 30,000 + 家境 × 60,000。\n成年基础年收入 = 32,000 + 家境 × 1,200 + 科研 × 1,500。\n家境不直接减轻病痛，需要搭配健康、气运或及时治疗。",
        "风险管理打法：平滑降低重大疾病风险，提高治疗成功率。\n气运不会让你完全免疫，也不会增加收入；仍要监测体征、体检并准备治疗费用。",
        "耐久自愈打法：提高基础免疫与恢复能力，加快轻病自愈，减轻疾病伤害及体征恶化。\n健康不能替代重病治疗，也不增加资金；要为不能自愈的疾病留预算。",
        "筛查与长期投入打法：提高成年收入，降低体检与续命费用。\n体检费用 = 30,000 − 科研 × 1,000；间隔随科研从 3 年缩短至 1 年。\n续命费用 = 1,200,000 − 科研 × 25,000，每次延长 10 年；支付后确定成功，不再随机突破。\n初始科研最多 15 点；18 岁起可后天训练，每 3 年一次，每局最多增加 5 点，总上限 17。每次训练费用 = 60,000 + 当前科研 × 8,000。\n科研有小幅体征负担，需搭配健康或主动管理体征。"};
    QMessageBox::information(this,"天赋说明",messages.value(type));
}
