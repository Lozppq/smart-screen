#include "VideoFunction.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>
#include <QVariantMap>
#include <QString>
#include <QDateTime>

VideoFunction::VideoFunction(QObject *parent)
    : BaseModelCtrl(parent)
{
    loadCurrentPath();
    // 定时器更新视频列表每秒更新一次
    m_videoListTimer.setInterval(1000);
    connect(&m_videoListTimer, &QTimer::timeout, this, &VideoFunction::loadVideoList);
    m_videoListTimer.start();
}

void VideoFunction::receiveMessageFromQml(int appId, int messageId, const QVariant &data)
{
    qDebug() << "VideoFunction::receiveMessageFromQml - appId:" << appId 
             << ", messageId:" << messageId 
             << ", data:" << data;
    
    // 这里可以添加对 appId 的判断，确保是视频相关的消息
    // 然后调用私有函数处理具体的视频消息
    
}

QVariantList VideoFunction::getVideoList() const
{
    return m_videoList;
}

void VideoFunction::setVideoList(const QVariantList &videoList)
{
    m_videoList = videoList;
    emit videoListChanged();
}

QString VideoFunction::getCurrentPath() const
{
    return m_currentPath;
}

void VideoFunction::setCurrentPath(const QString &currentPath)
{
    m_currentPath = currentPath;
    emit currentPathChanged();
}

void VideoFunction::loadCurrentPath()
{
    QVariantList videoList;
    QString resourcePath = QCoreApplication::applicationDirPath() + "/../../resource";
    resourcePath = QDir::cleanPath(resourcePath);
    setCurrentPath(resourcePath);
}

bool VideoFunction::isDirectSubPath(const QString &currentPath, const QString &subPath)
{
    QStringList currentPathList = currentPath.split('/');
    QStringList subPathList = subPath.split('/');
    if(currentPathList.size() + 1 == subPathList.size()) {
        for(int i = 0; i < currentPathList.size(); i++) {
            if(currentPathList[i] != subPathList[i]) {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool VideoFunction::isVideoExist(const QString &videoPath)
{
    for(int i = 0; i < m_videoList.size(); i++) {
        QVariantMap videoMap = m_videoList[i].toMap();
        if(videoMap["path"].toString() == videoPath) {
            return true;
        }
    }
    return false;
}

void VideoFunction::sortVideoList(QVariantList &videoList)
{
    std::sort(videoList.begin(), videoList.end(), [](const QVariant &a, const QVariant &b) {
        QVariantMap itemA = a.toMap();
        QVariantMap itemB = b.toMap();
        if(itemA["isDir"].toBool() && !itemB["isDir"].toBool()) {
            return true;
        }
        else if(!itemA["isDir"].toBool() && itemB["isDir"].toBool()) {
            return false;
        }
        else {
            return itemA["name"].toString() < itemB["name"].toString();
        }
    });
}

void VideoFunction::loadVideoList()
{
    QString resourcePath = QCoreApplication::applicationDirPath() + "/../../resource";
    resourcePath = QDir::cleanPath(resourcePath);
    
    // 存储所有文件和文件夹信息
    QList<QFileInfo> fileInfoList;
    
    // 递归遍历所有文件和文件夹
    QDirIterator iterator(resourcePath, 
                          QDir::AllEntries | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    
    while (iterator.hasNext()) {
        iterator.next();
        QFileInfo fileInfo = iterator.fileInfo();
        fileInfoList.append(fileInfo);
    }

    // 支持的视频文件扩展名
    QStringList videoExtensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", 
        "webm", "m4v", "3gp", "mpg", "mpeg", "ts", 
        "m2ts", "vob", "asf", "rm", "rmvb"
    };

    QVariantList newVideoList;
    bool isChanged = false;
    for(int i = 0; i < fileInfoList.size(); i++) {
        QFileInfo fileInfo = fileInfoList[i];
        if(!fileInfo.isDir() && !videoExtensions.contains(fileInfo.suffix().toLower())) {
            continue;
        }
        QVariantMap videoMap;
        videoMap["path"] = fileInfo.absoluteFilePath();
        videoMap["name"] = fileInfo.fileName();
        videoMap["isDir"] = fileInfo.isDir();
        videoMap["size"] = fileInfo.size();
        videoMap["dateTime"] = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
        videoMap["duration"] = 0;
        newVideoList.append(videoMap);
        isChanged = true;
    }
    if(isChanged) {
        sortVideoList(newVideoList);
        setVideoList(newVideoList);
    }
}