#include "RtspVideoItem.h"

#include <QByteArray>
#include <QDateTime>
#include <QMetaObject>
#include <QPainter>
#include <QRectF>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

struct RtspVideoItem::WorkerState
{
    std::atomic_bool stopRequested { false };
};

namespace {
QString ffmpegErrorToString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

void setOption(AVDictionary **options, const char *key, const char *value)
{
    av_dict_set(options, key, value, 0);
}
}

int interruptCallback(void *opaque)
{
    auto *state = static_cast<RtspVideoItem::WorkerState *>(opaque);
    return state && state->stopRequested.load() ? 1 : 0;
}

RtspVideoItem::RtspVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAntialiasing(false);
}

RtspVideoItem::~RtspVideoItem()
{
    stopWorker();
}

QString RtspVideoItem::source() const
{
    QMutexLocker locker(&m_mutex);
    return m_source;
}

void RtspVideoItem::setSource(const QString &source)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_source == source) {
            return;
        }
        m_source = source;
        m_frame = QImage();
        m_videoSize = QSize();
        m_startupDelayMs = -1;
        m_errorString.clear();
    }

    emit sourceChanged();
    emit videoSizeChanged();
    emit startupDelayMsChanged();
    emit errorStringChanged();
    update();
    startWorker();
}

QString RtspVideoItem::status() const
{
    QMutexLocker locker(&m_mutex);
    return m_status;
}

QString RtspVideoItem::errorString() const
{
    QMutexLocker locker(&m_mutex);
    return m_errorString;
}

QSize RtspVideoItem::videoSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_videoSize;
}

int RtspVideoItem::startupDelayMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_startupDelayMs;
}

void RtspVideoItem::paint(QPainter *painter)
{
    QImage frame;
    {
        QMutexLocker locker(&m_mutex);
        frame = m_frame;
    }

    painter->fillRect(boundingRect(), Qt::black);
    if (frame.isNull()) {
        return;
    }

    const QSizeF itemSize = boundingRect().size();
    QSizeF frameSize = frame.size();
    frameSize.scale(itemSize, Qt::KeepAspectRatio);

    const QRectF target((itemSize.width() - frameSize.width()) / 2.0,
                        (itemSize.height() - frameSize.height()) / 2.0,
                        frameSize.width(),
                        frameSize.height());
    painter->drawImage(target, frame);
}

void RtspVideoItem::start()
{
    startWorker();
}

void RtspVideoItem::stop()
{
    stopWorker();
}

void RtspVideoItem::handleDecodedFrame(const QImage &image, int startupDelayMs)
{
    bool sizeChanged = false;
    bool startupChanged = false;
    bool statusChangedNow = false;
    bool errorChangedNow = false;

    {
        QMutexLocker locker(&m_mutex);
        m_frame = image;
        if (m_videoSize != image.size()) {
            m_videoSize = image.size();
            sizeChanged = true;
        }
        if (m_startupDelayMs < 0 && startupDelayMs >= 0) {
            m_startupDelayMs = startupDelayMs;
            startupChanged = true;
        }
        if (m_status != QStringLiteral("Playing")) {
            m_status = QStringLiteral("Playing");
            statusChangedNow = true;
        }
        if (!m_errorString.isEmpty()) {
            m_errorString.clear();
            errorChangedNow = true;
        }
    }

    if (statusChangedNow) {
        emit statusChanged();
    }
    if (errorChangedNow) {
        emit errorStringChanged();
    }
    if (sizeChanged) {
        emit videoSizeChanged();
    }
    if (startupChanged) {
        emit startupDelayMsChanged();
    }
    update();
}

void RtspVideoItem::handleStatusChanged(const QString &status)
{
    setStatus(status);
}

void RtspVideoItem::handleErrorChanged(const QString &message)
{
    setErrorString(message);
}

void RtspVideoItem::startWorker()
{
    QString currentSource;
    {
        QMutexLocker locker(&m_mutex);
        currentSource = m_source;
        m_frame = QImage();
        m_videoSize = QSize();
        m_startupDelayMs = -1;
        m_errorString.clear();
    }

    stopWorker();

    if (currentSource.isEmpty()) {
        setStatus(QStringLiteral("Waiting"));
        setErrorString(QString());
        return;
    }

    setStatus(QStringLiteral("Connecting"));
    setErrorString(QString());
    emit videoSizeChanged();
    emit startupDelayMsChanged();

    m_state = std::make_shared<WorkerState>();
    m_worker = std::thread(&RtspVideoItem::decodeLoop, this, currentSource, m_state);
}

void RtspVideoItem::stopWorker()
{
    if (m_state) {
        m_state->stopRequested.store(true);
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }
    m_state.reset();
}

void RtspVideoItem::setStatus(const QString &status)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_status != status) {
            m_status = status;
            changed = true;
        }
    }
    if (changed) {
        emit statusChanged();
    }
}

void RtspVideoItem::setErrorString(const QString &message)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_errorString != message) {
            m_errorString = message;
            changed = true;
        }
    }
    if (changed) {
        emit errorStringChanged();
    }
}

void RtspVideoItem::setVideoSize(const QSize &size)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_videoSize != size) {
            m_videoSize = size;
            changed = true;
        }
    }
    if (changed) {
        emit videoSizeChanged();
    }
}

void RtspVideoItem::setStartupDelayMs(int delayMs)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_startupDelayMs != delayMs) {
            m_startupDelayMs = delayMs;
            changed = true;
        }
    }
    if (changed) {
        emit startupDelayMsChanged();
    }
}

void RtspVideoItem::decodeLoop(QString source, std::shared_ptr<WorkerState> state)
{
    static std::once_flag networkInitFlag;
    std::call_once(networkInitFlag, []() { avformat_network_init(); });

    const qint64 startupStart = QDateTime::currentMSecsSinceEpoch();
    bool firstFrameDelivered = false;

    AVFormatContext *formatContext = avformat_alloc_context();
    if (!formatContext) {
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("FFmpeg format context allocation failed.")));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Error")));
        return;
    }
    formatContext->interrupt_callback.callback = interruptCallback;
    formatContext->interrupt_callback.opaque = state.get();

    AVDictionary *options = nullptr;
    setOption(&options, "rtsp_transport", "tcp");
    setOption(&options, "fflags", "nobuffer");
    setOption(&options, "flags", "low_delay");
    setOption(&options, "max_delay", "0");
    setOption(&options, "probesize", "1048576");
    setOption(&options, "analyzeduration", "1000000");
    setOption(&options, "reorder_queue_size", "0");
    setOption(&options, "stimeout", "3000000");
    setOption(&options, "timeout", "3000000");

    int result = avformat_open_input(&formatContext, source.toUtf8().constData(), nullptr, &options);
    av_dict_free(&options);
    if (result < 0) {
        const QString message = QStringLiteral("FFmpeg open failed: ") + ffmpegErrorToString(result);
        avformat_free_context(formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection, Q_ARG(QString, message));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("Error")));
        return;
    }


    result = avformat_find_stream_info(formatContext, nullptr);
    if (result < 0) {
        const QString message = QStringLiteral("Stream info failed: ") + ffmpegErrorToString(result);
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection, Q_ARG(QString, message));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex < 0) {
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Video stream not found.")));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Decoder not found.")));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Codec context allocation failed.")));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    result = avcodec_parameters_to_context(codecContext, videoStream->codecpar);
    if (result < 0) {
        const QString message = QStringLiteral("Codec parameter copy failed: ") + ffmpegErrorToString(result);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection, Q_ARG(QString, message));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecContext->thread_count = 1;

    result = avcodec_open2(codecContext, codec, nullptr);
    if (result < 0) {
        const QString message = QStringLiteral("Decoder open failed: ") + ffmpegErrorToString(result);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection, Q_ARG(QString, message));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgbFrame = av_frame_alloc();
    if (!packet || !frame || !rgbFrame) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        QMetaObject::invokeMethod(this, "handleErrorChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Frame allocation failed.")));
        QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Error")));
        return;
    }

    SwsContext *swsContext = nullptr;
    QByteArray rgbBuffer;
    int lastWidth = 0;
    int lastHeight = 0;
    AVPixelFormat lastFormat = AV_PIX_FMT_NONE;

    while (!state->stopRequested.load()) {
        result = av_read_frame(formatContext, packet);
        if (result < 0) {
            if (result == AVERROR_EXIT || state->stopRequested.load()) {
                break;
            }
            QMetaObject::invokeMethod(this, "handleStatusChanged", Qt::QueuedConnection,
                                      Q_ARG(QString, QStringLiteral("Connecting")));
            av_packet_unref(packet);
            continue;
        }

        if (packet->stream_index != videoStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        result = avcodec_send_packet(codecContext, packet);
        av_packet_unref(packet);
        if (result < 0) {
            continue;
        }

        while (!state->stopRequested.load()) {
            result = avcodec_receive_frame(codecContext, frame);
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                break;
            }
            if (result < 0) {
                break;
            }

            const int width = frame->width;
            const int height = frame->height;
            const auto format = static_cast<AVPixelFormat>(frame->format);
            if (!swsContext || width != lastWidth || height != lastHeight || format != lastFormat) {
                if (swsContext) {
                    sws_freeContext(swsContext);
                }
                swsContext = sws_getContext(width, height, format,
                                            width, height, AV_PIX_FMT_BGRA,
                                            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                lastWidth = width;
                lastHeight = height;
                lastFormat = format;
                const int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_BGRA, width, height, 1);
                rgbBuffer.resize(bufferSize);
                av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize,
                                     reinterpret_cast<const uint8_t *>(rgbBuffer.constData()),
                                     AV_PIX_FMT_BGRA, width, height, 1);
            }

            if (!swsContext) {
                av_frame_unref(frame);
                continue;
            }

            sws_scale(swsContext, frame->data, frame->linesize, 0, height,
                      rgbFrame->data, rgbFrame->linesize);

            QImage image(reinterpret_cast<const uchar *>(rgbFrame->data[0]),
                         width, height, rgbFrame->linesize[0], QImage::Format_ARGB32);
            const int startupDelay = firstFrameDelivered
                                         ? -1
                                         : static_cast<int>(QDateTime::currentMSecsSinceEpoch() - startupStart);
            firstFrameDelivered = true;

            QMetaObject::invokeMethod(this, "handleDecodedFrame", Qt::QueuedConnection,
                                      Q_ARG(QImage, image.copy()),
                                      Q_ARG(int, startupDelay));
            av_frame_unref(frame);
        }
    }

    if (swsContext) {
        sws_freeContext(swsContext);
    }
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
}
