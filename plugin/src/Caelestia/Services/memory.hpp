#pragma once

#include <qqmlintegration.h>
#include <qvariant.h>

#include "tickingservice.hpp"

namespace caelestia::services {

class Memory : public TickingService {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(qreal used READ used NOTIFY changed)
    Q_PROPERTY(qreal total READ total NOTIFY changed)
    Q_PROPERTY(qreal percentage READ percentage NOTIFY changed)

public:
    explicit Memory(QObject* parent = nullptr);

    [[nodiscard]] qreal used() const;
    [[nodiscard]] qreal total() const;
    [[nodiscard]] qreal percentage() const;

signals:
    void changed();

protected:
    void tick() override;

private:
    quint64 m_used = 0;  // KiB
    quint64 m_total = 0; // KiB
};

} // namespace caelestia::services
