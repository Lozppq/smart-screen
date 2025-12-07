#include "AudioCode.h"
#include <QDebug>
#include <QDateTime>
#include <cinttypes>

AudioCode::AudioCode(QObject *parent)
    : QThread(parent)
    , m_formatContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_swrContext(nullptr)
    , m_audioStreamIndex(-1)
    , m_packet(nullptr)
    , m_audioFrame(nullptr)
    , m_audioBuffer(nullptr)
    , m_audioBufferSize(0)
    , m_filterGraph(nullptr)
    , m_buffersrcCtx(nullptr)
    , m_buffersinkCtx(nullptr)
    , m_playbackSpeed(1.0f)
    , m_isOpened(false)
    , m_isPlaying(true)
{
    
}

AudioCode::~AudioCode()
{
    // 先停止线程并等待结束，避免 "Destroyed while thread is still running" 错误
    if (isRunning()) {
        setPlaying(false);
        wait();  // 等待线程结束
    }
    closeAudio();
}

void AudioCode::setPlaying(bool isPlaying)
{
    QMutexLocker locker(&m_mutex);
    m_isPlaying = isPlaying;
}

bool AudioCode::isPlaying()
{
    QMutexLocker locker(&m_mutex);
    return m_isPlaying;
}

void AudioCode::seekTo(int seconds)
{
    if (!m_isOpened || !m_formatContext || m_audioStreamIndex < 0) {
        qDebug() << "Cannot seek: audio not opened";
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
    if (m_audioCodecContext) {
        avcodec_flush_buffers(m_audioCodecContext);
    }

    // 重新初始化过滤器，完全清空所有缓冲区
    // 这是最彻底的方法，确保过滤器从干净状态开始
    if (m_filterGraph) {
        initAudioFilter(m_playbackSpeed);
    }

    // 清空音频数据队列，避免显示旧帧
    m_audioDataQueue.clear();

    qDebug() << "Seeked to position:" << seconds << "seconds";
}

bool AudioCode::openAudio(const QString &filePath)
{
    closeAudio(); // 先清理之前的资源

    // 打开文件
    if (avformat_open_input(&m_formatContext, filePath.toUtf8().constData(), nullptr, nullptr) < 0) {
        qDebug() << "Failed to open audio file:" << filePath;
        return false;
    }

    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        qDebug() << "Failed to find stream info";
        return false;
    }

    // 查找音频流
    m_audioStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (m_audioStreamIndex < 0) {
        qDebug() << "No audio stream found";
        return false;
    }

    m_audioStream = m_formatContext->streams[m_audioStreamIndex];
    AVCodecParameters *audioParams = m_formatContext->streams[m_audioStreamIndex]->codecpar;
    const AVCodec *audioCodec = avcodec_find_decoder(audioParams->codec_id);
    
    if (!audioCodec) {
        qDebug() << "Failed to find audio decoder";
        return false;
    }

    m_audioCodecContext = avcodec_alloc_context3(audioCodec);
    if (!m_audioCodecContext) {
        qDebug() << "Failed to allocate audio codec context";
        return false;
    }

    if (avcodec_parameters_to_context(m_audioCodecContext, audioParams) < 0) {
        qDebug() << "Failed to copy audio codec parameters";
        return false;
    }

    if (avcodec_open2(m_audioCodecContext, audioCodec, nullptr) < 0) {
        qDebug() << "Failed to open audio codec";
        return false;
    }
    
    // 初始化音频重采样 - 单声道
    // 使用新版本FFmpeg的API
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    m_swrContext = swr_alloc();
    if (!m_swrContext) {
        qDebug() << "Failed to allocate swr context";
        return false;
    }

    swr_alloc_set_opts2(&m_swrContext,
                    &out_ch_layout, AV_SAMPLE_FMT_S16, 44100,
                    &m_audioCodecContext->ch_layout, m_audioCodecContext->sample_fmt, m_audioCodecContext->sample_rate,
                    0, nullptr);
    
    if (swr_init(m_swrContext) < 0) {
        qDebug() << "Failed to initialize swr context";
        return false;
    }

    // 分配内存
    m_packet = av_packet_alloc();
    m_audioFrame = av_frame_alloc();

    if (!m_packet || !m_audioFrame) {
        qDebug() << "Failed to allocate packet or frame";
        return false;
    }

    m_isOpened = true;
    
    // 初始化音频过滤器（默认1.0倍速）
    initAudioFilter(1.0f);
    
    qDebug() << "Audio opened successfully";
    return true;
}

bool AudioCode::getNextFrame()
{
    if (!m_isOpened) return false;

    while (av_read_frame(m_formatContext, m_packet) >= 0) {
        if (m_packet->stream_index == m_audioStreamIndex) {
            // 解码音频
            if (avcodec_send_packet(m_audioCodecContext, m_packet) >= 0) {
                if (avcodec_receive_frame(m_audioCodecContext, m_audioFrame) == 0) {
                    AVFrame *frameToProcess = m_audioFrame;
                    AVFrame *filteredFrame = nullptr;
                    
                    // 如果启用了过滤器，应用倍速处理
                    if (m_filterGraph && m_playbackSpeed != 1.0f) {
                        // 将原始帧添加到过滤器输入
                        int ret = av_buffersrc_add_frame_flags(m_buffersrcCtx, m_audioFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
                        if (ret < 0) {
                            av_packet_unref(m_packet);
                            continue;
                        }
                        
                        // 从过滤器输出获取处理后的帧
                        filteredFrame = av_frame_alloc();
                        if (!filteredFrame) {
                            av_packet_unref(m_packet);
                            continue;
                        }
                        
                        ret = av_buffersink_get_frame(m_buffersinkCtx, filteredFrame);
                        if (ret < 0) {
                            av_frame_free(&filteredFrame);
                            av_packet_unref(m_packet);
                            continue;
                        }
                        frameToProcess = filteredFrame;
                    }
                    
                    // 重采样音频 - 单声道
                    int outSamples = av_rescale_rnd(frameToProcess->nb_samples, 44100, m_audioCodecContext->sample_rate, AV_ROUND_UP);
                    int outBufferSize = av_samples_get_buffer_size(nullptr, 1, outSamples, AV_SAMPLE_FMT_S16, 1);
                    
                    if (m_audioBufferSize < outBufferSize) {
                        av_free(m_audioBuffer);
                        m_audioBuffer = (uint8_t*)av_malloc(outBufferSize);
                        m_audioBufferSize = outBufferSize;
                    }

                    uint8_t *outData[1] = {m_audioBuffer};
                    int outSamplesActual = swr_convert(m_swrContext, outData, outSamples, 
                                                     (const uint8_t**)frameToProcess->data, frameToProcess->nb_samples);

                    if (outSamplesActual > 0) {
                        // 单声道：样本数 × 1通道 × 2字节/样本
                        QByteArray data((char*)m_audioBuffer, outSamplesActual * 1 * 2);
                        
                        AudioData audioData;
                        audioData.audioData = data;
                        if (frameToProcess->pts != AV_NOPTS_VALUE) {
                            int64_t pts = frameToProcess->pts;
                            audioData.timestamp = av_rescale_q(pts, 
                                                               m_audioStream->time_base, 
                                                               AV_TIME_BASE_Q) / 1000; // 转换为毫秒
                        } else {
                            audioData.timestamp = 0;
                        }
                        m_audioDataQueue.push(audioData);
                        
                        // 释放过滤后的帧
                        if (filteredFrame) {
                            av_frame_free(&filteredFrame);
                        }
                        
                        av_packet_unref(m_packet);
                        return true; // 音频帧解码成功，返回true
                    }
                    
                    // 清理过滤后的帧
                    if (filteredFrame) {
                        av_frame_free(&filteredFrame);
                    }
                }
            }
        }
        av_packet_unref(m_packet);
    }

    return false; // 文件结束
}

void AudioCode::run()
{
    qDebug() << "AudioCode run";
    setPlaying(true);
    while (true) {
        if (!m_isOpened) {
            break;
        }
        if (!isPlaying()) {
            break;
        }
        if (m_audioDataQueue.size() >= MAX_AUDIO_BUFFER_SIZE) {
            msleep(1);
            continue;
        }
        if (!getNextFrame()) {
            break;
        }
    }
    qDebug() << "AudioCode run end";
    // 注意：暂停时不要closeAudio，只有停止时才关闭
}

void AudioCode::closeAudio()
{
    cleanup();
    m_isOpened = false;
}

void AudioCode::cleanup()
{
    // 释放过滤器资源
    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
        m_buffersrcCtx = nullptr;
        m_buffersinkCtx = nullptr;
    }
    
    if (m_swrContext) {
        swr_free(&m_swrContext);
    }
    
    if (m_audioBuffer) {
        av_free(m_audioBuffer);
        m_audioBuffer = nullptr;
    }
    
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    
    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }
    
    if (m_audioCodecContext) {
        avcodec_free_context(&m_audioCodecContext);
    }
    
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }
    
    m_audioBufferSize = 0;
}

bool AudioCode::initAudioFilter(float speed)
{
    // 限制速度范围：atempo 过滤器支持 0.5 到 2.0
    if (speed < 0.5f || speed > 2.0f) {
        qDebug() << "Speed must be between 0.5 and 2.0, clamping:" << speed;
        speed = qBound(0.5f, speed, 2.0f);
    }
    
    m_playbackSpeed = speed;
    
    // 如果速度是1.0，不需要过滤器
    if (qAbs(speed - 1.0f) < 0.01f) {
        if (m_filterGraph) {
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
            m_buffersrcCtx = nullptr;
            m_buffersinkCtx = nullptr;
        }
        return true;
    }
    
    // 清理旧的过滤器
    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
        m_buffersrcCtx = nullptr;
        m_buffersinkCtx = nullptr;
    }
    
    // 创建过滤器图
    m_filterGraph = avfilter_graph_alloc();
    if (!m_filterGraph) {
        qDebug() << "Failed to allocate filter graph";
        return false;
    }
    
    // 获取输入和输出过滤器
    const AVFilter *buffersrc = avfilter_get_by_name("abuffer");
    const AVFilter *buffersink = avfilter_get_by_name("abuffersink");
    const AVFilter *atempo = avfilter_get_by_name("atempo");
    
    if (!buffersrc || !buffersink || !atempo) {
        qDebug() << "Failed to get filters";
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    // 创建输入过滤器上下文 (abuffer)
    char args[512];
    AVChannelLayout ch_layout = m_audioCodecContext->ch_layout;
    int sample_rate = m_audioCodecContext->sample_rate;
    AVSampleFormat sample_fmt = m_audioCodecContext->sample_fmt;
    
    // 获取 channel layout mask
    uint64_t channel_mask = 0;
    bool has_channel_mask = false;
    
    if (ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
        channel_mask = ch_layout.u.mask;
        has_channel_mask = true;
    } else {
        // 根据通道数使用默认布局
        if (ch_layout.nb_channels == 1) {
            channel_mask = AV_CH_LAYOUT_MONO;
            has_channel_mask = true;
        } else if (ch_layout.nb_channels == 2) {
            channel_mask = AV_CH_LAYOUT_STEREO;
            has_channel_mask = true;
        }
        // 对于其他通道数，如果没有mask，只使用channels参数
    }
    
    // 构建 abuffer 参数
    if (has_channel_mask) {
        // 包含 channel_layout
        snprintf(args, sizeof(args),
                 "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channels=%d:channel_layout=0x%" PRIx64,
                 sample_rate, sample_rate,
                 av_get_sample_fmt_name(sample_fmt),
                 ch_layout.nb_channels,
                 channel_mask);
    } else {
        // 只使用channels参数（对于不常见的通道配置）
        qDebug() << "Warning: Cannot determine channel layout, using channels only";
        snprintf(args, sizeof(args),
                 "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channels=%d",
                 sample_rate, sample_rate,
                 av_get_sample_fmt_name(sample_fmt),
                 ch_layout.nb_channels);
    }
    
    int ret = avfilter_graph_create_filter(&m_buffersrcCtx, buffersrc, "in",
                                           args, nullptr, m_filterGraph);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        qDebug() << "Failed to create buffer source:" << errbuf;
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    // 创建 atempo 过滤器
    snprintf(args, sizeof(args), "tempo=%.2f", speed);
    AVFilterContext *atempoCtx = nullptr;
    ret = avfilter_graph_create_filter(&atempoCtx, atempo, "atempo",
                                       args, nullptr, m_filterGraph);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        qDebug() << "Failed to create atempo filter:" << errbuf;
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    // 创建输出过滤器上下文 (abuffersink)
    ret = avfilter_graph_create_filter(&m_buffersinkCtx, buffersink, "out",
                                       nullptr, nullptr, m_filterGraph);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        qDebug() << "Failed to create buffer sink:" << errbuf;
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    // 连接过滤器: abuffer -> atempo -> abuffersink
    ret = avfilter_link(m_buffersrcCtx, 0, atempoCtx, 0);
    if (ret < 0) {
        qDebug() << "Failed to link buffersrc to atempo";
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    ret = avfilter_link(atempoCtx, 0, m_buffersinkCtx, 0);
    if (ret < 0) {
        qDebug() << "Failed to link atempo to buffersink";
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    // 配置过滤器图
    ret = avfilter_graph_config(m_filterGraph, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        qDebug() << "Failed to configure filter graph:" << errbuf;
        avfilter_graph_free(&m_filterGraph);
        return false;
    }
    
    return true;
}

void AudioCode::setPlaybackSpeed(float speed)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_isOpened) {
        m_playbackSpeed = speed;
        return;
    }
    
    // 重新初始化过滤器
    if (!initAudioFilter(speed)) {
        qDebug() << "Failed to set playback speed to:" << speed;
    }
}
