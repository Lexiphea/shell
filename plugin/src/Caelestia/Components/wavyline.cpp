#include "wavyline.hpp"

#include <qpainter.h>
#include <qpainterpath.h>

namespace caelestia::components {

namespace {

template <typename T> void setAndUpdate(WavyLine* item, T& member, const T& value, void (WavyLine::*changed)()) {
    if constexpr (std::is_floating_point_v<T>) {
        if (qFuzzyCompare(member + 1.0, value + 1.0))
            return;
    } else if (member == value) {
        return;
    }

    member = value;
    emit(item->*changed)();
    item->update();
}

} // namespace

WavyLine::WavyLine(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_lineWidth(4)
    , m_amplitudeMultiplier(0.5)
    , m_frequency(6)
    , m_startX(0)
    , m_fullLength(0)
    , m_color(Qt::white)
    , m_waveProgress(0)
    , m_pathType(PathType::Linear)
    , m_startAngle(0)
    , m_fullAngle(360)
    , m_radius(-1)
    , m_value(1) {
    setAntialiasing(true);
}

int WavyLine::lineWidth() const {
    return m_lineWidth;
}

void WavyLine::setLineWidth(int lineWidth) {
    setAndUpdate(this, m_lineWidth, lineWidth, &WavyLine::lineWidthChanged);
}

qreal WavyLine::amplitudeMultiplier() const {
    return m_amplitudeMultiplier;
}

void WavyLine::setAmplitudeMultiplier(qreal amplitudeMultiplier) {
    setAndUpdate(this, m_amplitudeMultiplier, amplitudeMultiplier, &WavyLine::amplitudeMultiplierChanged);
}

int WavyLine::frequency() const {
    return m_frequency;
}

void WavyLine::setFrequency(int frequency) {
    setAndUpdate(this, m_frequency, frequency, &WavyLine::frequencyChanged);
}

qreal WavyLine::startX() const {
    return m_startX;
}

void WavyLine::setStartX(qreal startX) {
    setAndUpdate(this, m_startX, startX, &WavyLine::startXChanged);
}

qreal WavyLine::fullLength() const {
    return m_fullLength;
}

void WavyLine::setFullLength(qreal fullLength) {
    setAndUpdate(this, m_fullLength, fullLength, &WavyLine::fullLengthChanged);
}

QColor WavyLine::color() const {
    return m_color;
}

void WavyLine::setColor(const QColor& color) {
    setAndUpdate(this, m_color, color, &WavyLine::colorChanged);
}

qreal WavyLine::waveProgress() const {
    return m_waveProgress;
}

void WavyLine::setWaveProgress(qreal progress) {
    setAndUpdate(this, m_waveProgress, progress, &WavyLine::waveProgressChanged);
}

WavyLine::PathType WavyLine::pathType() const {
    return m_pathType;
}

void WavyLine::setPathType(PathType pathType) {
    setAndUpdate(this, m_pathType, pathType, &WavyLine::pathTypeChanged);
}

qreal WavyLine::startAngle() const {
    return m_startAngle;
}

void WavyLine::setStartAngle(qreal startAngle) {
    setAndUpdate(this, m_startAngle, startAngle, &WavyLine::startAngleChanged);
}

qreal WavyLine::fullAngle() const {
    return m_fullAngle;
}

void WavyLine::setFullAngle(qreal fullAngle) {
    setAndUpdate(this, m_fullAngle, fullAngle, &WavyLine::fullAngleChanged);
}

qreal WavyLine::radius() const {
    return m_radius;
}

void WavyLine::setRadius(qreal radius) {
    setAndUpdate(this, m_radius, radius, &WavyLine::radiusChanged);
}

qreal WavyLine::value() const {
    return m_value;
}

void WavyLine::setValue(qreal value) {
    setAndUpdate(this, m_value, value, &WavyLine::valueChanged);
}

void WavyLine::paint(QPainter* painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(m_color, m_lineWidth, Qt::SolidLine, Qt::RoundCap));

    if (m_pathType == PathType::Arc) {
        paintArc(painter);
    } else {
        paintLinear(painter);
    }
}

void WavyLine::paintLinear(QPainter* painter) {
    const auto amplitude = m_lineWidth * m_amplitudeMultiplier;
    const auto phase = m_waveProgress * 2 * M_PI;
    const auto centerY = height() / 2;
    const auto len = m_fullLength > 0 ? m_fullLength : 1;
    const auto start = m_lineWidth / 2.0;
    const auto fullEnd = width() - m_lineWidth / 2.0;
    const auto drawEnd = start + (fullEnd - start) * m_value;

    QPainterPath path;
    bool first = true;

    for (int x = m_lineWidth / 2; x <= drawEnd; ++x) {
        const auto theta = m_frequency * 2 * M_PI * (x + m_startX) / len + phase;
        const auto waveY = centerY + amplitude * qSin(theta);
        if (first) {
            path.moveTo(x, waveY);
            first = false;
        } else {
            path.lineTo(x, waveY);
        }
    }

    painter->drawPath(path);
}

void WavyLine::paintArc(QPainter* painter) {
    if (m_fullAngle <= 0) {
        return;
    }

    const auto amplitude = m_lineWidth * m_amplitudeMultiplier;
    const auto cx = width() / 2.0;
    const auto cy = height() / 2.0;
    const auto radius = m_radius > 0 ? m_radius : (qMin(width(), height()) - m_lineWidth - 2 * amplitude) / 2.0;

    if (radius <= 0) {
        return;
    }

    const auto phase = m_waveProgress * 2 * M_PI;
    const auto fullAngleRad = qDegreesToRadians(m_fullAngle);
    const auto arcLen = radius * fullAngleRad;
    const auto len = m_fullLength > 0 ? m_fullLength : arcLen;
    const auto drawAngleRad = fullAngleRad * m_value;

    if (drawAngleRad <= 0) {
        return;
    }

    const auto segments = qMax(64, qCeil(radius * drawAngleRad));
    const auto dTheta = drawAngleRad / segments;

    const auto startAngleRad = qDegreesToRadians(m_startAngle);
    QPainterPath path;

    for (int i = 0; i <= segments; ++i) {
        const auto theta = startAngleRad + i * dTheta;
        const auto s = i * dTheta * radius;
        const auto phi = m_frequency * 2 * M_PI * (s + m_startX) / len + phase;
        const auto r = radius + amplitude * qSin(phi);
        const auto px = cx + r * qCos(theta);
        const auto py = cy + r * qSin(theta);
        if (i == 0) {
            path.moveTo(px, py);
        } else {
            path.lineTo(px, py);
        }
    }

    painter->drawPath(path);
}

} // namespace caelestia::components
