#include "StartupDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>

StartupDialog::StartupDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("人生重开 - 天赋分配");
    setModal(true);
    setFixedSize(400, 300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 新增：游戏介绍与指引
    QLabel *introLabel = new QLabel(
        "<b><font size='5' color='#E91E63'>欢迎来到《器官保卫战》</font></b><br><br>"
        "<b>游戏目标</b>：平衡你的金钱、体征和免疫力，努力活到100岁！<br>"
        "<b>核心玩法</b>：<br>"
        "1. <font color='red'>随时关注左上角的血压、血糖、血脂。</font>如果它们偏高或偏低，会持续扣除你的生命值！请及时在下方【药箱】中服用对应的保健品控制。<br>"
        "2. <font color='red'>免疫力决定了你得重病的概率。</font>请通过吃高级保健品（如冬虫夏草）维持免疫力。<br>"
        "3. <font color='blue'>每3年的体检极其重要</font>，它能帮你查出早期的癌症，否则晚期癌症将带来毁灭性打击。<br><br>"
        "<i>👇 请分配你的初始天赋（强烈建议点击右侧的 <b>[ ? ]</b> 按钮查看详细机制说明）：</i>", this);
    introLabel->setWordWrap(true);
    introLabel->setStyleSheet("background-color: #f9f9f9; border: 1px solid #ccc; padding: 10px; border-radius: 5px;");
    mainLayout->addWidget(introLabel);
    
    // Name
    QHBoxLayout *nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("姓名:"));
    nameEdit = new QLineEdit("无名氏");
    nameLayout->addWidget(nameEdit);
    mainLayout->addLayout(nameLayout);
    
    // Points
    pointsLabel = new QLabel(QString("剩余天赋点: %1").arg(totalPoints)); // 初始每项0点
    pointsLabel->setAlignment(Qt::AlignCenter);
    pointsLabel->setStyleSheet("color: blue; font-weight: bold;");
    mainLayout->addWidget(pointsLabel);
    
    // Grid for attributes
    QGridLayout *gridLayout = new QGridLayout();
    
    auto addRow = [&](int row, const QString& name, QSpinBox*& spin, int helpType) {
        QLabel *label = new QLabel(name);
        spin = new QSpinBox();
        if (name == "科研") {
            spin->setRange(0, 17); // 科研上限17
        } else {
            spin->setRange(0, 15); // 其他上限15，下限0
        }
        spin->setValue(0);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, spin](int value){
            // 实时检查，如果不允许分配（剩余点数不足且尝试增加），则回退
            int used = wealthSpin->value() + luckSpin->value() + constiSpin->value() + researchSpin->value();
            if (used > totalPoints) {
                spin->setValue(value - (used - totalPoints)); // 强制回退到允许的最大值
            } else {
                updatePoints();
            }
        });
        
        QPushButton *helpBtn = new QPushButton("?");
        helpBtn->setFixedSize(20, 20);
        helpBtn->setStyleSheet("border-radius: 10px; background-color: #ddd;");
        connect(helpBtn, &QPushButton::clicked, this, [this, helpType](){ showHelp(helpType); });
        
        gridLayout->addWidget(label, row, 0);
        gridLayout->addWidget(spin, row, 1);
        gridLayout->addWidget(helpBtn, row, 2);
    };
    
    addRow(0, "家境", wealthSpin, 0);
    addRow(1, "气运", luckSpin, 1);
    addRow(2, "健康", constiSpin, 2);
    addRow(3, "科研", researchSpin, 3);
    
    mainLayout->addLayout(gridLayout);
    
    // Start Button
    startBtn = new QPushButton("开始人生");
    startBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 10px;");
    connect(startBtn, &QPushButton::clicked, this, [this](){
        if (totalPoints - (wealthSpin->value() + luckSpin->value() + constiSpin->value() + researchSpin->value()) < 0) {
            QMessageBox::warning(this, "错误", "天赋点超出了限制！");
            return;
        }
        accept();
    });
    mainLayout->addWidget(startBtn);
    
    updatePoints();
}

QString StartupDialog::getPlayerName() const { return nameEdit->text(); }
int StartupDialog::getWealth() const { return wealthSpin->value(); }
int StartupDialog::getLuck() const { return luckSpin->value(); }
int StartupDialog::getConstitution() const { return constiSpin->value(); }
int StartupDialog::getResearch() const { return researchSpin->value(); }

void StartupDialog::updatePoints() {
    int used = wealthSpin->value() + luckSpin->value() + constiSpin->value() + researchSpin->value();
    int remain = totalPoints - used;
    pointsLabel->setText(QString("剩余天赋点: %1").arg(remain));
    
    if (remain < 0) {
        pointsLabel->setStyleSheet("color: red; font-weight: bold;");
        startBtn->setEnabled(false);
    } else {
        pointsLabel->setStyleSheet("color: blue; font-weight: bold;");
        startBtn->setEnabled(true);
    }
}

void StartupDialog::showHelp(int type) {
    QString msg;
    if (type == 0) {
        msg = "【家境】\n决定你的初始资金。\n10级及以前资金增长平缓，11级起资金爆发式增长，15级满级可获得150万初始金币！";
    } else if (type == 1) {
        msg = "【气运】\n决定你的运气。\n气运越高，罹患致死性重大疾病（如癌症、心衰等）的概率越低，且50岁前基本免疫绝症。";
    } else if (type == 2) {
        msg = "【健康】\n决定你的基础免疫力和身体素质。\n健康度越高，小病自愈概率越高。\n【核心特效1】：大幅减缓血压、血糖、血脂随年龄的恶化速度。\n【核心特效2】：极大地抵消『科研』带来的免疫力下降副作用！";
    } else if (type == 3) {
        msg = "【科研】\n极度重要的后期天赋，但也伴随致命的健康风险！上限17级。\n【收益】：满级17时，80岁攻克绝症及100岁研发长寿技术的成功率为100%！（研发成功后每年还会获得10万专利费）。\n【副作用】：科研点数越高，成年后你的体征恶化得越快，且每年会固定扣除免疫力！";
    }
    QMessageBox::information(this, "天赋说明", msg);
}
