#pragma once

#include <qobject.h>
#include <qqmlintegration.h>
#include <qvariant.h>

namespace caelestia {

// Natural language calculator backed by SoulverCore (the engine behind Raycast's calculator).
// libSoulverWrapper.so (soulver-cpp) is loaded at runtime, so it is an optional dependency.
// SoulverCore asserts it is called from the main thread, so this is all synchronous.
class Soulver : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Soulver(QObject* parent = nullptr);

    // Returns { value, type }, or null if the expression isn't a calculation
    [[nodiscard]] Q_INVOKABLE QVariant eval(const QString& expr) const;

private:
    char* (*m_evaluate)(const char*) = nullptr;
};

} // namespace caelestia
