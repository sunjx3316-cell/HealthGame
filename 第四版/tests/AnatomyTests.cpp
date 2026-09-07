#include <QtTest>
#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>
#include <cmath>
#include "mainwindow.h"
#include "Database.h"
#include "AnatomyLab.h"
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>

class AnatomyTests:public QObject{
    Q_OBJECT
    QString output;
private slots:
    void initTestCase(){
        Database::getInstance().init();
        output=qEnvironmentVariable("HEALTHGAME_CAPTURE_DIR");
        if(!output.isEmpty())QDir().mkpath(output);
    }
    void atlasBindingsAndAgeMorph(){
        for(const auto& d:Database::getInstance().getAllDiseases()){
            auto raw=DiseaseVisuals::forDisease(d.id,3);
            QVERIFY2(!raw.target.isEmpty(),qPrintable(d.id));
            auto located=AnatomyGeometry::locatedVisual(d.organ,raw);
            QVERIFY2(located.amount>0,qPrintable(d.id+" missing target "+raw.target));
            QVERIFY(std::isfinite(located.focus.lengthSquared()));
        }
        float lastHeight=0;
        for(int age=0;age<=20;++age){
            auto head=AnatomyGeometry::agePosition({0,1.75f,0},age),foot=AnatomyGeometry::agePosition({0,-1.7f,0},age);
            float height=head.y()-foot.y();QVERIFY(height>=lastHeight);lastHeight=height;
        }
        auto adult=AnatomyGeometry::agePosition({0,1.75f,0},20);
        auto old=AnatomyGeometry::agePosition({0,1.75f,0},100);
        QVERIFY(old.y()<adult.y());QVERIFY(old.z()>adult.z());
        QCOMPARE(AnatomyGeometry::ageParameters(-100),AnatomyGeometry::ageParameters(0));
        QCOMPARE(AnatomyGeometry::ageParameters(500),AnatomyGeometry::ageParameters(110));
    }
    void ageAndDiseaseAtlasDialog(){
        AnatomyLab lab(20);lab.show();QVERIFY(QTest::qWaitForWindowExposed(&lab));
        auto* view=lab.findChild<AnatomyView*>();QVERIFY(view);view->setAnimationEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(view->rendererReady(),15000);
        auto* ages=lab.findChild<QSlider*>("atlasAge");QVERIFY(ages);
        QImage last;
        for(int age:{0,5,12,20,60,90}){
            ages->setValue(age);QTest::qWait(80);QCOMPARE(view->patientAge(),float(age));
            auto frame=view->grabFramebuffer();QVERIFY(!frame.isNull());QVERIFY(frame!=last);last=frame;
            if(!output.isEmpty())frame.save(output+QString("/age-%1.png").arg(age));
        }
        ages->setValue(30);
        auto* organ=lab.findChild<QComboBox*>("atlasOrgan");auto* diseases=lab.findChild<QComboBox*>("atlasDisease");
        auto* overview=lab.findChild<QCheckBox*>("atlasOverview");auto* cut=lab.findChild<QCheckBox*>("atlasCutaway");
        overview->setChecked(false);
        for(const QString& name:QStringList{"心脏","肺部","肝脏","胃部","皮肤"}){
            organ->setCurrentText(name);QTest::qWait(80);
            if(!output.isEmpty())view->grabFramebuffer().save(output+"/healthy-"+name+".png");
            cut->setChecked(true);QTest::qWait(80);
            if(!output.isEmpty())view->grabFramebuffer().save(output+"/internal-"+name+".png");
            cut->setChecked(false);
        }
        for(const QString& id:QStringList{"D02","D07","D18","D25","D29","D35","D40","D43","D45"}){
            const auto d=Database::getInstance().getDiseaseById(id);organ->setCurrentText(d.organ);
            int index=diseases->findData(id);QVERIFY(index>=0);diseases->setCurrentIndex(index);QTest::qWait(80);
            if(!output.isEmpty())view->grabFramebuffer().save(output+"/site-"+id+".png");
        }
    }
    void geometryIsFiniteAndDetailed(){
        for(const QString& name:QStringList{"心脏","肺部","肝脏","胃部","皮肤"}){
            auto m=AnatomyGeometry::organ(name);int triangles=0,inner=0;
            for(const auto& p:m.parts){
                triangles+=p.vertices.size()/3;if(p.layer==1)++inner;
                for(const auto& v:p.vertices){
                    QVERIFY(std::isfinite(v.position.x())&&std::isfinite(v.position.y())&&std::isfinite(v.position.z()));
                    QVERIFY(std::isfinite(v.normal.x())&&std::isfinite(v.normal.y())&&std::isfinite(v.normal.z()));
                }
            }
            QVERIFY2(triangles>10000,qPrintable(name));QVERIFY(inner>0);
        }
    }
    void realWindowAndDiseaseRendering(){
        MainWindow window(nullptr,false);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* view=window.anatomyView();QTRY_VERIFY_WITH_TIMEOUT(view->rendererReady(),15000);
        view->setAnimationEnabled(false);QTest::qWait(180);
        QVERIFY2(view->rendererReady(),qPrintable(view->rendererError()));
        auto healthy=view->grabFramebuffer();QVERIFY(!healthy.isNull());
        if(!output.isEmpty())window.grab().save(output+"/01-body.png");
        QPoint heart=view->organScreenPosition("心脏").toPoint();
        QTest::mouseClick(view,Qt::LeftButton,Qt::NoModifier,heart);
        QTRY_COMPARE(view->selectedOrgan(),QString("心脏"));
        QVERIFY(view->triangleCount()>10000);
        if(!output.isEmpty())window.grab().save(output+"/02-heart.png");
        auto* cut=window.findChild<QPushButton*>("cutawayButton");QVERIFY(cut);
        auto intact=view->grabFramebuffer();QTest::mouseClick(cut,Qt::LeftButton);QTest::qWait(100);
        auto sliced=view->grabFramebuffer();QVERIFY(intact!=sliced);
        if(!output.isEmpty())window.grab().save(output+"/03-heart-cutaway.png");
        for(const auto& pair:QList<QPair<QString,QString>>{{"肺部","D35"},{"肝脏","D33"},{"胃部","D34"},{"皮肤","D45"}}){
            window.selectOrgan(pair.first);
            auto states=window.gameEngine()->getOrgans();
            auto& o=states[pair.first];o.isDiseased=true;o.currentDisease=Database::getInstance().getDiseaseById(pair.second);o.diseaseStage=3;
            auto before=view->grabFramebuffer();view->setStates(states);QTest::qWait(100);auto after=view->grabFramebuffer();
            QVERIFY(before!=after);
            if(!output.isEmpty())view->grabFramebuffer().save(output+"/"+pair.second+"-disease.png");
            o.isDiseased=false;o.hasHiddenDisease=true;o.hiddenDisease=o.currentDisease;
            view->setStates(states);QTest::qWait(100);
            auto hidden=view->grabFramebuffer();QVERIFY(before==hidden);
        }
        window.selectOrgan("胃部");
        view->playTreatment("胃部","M_CANCER_MID_OP",true);QTest::qWait(500);
        if(!output.isEmpty())view->grabFramebuffer().save(output+"/04-treatment.png");
        auto front=view->grabFramebuffer();
        QTest::mousePress(view,Qt::LeftButton,Qt::NoModifier,view->rect().center());
        QTest::mouseMove(view,view->rect().center()+QPoint(120,30),50);
        QTest::mouseRelease(view,Qt::LeftButton,Qt::NoModifier,view->rect().center()+QPoint(120,30));
        QTest::qWait(100);QVERIFY(front!=view->grabFramebuffer());
        window.resize(1140,760);QTest::qWait(100);
        if(!output.isEmpty())window.grab().save(output+"/05-small-window.png");
    }
};
int main(int argc,char**argv){
    QSurfaceFormat f;f.setVersion(3,3);f.setProfile(QSurfaceFormat::CoreProfile);f.setDepthBufferSize(24);f.setSamples(4);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc,argv);
    AnatomyTests test;return QTest::qExec(&test,argc,argv);
}
#include "AnatomyTests.moc"
