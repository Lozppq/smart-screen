#include "VideoRender.h"
#include "../play/AudioOutput.h"
#include <QDateTime>
#include <QDebug>

VideoRender::VideoRender(QObject *parent)
    : QThread(parent)
    , m_videoCode(new VideoCode(this))
    , m_audioCode(new AudioCode(this))
    , m_isPlaying(false)
    , m_mutex()
    , m_videoDataQueue(m_videoCode->getVideoDataQueue())  // 引用必须在初始化列表中初始化
    , m_audioDataQueue(m_audioCode->getAudioDataQueue())  // 引用必须在初始化列表中初始化
    , m_playQueueIndex(-1)
    , m_lastTimestamp(0)
    , m_currentTimestamp(0)
    , m_lastPlayTimestamp(0)
    , m_frameInterval(0)
    , m_playSpeed(1.0f)
{
    
}

VideoRender::~VideoRender()
{
    // 先停止线程
    if (isRunning()) {
        setPlaying(false);
        wait();  // 等待线程结束
    }
    cleanup();
}

void VideoRender::cleanup()
{
    if (m_videoCode) {
        delete m_videoCode;
        m_videoCode = nullptr;
    }
    if (m_audioCode) {
        delete m_audioCode;
        m_audioCode = nullptr;
    }
}

int VideoRender::getWidth() const
{
    return m_videoCode->getWidth();
}

int VideoRender::getHeight() const
{
    return m_videoCode->getHeight();
}

int VideoRender::getDuration() const
{
    return m_currentTimestamp.load() / 1000;
}

int VideoRender::getTotalDuration() const
{
    if (m_videoCode) {
        return m_videoCode->getDuration();
    }
    return 0;
}

bool VideoRender::isPlaying()
{
    QMutexLocker locker(&m_mutex);
    return m_isPlaying;
}

void VideoRender::setPlaying(bool isPlaying)
{
    QMutexLocker locker(&m_mutex);
    m_isPlaying = isPlaying;
}

void VideoRender::seekTo(int seconds)
{
    setPlaying(false);
    // 同时跳转视频和音频，确保同步
    if (m_videoCode) {
        m_videoCode->seekTo(seconds);
    }
    if (m_audioCode) {
        m_audioCode->seekTo(seconds);
    }
    // 重置时间戳，避免显示错误的时间
    m_lastTimestamp.store(0);
    m_lastPlayTimestamp = 0;
    m_currentTimestamp.store(0);
}

void VideoRender::setPlaySpeed(float speed)
{
    m_playSpeed.store(speed);
    if (m_audioCode) {
        m_audioCode->setPlaybackSpeed(speed);
    }
}

bool VideoRender::openVideo(const QString &filePath)
{
    if(m_videoFilePath == filePath) {
        return true;
    }
    m_videoFilePath = filePath;
    if (!m_videoCode->openVideo(filePath)) {
        qDebug() << "Failed to open video file:" << filePath;
        return false;
    }
    if (!m_audioCode->openAudio(filePath)) {
        qDebug() << "Failed to open audio file:" << filePath;
        return false;
    }
    return true;
}

void VideoRender::closeVideo()
{
    setPlaying(false);
    m_videoFilePath.clear();
    m_lastTimestamp.store(0);
    m_lastPlayTimestamp = 0;
    m_frameInterval = 0;
    m_videoDataQueue.clear();
    m_audioDataQueue.clear();
    m_currentTimestamp.store(0);
}

void VideoRender::run()
{
    m_playQueueIndex = AudioOutput::getInstance()->addThreadIdToPlayQueue(reinterpret_cast<qintptr>(QThread::currentThreadId()));
    
    // 检查队列索引是否有效
    if (m_playQueueIndex < 0) {
        qDebug() << "Failed to add thread to play queue, queue is full!";
        return;
    }
    
    m_videoCode->start();
    m_audioCode->start();
    setPlaying(true);
    AudioData audioData;
    VideoData videoData;
    qDebug() << "VideoRender run " << m_playQueueIndex;
    
    while (true) {
        if (!isPlaying()) {
            break;
        }

        if(m_videoDataQueue.front(videoData))
        {
            // 以音频时间戳为基准同步
            if(videoData.timestamp <= m_currentTimestamp.load())
            {
                emit videoFrameReady(videoData.image);
                m_videoDataQueue.pop(videoData);
            }
        }

        qint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
        if(currentTimestamp - m_lastPlayTimestamp < m_frameInterval) {
            msleep(1);  // 避免CPU占用过高
            continue;
        }

        // 以音频数据时间戳为基准
        if(m_audioDataQueue.pop(audioData))
        {
            AudioOutput::getInstance()->addAudioDataToQueue(m_playQueueIndex, audioData.audioData);
            if(audioData.timestamp - m_lastTimestamp.load() >= 0 && m_lastTimestamp.load() != 0)
            {
                m_frameInterval = audioData.timestamp - m_lastTimestamp.load();
            }
            qint64 temp = m_currentTimestamp.load();
            if(temp == 0){
                m_currentTimestamp.store(audioData.timestamp);
            }else{
                m_currentTimestamp.store(temp + (int)(m_playSpeed.load() * m_frameInterval));
            }
            m_lastTimestamp.store(audioData.timestamp);
            m_lastPlayTimestamp = currentTimestamp;
        }
    }
    
    qDebug() << "VideoRender run end";
    m_videoCode->setPlaying(false);
    m_audioCode->setPlaying(false);
    if (m_playQueueIndex >= 0) {
        AudioOutput::getInstance()->removeThreadIdFromPlayQueue(m_playQueueIndex);
    }
}