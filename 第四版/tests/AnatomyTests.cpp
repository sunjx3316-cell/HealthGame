#include <QtTest>
#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>
#include <cmath>
#include "mainwindow.h"
#include "Database.h"
#include "AnatomyLab.h"
#include "LifeShape.h"
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QMouseEvent>

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
        AnatomyLab lab(20);lab.setAttribute(Qt::WA_DontShowOnScreen);lab.show();
        auto* view=lab.findChild<AnatomyView*>();QVERIFY(view);view->setAnimationEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(view->rendererReady(),15000);
        auto* ages=lab.findChild<QSlider*>("atlasAge");QVERIFY(ages);
        QImage last;
        for(int age:{0,5,12,20,60,90}){
            ages->setValue(age*10);QTest::qWait(80);QCOMPARE(view->patientAge(),float(age));
            auto frame=view->grabFramebuffer();QVERIFY(!frame.isNull());QVERIFY(frame!=last);last=frame;
            if(!output.isEmpty())frame.save(output+QString("/age-%1.png").arg(age));
        }
        ages->setValue(300);
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
    void continuousGrowthAndLesionGeometry(){
        auto ratio=[](float age){
            auto s=LifeShape::at(age);
            return s.map({.35f,1.04f,.03f}).x()/s.map({.14f,1.65f,0}).x();
        };
        QVERIFY(ratio(2)<ratio(12));QVERIFY(ratio(12)<ratio(28));QVERIFY(ratio(86)<ratio(28));
        for(float knot:{2,6,12,18,28,48,68,86}){
            QVector3D v(.12f,1.40f,.12f);
            auto at=LifeShape::at(knot).map(v);
            auto left=at-LifeShape::at(knot-.02f).map(v);
            auto right=LifeShape::at(knot+.02f).map(v)-at;
            QVERIFY((left-right).length()<.00012f);
        }
        auto lesion=[](int stage){
            auto v=AnatomyGeometry::locatedVisual("肺部",DiseaseVisuals::forDisease("D35",stage));
            return AnatomyGeometry::lesions("肺部",v);
        };
        auto early=lesion(1),late=lesion(3);
        QVERIFY(!early.parts.isEmpty());QVERIFY(late.parts.size()>early.parts.size());
        QVERIFY(AnatomyGeometry::lesions("肺部",{}).parts.isEmpty());
    }
    void portraitGallery(){
        if(output.isEmpty())QSKIP("Capture directory not requested");
        AnatomyView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(960,800);
        view.setAnimationEnabled(false);view.show();
        QVERIFY2(view.rendererReady(),qPrintable(view.rendererError()));
        for(int age:{0,2,6,12,18,28,48,68,86,110}){
            view.setAge(age);
            QVERIFY(view.grabFramebuffer().save(output+QString("/portrait-%1.png").arg(age)));
        }
        auto rotate=[&](QPointF end){
            QPoint start=view.rect().center();
            QTest::mousePress(&view,Qt::LeftButton,Qt::NoModifier,start);
            QMouseEvent move(QEvent::MouseMove,end,view.mapToGlobal(end.toPoint()),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(&view,&move);
            QTest::mouseRelease(&view,Qt::LeftButton,Qt::NoModifier,end.toPoint());
        };
        rotate(view.rect().center()+QPoint(140,0));
        for(int age:{0,6,12,86}){
            view.setAge(age);QVERIFY(view.grabFramebuffer().save(output+QString("/profile-%1.png").arg(age)));
        }
        view.resetCamera();
        view.setAge(28);view.setDissectionWindow(true);
        QVERIFY(view.grabFramebuffer().save(output+"/portrait-window.png"));
        for(const auto& d:Database::getInstance().getAllDiseases()){
            QMap<QString,OrganState> states;
            for(const auto& name:QStringList{"心脏","肺部","肝脏","胃部","皮肤"})states[name]=OrganState(name);
            auto& o=states[d.organ];o.isDiseased=true;o.currentDisease=d;o.diseaseStage=3;
            view.setOrgan(d.organ);view.setStates(states);
            view.setCutaway(AnatomyGeometry::prefersSection(d.organ,DiseaseVisuals::forState(o)));
            QVERIFY(view.grabFramebuffer().save(output+"/"+d.id+".png"));
        }
    }
    void growingPortraitFrames(){
        if(!qEnvironmentVariableIsSet("HEALTHGAME_CAPTURE_GROWTH")||output.isEmpty())QSKIP("Motion capture not requested");
        AnatomyView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(660,600);
        view.setAnimationEnabled(false);view.show();QVERIFY(view.rendererReady());
        for(int i=0;i<=72;++i){
            view.setAge(i*.25f);auto frame=view.grabFramebuffer();
            const int side=int(frame.height()*.43);
            frame=frame.copy((frame.width()-side)/2,int(frame.height()*.025),side,side);
            QVERIFY(frame.scaled(360,360,Qt::IgnoreAspectRatio,Qt::SmoothTransformation)
                .save(output+QString("/growth-%1.png").arg(i,3,10,QChar('0'))));
        }
    }
    void thoracicWindowPreservesExterior(){
        AnatomyView view;view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(960,800);view.setAnimationEnabled(false);view.show();
        QVERIFY2(view.rendererReady(),qPrintable(view.rendererError()));
        for(int age:{28,2,86}){
            view.setAge(age);view.setDissectionWindow(false);
            auto closed=view.grabFramebuffer().convertToFormat(QImage::Format_RGB32);
            view.setDissectionWindow(true);
            auto open=view.grabFramebuffer().convertToFormat(QImage::Format_RGB32);
            QVERIFY(!open.isNull());QCOMPARE(open.size(),closed.size());
            if(!output.isEmpty())QVERIFY(open.save(output+QString("/window-%1.png").arg(age)));
            auto changed=[&](double x,double y,double w,double h){
                int count=0;
                for(int j=int(y*open.height());j<int((y+h)*open.height());++j)
                    for(int i=int(x*open.width());i<int((x+w)*open.width());++i)
                        if(open.pixel(i,j)!=closed.pixel(i,j))++count;
                return count;
            };
            const int legs=changed(.29,.74,.42,.16);
            QVERIFY2(legs<10,qPrintable(QString("%1 years: %2 exterior leg pixels changed").arg(age).arg(legs)));
            QVERIFY(changed(.45,.09,.10,.08)<10);
            QVERIFY(changed(.30,.45,.10,.14)<10);
            QVERIFY(changed(.60,.45,.10,.14)<10);
            QVERIFY(changed(.42,.34,.16,.21)>500);
        }
        // A front opening must be occluded when viewing the back.
        QPoint from=view.rect().center(),to=from+QPoint(360,0);
        QTest::mousePress(&view,Qt::LeftButton,Qt::NoModifier,from);
        QMouseEvent move(QEvent::MouseMove,to,view.mapToGlobal(to),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&view,&move);QTest::mouseRelease(&view,Qt::LeftButton,Qt::NoModifier,to);
        view.setAge(28);auto back=view.grabFramebuffer();view.setDissectionWindow(false);
        auto intact=view.grabFramebuffer();QRect body(back.width()/3,back.height()/5,back.width()/3,back.height()/2);
        if(!output.isEmpty()){
            QVERIFY(back.save(output+"/back-open.png"));
            QVERIFY(intact.save(output+"/back-closed.png"));
        }
        QCOMPARE(back.copy(body),intact.copy(body));
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
        MainWindow window(nullptr,false);window.setAttribute(Qt::WA_DontShowOnScreen);window.show();
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
    QSurfaceFormat f;f.setVersion(3,3);f.setProfile(QSurfaceFormat::CoreProfile);f.setDepthBufferSize(24);f.setStencilBufferSize(8);f.setSamples(4);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc,argv);
    AnatomyTests test;return QTest::qExec(&test,argc,argv);
}
#include "AnatomyTests.moc"
