#include "AudioInput.h"
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

AudioInput::AudioInput(QObject *parent)
    : QThread(parent)
    , m_audioInput(nullptr)
    , m_audioInputDevice(nullptr)
    , m_initialized(false)
{
}

AudioInput::~AudioInput()
{
    cleanup();
}

AudioInput *AudioInput::getInstance()
{
    static AudioInput instance;
    return &instance;
}

bool AudioInput::initialize(int sampleRate, int channelCount, int sampleSize)
{
    if (m_initialized) {
        qDebug() << "AudioInput already initialized";
        return true;
    }

    // 1. 创建音频格式
    m_audioFormat.setSampleRate(sampleRate);
    m_audioFormat.setChannelCount(channelCount);
    m_audioFormat.setSampleSize(sampleSize);
    m_audioFormat.setCodec("audio/pcm");
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    // 2. 选择音频输入设备
    m_inputDevice = QAudioDeviceInfo::defaultInputDevice();
    if (m_inputDevice.isNull()) {
        qDebug() << "No audio input device available!";
        return false;
    }

    // 3. 检查格式支持
    if (!m_inputDevice.isFormatSupported(m_audioFormat)) {
        m_audioFormat = m_inputDevice.preferredFormat();  // 使用设备推荐的格式
        qDebug() << "Using device preferred format, sample rate:" << m_audioFormat.sampleRate()
                 << ", channels:" << m_audioFormat.channelCount();
    } else {
        qDebug() << "Using custom format, sample rate:" << m_audioFormat.sampleRate()
                 << ", channels:" << m_audioFormat.channelCount();
    }

    m_initialized = true;
    start();
    return true;
}

void AudioInput::run()
{
    if (!m_initialized) {
        qDebug() << "AudioInput not initialized, initializing with default settings...";
        if (!initialize()) {
            qDebug() << "Failed to initialize AudioInput!";
            return;
        }
    }

    // 创建 QAudioInput（注意：不能传入this作为父对象，因为this在主线程，而run()在子线程中执行）
    QAudioInput *audioInput = new QAudioInput(m_audioFormat, nullptr);
    if (!audioInput) {
        qDebug() << "Failed to create QAudioInput!";
        return;
    }

    // 设置缓冲区大小（16kHz, 16bit, 单声道 = 2字节/样本，20ms = 16000*0.02*2 = 640字节）
    int bufferSize = m_audioFormat.sampleRate() * m_audioFormat.sampleSize() / 8 * m_audioFormat.channelCount() / 50; // 20ms缓冲区
    audioInput->setBufferSize(bufferSize);
    qDebug() << "Audio input buffer size set to:" << bufferSize << "bytes";

    QIODevice *audioInputDevice = audioInput->start();
    if (!audioInputDevice) {
        qDebug() << "Failed to start audio input device!";
        delete audioInput;
        return;
    }

    qDebug() << "Input device:" << m_inputDevice.deviceName();
    qDebug() << "Initial audio input state:" << audioInput->state();

    // 使用事件循环方式运行（参考 WhisperASR）
    QEventLoop loop;
    QTimer *readTimer = new QTimer(nullptr);
    readTimer->setInterval(10); // 每10ms读取一次音频数据

    // 连接定时器，在定时器触发时读取音频数据
    QObject::connect(readTimer, &QTimer::timeout, [this, audioInput, audioInputDevice, readTimer, &loop]() {
        if (isInterruptionRequested()) {
            readTimer->stop();
            loop.quit();
            return;
        }

        // 从输入设备读取音频数据
        QByteArray data = audioInputDevice->readAll();
        if (data.size() > 0) {
            // 发出信号，将采集到的音频数据发送出去
            emit audioDataReady(data);
        }
    });

    readTimer->start();

    // 运行事件循环（直到线程被中断）
    loop.exec();

    readTimer->stop();
    delete readTimer;

    qDebug() << "Stopping audio input...";
    if (audioInputDevice) {
        audioInputDevice->close();
    }
    audioInput->stop();
    delete audioInput;
}

void AudioInput::cleanup()
{
    if (m_audioInputDevice) {
        m_audioInputDevice = nullptr;
    }

    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }

    m_initialized = false;
}
