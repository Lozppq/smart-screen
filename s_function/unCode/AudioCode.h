#ifndef AUDIOCODE_H
#define AUDIOCODE_H

#include <QObject>
#include <QByteArray>
#include <QThread>
#include <QMutex>
#include "../models/SPSCLockFreeQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#define MAX_AUDIO_BUFFER_SIZE 128

struct AudioData
{
    QByteArray audioData;
    qint64 timestamp;

    // 默认构造函数
    AudioData() : audioData(QByteArray()), timestamp(-1) {}
};

class AudioCode : public QThread
{
    Q_OBJECT

public:
    explicit AudioCode(QObject *parent = nullptr);
    ~AudioCode();

    bool openAudio(const QString &filePath);
    void closeAudio();
    bool getNextFrame();

    void setPlaying(bool isPlaying);
    bool isPlaying();

    // 跳转到指定时间，单位为秒
    void seekTo(int seconds);

    // 设置播放速度倍率 (0.5-2.0)
    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const { return m_playbackSpeed; }

    SPSCLockFreeQueue<AudioData, MAX_AUDIO_BUFFER_SIZE> &getAudioDataQueue() { return m_audioDataQueue; }

private:
    void run() override;
    void cleanup();
    bool initAudioFilter(float speed);
    
    // FFmpeg核心变量
    AVFormatContext *m_formatContext;
    AVCodecContext *m_audioCodecContext;
    SwrContext *m_swrContext;
    
    int m_audioStreamIndex;
    
    AVPacket *m_packet;
    AVFrame *m_audioFrame;
    AVStream *m_audioStream;
    
    uint8_t *m_audioBuffer;
    int m_audioBufferSize;
    
    // 音频过滤器相关
    AVFilterGraph *m_filterGraph;
    AVFilterContext *m_buffersrcCtx;
    AVFilterContext *m_buffersinkCtx;
    float m_playbackSpeed;
    
    bool m_isOpened;

    // 播放状态
    bool m_isPlaying;
    QMutex m_mutex;

    SPSCLockFreeQueue<AudioData, MAX_AUDIO_BUFFER_SIZE> m_audioDataQueue;
};

#endif // AUDIOCODE_H
