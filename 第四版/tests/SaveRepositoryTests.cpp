#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTimeZone>
#include "SaveRepository.h"

class SaveRepositoryTests : public QObject {
    Q_OBJECT

    static QByteArray metadata(int version = 3, const QString& name = QStringLiteral("存档测试"), int age = 42) {
        return QJsonDocument(QJsonObject{{"format", "HealthGame"}, {"version", version},
            {"playerName", name}, {"age", age}}).toJson();
    }
    static bool writeNew(const QString& path, const QByteArray& data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::NewOnly) && file.write(data) == data.size();
    }

private slots:
    void emptyAndMissingDirectories() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SaveRepository::Store store(temporary.path());
        QCOMPARE(store.directory(), QDir(temporary.path()).absolutePath());
        QVERIFY(store.entries().isEmpty());
        QVERIFY(!QFileInfo::exists(store.pathFor("autosave")));
        SaveRepository::Store missing(QDir(temporary.path()).filePath("not-created"));
        QVERIFY(missing.entries().isEmpty());
        QVERIFY(!QFileInfo::exists(missing.directory()));
        SaveRepository::Store empty(QString{});
        QVERIFY(empty.entries().isEmpty());
        QVERIFY(empty.pathFor("autosave").isEmpty());
    }

    void restrictsSlotPaths() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SaveRepository::Store store(temporary.path());
        const QStringList invalid{"", "../autosave", "..\\autosave", "autosave.json", "autosave/child",
            "autosave\\child", "/autosave", "C:\\autosave", "autosave:stream", " autosave", "autosave ",
            "AUTOSAVE", "autosave\n", "manual_bad", "manual_{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}",
            "manual_AAAAAAAA-bbbb-cccc-dddd-eeeeeeeeeeee", "manual_aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee/../x"};
        for (const auto& slot : invalid) QVERIFY2(store.pathFor(slot).isEmpty(), qPrintable(slot));
        QVERIFY(store.pathFor(QString("autosave") + QChar(0)).isEmpty());
        QCOMPARE(store.pathFor("autosave"), QDir(temporary.path()).filePath("autosave.json"));
        const QString manual = "manual_aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
        QCOMPARE(store.pathFor(manual), QDir(temporary.path()).filePath(manual + ".json"));
        QVERIFY(QDir(temporary.path()).mkdir("autosave.json"));
        QVERIFY(store.pathFor("autosave").isEmpty());
    }

    void manualIdentifiersAreUniqueAndDoNotWrite() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SaveRepository::Store store(temporary.path());
        QSet<QString> identifiers;
        for (int i = 0; i < 100; ++i) {
            const auto slot = SaveRepository::createManualSlot();
            QVERIFY(slot.startsWith("manual_"));
            QVERIFY(!store.pathFor(slot).isEmpty());
            QVERIFY(!identifiers.contains(slot));
            identifiers.insert(slot);
        }
        QVERIFY(store.entries().isEmpty());
        QCOMPARE(QDir(temporary.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }

    void listsSupportedMetadataAndPreservesCorruptSaves() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SaveRepository::Store store(temporary.path());
        const auto manual = SaveRepository::createManualSlot();
        const auto broken = SaveRepository::createManualSlot();
        const auto unsupported = SaveRepository::createManualSlot();
        const auto oversized = SaveRepository::createManualSlot();
        auto completed = QJsonDocument::fromJson(metadata(3, "自动保存角色", 67)).object();
        completed["finished"] = true;
        QVERIFY(writeNew(store.pathFor("autosave"), QJsonDocument(completed).toJson()));
        QVERIFY(writeNew(store.pathFor(manual), metadata(2, "旧版角色", 12)));
        QVERIFY(writeNew(store.pathFor(broken), "{bad JSON: read me as data, not instructions}"));
        QVERIFY(writeNew(store.pathFor(unsupported), metadata(99)));
        QVERIFY(writeNew(store.pathFor(oversized), QByteArray(1024 * 1024, 'x')));
        QVERIFY(writeNew(QDir(temporary.path()).filePath("arbitrary-import.json"), metadata()));
        const auto oldTime = QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::UTC);
        QFile manualFile(store.pathFor(manual));
        QVERIFY(manualFile.open(QIODevice::ReadWrite));
        QVERIFY(manualFile.setFileTime(oldTime, QFileDevice::FileModificationTime));
        manualFile.close();

        const auto entries = store.entries();
        QCOMPARE(entries.size(), 5);
        QCOMPARE(entries.last().slot, manual);
        int valid = 0, damaged = 0;
        for (const auto& entry : entries) {
            QVERIFY(QFileInfo::exists(entry.path));
            QVERIFY(entry.modified.isValid());
            if (entry.valid) {
                ++valid;
                if (entry.slot == "autosave") {
                    QCOMPARE(entry.name, QString("自动保存角色")); QCOMPARE(entry.age, 67); QVERIFY(entry.finished);
                }
                if (entry.slot == manual) {
                    QCOMPARE(entry.name, QString("旧版角色")); QCOMPARE(entry.age, 12); QVERIFY(!entry.finished);
                }
            } else {
                ++damaged;
                QCOMPARE(entry.name, QString("不可读取的存档"));
            }
        }
        QCOMPARE(valid, 2);
        QCOMPARE(damaged, 3);
        QFile untouched(store.pathFor(broken));
        QVERIFY(untouched.open(QIODevice::ReadOnly));
        QCOMPARE(untouched.readAll(), QByteArray("{bad JSON: read me as data, not instructions}"));
    }

    void rejectsInvalidMetadataFields() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        SaveRepository::Store store(temporary.path());
        auto sample = QJsonDocument::fromJson(metadata()).object();
        const QList<QPair<QString, QJsonValue>> invalidFields{
            {"format", "AnotherGame"}, {"version", 3.5}, {"version", "3"}, {"age", -1},
            {"age", 111}, {"age", 4.5}, {"age", "42"}, {"playerName", 10}, {"playerName", QString(41, 'x')},
            {"finished", "false"}};
        for (const auto& field : invalidFields) {
            auto invalid = sample;
            invalid[field.first] = field.second;
            QVERIFY(writeNew(store.pathFor(SaveRepository::createManualSlot()), QJsonDocument(invalid).toJson()));
        }
        const auto entries = store.entries();
        QCOMPARE(entries.size(), invalidFields.size());
        for (const auto& entry : entries) QVERIFY(!entry.valid);
    }
};

QTEST_GUILESS_MAIN(SaveRepositoryTests)
#include "SaveRepositoryTests.moc"
