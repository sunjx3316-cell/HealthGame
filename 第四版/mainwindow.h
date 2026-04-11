#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QMap>
#include <QTextEdit>
#include <QSlider>
#include <QLineEdit>
#include "GameEngine.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateTimerInterval();
    void showOrganDetail(const QString &organName);
    void returnToBodyView();
    void showGameOver(const QString& reason);
    void appendLogMessage(const QString& msg);
    void updateOrganButtonState(const QString& organName, bool isDiseased);
    void useMedicine();
    void showTreatmentGuide(); // 新增：显示诊疗指南槽函数
    void filterMedicines(const QString& searchText); // 新增：药品搜索过滤
    void saveGame();
    void loadGame();
    void flashScreen();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    
private:
    void setupUi();
    void createMedicineButtons();
    QString getAgeStageText(int age);
    QString getOrganFileName(const QString& organName);
    void crossFadeImage(QLabel* label, const QPixmap& newPixmap);

    // 引擎
    GameEngine *engine;
    QTimer *gameTimer;
    QTimer *realTimeTimer; // 新增实时计时器声明
    
    // 检查是否所有器官都健康的辅助函数
    bool areAllOrgansHealthy();

    // 核心 UI 组件
    QLabel *ageLabel;
    QLabel *ageStageLabel; 
    QLabel *moneyLabel; // 新增：金币显示
    QLabel *researchLabel; // 新增：科研显示
    QPushButton *buyResearchBtn; // 新增：购买科研点按钮
    QLabel *vitalsLabel; // 新增：体征显示 (血压/血糖/血脂)
    QProgressBar *healthBar;
    QProgressBar *immuneBar; // 免疫系统条
    QSlider *timeSlider;     // 时间流速控制
    QLabel *timeSpeedLabel;
    
    QStackedWidget *stackedWidget;
    
    // 页面1：人体全貌
    QWidget *bodyPage;
    QLabel *bodyBackgroundLabel;
    QMap<QString, QPushButton*> organButtons;
    
    // 页面2：器官详情
    QWidget *detailPage;
    QLabel *organDetailImageLabel; 
    QLabel *diseaseNameLabel;
    QTextEdit *symptomTextEdit;    
    QPushButton *returnBtn;
    QPushButton *guideBtn; // 新增：诊疗指南按钮
    
    // 底部：药箱和日志
    QWidget *bottomWidget;
    QTextEdit *logTextEdit;
    QWidget *medicineBoxWidget;
    QLineEdit *medicineSearchBar; // 新增：搜索框
    QMap<QString, QPushButton*> medicineButtons;

    QWidget *flashOverlay; // 红屏闪烁层
    QString currentViewedOrgan;
};

#endif // MAINWINDOW_H
