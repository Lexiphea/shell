#include "sensorslib.hpp"

#include <qloggingcategory.h>

#include <sensors/sensors.h>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <mutex>

namespace {

Q_LOGGING_CATEGORY(lcSensorsLib, "caelestia.services.sensorslib", QtInfoMsg)

} // namespace

namespace caelestia::shell::services::sensorslib {

namespace {

std::atomic<bool> g_initOk{ false };
std::once_flag g_initFlag;

void doInit() {
    if (sensors_init(nullptr) != 0) {
        qCWarning(lcSensorsLib, "sensors_init failed");
        g_initOk.store(false, std::memory_order_release);
        return;
    }
    g_initOk.store(true, std::memory_order_release);
    std::atexit([] {
        if (g_initOk.load(std::memory_order_acquire)) {
            sensors_cleanup();
        }
    });
}

[[nodiscard]] std::optional<double> readTempInput(const sensors_chip_name* chip, const sensors_feature* feat) {
    const auto* sf = sensors_get_subfeature(chip, feat, SENSORS_SUBFEATURE_TEMP_INPUT);
    if (!sf)
        return std::nullopt;
    double value = 0.0;
    if (sensors_get_value(chip, sf->number, &value) != 0)
        return std::nullopt;
    return value;
}

[[nodiscard]] QByteArray featureLabel(const sensors_chip_name* chip, const sensors_feature* feat) {
    char* raw = sensors_get_label(chip, feat);
    if (!raw)
        return {};
    QByteArray out(raw);
    std::free(raw);
    return out;
}

// Walks every labelled temperature feature, optionally restricted to one bus type
template <typename F> void forEachTempFeature(short busType, F&& fn) {
    int chipNr = 0;
    while (const auto* chip = sensors_get_detected_chips(nullptr, &chipNr)) {
        if (busType != SENSORS_BUS_TYPE_ANY && chip->bus.type != busType)
            continue;

        int featNr = 0;
        while (const auto* feat = sensors_get_features(chip, &featNr)) {
            if (feat->type != SENSORS_FEATURE_TEMP)
                continue;

            const auto label = featureLabel(chip, feat);
            if (label.isEmpty())
                continue;

            fn(chip, feat, label);
        }
    }
}

enum class GpuTempKind : std::uint8_t {
    None,
    Primary,
    Fallback,
};

// Indexed tempN labels and the vendor die labels are the reading we want,
// junction/mem only stand in when no primary sensor exists
GpuTempKind classifyGpuTempLabel(const QByteArray& label) {
    const auto tempIndexed = label.startsWith(QByteArrayView("temp")) && label.size() > 4 &&
                             std::isdigit(static_cast<unsigned char>(label[4]));
    if (tempIndexed || label == QByteArrayView("GPU core") || label == QByteArrayView("edge"))
        return GpuTempKind::Primary;
    if (label == QByteArrayView("junction") || label == QByteArrayView("mem"))
        return GpuTempKind::Fallback;

    return GpuTempKind::None;
}

struct TempAccum {
    double sum = 0.0;
    int count = 0;

    void add(double value);
    [[nodiscard]] std::optional<double> average() const;
};

void TempAccum::add(double value) {
    sum += value;
    ++count;
}

std::optional<double> TempAccum::average() const {
    if (count == 0)
        return std::nullopt;
    return sum / count;
}

void ensureInit() {
    std::call_once(g_initFlag, doInit);
}

} // namespace

std::optional<double> cpuPackageTemp() {
    ensureInit();
    if (!g_initOk.load(std::memory_order_acquire))
        return std::nullopt;

    std::optional<double> primary;  // Package id N / Tdie
    std::optional<double> fallback; // Tctl

    forEachTempFeature(
        SENSORS_BUS_TYPE_ANY, [&](const sensors_chip_name* chip, const sensors_feature* feat, const QByteArray& label) {
            if (label.startsWith(QByteArrayView("Package id ")) || label == QByteArrayView("Tdie")) {
                if (const auto v = readTempInput(chip, feat))
                    primary = v;
            } else if (label == QByteArrayView("Tctl")) {
                if (const auto v = readTempInput(chip, feat))
                    fallback = v;
            }
        });

    return primary.has_value() ? primary : fallback;
}

std::optional<double> gpuPciAverageTemp() {
    ensureInit();
    if (!g_initOk.load(std::memory_order_acquire))
        return std::nullopt;

    TempAccum primary;
    TempAccum fallback;

    forEachTempFeature(
        SENSORS_BUS_TYPE_PCI, [&](const sensors_chip_name* chip, const sensors_feature* feat, const QByteArray& label) {
            const auto kind = classifyGpuTempLabel(label);
            if (kind == GpuTempKind::None)
                return;

            const auto v = readTempInput(chip, feat);
            if (!v)
                return;

            if (kind == GpuTempKind::Primary)
                primary.add(*v);
            else
                fallback.add(*v);
        });

    if (const auto avg = primary.average())
        return avg;
    return fallback.average();
}

} // namespace caelestia::shell::services::sensorslib
