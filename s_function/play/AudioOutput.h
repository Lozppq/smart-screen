#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QMutex>
#include <QAudioOutput>
#include <QIODevice>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include "../models/SPSCLockFreeQueue.h"

#define AUDIO_OUTPUT_MAX_QUEUE 10
#define AUDIO_OUTPUT_MAX_QUEUE_SIZE 1024 * 8 // 1024个20ms音频数据大小
#define AUDIO_MAX_SIZE 1024 * 1024 * 512 // 64MB

class AudioOutput : public QThread
{
    Q_OBJECT

public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput();

    static AudioOutput *getInstance();

    // 初始化音频输出格式
    bool initialize(int sampleRate = 44100, int channelCount = 1, int sampleSize = 16);

    // 添加threadId到播放队列，返回队列索引
    int addThreadIdToPlayQueue(qintptr threadId);

    // 移除对列索引对应的threadId
    void removeThreadIdFromPlayQueue(int queueIndex);

    // 添加音频数据到队列
    void addAudioDataToQueue(int threadId, QByteArray audioData);

    // 带音量控制的混合（音量范围0.0-1.0）
    QByteArray mixAudioDataWithVolume(const QByteArray &data1, float volume1,
        const QByteArray &data2, float volume2);

signals:
    void playbackFinished();

private:
    void run() override;
    void cleanup();

    // 音频混合，平均算法，不考虑音量，仅限类内使用
    QByteArray mixAudioData(int queueSize, int minSize);
    
    // 音频格式
    QAudioFormat m_audioFormat;
    QAudioOutput *m_audioOutput;
    QIODevice *m_audioOutputDevice;
    QAudioDeviceInfo m_outputDevice;
    
    // 音频数据队列
    QByteArray m_audioDataList;
    // 等待混音对列
    QByteArray m_waitMixAudioData[AUDIO_OUTPUT_MAX_QUEUE];
    bool m_initialized;

    // 20ms音频数据大小
    int m_20msAudioDataSize;
    // 音频数据队列数组
    SPSCLockFreeQueue<QByteArray, AUDIO_OUTPUT_MAX_QUEUE_SIZE> m_audioDataQueue[AUDIO_OUTPUT_MAX_QUEUE];
    // 播放队列（使用 qintptr 可以安全存储线程 ID，无论是 int 还是指针）
    qintptr m_playQueue[AUDIO_OUTPUT_MAX_QUEUE];
    // 播放队列互斥锁
    QMutex m_playQueueMutex;
};

#endif // AUDIOOUTPUT_H
