#pragma once

#include <qtimer.h>

#include "service.hpp"

namespace caelestia::shell::services {

class TickingService : public Service {
    Q_OBJECT

public:
    explicit TickingService(QObject* parent = nullptr);

protected:
    virtual void tick() = 0;

    // Runs when the last user releases the service, to free anything tick() holds open
    virtual void stopped() {}

private:
    void start() final;
    void stop() final;

    void applyInterval(int ms);

    QTimer* m_timer;
};

} // namespace caelestia::shell::services
