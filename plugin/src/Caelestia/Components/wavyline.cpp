#include "wavyline.hpp"

#include <qquickwindow.h>
#include <qsggeometry.h>
#include <qsgmaterial.h>
#include <qsgmaterialshader.h>
#include <qsgnode.h>

#include <cmath>
#include <cstring>

namespace caelestia::components {

using Qt::StringLiterals::operator""_s;

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
    : QQuickItem(parent)
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
    setFlag(ItemHasContents);
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

namespace {

// Laid out as the shader's uniform block after qt_Matrix and qt_Opacity, in logical pixels
struct WaveUniforms {
    float dpr;
    float size[2];
    float color[4];
    float lineWidth;
    float amplitude;
    float waveK;  // Phase per pixel along the line
    float phase0; // Phase at the start of the line
    float start;  // Linear: first x. Arc: start angle in radians
    float end;    // Linear: last x. Arc: swept angle in radians
    float radius;
    float arc;
};

static_assert(sizeof(WaveUniforms) == 60);

class WaveMaterial : public QSGMaterial {
public:
    [[nodiscard]] QSGMaterialType* type() const override {
        static QSGMaterialType s_type;
        return &s_type;
    }

    [[nodiscard]] QSGMaterialShader* createShader(QSGRendererInterface::RenderMode mode) const override;

    int compare(const QSGMaterial* other) const override {
        const auto c = std::memcmp(&uniforms, &static_cast<const WaveMaterial*>(other)->uniforms, sizeof(uniforms));
        return c < 0 ? -1 : (c > 0 ? 1 : 0);
    }

    WaveUniforms uniforms{};
};

class WaveShader : public QSGMaterialShader {
public:
    WaveShader() {
        setShaderFileName(VertexStage, u":/shaders/wavyline.vert.qsb"_s);
        setShaderFileName(FragmentStage, u":/shaders/wavyline.frag.qsb"_s);
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override {
        Q_UNUSED(oldMaterial);
        QByteArray* const buf = state.uniformData();
        Q_ASSERT(buf->size() >= 128);

        if (state.isMatrixDirty())
            std::memcpy(buf->data(), state.combinedMatrix().constData(), 64);
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            std::memcpy(buf->data() + 64, &opacity, 4);
        }
        std::memcpy(buf->data() + 68, &static_cast<WaveMaterial*>(newMaterial)->uniforms, sizeof(WaveUniforms));
        return true;
    }
};

QSGMaterialShader* WaveMaterial::createShader(QSGRendererInterface::RenderMode mode) const {
    Q_UNUSED(mode);
    return new WaveShader;
}

} // namespace

QSGNode* WavyLine::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) {
    Q_UNUSED(data);

    WaveUniforms u{};
    const auto amplitude = m_lineWidth * m_amplitudeMultiplier;
    const auto phase = m_waveProgress * 2 * M_PI;
    bool visible = m_color.alpha() > 0 && width() > 0 && height() > 0;
    qreal waveK = 0;

    if (m_pathType == PathType::Arc) {
        const auto radius = m_radius > 0 ? m_radius : (qMin(width(), height()) - m_lineWidth - 2 * amplitude) / 2.0;
        const auto fullAngleRad = qDegreesToRadians(m_fullAngle);
        const auto len = m_fullLength > 0 ? m_fullLength : radius * fullAngleRad;
        const auto drawAngleRad = fullAngleRad * m_value;
        visible = visible && m_fullAngle > 0 && radius > 0 && drawAngleRad > 0;

        waveK = m_frequency * 2 * M_PI / len;
        u.start = static_cast<float>(qDegreesToRadians(m_startAngle));
        u.end = static_cast<float>(drawAngleRad);
        u.radius = static_cast<float>(radius);
        u.arc = 1;
    } else {
        const auto len = m_fullLength > 0 ? m_fullLength : 1;
        const auto fullEnd = width() - m_lineWidth / 2.0;
        // Matches the whole pixel steps the line was drawn with before
        const auto start = m_lineWidth / 2.0;
        const int first = m_lineWidth / 2;
        const auto last = std::floor(start + (fullEnd - start) * m_value);
        visible = visible && last > first;

        waveK = m_frequency * 2 * M_PI / len;
        u.start = static_cast<float>(first);
        u.end = static_cast<float>(last);
    }

    if (!visible) {
        delete oldNode;
        return nullptr;
    }

    u.dpr = static_cast<float>(window()->effectiveDevicePixelRatio());
    u.size[0] = static_cast<float>(width());
    u.size[1] = static_cast<float>(height());
    const auto alpha = m_color.alphaF();
    u.color[0] = m_color.redF() * alpha;
    u.color[1] = m_color.greenF() * alpha;
    u.color[2] = m_color.blueF() * alpha;
    u.color[3] = alpha;
    u.lineWidth = static_cast<float>(m_lineWidth);
    u.amplitude = static_cast<float>(amplitude);
    u.waveK = static_cast<float>(waveK);
    u.phase0 = static_cast<float>(waveK * m_startX + phase);

    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto* const geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        auto* const material = new WaveMaterial;
        material->setFlag(QSGMaterial::Blending);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    QSGGeometry::updateTexturedRectGeometry(node->geometry(), boundingRect(), QRectF(0, 0, 1, 1));
    static_cast<WaveMaterial*>(node->material())->uniforms = u;
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}

} // namespace caelestia::components
