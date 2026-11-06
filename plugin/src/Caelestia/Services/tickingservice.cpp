#include "tickingservice.hpp"

#include "config/dashboardconfig.hpp"
#include "config/rootnodes.hpp"

namespace caelestia::shell::services {

TickingService::TickingService(QObject* parent)
    : Service(parent)
    , m_timer(new QTimer(this)) {
    m_timer->setInterval(1000);
    QObject::connect(m_timer, &QTimer::timeout, this, [this] {
        tick();
    });

    const auto* dash = caelestia::shell::config::ConfigSingleton::instance()->dashboard();
    applyInterval(dash->resourceUpdateInterval());
    QObject::connect(
        dash, &caelestia::shell::config::DashboardConfig::resourceUpdateIntervalChanged, this, [this, dash] {
            applyInterval(dash->resourceUpdateInterval());
        });
}

void TickingService::start() {
    m_timer->start();
    tick();
}

void TickingService::stop() {
    m_timer->stop();
    stopped();
}

void TickingService::applyInterval(int ms) {
    if (ms > 0 && ms != m_timer->interval()) {
        m_timer->setInterval(ms);
    }
}

} // namespace caelestia::shell::services
