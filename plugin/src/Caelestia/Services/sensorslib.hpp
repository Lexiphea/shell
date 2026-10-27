#pragma once

#include <optional>

namespace caelestia::services::sensorslib {

[[nodiscard]] std::optional<double> cpuPackageTemp();
[[nodiscard]] std::optional<double> gpuPciAverageTemp();

} // namespace caelestia::services::sensorslib
