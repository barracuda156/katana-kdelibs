/*  This file is part of the KDE libraries
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "webp.h"
#include "kdebug.h"

#include <QVariant>

static const char* const s_webppluginformat = "webp";

static const ushort s_peekbuffsize = 32;

WebPHandler::WebPHandler()
    : m_quality(100),
    m_loopcount(0),
    m_imagecount(1),
    m_imagedelay(100),
    m_currentimage(0),
    m_webpanimdec(nullptr),
    m_framepainter(nullptr)
{
}

WebPHandler::~WebPHandler()
{
    if (m_webpanimdec) {
        WebPAnimDecoderDelete(m_webpanimdec);
        m_webpanimdec = nullptr;
    }

    if (m_framepainter) {
        delete m_framepainter;
        m_framepainter = nullptr;
    }
}

bool WebPHandler::canRead() const
{
    if (canRead(device())) {
        setFormat(s_webppluginformat);
        return true;
    }
    return false;
}

bool WebPHandler::read(QImage *image)
{
    if (m_currentimage == 0 && !m_webpanimdec) {
        // lazy init for compat
        if (!jumpToImage(0)) {
            return false;
        }
    }

    WebPIterator webpiter;
    const WebPDemuxer* webpdemuxer = WebPAnimDecoderGetDemuxer(m_webpanimdec);
    // NOTE: 0 will return the last frame
    int webpstatus = WebPDemuxGetFrame(webpdemuxer, m_currentimage + 1, &webpiter);
    if (Q_UNLIKELY(webpstatus == 0)) {
        kWarning() << "Could not get frame" << m_currentimage << webpstatus << m_webpanimdec;
        WebPDemuxReleaseIterator(&webpiter);
        return false;
    }

    QImage frame(webpiter.width, webpiter.height, QImage::Format_ARGB32);
    if (Q_UNLIKELY(frame.isNull())) {
        kWarning() << "Could not create image";
        WebPDemuxReleaseIterator(&webpiter);
        return false;
    }

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    const uint8_t* webpoutput = WebPDecodeARGBInto(
#else
    const uint8_t* webpoutput = WebPDecodeBGRAInto(
#endif
        webpiter.fragment.bytes, webpiter.fragment.size,
        reinterpret_cast<uint8_t*>(frame.bits()), frame.byteCount(),
        frame.bytesPerLine()
    );
    if (Q_UNLIKELY(!webpoutput)) {
        kWarning() << "Could not decode image";
        WebPDemuxReleaseIterator(&webpiter);
        return false;
    }

    switch (webpiter.dispose_method) {
        case WEBP_MUX_DISPOSE_BACKGROUND: {
            // yep, there are images attempting to dispose the background without previous frame
            if (!m_previousrect.isNull()) {
                m_framepainter->setCompositionMode(QPainter::CompositionMode_Clear);
                m_framepainter->fillRect(m_previousrect, m_background);
            }
            break;
        }
        case WEBP_MUX_DISPOSE_NONE: {
            break;
        }
        default: {
            kWarning() << "Unknown dispose method" << webpiter.dispose_method;
            break;
        }
    }

    switch (webpiter.blend_method) {
        case WEBP_MUX_BLEND: {
            m_framepainter->setCompositionMode(QPainter::CompositionMode_SourceOver);
            break;
        }
        case WEBP_MUX_NO_BLEND: {
            m_framepainter->setCompositionMode(QPainter::CompositionMode_Source);
            break;
        }
        default: {
            kWarning() << "Unknown blend method" << webpiter.blend_method;
            m_framepainter->setCompositionMode(QPainter::CompositionMode_Source);
            break;
        }
    }
    m_previousrect = QRectF(webpiter.x_offset, webpiter.y_offset, webpiter.width, webpiter.height);
    m_framepainter->drawImage(m_previousrect, frame);


    // bound to reasonable limits
    m_imagedelay = qBound(10, webpiter.duration, 10000);

    *image = m_framebuffer;

    WebPDemuxReleaseIterator(&webpiter);
    return true;
}

bool WebPHandler::write(const QImage &image)
{
    if (image.isNull()) {
        return false;
    } else if (Q_UNLIKELY(image.height() >= WEBP_MAX_DIMENSION || image.width() >= WEBP_MAX_DIMENSION)) {
        // limitation in WebP
        kWarning() << "Image dimension limit";
        return false;
    }

    const QImage image32 = image.convertToFormat(QImage::Format_ARGB32);

    uint8_t *webpoutput = nullptr;
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    // TODO: move alpha?
    const size_t webpsize = WebPEncodeRGBA(
#else
    const size_t webpsize = WebPEncodeBGRA(
#endif
        image32.constBits(),
        image32.width(), image32.height(), image32.width() * 4,
        m_quality,
        &webpoutput
    );

    if (Q_UNLIKELY(webpsize == 0)) {
        kWarning() << "Could not encode image";
        WebPFree(webpoutput);
        return false;
    }

    if (Q_UNLIKELY(device()->write(reinterpret_cast<const char*>(webpoutput), webpsize) != webpsize)) {
        kWarning() << "Could not write image";
        WebPFree(webpoutput);
        return false;
    }
    WebPFree(webpoutput);

    return true;
}

QByteArray WebPHandler::name() const
{
    return s_webppluginformat;
}

bool WebPHandler::supportsOption(QImageIOHandler::ImageOption option) const
{
    switch (option) {
        case QImageIOHandler::Quality:
        case QImageIOHandler::Size:
        case QImageIOHandler::Animation: {
            return true;
        }
        default: {
            return false;
        }
    }
    Q_UNREACHABLE();
}

QVariant WebPHandler::option(QImageIOHandler::ImageOption option) const
{
    switch (option) {
        case QImageIOHandler::Quality: {
            return m_quality;
        }
        case QImageIOHandler::Size: {
            const QByteArray data = device()->peek(s_peekbuffsize);

            WebPBitstreamFeatures webpfeatures;
            const VP8StatusCode vp8statusret = WebPGetFeatures(
                reinterpret_cast<const uint8_t*>(data.constData()), data.size(),
                &webpfeatures
            );
            if (vp8statusret != VP8_STATUS_OK) {
                kWarning() << "Could not get image features for size option";
                return QVariant(QSize());
            }
            return QVariant(QSize(webpfeatures.width, webpfeatures.height));
        }
        case QImageIOHandler::Animation: {
            const QByteArray data = device()->peek(s_peekbuffsize);

            WebPBitstreamFeatures webpfeatures;
            const VP8StatusCode vp8statusret = WebPGetFeatures(
                reinterpret_cast<const uint8_t*>(data.constData()), data.size(),
                &webpfeatures
            );
            if (vp8statusret != VP8_STATUS_OK) {
                kWarning() << "Could not get image features for animation option";
                return QVariant(bool(false));
            }
            return QVariant(bool(webpfeatures.has_animation));
        }
        default: {
            return QVariant();
        }
    }
    Q_UNREACHABLE();
}

void WebPHandler::setOption(QImageIOHandler::ImageOption option, const QVariant &value)
{
    if (option == QImageIOHandler::Quality) {
        const int newquality = value.toInt();
        // -1 means default
        if (newquality == -1) {
            m_quality = 100;
        } else {
            m_quality = qBound(0, newquality, 100);
        }
    }
}

bool WebPHandler::jumpToImage(int imageNumber)
{
    if (imageNumber == 0) {
        const qint64 devicepos = device()->pos();
        m_data = device()->readAll();
        device()->seek(devicepos);

        if (m_webpanimdec) {
            WebPAnimDecoderDelete(m_webpanimdec);
            m_webpanimdec = nullptr;
        }

        if (m_framepainter) {
            delete m_framepainter;
            m_framepainter = nullptr;
        }
        m_framebuffer = QImage();
        m_background = QColor();

        m_webpdata = { reinterpret_cast<const uint8_t*>(m_data.constData()), size_t(m_data.size()) };
        WebPAnimDecoderOptionsInit(&m_webpanimoptions);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
        m_webpanimoptions.color_mode = WEBP_CSP_MODE::MODE_ARGB;
#else
        m_webpanimoptions.color_mode = WEBP_CSP_MODE::MODE_BGRA;
#endif
        m_webpanimdec = WebPAnimDecoderNew(
            &m_webpdata,
            &m_webpanimoptions
        );
        if (Q_UNLIKELY(!m_webpanimdec)) {
            kWarning() << "Could not create animation decoder";
            m_data.clear();
            return false;
        }

        WebPAnimInfo webpaniminfo;
        int webpstatus = WebPAnimDecoderGetInfo(m_webpanimdec, &webpaniminfo);
        if (Q_UNLIKELY(webpstatus == 0)) {
            kWarning() << "Could not get animation information";
            m_data.clear();
            WebPAnimDecoderDelete(m_webpanimdec);
            return false;
        }

        m_loopcount = webpaniminfo.loop_count;
        m_imagecount = webpaniminfo.frame_count;

        m_framebuffer = QImage(webpaniminfo.canvas_width, webpaniminfo.canvas_height, QImage::Format_ARGB32_Premultiplied);
        if (Q_UNLIKELY(m_framebuffer.isNull())) {
            kWarning() << "Could not create buffer image";
            return false;
        }
        // NOTE: have to fill, areas of frames may not be drawn
        m_framebuffer.fill(Qt::transparent);
        m_framepainter = new QPainter(&m_framebuffer);
        m_background = QColor(QRgb(webpaniminfo.bgcolor));
    }
    if (imageNumber >= m_imagecount) {
        return false;
    }
    m_currentimage = imageNumber;
    return true;
}

int WebPHandler::loopCount() const
{
    return m_loopcount;
}

int WebPHandler::imageCount() const
{
    return m_imagecount;
}

int WebPHandler::nextImageDelay() const
{
    return m_imagedelay;
}

int WebPHandler::currentImageNumber() const
{
    return m_currentimage;
}

bool WebPHandler::canRead(QIODevice *device)
{
    if (Q_UNLIKELY(!device)) {
        kWarning() << "Called with no device";
        return false;
    }

    // WebP file header: 4 bytes "RIFF", 4 bytes length, 4 bytes "WEBP"
    const QByteArray header = device->peek(12);
    if (header.size() == 12 && header.startsWith("RIFF") && header.endsWith("WEBP")) {
        return true;
    }
    return false;
}

QList<QByteArray> WebPPlugin::mimeTypes() const
{
    static const QList<QByteArray> list = QList<QByteArray>()
        << "image/webp";
    return list;
}

QImageIOPlugin::Capabilities WebPPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == s_webppluginformat) {
        return QImageIOPlugin::Capabilities(QImageIOPlugin::CanRead | QImageIOPlugin::CanWrite);
    }
    if (!device || !device->isOpen()) {
        return 0;
    }
    QImageIOPlugin::Capabilities cap = 0;
    if (device->isReadable() && WebPHandler::canRead(device)) {
        cap |= QImageIOPlugin::CanRead;
    }
    if (format == s_webppluginformat && device->isWritable()) {
        cap |= QImageIOPlugin::CanWrite;
    }
    return cap;
}

QImageIOHandler *WebPPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new WebPHandler();
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

Q_EXPORT_PLUGIN(WebPPlugin)
