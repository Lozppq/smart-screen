#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>
#include <QQmlContext>
#include <QObject>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDebug>
#include "QmlBridgeToCpp.h"
#include "../../s_function/play/VideoFrame.h"
#include "../video/VideoFunction.h"
#include "../music/MusicFunction.h"
#include "../../s_function/play/AudioOutput.h"
#include "../../s_function/audioSynthetic/EspeakTTS.h"
#include "../../s_function/audioSynthetic/EkhoTTS.h"

int main(int argc, char *argv[])
{
    // 创建应用实例
    QGuiApplication app(argc, argv);

    // 创建QML引擎
    QQmlApplicationEngine engine;
    
    // 添加QML模块导入路径（从资源文件导入）
    engine.addImportPath("qrc:/UI");

    AudioOutput::getInstance()->initialize();
    EkhoTTS::getInstance()->initialize();

    // 在主线程中获取队列索引（使用主线程ID）
    // qintptr mainThreadId = reinterpret_cast<qintptr>(QThread::currentThreadId());
    // int queueIndex = AudioOutput::getInstance()->addThreadIdToPlayQueue(mainThreadId);
    
    // EkhoTTS::getInstance()->addTextToQueue("你好，我是小爱同学，很高兴认识你，今天天气不错，是个好天气，你好，我是小爱同学，很高兴认识你，今天天气不错，是个好天气，你好，我是小爱同学，很高兴认识你，今天天气不错，是个好天气，你好，我是小爱同学，很高兴认识你，今天天气不错，是个好天气，你好，我是小爱同学，很高兴认识你，今天天气不错，是个好天气");

    // if (queueIndex >= 0) {
    //     // 使用 ekho 可执行文件生成语音，直接从标准输出读取 PCM 数据
    //     QString text = "你好，我是小爱同学，很高兴认识你";
    //     QByteArray audioData = EkhoTTS::getInstance()->synthesize(text);
    //     if (!audioData.isEmpty()) {
    //         qDebug() << "成功从标准输出读取 PCM 数据，大小:" << audioData.size() << "字节";
    //         // 添加音频数据到队列
    //         AudioOutput::getInstance()->addAudioDataToQueue(queueIndex, audioData);
    //     } else {
    //         qDebug() << "标准输出数据为空";
    //     }
    // } else {
    //     qDebug() << "无法获取音频队列索引";
    // }

    VideoFunction videoFunction;
    MusicFunction musicFunction;
    
    QmlBridgeToCpp::getInstance()->addModule(VIDEO_APP_ID, &videoFunction);
    QmlBridgeToCpp::getInstance()->addModule(MUSIC_APP_ID, &musicFunction);
    
    // 在加载QML之前注册全局属性
    engine.rootContext()->setContextProperty("QmlBridgeToCpp", QmlBridgeToCpp::getInstance());
    engine.rootContext()->setContextProperty("VideoFunction", &videoFunction);
    
    // 注册VideoFrame为QML类型，当做控件来使用
    qmlRegisterType<VideoFrame>("VideoFrame", 1, 0, "VideoFrame");
    
    const QUrl mainWindowUrl(QStringLiteral("qrc:/UI/MainWindow/MainWindow.qml"));

    // 加载QML文件
    engine.load(mainWindowUrl);

    // 检查是否加载成功
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    // 步骤 2：获取主窗口的 QML 实例（关键）
    QObject *mainWindowInstance = nullptr;
    // 等待引擎加载完成，再获取实例（避免加载未完成导致空指针）
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [&](QObject *obj, const QUrl &objUrl) {
                         if (obj && objUrl == mainWindowUrl) {
                             // 确认加载的是主窗口，赋值给实例指针
                             mainWindowInstance = obj;
                             // 将主窗口实例暴露为全局属性 "MainWindow"（QML 中可直接用）
                             engine.rootContext()->setContextProperty("MainWindow", mainWindowInstance);
                         }
                         if (!obj && mainWindowUrl == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    

    // 进入事件循环
    return app.exec();
}
