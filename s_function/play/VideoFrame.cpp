// Copyright (c) 2025
// SPDX-License-Identifier: MIT

#include "VideoFrame.h"
#include "AudioOutput.h"
#include <QPainter>
#include <QDebug>

VideoFrame::VideoFrame(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    m_videoRender = new VideoRender(this);
    // 使用Qt::QueuedConnection确保跨线程安全
    connect(m_videoRender, &VideoRender::videoFrameReady, this, &VideoFrame::setCurrentImage, Qt::QueuedConnection);
}

VideoFrame::~VideoFrame()
{
    if (m_videoRender) {
        // 先停止播放
        m_videoRender->setPlaying(false);
        // 等待线程结束
        if (m_videoRender->isRunning()) {
            m_videoRender->wait();
        }
        delete m_videoRender;
        m_videoRender = nullptr;
    }
}

int VideoFrame::getWidth() const
{
    return m_videoRender->getWidth();
}

int VideoFrame::getHeight() const
{
    return m_videoRender->getHeight();
}

int VideoFrame::getDuration() const
{
    return m_videoRender->getDuration();
}

int VideoFrame::getTotalDuration() const
{
    return m_videoRender->getTotalDuration();
}

void VideoFrame::seekTo(int seconds)
{
    m_videoRender->seekTo(seconds);
}

void VideoFrame::setPlaySpeed(float speed)
{
    m_videoRender->setPlaySpeed(speed);
}

bool VideoFrame::openVideo(const QString &filePath)
{
    return m_videoRender->openVideo(filePath);
}

void VideoFrame::setPlaying(bool isPlaying)
{
    if(isPlaying)
    {
        m_videoRender->start();
    }
    else
    {
        m_videoRender->setPlaying(false);
    }
}

void VideoFrame::closeVideo()
{
    m_videoRender->closeVideo();
}

void VideoFrame::paint(QPainter *painter)
{
    if (m_currentImage.isNull()) {
        painter->fillRect(boundingRect(), Qt::black);
        return;
    }

    painter->drawImage(boundingRect(), m_currentImage);
}

QImage VideoFrame::currentImage() const
{
    return m_currentImage;
}

void VideoFrame::setCurrentImage(const QImage &image)
{
    if (m_currentImage.cacheKey() == image.cacheKey()) {
        return;
    }

    m_currentImage = image;
    emit currentImageChanged();
    update();
}

