#include "memory.hpp"

#include <qfile.h>
#include <qregularexpression.h>

namespace caelestia::shell::services {

using Qt::StringLiterals::operator""_s;

Memory::Memory(QObject* parent)
    : TickingService(parent) {}

qreal Memory::used() const {
    return static_cast<qreal>(m_used);
}

qreal Memory::total() const {
    return static_cast<qreal>(m_total);
}

qreal Memory::percentage() const {
    return m_total > 0 ? used() / total() : 0.0;
}

void Memory::tick() {
    QFile f(u"/proc/meminfo"_s);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    static const QRegularExpression k_reTotal(u"MemTotal: *(\\d+)"_s);
    static const QRegularExpression k_reAvail(u"MemAvailable: *(\\d+)"_s);
    const QString text = QString::fromLatin1(data);

    const auto totalMatch = k_reTotal.match(text);
    const auto availMatch = k_reAvail.match(text);
    if (!totalMatch.hasMatch() || !availMatch.hasMatch()) {
        return;
    }

    const quint64 totalKib = totalMatch.captured(1).toULongLong();
    const quint64 availKib = availMatch.captured(1).toULongLong();
    if (totalKib == 0) {
        return;
    }
    const quint64 usedKib = totalKib > availKib ? totalKib - availKib : 0;

    if (totalKib == m_total && usedKib == m_used) {
        return;
    }
    m_total = totalKib;
    m_used = usedKib;
    emit changed();
}

} // namespace caelestia::shell::services
