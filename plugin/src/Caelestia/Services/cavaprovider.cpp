#include "cavaprovider.hpp"

#include <qdir.h>
#include <qfile.h>
#include <qloggingcategory.h>

#include <cava/cavacore.h>
#include <fftw3.h>

#include <cstddef>

#include "audiocollector.hpp"
#include "audioprovider.hpp"

namespace {

Q_LOGGING_CATEGORY(lcCava, "caelestia.services.cava", QtInfoMsg)

using Qt::StringLiterals::operator""_s;

QByteArray wisdomPath() {
    QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
    if (cache.isEmpty())
        cache = QDir::homePath() + u"/.cache"_s;
    QDir().mkpath(cache + u"/caelestia"_s);
    return QFile::encodeName(cache + u"/caelestia/fftw-wisdom"_s);
}

} // namespace

namespace caelestia::shell::services {

CavaProcessor::CavaProcessor(QObject* parent)
    : AudioProcessor(parent)
    , m_plan(nullptr)
    , m_in(new double[ac::k_bufferSize])
    , m_out(nullptr)
    , m_bars(0) {};

CavaProcessor::~CavaProcessor() {
    cleanup();
    delete[] m_in;
}

void CavaProcessor::process() {
    if (!m_plan) {
        return;
    }

    // cava keeps its own window and wants only the new samples. With none it still lets the bars fall
    const int count = static_cast<int>(AudioCollector::instance().readNew(m_in, ac::k_bufferSize, m_cursor));
    cava_execute(m_in, count, m_out, m_plan);

    // Apply monstercat filter
    QVector<double> values(m_bars);

    // Left to right pass
    const double inv = 1.0 / 1.5;
    double carry = 0.0;
    for (int i = 0; i < m_bars; ++i) {
        carry = std::max(m_out[i], carry * inv);
        values[i] = carry;
    }

    // Right to left pass and combine
    carry = 0.0;
    for (int i = m_bars - 1; i >= 0; --i) {
        carry = std::max(m_out[i], carry * inv);
        values[i] = std::max(values[i], carry);
    }

    // Update values
    if (values != m_values) {
        m_values = std::move(values);
        emit valuesChanged(m_values);
    }
}

void CavaProcessor::setBars(int bars) {
    if (m_bars == bars) {
        return;
    }

    m_bars = bars;
    cleanup();
    if (m_bars > 0) {
        // cava plans its FFTs with FFTW_MEASURE, which times candidate algorithms for ~0.3s of CPU on every start.
        // FFTW wisdom keeps the winners, so only the first start measures. Nothing else here uses double precision FFTW
        const QByteArray wisdom = wisdomPath();
        fftw_import_wisdom_from_filename(wisdom.constData());
        m_plan = cava_init(m_bars, ac::k_sampleRate, 1, 1, 0.85, 50, 10000);
        fftw_export_wisdom_to_filename(wisdom.constData());
        m_out = new double[static_cast<size_t>(m_bars)];
    }
}

void CavaProcessor::cleanup() {
    if (m_plan) {
        cava_destroy(m_plan);
        m_plan = nullptr;
    }

    if (m_out) {
        delete[] m_out;
        m_out = nullptr;
    }
}

CavaProvider::CavaProvider(QObject* parent)
    : AudioProvider(parent)
    , m_bars(0)
    , m_values(m_bars, 0.0) {
    m_processor = new CavaProcessor();
    init();

    connect(static_cast<CavaProcessor*>(m_processor), &CavaProcessor::valuesChanged, this, &CavaProvider::updateValues);
}

int CavaProvider::bars() const {
    return m_bars;
}

void CavaProvider::setBars(int bars) {
    if (bars < 0) {
        qCWarning(lcCava) << "setBars: bars must be greater than 0. Setting to 0.";
        bars = 0;
    }

    if (m_bars == bars) {
        return;
    }

    m_values.resize(bars, 0.0);
    m_bars = bars;
    emit barsChanged();
    emit valuesChanged();

    QMetaObject::invokeMethod(
        static_cast<CavaProcessor*>(m_processor), &CavaProcessor::setBars, Qt::QueuedConnection, bars);
}

QVector<double> CavaProvider::values() const {
    return m_values;
}

void CavaProvider::updateValues(const QVector<double>& values) {
    if (values != m_values) {
        m_values = values;
        emit valuesChanged();
    }
}

} // namespace caelestia::shell::services
