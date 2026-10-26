#include "blobgroup.hpp"

#include "blobinvertedrect.hpp"
#include "blobshape.hpp"

namespace caelestia::blobs {

namespace {

void touch(BlobShape* shape) {
    shape->polish();
    shape->update();
}

} // namespace

BlobGroup::BlobGroup(QObject* parent)
    : QObject(parent) {}

BlobGroup::~BlobGroup() {
    for (auto* shape : std::as_const(m_shapes))
        shape->m_group = nullptr;
    if (m_invertedRect)
        m_invertedRect->m_group = nullptr;
}

qreal BlobGroup::smoothing() const {
    return m_smoothing;
}

void BlobGroup::setSmoothing(qreal s) {
    if (qFuzzyCompare(m_smoothing, s))
        return;
    m_smoothing = s;
    emit smoothingChanged();
    markDirty();
}

QColor BlobGroup::color() const {
    return m_color;
}

void BlobGroup::setColor(const QColor& c) {
    if (m_color == c)
        return;
    m_color = c;
    emit colorChanged();
    // Colour is read straight from the group when painting, so a repaint is enough
    for (auto* shape : std::as_const(m_shapes))
        shape->update();
    if (m_invertedRect)
        m_invertedRect->update();
}

bool BlobGroup::cornerFill() const {
    return m_cornerFill;
}

void BlobGroup::setCornerFill(bool e) {
    if (m_cornerFill == e)
        return;
    m_cornerFill = e;
    emit cornerFillChanged();
    markDirty();
}

void BlobGroup::addShape(BlobShape* shape) {
    if (!shape || m_shapes.contains(shape))
        return;
    m_shapes.append(shape);
    markDirty();
}

void BlobGroup::removeShape(BlobShape* shape) {
    m_shapes.removeOne(shape);
    markDirty();
}

void BlobGroup::setInvertedRect(BlobInvertedRect* rect) {
    if (m_invertedRect == rect)
        return;
    m_invertedRect = rect;
    markDirty();
}

void BlobGroup::clearInvertedRect(BlobInvertedRect* rect) {
    if (m_invertedRect != rect)
        return;
    m_invertedRect = nullptr;
    markDirty();
}

const QList<BlobShape*>& BlobGroup::shapes() const {
    return m_shapes;
}

BlobInvertedRect* BlobGroup::invertedRect() const {
    return m_invertedRect;
}

void BlobGroup::markDirty() {
    m_physicsUpdated = false;
    for (auto* shape : std::as_const(m_shapes))
        touch(shape);
    if (m_invertedRect)
        touch(m_invertedRect);
}

void BlobGroup::markShapeDirty(BlobShape* source) {
    m_physicsUpdated = false;

    touch(source);

    // Neighbours of both the old and new position need updating, or a large move can skip shapes it lands next to
    const QRectF current = source->paddedSceneRect(source->mapToScene(QPointF(0, 0)), static_cast<float>(m_smoothing));
    const qreal pad = m_smoothing * 2.0;
    const QRectF srcRect = source->m_paddedSceneRect.united(current).adjusted(-pad, -pad, pad, pad);

    for (auto* shape : std::as_const(m_shapes)) {
        if (shape != source && srcRect.intersects(shape->m_paddedSceneRect))
            touch(shape);
    }

    if (m_invertedRect && m_invertedRect != source)
        touch(m_invertedRect);
}

void BlobGroup::ensurePhysicsUpdated() {
    if (m_physicsUpdated)
        return;
    m_physicsUpdated = true;
    for (auto* shape : std::as_const(m_shapes))
        shape->updatePhysics();
}

} // namespace caelestia::blobs
