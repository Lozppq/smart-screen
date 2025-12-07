// Copyright (c) 2025
// SPDX-License-Identifier: MIT

#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <QImage>
#include <QQuickPaintedItem>
#include "VideoRender.h"

class VideoFrame : public QQuickPaintedItem
{
    Q_OBJECT

public:
    explicit VideoFrame(QQuickItem *parent = nullptr);
    ~VideoFrame();

    void paint(QPainter *painter) override;

    QImage currentImage() const;

    Q_INVOKABLE void setPlaying(bool isPlaying);
    Q_INVOKABLE bool openVideo(const QString &filePath);
    Q_INVOKABLE void closeVideo();
    Q_INVOKABLE int getWidth() const;
    Q_INVOKABLE int getHeight() const;
    Q_INVOKABLE int getDuration() const;
    Q_INVOKABLE int getTotalDuration() const;
    Q_INVOKABLE void seekTo(int seconds);
    Q_INVOKABLE void setPlaySpeed(float speed);

signals:
    void currentImageChanged();

public slots:
    void setCurrentImage(const QImage &image);

private:
    QImage m_currentImage;
    VideoRender *m_videoRender;
};

#endif // VIDEOFRAME_H