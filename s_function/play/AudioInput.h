#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QAudioInput>
#include <QIODevice>
#include <QAudioFormat>
#include <QAudioDeviceInfo>

class AudioInput : public QThread
{
    Q_OBJECT

public:
    explicit AudioInput(QObject *parent = nullptr);
    ~AudioInput();

    static AudioInput *getInstance();

    // 初始化音频输入格式
    bool initialize(int sampleRate = 16000, int channelCount = 1, int sampleSize = 16);

signals:
    // 音频数据采集信号
    void audioDataReady(QByteArray audioData);

private:
    void run() override;
    void cleanup();
    
    // 音频格式
    QAudioFormat m_audioFormat;
    QAudioInput *m_audioInput;
    QIODevice *m_audioInputDevice;
    QAudioDeviceInfo m_inputDevice;
    
    bool m_initialized;
};

#endif // AUDIOINPUT_H
