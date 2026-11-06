#pragma once

#include <qobject.h>
#include <qqmlintegration.h>

namespace caelestia::shell::models {

// fzf style matching of arbitrary objects by a string property
class Fuzzy : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Fuzzy(QObject* parent = nullptr);

    // Items whose key property matches the query, best first. Returns all items if the query is empty
    [[nodiscard]] Q_INVOKABLE static QObjectList search(
        const QObjectList& items, const QString& query, const QString& key);
};

} // namespace caelestia::shell::models
