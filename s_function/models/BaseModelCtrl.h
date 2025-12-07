#ifndef BASEMODELCTRL_H
#define BASEMODELCTRL_H

#include <QObject>
#include <QVariant>

class BaseModelCtrl : public QObject
{
    Q_OBJECT
public:
    // 纯虚析构函数，确保派生类正确析构
    virtual ~BaseModelCtrl() = default;
    
    // 纯虚函数，子类必须实现
    virtual void receiveMessageFromQml(int appId, int messageId, const QVariant &data) = 0;

protected:
    // 受保护的构造函数，只能被派生类调用
    BaseModelCtrl(QObject *parent = nullptr) : QObject(parent) {}
};

#endif // BASEMODELCTRL_H
