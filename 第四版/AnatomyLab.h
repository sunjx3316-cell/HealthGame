#pragma once
#include <QDialog>
class AnatomyView;
class QComboBox;
class QSlider;
class QLabel;
class QCheckBox;
// Read-only sandbox: no GameEngine pointer and no write path into a playthrough.
class AnatomyLab:public QDialog{
    Q_OBJECT
public:
    explicit AnatomyLab(int age,QWidget* parent=nullptr);
private:
    void refresh();
    AnatomyView* view;
    QComboBox *organ,*disease,*stage;
    QSlider* ageSlider;
    QLabel *ageLabel,*description;
    QCheckBox *overview,*cut,*healthy;
};

