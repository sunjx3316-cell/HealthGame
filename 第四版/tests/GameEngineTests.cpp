#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "GameEngine.h"
#include "Database.h"
#include "DiseaseVisual.h"
#include "DiseaseRules.h"

class GameEngineTests:public QObject{
    Q_OBJECT
    void infect(GameEngine& e,const QString& id,int stage=0){
        auto d=Database::getInstance().getDiseaseById(id);
        auto& o=e.organs[d.organ];o=OrganState(d.organ);o.isDiseased=true;o.currentDisease=d;o.diseaseStage=stage;
        if(stage)o.currentDisease.treatmentStages=stage==1?d.earlyTreatment:stage==2?d.midTreatment:d.lateTreatment;
        auto& h=e.diseaseHistory[id];++h.episodes;h.lastOnsetTick=e.absoluteTick();
        if(DiseaseRules::major(d))e.lastMajorOnsetTick=e.absoluteTick();
    }
private slots:
    void talentsAreBoundedAndViable(){
        GameEngine e;e.setPlayerAttributes("once",15,5,5,5);int m=e.money;
        e.setPlayerAttributes("reroll",0,0,15,15);QCOMPARE(e.money,m);QCOMPARE(e.research,5);
        QCOMPARE(e.money,930000);e.age=18;
        for(int i=0;i<5;++i){e.money=10000000;e.age=18+i*3;QVERIFY(e.buyResearchPoint());QVERIFY(!e.buyResearchPoint());}
        e.age=40;QVERIFY(!e.buyResearchPoint());
        for(int res:{0,5,15}){GameEngine p;p.research=res;p.money=2000000;p.age=79;p.currentTick=9;p.tick();
            QVERIFY(p.hasPendingMilestone());QVERIFY(!p.isFinished());int before=p.money;
            QVERIFY(p.payForLifeExtension());QCOMPARE(before-p.money,p.getLifeExtensionCost());}
    }
    void majorSpacingAndCommonRecurrence(){
        GameEngine e;e.age=50;e.startDisease(Database::getInstance().getDiseaseById("D17"));
        e.cure(e.organs["肺部"],0);e.normalizeOrgan(e.organs["肺部"]);
        auto major=Database::getInstance().getDiseaseById("D34"),cold=Database::getInstance().getDiseaseById("D10");
        QVERIFY(!e.diseaseEligible(major));QVERIFY(e.diseaseEligible(cold));
        e.age=52;e.currentTick=9;QVERIFY(!e.diseaseEligible(major));e.age=53;e.currentTick=0;QVERIFY(e.diseaseEligible(major));
        e.startDisease(cold);e.cure(e.organs["肺部"],0);e.normalizeOrgan(e.organs["肺部"]);
        QVERIFY(!e.diseaseEligible(cold));e.currentTick=6;QVERIFY(e.diseaseEligible(cold));
        e.age=105;QVERIFY(e.diseaseEligible(major));
    }
    void structureRemovalCrossDiseaseAndNoBroadBypass(){
        GameEngine e;e.age=18;e.money=1000000;infect(e,"D37");
        QVERIFY(e.applyMedicine("肝脏","M37",1));QVERIFY(e.organs["肝脏"].removedDiseases.contains("D37"));
        QVERIFY(!e.diseaseEligible(Database::getInstance().getDiseaseById("D18")));
        QVERIFY(!e.diseaseEligible(Database::getInstance().getDiseaseById("D37")));
        QVERIFY(e.diseaseEligible(Database::getInstance().getDiseaseById("D23")));
        infect(e,"D29");QVERIFY(!e.medicineMatches(e.organs["胃部"],Database::getInstance().getMedicineById("M_BROAD_SUPER")));
        QVERIFY(e.applyMedicine("胃部","M29",1));e.age=60;
        QVERIFY(!e.diseaseEligible(Database::getInstance().getDiseaseById("D29")));
        QVERIFY(e.diseaseEligible(Database::getInstance().getDiseaseById("D22")));
        QTemporaryDir dir;auto path=dir.filePath("s.json");QVERIFY(e.saveGame(path));GameEngine copy;
        QVERIFY2(copy.loadGame(path),qPrintable(copy.lastError()));QVERIFY(!copy.diseaseEligible(Database::getInstance().getDiseaseById("D29")));
    }
    void smallIllnessIsNotMajorShieldAndAcuteIsNotLatent(){
        GameEngine e;e.age=50;e.money=1000000;
        e.startDisease(Database::getInstance().getDiseaseById("D22"));
        auto cancer=Database::getInstance().getDiseaseById("D34");QVERIFY(e.diseaseEligible(cancer));e.startDisease(cancer);
        QCOMPARE(e.organs["胃部"].currentDisease.id,QString("D22"));QCOMPARE(e.organs["胃部"].coexisting.size(),1);
        QVERIFY(e.performCheckup());QCOMPARE(e.organs["胃部"].currentDisease.id,QString("D34"));
        QCOMPARE(e.organs["胃部"].coexisting[0].currentDisease.id,QString("D22"));
        QTemporaryDir dir;auto p=dir.filePath("s.json");QVERIFY(e.saveGame(p));GameEngine copy;
        QVERIFY2(copy.loadGame(p),qPrintable(copy.lastError()));QCOMPARE(copy.organs["胃部"].coexisting.size(),1);
        e.cure(e.organs["胃部"],0);e.normalizeOrgan(e.organs["胃部"]);QCOMPARE(e.organs["胃部"].currentDisease.id,QString("D22"));
        GameEngine acute;acute.age=70;acute.startDisease(Database::getInstance().getDiseaseById("D42"));
        QVERIFY(acute.organs["心脏"].isDiseased);QVERIFY(!acute.organs["心脏"].hasHiddenDisease);
    }
    void noHealthFarmingAndImmuneSpam(){
        GameEngine e;e.age=50;e.money=100000;e.health=60;infect(e,"D22");
        QVERIFY(e.applyMedicine("胃部","M22",1));QCOMPARE(e.health,60.0);
        infect(e,"D22");e.updateDisease(e.organs["胃部"]);double hp=e.health;
        QVERIFY(e.applyMedicine("胃部","M22",1));QVERIFY(e.health>=hp);QVERIFY(e.health<60);
        e.immunity=60;QVERIFY(e.applyMedicine("","M_SUPP_07",1));QCOMPARE(e.immunity,78.0);
        int cash=e.money;QVERIFY(!e.applyMedicine("","M_SUPP_01",1));QCOMPARE(e.money,cash);
        for(int i=0;i<10;++i)e.updateImmunityCurve();QVERIFY(e.immunity<78);
    }
    void supportTherapyIsNotTransplantAndCannotStack(){
        GameEngine e;e.age=70;e.money=1000000;infect(e,"D52");
        QVERIFY(e.applyMedicine("肝脏","M52",1));QVERIFY(e.organs["肝脏"].isDiseased);
        int cash=e.money,effects=e.activeSideEffects.size(),until=e.organs["肝脏"].controlledUntilTick;
        e.currentTick=1;QVERIFY(!e.applyMedicine("肝脏","M52",1));QCOMPARE(e.money,cash);
        QCOMPARE(e.activeSideEffects.size(),effects);QCOMPARE(e.organs["肝脏"].controlledUntilTick,until);
        QVERIFY(e.applyMedicine("肝脏","M53",1));QVERIFY(!e.organs["肝脏"].isDiseased);QVERIFY(e.organs["肝脏"].isTransplanted);
        e.age=71;e.money=1000000;e.updateYear();QCOMPARE(e.money,1000000+e.getAnnualIncome()-4000);
    }
    void legacyPartialCancerAndResearchMigration(){
        QTemporaryDir dir;GameEngine e;e.age=50;e.money=900000;infect(e,"D34",2);e.organs["胃部"].currentTreatmentStage=1;
        const auto path=dir.filePath("old.json");QVERIFY(e.saveGame(path));QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));
        auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();root["version"]=2;root["wealth"]=15;root["luck"]=15;root["research"]=12;
        auto arr=root["organs"].toArray();for(int i=0;i<arr.size();++i){auto o=arr[i].toObject();if(o["name"]=="胃部"){o["removed"]=QJsonArray{"D34"};arr[i]=o;}}root["organs"]=arr;
        QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();
        GameEngine migrated;QVERIFY2(migrated.loadGame(path),qPrintable(migrated.lastError()));QCOMPARE(migrated.research,12);
        QVERIFY(migrated.organs["胃部"].removedDiseases.isEmpty());QVERIFY(!migrated.canBuyResearchPoint());
        auto next=dir.filePath("new.json");QVERIFY(migrated.saveGame(next));GameEngine again;QVERIFY2(again.loadGame(next),qPrintable(again.lastError()));
    }
    void impossibleAnatomyAndHistorySaveRejected(){
        QTemporaryDir dir;GameEngine e;e.age=50;infect(e,"D29");const auto path=dir.filePath("s.json");QVERIFY(e.saveGame(path));
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();
        auto arr=root["organs"].toArray();for(int i=0;i<arr.size();++i){auto o=arr[i].toObject();if(o["name"]=="胃部"){o["removed"]=QJsonArray{"D29"};arr[i]=o;}}root["organs"]=arr;
        QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();int cash=e.money;
        QVERIFY(!e.loadGame(path));QCOMPARE(e.money,cash);QVERIFY(e.organs["胃部"].isDiseased);
    }
    void congenitalCannotRepeatAndCancerCanReturn(){
        GameEngine e;auto birth=Database::getInstance().getDiseaseById("D01");e.startDisease(birth);
        e.cure(e.organs["心脏"],0);e.normalizeOrgan(e.organs["心脏"]);e.age=4;QVERIFY(!e.diseaseEligible(birth));
        e.age=50;auto cancer=Database::getInstance().getDiseaseById("D34");e.startDisease(cancer);e.money=1000000;
        QVERIFY(e.performCheckup());e.cure(e.organs["胃部"],0);e.normalizeOrgan(e.organs["胃部"]);
        e.age=54;e.currentTick=9;QVERIFY(!e.diseaseEligible(cancer));e.age=55;e.currentTick=0;QVERIFY(e.diseaseEligible(cancer));
        QVERIFY(e.organs["胃部"].removedDiseases.isEmpty());
    }
    void legacyRemovedGallbladderRepairPreservesProgress(){
        QTemporaryDir dir;GameEngine e;e.age=18;e.money=888888;infect(e,"D18");const auto p=dir.filePath("s.json");QVERIFY(e.saveGame(p));
        QFile f(p);QVERIFY(f.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();root["version"]=2;
        auto arr=root["organs"].toArray();for(int i=0;i<arr.size();++i){auto o=arr[i].toObject();if(o["name"]=="肝脏"){o["removed"]=QJsonArray{"D37"};arr[i]=o;}}root["organs"]=arr;
        QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();
        GameEngine migrated;QVERIFY2(migrated.loadGame(p),qPrintable(migrated.lastError()));QCOMPARE(migrated.money,888888);
        QCOMPARE(migrated.age,18);QVERIFY(!migrated.organs["肝脏"].isDiseased);QVERIFY(migrated.organs["肝脏"].removedDiseases.contains("D37"));
        QVERIFY(migrated.saveGame(p));GameEngine again;QVERIFY2(again.loadGame(p),qPrintable(again.lastError()));
    }
    void generatedStatesRemainLoadableAndReplayIdentically(){
        QTemporaryDir dir;const auto p=dir.filePath("s.json");GameEngine a;a.rng.seed(914);a.setPlayerAttributes("replay",7,7,9,7);a.money=9000000;
        for(int i=0;i<750&&!a.finished;++i){
            for(const auto& o:a.getOrgans())if(o.isDiseased){
                for(const auto& m:Database::getInstance().getAllMedicines())if(m.id!="M52"&&a.medicineMatches(o,m)){a.applyMedicine(o.name,m.id,1);break;}
            }
            if(a.age>=30&&a.age-a.lastCheckupAge>=a.getCheckupInterval())a.performCheckup();
            if(a.finished)break;
            if(i%11==0){
                QVERIFY(a.saveGame(p));GameEngine b;QVERIFY2(b.loadGame(p),qPrintable(b.lastError()));b.setPaused(false);
                a.tick();b.tick();QCOMPARE(a.health,b.health);QCOMPARE(a.money,b.money);QVERIFY(a.rng==b.rng);
                QCOMPARE(a.diseaseHistory.size(),b.diseaseHistory.size());QCOMPARE(a.lastMajorOnsetTick,b.lastMajorOnsetTick);
                for(const auto& name:a.organs.keys()){QCOMPARE(a.organs[name].currentDisease.id,b.organs[name].currentDisease.id);QCOMPARE(a.organs[name].coexisting.size(),b.organs[name].coexisting.size());}
            }else a.tick();
        }
    }
    void initialAndFullReset(){
        GameEngine e;e.setPlayerAttributes("测试",5,5,10,10);
        e.organs["肝脏"].removedDiseases={"D37"};e.organs["心脏"].hasHiddenDisease=true;
        e.organs["心脏"].hiddenDisease=Database::getInstance().getDiseaseById("D32");
        e.maxAge=110;e.hasLifeExtensionTech=true;e.lastCheckupAge=80;
        e.initGame();QCOMPARE(e.getAge(),0);QCOMPARE(e.getMaxAge(),80);QCOMPARE(e.getLastCheckupAge(),-3);
        QVERIFY(!e.hasLifeExtensionTech);
        for(const auto& o:e.organs){QVERIFY(!o.hasHiddenDisease);QVERIFY(o.removedDiseases.isEmpty());}
    }
    void invalidDosagesAndPrematureExtensions(){
        GameEngine e;e.money=2000000;infect(e,"D22");
        for(int amount:{-100,-1,0,11,2147483647}){
            QVERIFY(!e.applyMedicine("胃部","M22",amount));QCOMPARE(e.money,2000000);QCOMPARE(e.health,100.0);
        }
        QVERIFY(!e.payForLifeExtension());QVERIFY(!e.payForLifeExtension(-1,100));
        QCOMPARE(e.maxAge,80);QCOMPARE(e.money,2000000);
    }
    void treatmentRespectsSelection(){
        GameEngine e;e.money=1000000;infect(e,"D17");infect(e,"D22");
        QVERIFY(!e.applyMedicine("胃部","M18",1));
        QVERIFY(e.organs["肺部"].isDiseased);QVERIFY(e.organs["胃部"].isDiseased);
        QVERIFY(e.applyMedicine("肺部","M18",1));QVERIFY(!e.organs["肺部"].isDiseased);
        QVERIFY(e.organs["胃部"].isDiseased);
    }
    void specialSurgeryNotUniversal(){
        GameEngine e;e.money=1000000;infect(e,"D22");
        QVERIFY(!e.medicineMatches(e.organs["胃部"],Database::getInstance().getMedicineById("M_CANCER_EARLY_OP")));
        QVERIFY(!e.applyMedicine("胃部","M_CANCER_EARLY_OP",1));QVERIFY(e.organs["胃部"].isDiseased);
    }
    void stagesRequireExactMedicine(){
        GameEngine e;infect(e,"D34",2);
        QVERIFY(e.medicineMatches(e.organs["胃部"],Database::getInstance().getMedicineById("M_CANCER_MID_OP")));
        QVERIFY(!e.medicineMatches(e.organs["胃部"],Database::getInstance().getMedicineById("M_CANCER_MID_CHEMO")));
        QVERIFY(!e.medicineMatches(e.organs["胃部"],Database::getInstance().getMedicineById("M_BROAD_SUPER")));
    }
    void selectedSurgeryDoesNotRemoveParentOrgan(){
        GameEngine e;e.money=100000;infect(e,"D29");
        QVERIFY(e.applyMedicine("胃部","M29",1));QVERIFY(!e.organs["胃部"].isRemoved);
        QVERIFY(e.organs["胃部"].removedDiseases.contains("D29"));
    }
    void latentDiseaseCannotBeOverwritten(){
        GameEngine e;e.rng.seed(12);e.age=55;
        auto& o=e.organs["胃部"];o.hasHiddenDisease=true;o.hiddenDisease=Database::getInstance().getDiseaseById("D34");o.diseaseStage=1;
        for(int i=0;i<100;++i)e.generateDiseases();
        bool found=o.hasHiddenDisease&&o.hiddenDisease.id=="D34";
        for(const auto& c:o.coexisting)found|=c.hasHiddenDisease&&c.hiddenDisease.id=="D34";
        QVERIFY(found);
    }
    void checkupCooldownAndProgressPreserved(){
        GameEngine e;e.money=100000;e.age=45;
        auto& o=e.organs["胃部"];o.hasHiddenDisease=true;o.hiddenDisease=Database::getInstance().getDiseaseById("D34");
        o.diseaseStage=1;o.hiddenTicks=7;
        QCOMPARE(DiseaseVisuals::forState(o).pattern,0);
        QVERIFY(e.performCheckup());QCOMPARE(e.money,70000);QVERIFY(o.isDiseased);QVERIFY(!o.hasHiddenDisease);
        QCOMPARE(o.diseasedDays,7);QCOMPARE(o.diseaseStage,1);QVERIFY(DiseaseVisuals::forState(o).pattern!=0);
        QVERIFY(!e.performCheckup());QCOMPARE(e.money,70000);
        for(int i=0;i<3;++i)e.applyDamageAndHeal();
        QCOMPARE(o.diseaseStage,2);
    }
    void pauseAndSlowTimeUseOneClock(){
        GameEngine a,b;a.rng.seed(99);b.rng.seed(99);infect(a,"D22");infect(b,"D22");
        a.setTimeMultiplier(1);b.setTimeMultiplier(0.1);
        for(int i=0;i<40;++i)a.advanceTime(0.025);
        for(int i=0;i<400;++i)b.advanceTime(0.025);
        QCOMPARE(a.currentTick,b.currentTick);QCOMPARE(a.health,b.health);QCOMPARE(a.immunity,b.immunity);
        double hp=b.health,immune=b.immunity;b.setPaused(true);
        for(int i=0;i<1000;++i)b.advanceTime(0.1);
        QCOMPARE(b.health,hp);QCOMPARE(b.immunity,immune);
    }
    void medicineConflictIndependentOfSideEffects(){
        GameEngine e;e.money=100000;e.immunity=100;infect(e,"D16");
        QVERIFY(e.applyMedicine("皮肤","M16",1));e.activeSideEffects.clear();
        QString conflict;QVERIFY(e.checkConflict("M19",conflict));int money=e.money;
        QVERIFY(!e.applyMedicine("皮肤","M19",1));QCOMPARE(e.money,money);
        for(int i=0;i<5;++i)e.updateSideEffects();
        QVERIFY(!e.checkConflict("M19",conflict));
    }
    void vitalBoundsAndDeathCannotReverse(){
        GameEngine e;e.money=10000000;e.bloodSugar=4;
        QVERIFY(!e.applyMedicine("","M_SUPP_04",10));QCOMPARE(e.bloodSugar,4.0);QCOMPARE(e.money,10000000);
        e.bpSys=80;e.bpDia=50;QVERIFY(!e.applyMedicine("","M_SUPP_02",1));
        e.immunity=100;QVERIFY(!e.applyMedicine("","M_SUPP_01",1));
        QSignalSpy death(&e,&GameEngine::gameOver);e.forceDeath("test");e.forceDeath("again");
        QCOMPARE(death.count(),1);int money=e.money;
        QVERIFY(!e.applyMedicine("","M_SUPP_01",1));QCOMPARE(e.money,money);QCOMPARE(e.health,0.0);
    }
    void noResurrectionWithinTick(){
        GameEngine e;e.health=0.000001;infect(e,"D06");e.organs["皮肤"].autoHealTimer=1;
        e.tick();QVERIFY(e.isFinished());QCOMPARE(e.health,0.0);
    }
    void ageCoverageAndVisualMapping(){
        auto& db=Database::getInstance();
        QCOMPARE(db.getAllDiseases().size(),54);
        for(const auto& d:db.getAllDiseases()){
            QVERIFY2(DiseaseVisuals::contains(d.id),qPrintable(d.id));
            QVERIFY(d.minAge<=d.maxAge);
            for(const auto& stages:{d.earlyTreatment,d.midTreatment,d.lateTreatment})
                for(const auto& id:stages)QVERIFY2(!db.getMedicineById(id).id.isEmpty(),qPrintable(id));
        }
        for(const auto& organ:{"心脏","肺部","肝脏","胃部","皮肤"}){
            QVERIFY(!db.getDiseasesByAgeAndOrgan(8,organ).isEmpty());
            QVERIFY(!db.getDiseasesByAgeAndOrgan(105,organ).isEmpty());
        }
    }
    void milestoneAccountingAndHardCap(){
        GameEngine e;e.research=17;e.money=6000000;e.age=79;e.currentTick=9;
        e.tick();QCOMPARE(e.age,80);QVERIFY(e.pendingMilestone);QCOMPARE(e.money,6000000+e.getAnnualIncome());
        int m=e.money;QVERIFY(!e.payForLifeExtension(1,10));QCOMPARE(e.money,m);
        e.setLifeExtensionTechName("Test");QVERIFY(e.payForLifeExtension());QCOMPARE(e.maxAge,90);
        QVERIFY(!e.payForLifeExtension());
        e.age=89;e.currentTick=9;e.tick();QVERIFY(e.pendingMilestone);QVERIFY(e.payForLifeExtension());QCOMPARE(e.maxAge,100);
        e.age=99;e.currentTick=9;e.tick();QVERIFY(e.pendingMilestone);e.setLifeExtensionTechName("Long");
        QVERIFY(e.payForLifeExtension());QCOMPARE(e.maxAge,110);
        e.age=109;e.currentTick=9;e.tick();QVERIFY(e.finished);QCOMPARE(e.age,110);QVERIFY(!e.payForLifeExtension());
    }
    void saveRoundTripAndDeterministicContinuation(){
        QTemporaryDir dir;GameEngine a,b;a.money=900000;a.age=45;a.rng.seed(73);
        infect(a,"D34",2);a.organs["胃部"].currentTreatmentStage=1;
        a.organs["肝脏"].removedDiseases={"D37"};a.activeSideEffects.append({"test",3,0.4,"M22"});
        a.medicationCooldowns["M16"]=4;a.lastCheckupAge=44;a.tickAccumulator=0.35;
        QVERIFY(a.saveGame(dir.filePath("state.json")));QVERIFY2(b.loadGame(dir.filePath("state.json")),qPrintable(b.lastError()));
        QCOMPARE(b.age,a.age);QCOMPARE(b.money,a.money);QCOMPARE(b.tickAccumulator,a.tickAccumulator);
        QCOMPARE(b.organs["胃部"].currentTreatmentStage,1);QCOMPARE(b.organs["肝脏"].removedDiseases,a.organs["肝脏"].removedDiseases);
        QCOMPARE(b.activeSideEffects.size(),1);QCOMPARE(b.medicationCooldowns,a.medicationCooldowns);QVERIFY(b.isPaused());
        b.setPaused(false);
        for(int i=0;i<8;++i){a.tick();b.tick();QCOMPARE(b.health,a.health);QCOMPARE(b.immunity,a.immunity);QCOMPARE(b.money,a.money);}
        for(const auto& name:a.organs.keys())QCOMPARE(b.organs[name].currentDisease.id,a.organs[name].currentDisease.id);
    }
    void corruptSaveIsAtomic(){
        QTemporaryDir dir;GameEngine e;const QString path=dir.filePath("s.json");QVERIFY(e.saveGame(path));
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();
        root["money"]=-100;
        QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();
        int money=e.money;QVERIFY(!e.loadGame(path));QCOMPARE(e.money,money);QCOMPARE(e.health,100.0);
        root["money"]=10000;auto organs=root["organs"].toArray();auto o=organs[0].toObject();o["disease"]="D_UNKNOWN";organs[0]=o;root["organs"]=organs;
        QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();
        QVERIFY(!e.loadGame(path));QCOMPARE(e.money,money);
    }
};
QTEST_GUILESS_MAIN(GameEngineTests)
#include "GameEngineTests.moc"
