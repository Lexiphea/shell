#include "gpu.hpp"

#include <qdir.h>
#include <qdiriterator.h>
#include <qfile.h>
#include <qlibrary.h>
#include <qregularexpression.h>

#include <mutex>
#include <optional>

#include "config/rootnodes.hpp"
#include "config/serviceconfig.hpp"
#include "sensorslib.hpp"

namespace caelestia::services {

using Qt::StringLiterals::operator""_s;

namespace {

QStringList gpuBusyFiles() {
    static const QRegularExpression k_cardRe(u"^card\\d+$"_s);

    QStringList files;
    QDirIterator it(u"/sys/class/drm"_s, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString path = it.next();
        if (!k_cardRe.match(it.fileName()).hasMatch()) {
            continue;
        }
        const QString busy = path + u"/device/gpu_busy_percent"_s;
        if (QFile::exists(busy)) {
            files << busy;
        }
    }
    return files;
}

QString cleanName(QString s) {
    static const QRegularExpression k_noise(u"\\(R\\)|\\(TM\\)|Graphics"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression k_spaces(u"\\s+"_s);
    s.replace(k_noise, {});
    s.replace(k_spaces, u" "_s);
    return s.trimmed();
}

QString parseNvidiaName(const QByteArray& out) {
    const QString first = QString::fromUtf8(out).split(u'\n').value(0).trimmed();
    return first.isEmpty() ? QString() : cleanName(first);
}

QString parseGlxinfoName(const QByteArray& out) {
    const QStringList lines = QString::fromUtf8(out).split(u'\n');
    for (const QString& line : lines) {
        const qsizetype idx = line.indexOf(u"Device:"_s);
        if (idx < 0) {
            continue;
        }

        QString rest = line.mid(idx + 7);
        const qsizetype paren = rest.indexOf(u'(');
        if (paren >= 0) {
            rest = rest.left(paren);
        }

        const QString cleaned = cleanName(rest);
        if (!cleaned.isEmpty()) {
            return cleaned;
        }
    }

    return {};
}

QString parseLspciName(const QByteArray& out) {
    static const QRegularExpression k_lineRe(u"vga|3d controller|display"_s, QRegularExpression::CaseInsensitiveOption);

    const QStringList lines = QString::fromUtf8(out).split(u'\n');
    QString match;
    for (const QString& line : lines) {
        if (k_lineRe.match(line).hasMatch()) {
            match = line;
            break;
        }
    }

    if (match.isEmpty()) {
        return {};
    }

    static const QRegularExpression k_bracketRe(u"\\[([^\\]]+)\\][^\\[]*$"_s);
    const auto bracket = k_bracketRe.match(match);
    if (bracket.hasMatch()) {
        return cleanName(bracket.captured(1));
    }

    // Split on a colon followed by whitespace so the PCI slot ("00:02.0") is not
    // mistaken for the class/name separator ("controller: Device").
    static const QRegularExpression k_colonRe(u":\\s+(.+)"_s);
    const auto colon = k_colonRe.match(match);
    if (colon.hasMatch()) {
        return cleanName(colon.captured(1));
    }

    return {};
}

struct NameSource {
    QString program;
    QStringList args;
    QString (*parse)(const QByteArray&);
};

// Name probes in priority order; the first non-empty result wins. Which of them run
// depends on the resolved type.
const std::array<NameSource, 3>& nameSources() {
    static const std::array<NameSource, 3> k_sources = {
        {
            {
                .program = u"nvidia-smi"_s,
                .args = { u"--query-gpu=name"_s, u"--format=csv,noheader"_s },
                .parse = &parseNvidiaName,
            },
            {
                .program = u"glxinfo"_s,
                .args = { u"-B"_s },
                .parse = &parseGlxinfoName,
            },
            {
                .program = u"lspci"_s,
                .args = {},
                .parse = &parseLspciName,
            },
        },
    };
    return k_sources;
}

// NVML, the library nvidia-smi is built on. Loaded at runtime so the plugin builds and runs without the NVIDIA driver.
// Querying an open handle takes ~0.2ms where starting nvidia-smi took ~15ms of CPU, most of it initialising NVML.
// Its calls can block on the driver, so they only run on Gpu's NVML thread. The lock covers the brief overlap of two
// Gpu instances while QML reloads
class Nvml {
public:
    // Each reading is missing if the GPU doesn't support it
    struct Sample {
        std::optional<unsigned int> usage;
        std::optional<unsigned int> temperature;
    };

    // Opens the first GPU, unless already open. False if the driver isn't there
    bool open() {
        const std::scoped_lock lock(m_mutex);
        if (m_device) {
            return true;
        }
        if (!m_lib.isLoaded()) {
            m_lib.setFileNameAndVersion(u"nvidia-ml"_s, 1);
            if (!m_lib.load()) {
                return false;
            }
            m_init = reinterpret_cast<Init>(m_lib.resolve("nvmlInit_v2"));
            m_shutdown = reinterpret_cast<Shutdown>(m_lib.resolve("nvmlShutdown"));
            m_handleByIndex = reinterpret_cast<HandleByIndex>(m_lib.resolve("nvmlDeviceGetHandleByIndex_v2"));
            m_utilization = reinterpret_cast<Utilization>(m_lib.resolve("nvmlDeviceGetUtilizationRates"));
            m_temperature = reinterpret_cast<Temperature>(m_lib.resolve("nvmlDeviceGetTemperature"));
        }
        if (!m_init || !m_shutdown || !m_handleByIndex || !m_utilization || !m_temperature || m_init() != k_success) {
            return false;
        }
        if (m_handleByIndex(0, &m_device) != k_success) {
            m_device = nullptr;
            m_shutdown();
            return false;
        }
        return true;
    }

    // Lets the driver power the GPU down again
    void close() {
        const std::scoped_lock lock(m_mutex);
        if (m_device) {
            m_device = nullptr;
            m_shutdown();
        }
    }

    // Reads the open GPU, or nothing if open() failed
    [[nodiscard]] Sample sample() const {
        const std::scoped_lock lock(m_mutex);
        Sample out;
        if (!m_device) {
            return out;
        }
        if (UtilizationRates rates{}; m_utilization(m_device, &rates) == k_success) {
            out.usage = rates.gpu;
        }
        if (unsigned int temp = 0; m_temperature(m_device, k_temperatureGpu, &temp) == k_success) {
            out.temperature = temp;
        }
        return out;
    }

private:
    // The subset of nvml.h used here
    using Device = void*;

    struct UtilizationRates {
        unsigned int gpu;
        unsigned int memory;
    };

    static constexpr int k_success = 0;
    static constexpr int k_temperatureGpu = 0;
    using Init = int (*)();
    using Shutdown = int (*)();
    using HandleByIndex = int (*)(unsigned int, Device*);
    using Utilization = int (*)(Device, UtilizationRates*);
    using Temperature = int (*)(Device, int, unsigned int*);

    mutable std::mutex m_mutex;
    QLibrary m_lib;
    Init m_init = nullptr;
    Shutdown m_shutdown = nullptr;
    HandleByIndex m_handleByIndex = nullptr;
    Utilization m_utilization = nullptr;
    Temperature m_temperature = nullptr;
    Device m_device = nullptr;
};

Nvml& nvml() {
    static Nvml s_nvml;
    return s_nvml;
}

// Indices within nameSources(): nvidia-smi, then the driver-agnostic probes.
constexpr int k_nvidiaSource = 0;
constexpr int k_firstGenericSource = 1;

} // namespace

Gpu::Gpu(QObject* parent)
    : TickingService(parent) {
    // One thread, so NVML opens, reads and closes in the order they were asked for
    m_nvmlThread.setMaxThreadCount(1);
    m_busyFiles = gpuBusyFiles();

    const auto* svc = caelestia::config::ConfigSingleton::instance()->services();
    m_userType = svc->gpuType();
    QObject::connect(svc, &caelestia::config::ServiceConfig::gpuTypeChanged, this, [this, svc] {
        const GpuType value = svc->gpuType();
        if (value == m_userType) {
            return;
        }
        m_userType = value;
        resolveGpu();
    });

    resolveGpu();
}

GpuType Gpu::type() const {
    return m_type;
}

QString Gpu::name() const {
    return m_name;
}

bool Gpu::detecting() const {
    return m_detecting;
}

qreal Gpu::percentage() const {
    return m_percentage;
}

qreal Gpu::temperature() const {
    return m_temperature;
}

void Gpu::setType(GpuType value) {
    if (value == m_type) {
        return;
    }
    if (m_type == GpuType::Nvidia) {
        closeNvml();
    }
    m_type = value;
    if (m_type == GpuType::None) {
        resetUsage();
    }
    emit typeChanged();
}

void Gpu::setName(QString value) {
    if (value == m_name) {
        return;
    }
    m_name = std::move(value);
    emit nameChanged();
}

void Gpu::setDetecting(bool value) {
    if (value == m_detecting) {
        return;
    }
    m_detecting = value;
    emit detectingChanged();
}

void Gpu::tick() {
    if (m_type == GpuType::Generic) {
        readGenericUsage();
        readGpuTemperature();
    } else if (m_type == GpuType::Nvidia) {
        readNvidiaUsage();
    } else {
        resetUsage();
    }
}

void Gpu::resolveGpu() {
    // Supersede any chain still in flight so its callbacks cannot write stale state
    const int generation = ++m_generation;

    if (m_userType != GpuType::Auto) {
        setType(m_userType);
    }

    if (m_userType == GpuType::None) {
        setName({});
        setDetecting(false);
        return;
    }

    setName({});
    setDetecting(true);
    tryNameSource(m_userType == GpuType::Generic ? k_firstGenericSource : k_nvidiaSource, generation);
}

int Gpu::probeEnd() const {
    return m_userType == GpuType::Nvidia ? k_firstGenericSource : static_cast<int>(nameSources().size());
}

void Gpu::tryNameSource(int index, int generation) {
    const NameSource& src = nameSources().at(static_cast<std::size_t>(index));
    runProcess(src.program, src.args, [this, index, generation, parse = src.parse](const QByteArray& out) {
        finishNameSource(index, generation, parse(out));
    });
}

void Gpu::finishNameSource(int index, int generation, QString name) {
    if (generation != m_generation) {
        return; // superseded by a newer resolution
    }

    // Under Auto the NVIDIA name probe doubles as the type probe: a non-empty result
    // means an NVIDIA GPU is present and queryable.
    if (m_userType == GpuType::Auto && index == k_nvidiaSource) {
        if (!name.isEmpty())
            setType(GpuType::Nvidia);
        else
            setType(m_busyFiles.isEmpty() ? GpuType::None : GpuType::Generic);

        if (m_type == GpuType::None) {
            setName({});
            setDetecting(false);
            return;
        }
    }

    if (!name.isEmpty()) {
        setName(std::move(name));
        setDetecting(false);
        return;
    }

    // Fall through to the next applicable source
    const int next = index + 1;
    if (next < probeEnd()) {
        tryNameSource(next, generation);
    } else {
        setName({});
        setDetecting(false);
    }
}

void Gpu::runProcess(const QString& program, const QStringList& args, std::function<void(const QByteArray&)> callback) {
    auto* proc = new QProcess(this);
    proc->setStandardErrorFile(QProcess::nullDevice());

    // Deliver the result exactly once, then tear the process down. A crash, a missing
    // binary or a failed run yields empty output so the caller can fall through
    // gracefully: only FailedToStart skips finished(), and a crash reports CrashExit there.
    const auto finish = [proc, callback = std::move(callback)](const QByteArray& out) {
        callback(out);
        proc->deleteLater();
    };

    QObject::connect(proc, &QProcess::finished, this, [finish, proc](int code, QProcess::ExitStatus status) {
        const bool ok = status == QProcess::NormalExit && code == 0; // Fail on crashes and non-zero exit codes
        finish(ok ? proc->readAllStandardOutput() : QByteArray());
    });
    QObject::connect(proc, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            finish(QByteArray());
        }
    });

    proc->start(program, args);
}

void Gpu::readGenericUsage() {
    qreal sum = 0.0;
    int count = 0;
    for (const QString& path : std::as_const(m_busyFiles)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        bool ok = false;
        const qreal v = f.readAll().trimmed().toDouble(&ok);
        f.close();
        if (ok) {
            sum += v;
            ++count;
        }
    }
    setPercentage(count > 0 ? sum / count / 100.0 : 0.0);
}

void Gpu::stopped() {
    closeNvml();
}

void Gpu::readNvidiaUsage() {
    if (m_nvidiaQuerying) {
        return;
    }
    m_nvidiaQuerying = true;
    const int generation = m_generation;
    m_nvmlThread.start([this, generation] {
        const auto sample = nvml().open() ? nvml().sample() : Nvml::Sample{};
        QMetaObject::invokeMethod(this, [this, generation, sample] {
            m_nvidiaQuerying = false;

            // The type moved out from under the sample, so it no longer describes the GPU
            if (generation != m_generation) {
                return;
            }
            if (sample.usage) {
                setPercentage(*sample.usage / 100.0);
            }
            if (sample.temperature) {
                setTemperature(*sample.temperature);
            }
        });
    });
}

void Gpu::closeNvml() {
    m_nvmlThread.start([] {
        nvml().close();
    });
}

void Gpu::readGpuTemperature() {
    setTemperature(sensorslib::gpuPciAverageTemp().value_or(0.0));
}

void Gpu::resetUsage() {
    setPercentage(0.0);
    setTemperature(0.0);
}

void Gpu::setPercentage(qreal value) {
    if (std::abs(value - m_percentage) > 0.0001) {
        m_percentage = value;
        emit percentageChanged();
    }
}

void Gpu::setTemperature(qreal value) {
    if (std::abs(value - m_temperature) > 0.05) {
        m_temperature = value;
        emit temperatureChanged();
    }
}

} // namespace caelestia::services
