#ifndef VIDEOCODE_H
#define VIDEOCODE_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QMutex>
#include "../models/SPSCLockFreeQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#define MAX_VIDEO_BUFFER_SIZE 64 

struct VideoData
{
    QImage image;
    qint64 timestamp;

    // 默认构造函数
    VideoData() : image(QImage()), timestamp(-1) {}
};

class VideoCode : public QThread
{
    Q_OBJECT

public:
    explicit VideoCode(QObject *parent = nullptr);
    ~VideoCode();

    bool openVideo(const QString &filePath);
    void closeVideo();
    bool getNextFrame();

    void setPlaying(bool isPlaying);
    bool isPlaying();

    double getVideoFps() const { return m_videoFps; }
    int getWidth() const { return m_videoCodecContext ? m_videoCodecContext->width : 0; }
    int getHeight() const { return m_videoCodecContext ? m_videoCodecContext->height : 0; }
    int getDuration() const;  // 返回总时长（秒）

    // 跳转到指定时间，单位为秒
    void seekTo(int seconds);

    SPSCLockFreeQueue<VideoData, MAX_VIDEO_BUFFER_SIZE> &getVideoDataQueue() { return m_videoDataQueue; }


private:
    void run() override;
    void cleanup();
    
    // FFmpeg核心变量
    AVFormatContext *m_formatContext;
    AVCodecContext *m_videoCodecContext;
    SwsContext *m_swsContext;
    
    int m_videoStreamIndex;
    
    AVPacket *m_packet;
    AVFrame *m_videoFrame;
    AVStream *m_videoStream;
    bool m_isOpened;

    // 视频帧率
    double m_videoFps;
    
    // 播放状态
    bool m_isPlaying;
    QMutex m_mutex;

    SPSCLockFreeQueue<VideoData, MAX_VIDEO_BUFFER_SIZE> m_videoDataQueue;
};

#endif // VIDEOCODE_H
