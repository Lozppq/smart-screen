#include "MusicFunction.h"
#include <QDebug>

MusicFunction::MusicFunction(QObject *parent)
    : BaseModelCtrl(parent)
{
    // 构造函数初始化
}

void MusicFunction::receiveMessageFromQml(int appId, int messageId, const QVariant &data)
{
    qDebug() << "MusicFunction::receiveMessageFromQml - appId:" << appId 
             << ", messageId:" << messageId 
             << ", data:" << data;
    
    // 这里可以添加对 appId 的判断，确保是音乐相关的消息
    // 然后调用私有函数处理具体的音乐消息
    
}

