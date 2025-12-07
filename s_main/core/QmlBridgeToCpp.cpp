#include "QmlBridgeToCpp.h"

QmlBridgeToCpp::QmlBridgeToCpp(QObject *parent)
    : QObject(parent)
{
}

QmlBridgeToCpp::~QmlBridgeToCpp()
{

}

QmlBridgeToCpp *QmlBridgeToCpp::getInstance()
{
    static QmlBridgeToCpp instance;
    return &instance;
}

void QmlBridgeToCpp::sendMessageToCpp(int appId, int messageId, const QVariant &data)
{
    qDebug() << "sendMessageToCpp: " << appId << " " << messageId << " " << data;
    AppId id = static_cast<AppId>(appId);
    if (m_messageMap.contains(id) && m_messageMap[id]) {
        m_messageMap[id]->receiveMessageFromQml(appId, messageId, data);
    } else {
        qDebug() << "Warning: No module registered for AppId:" << appId;
    }
}

void QmlBridgeToCpp::sendMessageToQml(int appId, int messageId, const QVariant &data)
{
    emit sendMessageToQmlSignal(appId, messageId, data);
}   

void QmlBridgeToCpp::addModule(AppId appId, BaseModelCtrl *module)
{
    m_messageMap[appId] = module;
}
