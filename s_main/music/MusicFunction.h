#ifndef MUSICFUNCTION_H
#define MUSICFUNCTION_H

#include <QObject>
#include <QVariant>
#include "../../s_function/models/BaseModelCtrl.h"

class MusicFunction : public BaseModelCtrl
{
    Q_OBJECT

public:
    explicit MusicFunction(QObject *parent = nullptr);
    virtual ~MusicFunction() override = default;

    // 重写基类的纯虚函数
    void receiveMessageFromQml(int appId, int messageId, const QVariant &data) override;
};

#endif // MUSICFUNCTION_H

