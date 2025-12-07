#include "AudioOutput.h"
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>

AudioOutput::AudioOutput(QObject *parent)
    : QThread(parent)
    , m_audioOutput(nullptr)
    , m_audioOutputDevice(nullptr)
    , m_initialized(false)
    , m_20msAudioDataSize(0)
{
    
}

AudioOutput *AudioOutput::getInstance()
{
    static AudioOutput instance;
    return &instance;
}

AudioOutput::~AudioOutput()
{
    cleanup();
}

int AudioOutput::addThreadIdToPlayQueue(qintptr threadId)
{
    QMutexLocker locker(&m_playQueueMutex);
    qDebug() << "addThreadIdToPlayQueue threadId:" << threadId;
    int index = -1;
    for (int i = 0; i < AUDIO_OUTPUT_MAX_QUEUE; i++) {
        if (m_playQueue[i] == threadId) {
            qDebug() << "addThreadIdToPlayQueue index:" << i;
            return i;
        }
        else if (m_playQueue[i] == 0) {
            index = i;
        }
    }
    if (index >= 0) {
        m_playQueue[index] = threadId;
    }
    qDebug() << "addThreadIdToPlayQueue index:" << index;
    return index;
}

void AudioOutput::removeThreadIdFromPlayQueue(int queueIndex)
{
    QMutexLocker locker(&m_playQueueMutex);
    m_playQueue[queueIndex % AUDIO_OUTPUT_MAX_QUEUE] = 0;
}

void AudioOutput::addAudioDataToQueue(int queueIndex, QByteArray audioData)
{
    // 如果一包大于20ms音频数据大小，则拆分成多个20ms音频数据
    int maxSize = m_20msAudioDataSize;
    if (audioData.size() > maxSize) {
        int offset = 0;
        int totalSize = audioData.size();
        
        // 拆分完整的数据包
        while (offset + maxSize <= totalSize) {
            QByteArray packet = audioData.mid(offset, maxSize);
            m_audioDataQueue[queueIndex].push(packet);
            offset += maxSize;
        }
        
        // 处理剩余的不完整数据包（如果有）
        if (offset < totalSize) {
            QByteArray remainingPacket = audioData.mid(offset);
            m_audioDataQueue[queueIndex].push(remainingPacket);
        }
    }
    else {
        // 小于等于20ms的数据包直接添加
        m_audioDataQueue[queueIndex].push(audioData);
    }
}

// 音频混合，平均算法，不考虑音量，仅限类内使用
QByteArray AudioOutput::mixAudioData(int queueSize, int minSize)
{
    QByteArray mixed(minSize, 0);
    int16_t *dst = (int16_t*)mixed.data();
    int maxSampleCount = minSize / sizeof(int16_t);
    for(int i = 0; i < AUDIO_OUTPUT_MAX_QUEUE; ++i){
        if(!m_waitMixAudioData[i].isEmpty()){
            const int16_t *src = (const int16_t*)m_waitMixAudioData[i].constData();
            for(int j = 0; j < maxSampleCount; ++j){
                dst[j] += (int16_t)(src[j] / queueSize);
            }
            m_waitMixAudioData[i].remove(0, minSize);
        }
    }

    return mixed;
}

// 音频混合，带音量控制，可供外部调用
QByteArray AudioOutput::mixAudioDataWithVolume(const QByteArray &data1, float volume1,
    const QByteArray &data2, float volume2)
{
    int size1 = data1.size();
    int size2 = data2.size();
    int maxSize = qMax(size1, size2);

    QByteArray mixed(maxSize, 0);

    const int16_t *src1 = (const int16_t*)data1.constData();
    const int16_t *src2 = (const int16_t*)data2.constData();
    int16_t *dst = (int16_t*)mixed.data();

    int sampleCount1 = size1 / sizeof(int16_t);
    int sampleCount2 = size2 / sizeof(int16_t);
    int maxSampleCount = maxSize / sizeof(int16_t);

    for (int i = 0; i < maxSampleCount; i++) {
        float mixedValue = 0.0f;

        // 如果data1还有数据，加上它（应用音量）
        if (i < sampleCount1) {
            mixedValue += (float)src1[i] * volume1;
        }

        // 如果data2还有数据，加上它（应用音量）
        if (i < sampleCount2) {
            mixedValue += (float)src2[i] * volume2;
        }

        // 限制范围
        if (mixedValue > 32767.0f){
            mixedValue = 32767.0f;
        }
        if (mixedValue < -32768.0f){
            mixedValue = -32768.0f;
        }

        dst[i] = (int16_t)mixedValue;
    }

    return mixed;
}

bool AudioOutput::initialize(int sampleRate, int channelCount, int sampleSize)
{
    if (m_initialized) {
        qDebug() << "AudioOutput already initialized";
        return true;
    }

    // 1. 创建音频格式
    m_audioFormat.setSampleRate(sampleRate);
    m_audioFormat.setChannelCount(channelCount);
    m_audioFormat.setSampleSize(sampleSize);
    m_audioFormat.setCodec("audio/pcm");
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    // 2. 选择音频输出设备
    m_outputDevice = QAudioDeviceInfo::defaultOutputDevice();
    if (m_outputDevice.isNull()) {
        qDebug() << "No audio output device available!";
        return false;
    }

    // 3. 检查格式支持
    if (!m_outputDevice.isFormatSupported(m_audioFormat)) {
        m_audioFormat = m_outputDevice.preferredFormat();  // 使用设备推荐的格式
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

void AudioOutput::run()
{
    if (!m_initialized) {
        qDebug() << "AudioOutput not initialized, initializing with default settings...";
        if (!initialize()) {
            qDebug() << "Failed to initialize AudioOutput!";
            return;
        }
    }

    // 创建 QAudioOutput（注意：不能传入this作为父对象，因为this在主线程，而run()在子线程中执行）
    QAudioOutput *audioOutput = new QAudioOutput(m_audioFormat, nullptr);
    if (!audioOutput) {
        qDebug() << "Failed to create QAudioOutput!";
        return;
    }

    // 设置缓冲区大小（44100Hz, 16bit, 单声道 = 2字节/样本，20ms = 44100*0.02*2 = 1764字节）
    m_20msAudioDataSize = m_audioFormat.sampleRate() * m_audioFormat.sampleSize() / 8 * m_audioFormat.channelCount() / 50; // 20ms缓冲区
    audioOutput->setBufferSize(m_20msAudioDataSize * 4);
    qDebug() << "Audio output buffer size set to:" << m_20msAudioDataSize << "bytes";

    QIODevice *audioOutputDevice = audioOutput->start();
    if (!audioOutputDevice) {
        qDebug() << "Failed to start audio output device!";
        delete audioOutput;
        return;
    }

    qDebug() << "Output device:" << m_outputDevice.deviceName();
    qDebug() << "Initial audio output state:" << audioOutput->state();

    // 使用事件循环方式运行（参考 WhisperASR）
    QEventLoop loop;
    QTimer *writeTimer = new QTimer(nullptr);
    writeTimer->setInterval(20); // 每10ms检查一次是否有数据需要写入

    // 连接定时器，在定时器触发时写入音频数据
    QObject::connect(writeTimer, &QTimer::timeout, [this, audioOutput, audioOutputDevice, writeTimer, &loop]() {
        if (isInterruptionRequested()) {
            writeTimer->stop();
            loop.quit();
            return;
        }

        int minSize = AUDIO_MAX_SIZE, queueSize = 0;
        for(int i = 0; i < AUDIO_OUTPUT_MAX_QUEUE; ++i){
            if(!m_audioDataQueue[i].empty()){
                QByteArray temp;
                while (m_waitMixAudioData[i].size() < m_20msAudioDataSize && !m_audioDataQueue[i].empty()) {
                    m_audioDataQueue[i].pop(temp);
                    m_waitMixAudioData[i].append(temp);
                }
                minSize = qMin(minSize, m_waitMixAudioData[i].size());
                queueSize++;
            }
        }

        QByteArray audioData1;
        if(queueSize > 0){
            audioData1 = mixAudioData(queueSize, minSize);
        }

        if(audioData1.size() > 0) {
            m_audioDataList.append(audioData1);
        }
        
        if (!m_audioDataList.isEmpty() && audioOutputDevice) {
            // 写入20ms音频数据到输出设备
            QByteArray audioData = m_audioDataList.left(m_20msAudioDataSize);
            qint64 written = audioOutputDevice->write(audioData);
            m_audioDataList.remove(0, written);
        }
    });

    writeTimer->start();

    // 运行事件循环（直到线程被中断或停止播放）
    loop.exec();

    writeTimer->stop();
    delete writeTimer;

    qDebug() << "Stopping audio output...";
    if (audioOutputDevice) {
        audioOutputDevice->close();
    }
    audioOutput->stop();
    delete audioOutput;
}

void AudioOutput::cleanup()
{
    if (m_audioOutputDevice) {
        m_audioOutputDevice = nullptr;
    }

    if (m_audioOutput) {
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }

    m_audioDataList.clear();

    m_initialized = false;
}
