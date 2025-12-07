#ifndef QMLBRIDGETOCPP_H
#define QMLBRIDGETOCPP_H

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QDebug>
#include "../../s_function/models/BaseModelCtrl.h"

enum AppId
{
    // 应用ID枚举，可根据需要添加
    VIDEO_APP_ID = 1,
    MUSIC_APP_ID = 2,
    SETTING_APP_ID = 3,
    ADVERTISE_APP_ID = 4,
};

class QmlBridgeToCpp : public QObject
{
    Q_OBJECT
public:
    explicit QmlBridgeToCpp(QObject *parent = nullptr);
    ~QmlBridgeToCpp();

    static QmlBridgeToCpp *getInstance();
    void addModule(AppId appId, BaseModelCtrl *module);

    Q_INVOKABLE void sendMessageToCpp(int appId, int messageId, const QVariant &data);
    void sendMessageToQml(int appId, int messageId, const QVariant &data);

signals:
    void sendMessageToQmlSignal(int appId, int messageId, const QVariant &data);

private:
    QMap<AppId, BaseModelCtrl *> m_messageMap;
};

#endif // QMLBRIDGETOCPP_H
