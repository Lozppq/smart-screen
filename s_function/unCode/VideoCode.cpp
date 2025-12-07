#include "VideoCode.h"
#include <QDebug>
#include <QDateTime>

VideoCode::VideoCode(QObject *parent)
    : QThread(parent)
    , m_formatContext(nullptr)
    , m_videoCodecContext(nullptr)
    , m_swsContext(nullptr)
    , m_videoStreamIndex(-1)
    , m_packet(nullptr)
    , m_videoFrame(nullptr)
    , m_isOpened(false)
    , m_videoFps(0.0)
    , m_isPlaying(true)
{
    
}

VideoCode::~VideoCode()
{
    // 先停止线程并等待结束，避免 "Destroyed while thread is still running" 错误
    if (isRunning()) {
        setPlaying(false);
        wait();  // 等待线程结束
    }
    closeVideo();
}

void VideoCode::setPlaying(bool isPlaying)
{
    QMutexLocker locker(&m_mutex);
    m_isPlaying = isPlaying;
}

bool VideoCode::isPlaying()
{
    QMutexLocker locker(&m_mutex);
    return m_isPlaying;
}

int VideoCode::getDuration() const
{
    if (!m_formatContext || m_formatContext->duration == AV_NOPTS_VALUE) {
        return 0;
    }
    // duration 单位是 AV_TIME_BASE (通常是微秒)
    // 转换为秒：duration / AV_TIME_BASE
    return static_cast<int>(m_formatContext->duration / AV_TIME_BASE);
}

void VideoCode::seekTo(int seconds)
{
    if (!m_isOpened || !m_formatContext || m_videoStreamIndex < 0) {
        qDebug() << "Cannot seek: video not opened";
        return;
    }

    // 将秒数转换为 AV_TIME_BASE 单位的时间戳
    // AV_TIME_BASE 通常是 1000000 (微秒)
    int64_t timestamp = static_cast<int64_t>(seconds) * AV_TIME_BASE;
    
    // 使用 av_seek_frame 跳转到指定位置
    // 参数说明：
    // - m_formatContext: 格式上下文
    // - -1: 对所有流进行跳转（如果使用 -1，timestamp 必须是 AV_TIME_BASE 单位）
    // - timestamp: 目标时间戳
    // - AVSEEK_FLAG_BACKWARD: 向后查找最近的 keyframe（更安全）
    int ret = av_seek_frame(m_formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    
    if (ret < 0) {
        qDebug() << "Failed to seek to position:" << seconds << "seconds";
        return;
    }
    
    // 清空解码器缓冲区，确保从新位置开始解码
    if (m_videoCodecContext) {
        avcodec_flush_buffers(m_videoCodecContext);
    }
    
    // 清空视频数据队列，避免显示旧帧
    m_videoDataQueue.clear();
    
    qDebug() << "Seeked to position:" << seconds << "seconds";
}

bool VideoCode::openVideo(const QString &filePath)
{
    closeVideo(); // 先清理之前的资源

    // 打开文件
    if (avformat_open_input(&m_formatContext, filePath.toUtf8().constData(), nullptr, nullptr) < 0) {
        qDebug() << "Failed to open video file:" << filePath;
        return false;
    }

    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        qDebug() << "Failed to find stream info";
        return false;
    }

    // 查找视频流
    m_videoStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIndex < 0) {
        qDebug() << "No video stream found";
        return false;
    }

    m_videoStream = m_formatContext->streams[m_videoStreamIndex];
    AVCodecParameters *videoParams = m_formatContext->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *videoCodec = avcodec_find_decoder(videoParams->codec_id);
    
    if (!videoCodec) {
        qDebug() << "Failed to find video decoder";
        return false;
    }
    
    m_videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (!m_videoCodecContext) {
        qDebug() << "Failed to allocate video codec context";
        return false;
    }

    if (avcodec_parameters_to_context(m_videoCodecContext, videoParams) < 0) {
        qDebug() << "Failed to copy video codec parameters";
        return false;
    }

    if (avcodec_open2(m_videoCodecContext, videoCodec, nullptr) < 0) {
        qDebug() << "Failed to open video codec";
        return false;
    }
    
    // 初始化图像转换
    m_swsContext = sws_getContext(
        m_videoCodecContext->width, m_videoCodecContext->height, m_videoCodecContext->pix_fmt,
        m_videoCodecContext->width, m_videoCodecContext->height, AV_PIX_FMT_RGB24,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    if (!m_swsContext) {
        qDebug() << "Failed to initialize sws context";
        return false;
    }

    // 获取视频帧率
    m_videoFps = av_q2d(m_formatContext->streams[m_videoStreamIndex]->avg_frame_rate);
    if (m_videoFps <= 0) {
        // 如果无法获取帧率，使用默认值
        m_videoFps = 30.0;
    }
    qDebug() << "Video FPS: " << m_videoFps;
    qDebug() << "Video resolution: " << m_videoCodecContext->width << "x" << m_videoCodecContext->height;

    // 分配内存
    m_packet = av_packet_alloc();
    m_videoFrame = av_frame_alloc();

    if (!m_packet || !m_videoFrame) {
        qDebug() << "Failed to allocate packet or frame";
        return false;
    }

    m_isOpened = true;
    qDebug() << "Video opened successfully";
    return true;
}

bool VideoCode::getNextFrame()
{
    if (!m_isOpened) return false;

    while (av_read_frame(m_formatContext, m_packet) >= 0) {
        if (m_packet->stream_index == m_videoStreamIndex) {
            // 解码视频
            if (avcodec_send_packet(m_videoCodecContext, m_packet) >= 0) {
                if (avcodec_receive_frame(m_videoCodecContext, m_videoFrame) == 0) {
                    // 转换为RGB
                    uint8_t *rgbBuffer = new uint8_t[m_videoCodecContext->width * m_videoCodecContext->height * 3];
                    uint8_t *rgbData[4] = {rgbBuffer, nullptr, nullptr, nullptr};
                    int rgbLinesize[4] = {m_videoCodecContext->width * 3, 0, 0, 0};

                    sws_scale(m_swsContext, m_videoFrame->data, m_videoFrame->linesize,
                             0, m_videoCodecContext->height, rgbData, rgbLinesize);

                    // 创建QImage并立即复制数据，避免引用已释放的内存
                    QImage image(rgbBuffer, m_videoCodecContext->width, m_videoCodecContext->height, 
                               m_videoCodecContext->width * 3, QImage::Format_RGB888);
                    // 创建深拷贝，确保数据安全
                    VideoData videoData;
                    videoData.image = image.copy();
                    delete[] rgbBuffer;
                    if (m_videoFrame->pts != AV_NOPTS_VALUE) {
                        videoData.timestamp = av_rescale_q(m_videoFrame->pts, 
                                                           m_videoStream->time_base, 
                                                           AV_TIME_BASE_Q) / 1000; // 转换为毫秒
                    } else {
                        videoData.timestamp = 0;
                    }
                    m_videoDataQueue.push(videoData);
                    av_packet_unref(m_packet);
                    return true; // 视频帧解码成功，返回true
                }
            }
        }
        av_packet_unref(m_packet);
    }

    return false; // 文件结束
}

void VideoCode::run()
{
    qDebug() << "VideoCode run";
    setPlaying(true);
    while (true) {
        if (!m_isOpened) {
            break;
        }
        if (!isPlaying()) {
            break;
        }
        if (m_videoDataQueue.size() >= MAX_VIDEO_BUFFER_SIZE) {
            msleep(1);
            continue;
        }
        if (!getNextFrame()) {
            break;
        }
    }
    qDebug() << "VideoCode run end";
    // 注意：暂停时不要closeVideo，只有停止时才关闭
}

void VideoCode::closeVideo()
{
    cleanup();
    m_isOpened = false;
}

void VideoCode::cleanup()
{
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    
    if (m_videoFrame) {
        av_frame_free(&m_videoFrame);
    }
    
    if (m_videoCodecContext) {
        avcodec_free_context(&m_videoCodecContext);
    }
    
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }
    
    m_videoFps = 0.0;
}
