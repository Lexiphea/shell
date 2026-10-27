#pragma once

#include <qtimer.h>

#include "service.hpp"

namespace caelestia::services {

class TickingService : public Service {
    Q_OBJECT

public:
    explicit TickingService(QObject* parent = nullptr);

protected:
    virtual void tick() = 0;

private:
    void start() final;
    void stop() final;

    void applyInterval(int ms);

    QTimer* m_timer;
};

} // namespace caelestia::services
