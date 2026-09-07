#include <QtTest>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>
#include <QTemporaryDir>
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
