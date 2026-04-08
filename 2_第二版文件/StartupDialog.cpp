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
    
    // Title
    QLabel *titleLabel = new QLabel("欢迎来到《器官保卫战》\n\n请设定你的身份与天赋");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(QFont("Arial", 12, QFont::Bold));
    mainLayout->addWidget(titleLabel);
    
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
        spin->setRange(0, 15); // 上限15，下限0
        spin->setValue(0);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &StartupDialog::updatePoints);
        
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
        msg = "【家境】\n决定你的初始资金。\n家境越好，开局越容易买得起昂贵的医疗和保健品，但成年后的收入只与你的科研能力挂钩。";
    } else if (type == 1) {
        msg = "【气运】\n决定你的运气。\n气运越高，罹患致死性重大疾病（如癌症、心衰等）的概率越低。";
    } else if (type == 2) {
        msg = "【健康】\n决定你的基础免疫力和身体素质。\n健康度越高，免疫力随年龄下降的速度越慢，小病自愈的概率越高，且对异常血压、血糖的抵抗力更强。";
    } else if (type == 3) {
        msg = "【科研】\n极度重要的后期天赋，但也伴随致命的健康风险！\n决定了你成年后的每年收入，同时也是80岁攻克绝症、100岁研发长寿技术的唯一依仗。\n【警告】：高强度的科研会严重透支身体！科研点数越高，成年后你的血压、血糖等体征恶化得越快，且每年会固定扣除免疫力！";
    }
    QMessageBox::information(this, "天赋说明", msg);
}
