#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct SaveEntry {
    QString slot;
    QString name;
    QString path;
    int age = 0;
    QDateTime modified;
    bool finished = false;
    // Metadata is readable and supported. GameEngine still validates the full save.
    bool valid = false;
};

namespace SaveRepository {

// An explicitly scoped repository, primarily useful for tests. It never selects
// a fallback location or creates, overwrites, or deletes save files.
class Store {
public:
    explicit Store(const QString& explicitDirectory);
    QString directory() const;
    QList<SaveEntry> entries() const;
    QString pathFor(const QString& slot) const;

private:
    QString root;
};

// Prefer a writable saves folder beside the executable. If it is unavailable,
// use the application's per-user data folder. Empty means neither is writable.
QString directory();
QList<SaveEntry> entries();
// Only "autosave" and "manual_<canonical lowercase UUID>" are accepted.
// Invalid slots and existing symbolic-link targets return an empty path.
QString pathFor(const QString& slot);
// Generates an identifier only; saving is always GameEngine's responsibility.
QString createManualSlot();

} // namespace SaveRepository
