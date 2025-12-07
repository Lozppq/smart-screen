#ifndef VIDEORENDER_H
#define VIDEORENDER_H

#include "../unCode/VideoCode.h"
#include "../unCode/AudioCode.h"
#include <atomic>

class VideoRender : public QThread
{
    Q_OBJECT
public:
    explicit VideoRender(QObject *parent = nullptr);
    ~VideoRender();

    bool isPlaying();
    void setPlaying(bool isPlaying);
    bool openVideo(const QString &filePath);
    void closeVideo();
    int getWidth() const;
    int getHeight() const;
    int getDuration() const;
    int getTotalDuration() const;  // 返回视频总时长（秒）

    // 跳转到指定时间，单位为秒
    void seekTo(int seconds);

    // 设置播放倍速
    void setPlaySpeed(float speed);

signals:
    void videoFrameReady(const QImage &image);

private:
    void run() override;
    void cleanup();

    VideoCode *m_videoCode;
    AudioCode *m_audioCode;

    bool m_isPlaying;
    QMutex m_mutex;
    // 队列引用，必须在初始化列表中初始化
    SPSCLockFreeQueue<VideoData, MAX_VIDEO_BUFFER_SIZE> &m_videoDataQueue;
    SPSCLockFreeQueue<AudioData, MAX_AUDIO_BUFFER_SIZE> &m_audioDataQueue;

    // 播放队列索引
    int m_playQueueIndex;
    // 视频文件路径
    QString m_videoFilePath;
    // 上一帧时间戳
    std::atomic<qint64> m_lastTimestamp;
    // 当前帧时间戳
    std::atomic<qint64> m_currentTimestamp;
    //上一帧播放时间戳
    qint64 m_lastPlayTimestamp;
    // 帧间间隔
    int m_frameInterval;
    // 播放倍速
    std::atomic<float> m_playSpeed;
};




#endif // VIDEORENDER_H