#ifndef VIDEOFUNCTION_H
#define VIDEOFUNCTION_H

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QTimer>
#include "../../s_function/models/BaseModelCtrl.h"

class VideoFunction : public BaseModelCtrl
{
    Q_OBJECT
    Q_PROPERTY(QVariantList videoList READ getVideoList WRITE setVideoList NOTIFY videoListChanged)
    Q_PROPERTY(QString currentPath READ getCurrentPath WRITE setCurrentPath NOTIFY currentPathChanged)
public:
    explicit VideoFunction(QObject *parent = nullptr);
    virtual ~VideoFunction() override = default;

    // 重写基类的纯虚函数
    void receiveMessageFromQml(int appId, int messageId, const QVariant &data) override;

    // 获取视频列表
    void loadVideoList();
    QVariantList getVideoList() const;
    void setVideoList(const QVariantList &videoList);

    // 获取当前路径
    QString getCurrentPath() const;
    void setCurrentPath(const QString &currentPath);
    void loadCurrentPath();

    // 判断是否已经存在视频
    bool isVideoExist(const QString &videoPath);

    // 对当前列表排序
    void sortVideoList(QVariantList &videoList);

    // 判断是否是直接子路径
    Q_INVOKABLE bool isDirectSubPath(const QString &currentPath, const QString &subPath);

signals:
    void videoListChanged();
    void currentPathChanged();

private:
    QVariantList m_videoList;
    QString m_currentPath;
    QTimer m_videoListTimer;
};

#endif // VIDEOFUNCTION_H

