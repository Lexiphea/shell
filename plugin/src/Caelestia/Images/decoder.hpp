#pragma once

#include <qimage.h>
#include <qsize.h>
#include <qstring.h>

#include <functional>

namespace caelestia::shell::images {

struct DecodeResult {
    QImage image;
    QString error;
};

// Decodes the image at path at the size targetFor picks from the source size. PNGs go through Wuffs, and JPEGs through
// libjpeg so they can skip most of the work when scaled down. Videos give their first frame through ffmpeg. Qt decodes
// anything else
DecodeResult decodeImage(const QString& path, const std::function<QSize(QSize)>& targetFor);

} // namespace caelestia::shell::images
