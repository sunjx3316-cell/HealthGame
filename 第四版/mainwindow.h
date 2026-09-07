#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QListWidget>
#include <QElapsedTimer>
#include <QTimer>
#include "GameEngine.h"
#include "AnatomyView.h"
#include "SaveRepository.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr,bool interactive=true,const QString& saveDirectory={});
    GameEngine* gameEngine() const { return engine; }
    AnatomyView* anatomyView() const { return anatomy; }
    void selectOrgan(const QString& name);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    friend class SaveFlowTests;
    void setupUi();
    void refresh();
    void refreshMedicines();
    void refreshMedicineDetail();
    void treat();
    void newGame();
    void saveGame();
    void loadGame();
    void showSaveManager(bool startup=false);
    bool writeManualSave();
    bool writeAutosave(bool showFailure=false);
    QList<SaveEntry> saveEntries() const;
    QString savePath(const QString& slot) const;
    void runCheckup();
    void handleMilestone(int type);
    void showGameOver(const QString& reason);
    void log(const QString& message);
    void setPaused(bool value);

    GameEngine* engine;
    AnatomyView* anatomy;
    QTimer timer;
    QTimer autosaveTimer;
    QElapsedTimer elapsed;
    bool interactive,manualPause=false,refreshing=false;
    bool hasCurrentSession=false,suppressSaveTracking=false,saveDirty=false,autosaveArmed=false;
    QString selected;
    QString saveRoot;
    QLabel *ageLabel,*moneyLabel,*researchLabel,*vitalsLabel,*titleLabel,*organStatus,*diseaseLabel,*speedLabel;
    QLabel *medicineInfo,*checkupLabel,*researchInfo;
    QProgressBar *healthBar,*immuneBar;
    QMap<QString,QPushButton*> organButtons;
    QPushButton *pauseButton,*bodyButton,*cutButton,*skinButton,*treatButton,*checkupButton,*researchButton;
    QTextBrowser *details,*journal;
    QLineEdit* search;
    QComboBox* category;
    QListWidget* medicines;
    QSpinBox* dosage;
    QSlider* speed;
};
