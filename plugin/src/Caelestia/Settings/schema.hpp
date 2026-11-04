#pragma once

#include <qhash.h>
#include <qlist.h>
#include <qmetaobject.h>
#include <qmetatype.h>
#include <qstring.h>
#include <qvariant.h>

namespace caelestia::shell::settings {

class ValueCodec;

struct Annotation {
    QVariant defaultValue;
    bool globalOnly = false;
    // NOLINTNEXTLINE(readability-redundant-member-init) callers will get missing field init warnings without
    QList<QMetaType> allowedTypes = {}; // For QVariant properties
};

struct Descriptor {
    QString key;
    QMetaType type;
    int metaIndex;
    bool isNode;
    Annotation annotation;
    const ValueCodec* codec = nullptr; // Null for nodes and unsupported types

    [[nodiscard]] const QVariant& defaultValue() const { return annotation.defaultValue; }

    [[nodiscard]] bool globalOnly() const { return annotation.globalOnly; }

    [[nodiscard]] QString typeString() const;
    [[nodiscard]] bool accepts(const QMetaType& valueType) const;
};

class Schema {
public:
    static Schema build(const QMetaObject* meta, int baseOffset, bool includeReadOnly = false);
    static void annotate(const QMetaObject* meta, const QString& key, Annotation annotation);

    [[nodiscard]] const QList<Descriptor>& descriptors() const;
    [[nodiscard]] const Descriptor* get(const QString& key) const;

private:
    explicit Schema() = default;

    QList<Descriptor> m_descriptors;
    QHash<QString, qsizetype> m_keyToIndex;
};

} // namespace caelestia::shell::settings
