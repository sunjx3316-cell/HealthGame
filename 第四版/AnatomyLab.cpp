#include "AnatomyLab.h"
#include "AnatomyView.h"
#include "Database.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QPropertyAnimation>
AnatomyLab::AnatomyLab(int age,QWidget* parent):QDialog(parent){
    setWindowTitle("解剖图谱 · 年龄与病灶演示");resize(1260,860);setMinimumSize(950,660);
    auto* root=new QVBoxLayout(this);
    auto* heading=new QLabel("解剖图谱  /  ANATOMY ATLAS");heading->setStyleSheet("font-size:20px;font-weight:600;color:#e5ede4;padding:10px;");
    root->addWidget(heading);
    auto* note=new QLabel("拖动年龄观察生长，点选疾病观察部位。人物来自 MakeHuman 年龄网格，器官来自 Z-Anatomy / BodyParts3D。年龄和病灶为游戏示意。");
    note->setWordWrap(true);note->setStyleSheet("color:#8dafaa;padding:4px 10px;");root->addWidget(note);
    auto* controls=new QHBoxLayout;
    organ=new QComboBox;organ->setObjectName("atlasOrgan");organ->addItems({"心脏","肺部","肝脏","胃部","皮肤"});
    disease=new QComboBox;disease->setObjectName("atlasDisease");disease->setMinimumWidth(220);
    stage=new QComboBox;stage->addItems({"早期 / 轻度","中期 / 进展","晚期 / 重度"});stage->setObjectName("atlasStage");
    overview=new QCheckBox("全身定位");overview->setChecked(true);overview->setObjectName("atlasOverview");
    cut=new QCheckBox("内部剖视");cut->setObjectName("atlasCutaway");
    healthy=new QCheckBox("健康对照");healthy->setObjectName("atlasHealthy");
    controls->addWidget(organ);controls->addWidget(disease,1);controls->addWidget(stage);controls->addWidget(overview);controls->addWidget(cut);controls->addWidget(healthy);
    root->addLayout(controls);
    view=new AnatomyView;root->addWidget(view,1);
    auto* ages=new QHBoxLayout;ages->addWidget(new QLabel("年龄形态"));
    ageSlider=new QSlider(Qt::Horizontal);ageSlider->setObjectName("atlasAge");ageSlider->setRange(0,1100);ageSlider->setValue(age*10);
    ageSlider->setTickPosition(QSlider::TicksBelow);ageSlider->setTickInterval(100);
    auto* transition=new QPropertyAnimation(ageSlider,"value",this);
    transition->setEasingCurve(QEasingCurve::InOutCubic);
    connect(ageSlider,&QSlider::sliderPressed,transition,&QPropertyAnimation::stop);
    ageLabel=new QLabel;ageLabel->setMinimumWidth(130);
    ages->addWidget(ageSlider,1);ages->addWidget(ageLabel);
    for(int a:{0,5,12,20,60,90}){
        auto* preset=new QPushButton(QString::number(a)+" 岁");preset->setFixedWidth(65);
        connect(preset,&QPushButton::clicked,this,[this,a,transition]{
            transition->stop();transition->setStartValue(ageSlider->value());transition->setEndValue(a*10);
            transition->setDuration(600+qAbs(ageSlider->value()-a*10));transition->start();
        });ages->addWidget(preset);
    }root->addLayout(ages);
    description=new QLabel;description->setWordWrap(true);description->setMinimumHeight(58);
    description->setStyleSheet("color:#b5cbc4;padding:8px;");root->addWidget(description);
    auto* bottom=new QHBoxLayout;
    auto* window=new QCheckBox("胸腹透视");bottom->addWidget(window);
    connect(window,&QCheckBox::toggled,view,&AnatomyView::setDissectionWindow);
    connect(overview,&QCheckBox::toggled,window,&QCheckBox::setEnabled);
    auto* rotate=new QCheckBox("自动旋转");connect(rotate,&QCheckBox::toggled,view,&AnatomyView::setAutoRotate);
    auto* reset=new QPushButton("复位视角");connect(reset,&QPushButton::clicked,view,&AnatomyView::resetCamera);
    auto* effect=new QPushButton("治疗效果演示");
    connect(effect,&QPushButton::clicked,this,[this]{
        const auto d=Database::getInstance().getDiseaseById(disease->currentData().toString());
        auto ms=Database::getInstance().getMedicinesForDisease(d.id);
        view->playTreatment(organ->currentText(),ms.isEmpty()?"DEMO":ms.first().id,true);
    });
    auto* close=new QPushButton("返回游戏");connect(close,&QPushButton::clicked,this,&QDialog::accept);
    bottom->addWidget(rotate);bottom->addWidget(reset);bottom->addWidget(effect);bottom->addStretch();bottom->addWidget(close);root->addLayout(bottom);
    auto fill=[this]{
        QSignalBlocker blocker(disease);disease->clear();disease->addItem("健康解剖","");
        for(const auto& d:Database::getInstance().getAllDiseases())if(d.organ==organ->currentText())disease->addItem(d.id+" · "+d.name,d.id);
    };
    fill();
    connect(organ,&QComboBox::currentIndexChanged,this,[this,fill]{fill();refresh();});
    connect(disease,&QComboBox::currentIndexChanged,this,[this]{
        const auto visual=DiseaseVisuals::forDisease(disease->currentData().toString());
        cut->setChecked(AnatomyGeometry::prefersSection(organ->currentText(),visual));refresh();
    });
    connect(stage,&QComboBox::currentIndexChanged,this,&AnatomyLab::refresh);
    connect(ageSlider,&QSlider::valueChanged,this,&AnatomyLab::refresh);
    for(auto* check:{overview,cut,healthy})connect(check,&QCheckBox::toggled,this,&AnatomyLab::refresh);
    connect(view,&AnatomyView::organSelected,this,[this](const QString& s){organ->setCurrentText(s);overview->setChecked(false);});
    refresh();
}
void AnatomyLab::refresh(){
    const float age=ageSlider->value()/10.f;view->setAge(age);
    ageLabel->setText(QString("%1 岁 · %2").arg(age).arg(age<3?"婴幼儿":age<12?"儿童":age<20?"青少年":age<60?"成人":"老年"));
    QMap<QString,OrganState> states;for(const QString& s:QStringList{"心脏","肺部","肝脏","胃部","皮肤"})states[s]=OrganState(s);
    const auto d=Database::getInstance().getDiseaseById(disease->currentData().toString());
    if(!d.id.isEmpty()&&!healthy->isChecked()){
        auto& o=states[organ->currentText()];o.isDiseased=true;o.currentDisease=d;o.diseaseStage=stage->currentIndex()+1;
    }
    view->setStates(states);view->setOrgan(overview->isChecked()?"":organ->currentText());
    view->setCutaway(cut->isChecked());
    stage->setEnabled(!d.id.isEmpty());healthy->setEnabled(!d.id.isEmpty());
    const auto v=DiseaseVisuals::forDisease(d.id);
    description->setText(d.id.isEmpty()?"拖动旋转、滚轮缩放；点击全身模型进入器官。年龄滑块可对照人体比例、体态及器官形态。":
        d.name+" · "+v.description+"\n结构绑定："+v.target+"。为演示代表性部位，不代表此病总在固定位置发生。"+(age<d.minAge||age>d.maxAge?"  当前年龄不在游戏该病的自然触发范围内。":""));
}
