#include "SaveRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 maximumMetadataBytes = 1024 * 1024;

bool validSlot(const QString& slot) {
    static const QRegularExpression pattern(QStringLiteral(
        "\\A(?:autosave|manual_[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\\z"));
    return pattern.match(slot).hasMatch();
}

QString writableDirectory(const QString& path) {
    if (path.isEmpty() || !QDir().mkpath(path)) return {};
    QTemporaryFile probe(QDir(path).filePath(QStringLiteral(".healthgame-write-XXXXXX")));
    if (!probe.open()) return {};
    // The newly created probe removes itself; no existing save is touched.
    return QDir(path).absolutePath();
}

bool readMetadata(const QString& path, SaveEntry& entry) {
    if (path.isEmpty()) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() >= maximumMetadataBytes)
        return false;
    // Also bound the read if another process grows the file after size().
    const QByteArray bytes = file.read(maximumMetadataBytes);
    if (bytes.size() >= maximumMetadataBytes || !file.atEnd() || file.error() != QFileDevice::NoError)
        return false;
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto object = document.object();
    const auto version = object.value(QStringLiteral("version"));
    const auto age = object.value(QStringLiteral("age"));
    const auto name = object.value(QStringLiteral("playerName"));
    const auto finished = object.value(QStringLiteral("finished"));
    if (object.value(QStringLiteral("format")) != QStringLiteral("HealthGame")
        || !version.isDouble() || (version.toDouble() != 2 && version.toDouble() != 3)
        || !age.isDouble() || !std::isfinite(age.toDouble())
        || age.toDouble() < 0 || age.toDouble() > 110 || std::floor(age.toDouble()) != age.toDouble()
        || !name.isString() || name.toString().size() > 40
        || (!finished.isUndefined() && !finished.isBool()))
        return false;
    entry.name = name.toString().trimmed();
    if (entry.name.isEmpty()) entry.name = QStringLiteral("无名氏");
    entry.age = age.toInt();
    entry.finished = finished.toBool(false);
    return true;
}
} // namespace

namespace SaveRepository {

Store::Store(const QString& explicitDirectory) {
    if (!explicitDirectory.isEmpty()) root = QDir(explicitDirectory).absolutePath();
}

QString Store::directory() const { return root; }

QString Store::pathFor(const QString& slot) const {
    if (root.isEmpty() || !validSlot(slot)) return {};
    const auto path = QDir(root).filePath(slot + QStringLiteral(".json"));
    const QFileInfo target(path);
    if (target.isSymLink() || (target.exists() && !target.isFile())) return {};
    return path;
}

QList<SaveEntry> Store::entries() const {
    QList<SaveEntry> result;
    if (root.isEmpty()) return result;
    const QDir folder(root);
    for (const auto& info : folder.entryInfoList({QStringLiteral("*.json")},
             QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::NoSort)) {
        const auto fileName = info.fileName();
        if (!fileName.endsWith(QStringLiteral(".json"))) continue;
        const auto slot = fileName.chopped(5);
        if (!validSlot(slot)) continue;
        SaveEntry entry;
        entry.slot = slot;
        entry.path = pathFor(slot);
        entry.modified = info.lastModified();
        entry.name = QStringLiteral("不可读取的存档");
        entry.valid = !info.isSymLink() && readMetadata(entry.path, entry);
        result.append(entry);
    }
    std::sort(result.begin(), result.end(), [](const SaveEntry& a, const SaveEntry& b) {
        if (a.modified != b.modified) return a.modified > b.modified;
        return a.slot < b.slot;
    });
    return result;
}

QString directory() {
    static const QString chosen = [] {
        const auto applicationPath = QCoreApplication::applicationDirPath();
        if (!applicationPath.isEmpty()) {
            const auto primary = writableDirectory(QDir(applicationPath).filePath(QStringLiteral("saves")));
            if (!primary.isEmpty()) return primary;
        }
        const auto localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        return localData.isEmpty() ? QString() : writableDirectory(QDir(localData).filePath(QStringLiteral("saves")));
    }();
    return chosen;
}

QList<SaveEntry> entries() { return Store(directory()).entries(); }
QString pathFor(const QString& slot) { return Store(directory()).pathFor(slot); }
QString createManualSlot() { return QStringLiteral("manual_") + QUuid::createUuid().toString(QUuid::WithoutBraces); }

} // namespace SaveRepository
