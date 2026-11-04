#pragma once

#include <optional>

namespace caelestia::shell::services::sensorslib {

[[nodiscard]] std::optional<double> cpuPackageTemp();
[[nodiscard]] std::optional<double> gpuPciAverageTemp();

} // namespace caelestia::shell::services::sensorslib
