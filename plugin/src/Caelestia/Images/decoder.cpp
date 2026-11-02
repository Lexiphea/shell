#include "decoder.hpp"

#include <qfile.h>
#include <qimagereader.h>

#include <jpeglib.h>
#include <wuffs-v0.4.c>

#include <algorithm>
#include <cmath>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>

namespace caelestia::images {

using Qt::StringLiterals::operator""_s;

namespace {

QImage scaledTo(const QImage& image, QSize size) {
    if (image.isNull() || image.size() == size)
        return image;
    return image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

class PngCallbacks final : public wuffs_aux::DecodeImageCallbacks {
    wuffs_base__pixel_format SelectPixfmt(const wuffs_base__image_config& /*config*/) override {
        // Byte order of QImage::Format_ARGB32_Premultiplied on little-endian
        return wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL);
    }
};

QImage decodePng(QByteArrayView data, const std::function<QSize(QSize)>& targetFor) {
    PngCallbacks callbacks;
    wuffs_aux::sync_io::MemoryInput input(data.data(), static_cast<size_t>(data.size()));
    wuffs_aux::DecodeImageResult result = wuffs_aux::DecodeImage(callbacks, input);
    if (!result.error_message.empty())
        return {};

    const auto plane = result.pixbuf.plane(0);
    const QImage image(plane.ptr, static_cast<int>(result.pixbuf.pixcfg.width()),
        static_cast<int>(result.pixbuf.pixcfg.height()), static_cast<qsizetype>(plane.stride),
        QImage::Format_ARGB32_Premultiplied, std::free, result.pixbuf_mem_owner.release());
    return scaledTo(image, targetFor(image.size()));
}

struct JpegError {
    jpeg_error_mgr mgr;
    std::jmp_buf jump;
};

bool hasAllDc(const jpeg_decompress_struct& info) {
    for (int i = 0; i < info.num_components; ++i) {
        if (info.coef_bits[i][0] < 0)
            return false;
    }
    return true;
}

// libjpeg reports errors by longjmp, which skips destructors, so every non-trivial local is created before setjmp
QImage decodeJpeg(QByteArrayView data, const std::function<QSize(QSize)>& targetFor) {
    jpeg_decompress_struct info{};
    JpegError error{};
    QImage image;
    QSize target;

    info.err = jpeg_std_error(&error.mgr);
    error.mgr.error_exit = [](j_common_ptr common) {
        std::longjmp(reinterpret_cast<JpegError*>(common->err)->jump, 1);
    };
    error.mgr.output_message = [](j_common_ptr) {
    };

    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&info);
        return {};
    }

    jpeg_CreateDecompress(&info, JPEG_LIB_VERSION, sizeof(info));
    jpeg_mem_src(&info, reinterpret_cast<const unsigned char*>(data.data()), static_cast<unsigned long>(data.size()));
    jpeg_read_header(&info, TRUE);

    const QSize source(static_cast<int>(info.image_width), static_cast<int>(info.image_height));
    target = targetFor(source);

    // libjpeg-turbo can decode at n/8 of the size, skipping most of the work, so use the smallest n that covers target
    const qreal ratio = std::max(
        target.width() / static_cast<qreal>(source.width()), target.height() / static_cast<qreal>(source.height()));
    info.scale_num = std::clamp(static_cast<unsigned int>(std::ceil(ratio * 8)), 1u, 8u);
    info.scale_denom = 8;
    // Byte order of QImage::Format_RGB32 on little-endian
    info.out_color_space = JCS_EXT_BGRA;

    // At 1/8 each pixel is just its block's DC coefficient, and progressive JPEGs send all of those first, so the rest
    // of the file can be skipped. This takes a 4K progressive thumbnail from ~75ms to a fraction of that
    const bool dcOnly = info.scale_num == 1 && jpeg_has_multiple_scans(&info);
    info.buffered_image = dcOnly ? TRUE : FALSE;

    jpeg_start_decompress(&info);
    image = QImage(static_cast<int>(info.output_width), static_cast<int>(info.output_height), QImage::Format_RGB32);

    if (dcOnly) {
        // Some encoders send the DC coefficients of each component in separate scans, so wait for all of them
        for (;;) {
            const int status = jpeg_consume_input(&info);
            if (status == JPEG_REACHED_EOI || (status == JPEG_SCAN_COMPLETED && hasAllDc(info)))
                break;
        }
        jpeg_start_output(&info, info.input_scan_number);
    }

    while (info.output_scanline < info.output_height) {
        JSAMPROW row = image.scanLine(static_cast<int>(info.output_scanline));
        jpeg_read_scanlines(&info, &row, 1);
    }

    if (dcOnly)
        jpeg_finish_output(&info);
    jpeg_destroy_decompress(&info);

    return scaledTo(image, target);
}

DecodeResult decodeWithQt(const QString& path, const std::function<QSize(QSize)>& targetFor) {
    QImageReader reader(path);
    const QSize source = reader.size();
    if (source.isValid())
        reader.setScaledSize(targetFor(source));

    QImage image = reader.read();
    if (image.isNull())
        return { {}, u"Failed to decode "_s + path + u": "_s + reader.errorString() };

    if (!source.isValid())
        image = scaledTo(image, targetFor(image.size()));
    return { image, {} };
}

} // namespace

DecodeResult decodeImage(const QString& path, const std::function<QSize(QSize)>& targetFor) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { {}, u"Failed to open "_s + path + u": "_s + file.errorString() };

    const QByteArray data = file.readAll();
    QImage image;
    if (data.startsWith("\x89PNG\r\n\x1a\n"))
        image = decodePng(data, targetFor);
    else if (data.startsWith("\xff\xd8\xff"))
        image = decodeJpeg(data, targetFor);

    // Other formats, and files the fast paths reject (e.g. CMYK JPEGs)
    if (image.isNull())
        return decodeWithQt(path, targetFor);
    return { image, {} };
}

} // namespace caelestia::images
