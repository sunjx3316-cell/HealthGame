#include <QtTest>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalBlocker>
#include <QTemporaryDir>
#include <random>
#include <sstream>
#include "mainwindow.h"
#include "StartupDialog.h"

// Exercise the real window flow, with every file confined to a temporary store.
class SaveFlowTests : public QObject {
    Q_OBJECT
    static QByteArray contents(const QString& path){
        QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return file.readAll();
    }
    static bool checkpoint(const QString& path,const QString& name){
        GameEngine engine;engine.setPlayerAttributes(name,7,7,9,7);return engine.saveGame(path);
    }
    static void enableTestSession(MainWindow& window){
        window.timer.stop();window.interactive=true;
    }
    static void captureDialog(QDialog* dialog,const QString& name){
        const auto output=qEnvironmentVariable("HEALTHGAME_CAPTURE_DIR");
        if(output.isEmpty()||!dialog)return;
        QDir().mkpath(output);dialog->grab().save(QDir(output).filePath(name+".png"));
    }
private slots:
    void recommendedChoiceSurvivesSimulationRefresh(){
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const QString path=dir.filePath("case.json");QVERIFY(checkpoint(path,"治疗选择"));
        const auto healthy=contents(path);
        auto record=QJsonDocument::fromJson(healthy).object();
        std::ostringstream rng;rng<<std::mt19937(12);record["rng"]=QString::fromStdString(rng.str());
        auto organs=record["organs"].toArray();
        for(int i=0;i<organs.size();++i){
            auto o=organs[i].toObject();if(o["name"]!="肺部")continue;
            o["disease"]="D10";o["autoHeal"]=10;organs[i]=o;
        }
        record["organs"]=organs;record["history"]=QJsonObject{{"D10",QJsonObject{{"episodes",1},{"onset",0},{"recovery",-1000}}}};
        QFile fixture(path);QVERIFY(fixture.open(QIODevice::WriteOnly));fixture.write(QJsonDocument(record).toJson());fixture.close();
        MainWindow window(nullptr,false,dir.path());window.timer.stop();
        window.setAttribute(Qt::WA_DontShowOnScreen);window.show();
        QCoreApplication::processEvents();
        QVERIFY2(window.engine->loadGame(path),qPrintable(window.engine->lastError()));
        window.selectOrgan("肺部");auto* list=window.medicines;QVERIFY(list->count()>1);
        list->setCurrentRow(1);window.dosage->setValue(2);
        QPersistentModelIndex choice(list->currentIndex());auto id=choice.data(Qt::UserRole);
        QSignalSpy resets(list->model(),&QAbstractItemModel::modelReset);
        QSignalSpy inserted(list->model(),&QAbstractItemModel::rowsInserted);
        QSignalSpy removed(list->model(),&QAbstractItemModel::rowsRemoved);
        auto* popup=window.category->view();
        popup->window()->setAttribute(Qt::WA_DontShowOnScreen);
        window.category->showPopup();
        QVERIFY(popup->isVisible());
        const QModelIndex highlighted=popup->model()->index(2,0);
        popup->setCurrentIndex(highlighted);
        window.engine->setPaused(false);
        for(int i=0;i<3;++i){
            window.engine->tick();QCoreApplication::processEvents();
            QVERIFY(popup->isVisible());QCOMPARE(popup->currentIndex(),highlighted);
        }
        window.category->hidePopup();
        window.engine->tick();QCoreApplication::processEvents();QVERIFY(!popup->isVisible());
        QCOMPARE(resets.count(),0);QCOMPARE(inserted.count(),0);QCOMPARE(removed.count(),0);QVERIFY(choice.isValid());
        QCOMPARE(list->currentIndex(),QModelIndex(choice));QCOMPARE(list->currentItem()->data(Qt::UserRole),id);
        QCOMPARE(window.dosage->value(),2);
        window.search->setText("奥司他韦");QCOMPARE(list->count(),1);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),QString("M10"));
        const QPersistentModelIndex filtered(list->currentIndex());
        window.search->clear();QVERIFY(filtered.isValid());QCOMPARE(list->currentIndex(),QModelIndex(filtered));
        window.search->setText("奥司他韦");
        QVERIFY(fixture.open(QIODevice::WriteOnly));fixture.write(healthy);fixture.close();
        QVERIFY(window.engine->loadGame(path));QCOMPARE(list->count(),0);
        QVERIFY(!window.treatButton->isEnabled());
    }
    void startupCancelDoesNotOverwriteExistingLife(){
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto path=SaveRepository::Store(dir.path()).pathFor("autosave");
        QVERIFY(checkpoint(path,"之前的人生"));const auto original=contents(path);
        MainWindow window(nullptr,false,dir.path());enableTestSession(window);
        bool sawManager=false;
        QTimer::singleShot(0,&window,[&]{
            if(auto* dialog=window.findChild<QDialog*>("saveManager")){sawManager=true;dialog->reject();}
        });
        window.showSaveManager(true);
        QVERIFY(sawManager);QVERIFY(!window.hasCurrentSession);QVERIFY(window.engine->isPaused());
        window.setPaused(false);QVERIFY(window.engine->isPaused());
        QVERIFY(window.writeAutosave());QCOMPARE(contents(path),original);
        QCloseEvent close;window.closeEvent(&close);QVERIFY(close.isAccepted());
        QCOMPARE(contents(path),original);
    }
    void restoringManualLifeStaysPausedUntilRealProgress(){
        QTemporaryDir dir;QVERIFY(dir.isValid());SaveRepository::Store store(dir.path());
        const auto autoPath=store.pathFor("autosave"),slot=SaveRepository::createManualSlot();
        QVERIFY(checkpoint(autoPath,"自动档原人生"));const auto original=contents(autoPath);
        QVERIFY(checkpoint(store.pathFor(slot),"手动档人生"));
        MainWindow window(nullptr,false,dir.path());enableTestSession(window);
        bool clicked=false;
        QTimer::singleShot(0,&window,[&]{
            auto* dialog=window.findChild<QDialog*>("saveManager");if(!dialog)return;
            auto* list=dialog->findChild<QListWidget*>("saveSlotList");
            auto* resume=dialog->findChild<QPushButton*>("resumeSave");
            if(!list||!resume){dialog->reject();return;}
            for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString()==slot){list->setCurrentRow(row);captureDialog(dialog,"save-manager");clicked=true;resume->click();return;}
            dialog->reject();
        });
        window.showSaveManager(true);
        QVERIFY(clicked);QVERIFY(window.hasCurrentSession);QVERIFY(window.engine->isPaused());
        QCOMPARE(window.engine->getPlayerName(),QString("手动档人生"));
        QVERIFY(!window.saveDirty);QVERIFY(!window.autosaveArmed);
        QVERIFY(window.writeAutosave());QCOMPARE(contents(autoPath),original);
        window.setPaused(false);window.engine->tick();window.setPaused(true);
        QVERIFY(window.saveDirty);QVERIFY(window.autosaveArmed);
        // stateChanged only marks dirty; saving must occur outside the inTick signal.
        QCOMPARE(contents(autoPath),original);QVERIFY(window.writeAutosave());
        QVERIFY(contents(autoPath)!=original);QVERIFY(!window.saveDirty);
        GameEngine restored;QVERIFY2(restored.loadGame(autoPath),qPrintable(restored.lastError()));
        QCOMPARE(restored.getPlayerName(),QString("手动档人生"));
    }
    void manualSavesAreIndependentAndKeepAutomaticCheckpoint(){
        QTemporaryDir dir;QVERIFY(dir.isValid());SaveRepository::Store store(dir.path());
        const auto autoPath=store.pathFor("autosave");QVERIFY(checkpoint(autoPath,"自动档"));
        const auto original=contents(autoPath);
        MainWindow window(nullptr,false,dir.path());enableTestSession(window);
        window.engine->setPlayerAttributes("当前人生",7,7,9,7);window.hasCurrentSession=true;
        QVERIFY(window.writeManualSave());QVERIFY(window.writeManualSave());
        QCOMPARE(store.entries().size(),3);QCOMPARE(contents(autoPath),original);
        for(const auto& entry:store.entries())if(entry.slot!="autosave"){
            QVERIFY(entry.valid);QCOMPARE(entry.name,QString("当前人生"));
        }
    }
    void newLifeArchivesOldAutomaticCheckpoint(){
        QTemporaryDir dir;QVERIFY(dir.isValid());SaveRepository::Store store(dir.path());
        const auto autoPath=store.pathFor("autosave");QVERIFY(checkpoint(autoPath,"旧人生"));
        const auto original=contents(autoPath);
        MainWindow window(nullptr,false,dir.path());enableTestSession(window);
        bool configured=false;
        QTimer::singleShot(0,&window,[&]{
            if(auto* dialog=window.findChild<StartupDialog*>()){captureDialog(dialog,"talent-allocation");configured=true;dialog->accept();}
        });
        window.newGame();QVERIFY(configured);QVERIFY(window.hasCurrentSession);
        QCOMPARE(store.entries().size(),2);QCOMPARE(contents(autoPath),original);
        QVERIFY(!window.autosaveArmed);QVERIFY(window.writeAutosave());QCOMPARE(contents(autoPath),original);
        window.engine->tick();QVERIFY(window.saveDirty);QVERIFY(window.writeAutosave());
        QVERIFY(contents(autoPath)!=original);
        for(const auto& entry:store.entries())if(entry.slot!="autosave")QCOMPARE(contents(entry.path),original);
    }
    void closeFlushesOnlyChangedOwnedLife(){
        QTemporaryDir dir;QVERIFY(dir.isValid());SaveRepository::Store store(dir.path());
        MainWindow window(nullptr,false,dir.path());enableTestSession(window);
        window.engine->setPlayerAttributes("关闭保存",7,7,9,7);window.hasCurrentSession=true;
        window.setPaused(false);window.engine->tick();window.setPaused(true);
        QVERIFY(window.saveDirty);QVERIFY(!QFileInfo::exists(store.pathFor("autosave")));
        QCloseEvent close;window.closeEvent(&close);QVERIFY(close.isAccepted());
        QVERIFY(!window.saveDirty);GameEngine restored;
        QVERIFY2(restored.loadGame(store.pathFor("autosave")),qPrintable(restored.lastError()));
        QCOMPARE(restored.getPlayerName(),QString("关闭保存"));
    }
    void pendingMilestoneDoesNotReplaceCheckpoint(){
        QTemporaryDir dir;QVERIFY(dir.isValid());SaveRepository::Store store(dir.path());
        const auto autoPath=store.pathFor("autosave");QVERIFY(checkpoint(autoPath,"事件前"));
        const auto original=contents(autoPath);
        auto object=QJsonDocument::fromJson(original).object();object["age"]=79;object["tick"]=9;
        const auto nearMilestone=store.pathFor(SaveRepository::createManualSlot());
        QFile file(nearMilestone);QVERIFY(file.open(QIODevice::WriteOnly));
        const auto fixture=QJsonDocument(object).toJson();QCOMPARE(file.write(fixture),fixture.size());file.close();
        MainWindow window(nullptr,false,dir.path());enableTestSession(window);
        QVERIFY2(window.engine->loadGame(nearMilestone),qPrintable(window.engine->lastError()));
        window.hasCurrentSession=true;window.setPaused(false);
        {
            // Suppress modal milestone handling while exercising its save guard.
            QSignalBlocker blocker(window.engine);window.engine->tick();
        }
        QVERIFY(window.engine->hasPendingMilestone());window.saveDirty=true;window.autosaveArmed=true;
        QVERIFY(!window.writeAutosave());QCOMPARE(contents(autoPath),original);QVERIFY(window.saveDirty);
    }
};
QTEST_MAIN(SaveFlowTests)
#include "SaveFlowTests.moc"
