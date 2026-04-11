#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

class StartupDialog : public QDialog {
    Q_OBJECT
public:
    explicit StartupDialog(QWidget *parent = nullptr);
    
    QString getPlayerName() const;
    int getWealth() const;
    int getLuck() const;
    int getConstitution() const;
    int getResearch() const; // 新增科研获取

private slots:
    void updatePoints();
    void showHelp(int type);

private:
    QLineEdit *nameEdit;
    QSpinBox *wealthSpin;
    QSpinBox *luckSpin;
    QSpinBox *constiSpin;
    QSpinBox *researchSpin; // 新增科研组件
    QLabel *pointsLabel;
    QPushButton *startBtn;
    
    int totalPoints = 30; // 初始天赋点变为30
};
