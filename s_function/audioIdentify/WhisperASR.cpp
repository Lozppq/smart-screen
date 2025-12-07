#include "WhisperASR.h"
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <cstring>
#include <thread>
#include <algorithm>

#include <QAudioInput>
#include <QIODevice>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>

static QMutex whisperMutex;  // 保护Whisper的多线程访问

WhisperASR::WhisperASR(QObject *parent)
    : QThread(parent)
    , m_initialized(false)
    , m_context(nullptr)
    , m_verbose(false)
    , m_minResultLength(1)
{
}

WhisperASR *WhisperASR::getInstance()
{
    static WhisperASR instance;
    return &instance;
}

WhisperASR::~WhisperASR()
{
    cleanup();
}

void WhisperASR::addAudioData(const QByteArray &audioData)
{
    QMutexLocker locker(&m_audioDataListMutex);
    m_audioDataList.append(audioData);
}

bool WhisperASR::initialize(const QString &modelPath, const QString &language)
{
    if (m_initialized) {
        qDebug() << "WhisperASR already initialized";
        return true;
    }

    // 加载模型
    QMutexLocker locker(&whisperMutex);
    
    qDebug() << "Loading Whisper model from:" << modelPath;
    
    // 使用whisper_init_from_file_with_params加载模型
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // 使用CPU，如果需要GPU可以设置为true
    
    m_context = whisper_init_from_file_with_params(modelPath.toUtf8().constData(), cparams);
    
    if (m_context == nullptr) {
        qDebug() << "Failed to load Whisper model";
        return false;
    }

    // 配置参数
    m_params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    m_params.print_progress = m_verbose;
    m_params.print_special = m_verbose;
    m_params.print_realtime = false;
    m_params.print_timestamps = m_verbose;
    
    // 设置语言
    if (language == "zh") {
        m_params.language = strdup("zh");
        m_params.translate = false;  // 中文不需要翻译
        
        // 设置初始提示词，引导模型输出简体中文（而不是繁体中文）
        // 使用简体中文常见词汇作为提示，帮助模型识别为简体中文模式
        const char *simplified_chinese_prompt = "你好，这是一个使用简体中文的语音识别系统。";
        m_params.initial_prompt = strdup(simplified_chinese_prompt);
        
        qDebug() << "Setting language to Simplified Chinese (简体中文)";
        qDebug() << "Initial prompt set to guide simplified Chinese output";
    } else if (language == "auto") {
        m_params.language = nullptr;  // 自动检测
        m_params.initial_prompt = nullptr;
        qDebug() << "Auto-detecting language";
    } else {
        m_params.language = strdup(language.toUtf8().constData());
        m_params.initial_prompt = nullptr;
        qDebug() << "Setting language to:" << language;
    }
    
    // 设置线程数
    m_params.n_threads = std::min(4, (int)std::thread::hardware_concurrency());
    
    m_params.offset_ms = 0;
    m_params.duration_ms = 0;
    
    // 开启温度采样以提升中文识别效果
    m_params.temperature = 0.0f;
    m_params.temperature_inc = 0.2f;
    
    m_initialized = true;
    qDebug() << "WhisperASR initialized successfully with" << m_params.n_threads << "threads";
    return true;
}

void WhisperASR::setVerbose(bool verbose)
{
    m_verbose = verbose;
    if (m_initialized) {
        m_params.print_progress = verbose;
        m_params.print_special = verbose;
        m_params.print_realtime = false;
        m_params.print_timestamps = verbose;
    }
}

void WhisperASR::run()
{
    // 1. 创建音频格式
    QAudioFormat format;
    format.setSampleRate(16000);        // 采样率
    format.setChannelCount(1);          // 单声道
    format.setSampleType(QAudioFormat::SignedInt);  // Qt5使用setSampleType
    format.setSampleSize(16);           // 16位
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec("audio/pcm");

    // 2. 选择音频设备
    QAudioDeviceInfo inputDevice = QAudioDeviceInfo::defaultInputDevice();
    if (inputDevice.isNull()) {
        qDebug() << "No audio input device available!";
        return;
    }
    
    // 3. 检查格式支持
    if (!inputDevice.isFormatSupported(format)) {
        format = inputDevice.preferredFormat();  // 使用设备推荐的格式
        qDebug() << "Using device preferred format, sample rate:" << format.sampleRate();
    } else {
        qDebug() << "Using custom format, sample rate:" << format.sampleRate();
    }

    // 4. 创建 QAudioInput 并启动
    // 注意：不能传入this作为父对象，因为this在主线程，而run()在子线程中执行
    QAudioInput *audioInput = new QAudioInput(format, nullptr);
    if (!audioInput) {
        qDebug() << "Failed to create QAudioInput!";
        return;
    }

    // 设置缓冲区大小（16kHz, 16bit, 单声道 = 2字节/样本，20ms = 16000*0.02*2 = 640字节）
    int bufferSize = format.sampleRate() * format.sampleSize() / 8 * format.channelCount() / 50; // 20ms缓冲区
    audioInput->setBufferSize(bufferSize);
    qDebug() << "Buffer size set to:" << bufferSize << "bytes";

    QIODevice *audioDevice = audioInput->start();
    if (!audioDevice) {
        qDebug() << "Failed to start audio input device!";
        delete audioInput;
        return;
    }

    qDebug() << "Input device:" << inputDevice.deviceName();
    qDebug() << "Initial audio input state:" << audioInput->state();

    initialize("/mnt/hgfs/share/demo1/thirdParty/whisper/models/ggml-tiny-q5_1.bin", "zh");
    
    // 简单方法：在子线程中运行事件循环，让 QAudioInput 能正常激活
    QEventLoop loop;
    QTimer *readTimer = new QTimer(nullptr);
    readTimer->setInterval(10); // 每10ms读取一次
    
    // 连接定时器，在定时器触发时读取音频数据
    QObject::connect(readTimer, &QTimer::timeout, [this, audioInput, audioDevice, format, readTimer, &loop]() {
        if (isInterruptionRequested()) {
            readTimer->stop();
            loop.quit();
            return;
        }
        
        QByteArray data = audioDevice->readAll();
        if (data.size() > 0) {
            static QByteArray audioData;
            static bool is_speaking = false;

            // 检测是否有人在说话
            bool speaking = isSpeaking(data);
            if (speaking) {
                qDebug() << "检测到语音，音频数据大小:" << data.size() << "字节, 状态:" << audioInput->state();
                is_speaking = true;
            }
            else if(audioData.size() > 0)
            {   
                static int count = 0;
                if (count < 10) {
                    count++;
                }
                else{
                    // 将音频数据添加到队列
                    QMutexLocker locker(&m_audioDataListMutex);
                    m_audioDataList.append(audioData);
                    audioData.clear();
                    count = 0;
                    is_speaking = false;
                }
            }

            if (is_speaking) {
                audioData.append(data);
            }
        }
        
        // 处理队列中的音频数据（只在检测到说话时才处理）
        if (!m_audioDataList.isEmpty()) {
            QByteArray audioData;
            {
                QMutexLocker locker(&m_audioDataListMutex);
                audioData = m_audioDataList;
                m_audioDataList.clear();
            }
            
            if (!audioData.isEmpty()) {
                // 检测整个音频片段是否包含说话内容
                bool hasSpeech = isSpeaking(audioData);
                if (hasSpeech) {
                    qDebug() << "检测到语音，开始识别处理...";
                    processAudio(audioData, format.sampleRate());
                } else {
                    // 静音片段，不处理（可选：记录日志）
                    // qDebug() << "静音片段，跳过处理";
                }
            }
        }
    });
    
    readTimer->start();
    
    // 运行事件循环（直到线程被中断）
    loop.exec();
    
    readTimer->stop();
    delete readTimer;

    qDebug() << "Stopping audio input...";
    audioDevice->close();
    audioInput->stop();
    delete audioInput;
}

bool WhisperASR::isSpeaking(const QByteArray &audioData, double threshold)
{
    if (audioData.isEmpty()) {
        return false;
    }
    
    // 将PCM16字节数据转换为int16数组
    int sampleCount = audioData.size() / 2;  // PCM16 = 2 bytes per sample
    if (sampleCount == 0) {
        return false;
    }
    
    const int16_t *samples = reinterpret_cast<const int16_t*>(audioData.constData());
    
    // 计算RMS（均方根）值
    double sumSquares = 0.0;
    for (int i = 0; i < sampleCount; i++) {
        double sample = static_cast<double>(samples[i]) / 32768.0;  // 归一化到[-1, 1]
        sumSquares += sample * sample;
    }
    
    double rms = sqrt(sumSquares / sampleCount);
    
    // 转换为分贝值 (dB)
    // 避免log(0)的情况
    if (rms < 1e-10) {
        return false;
    }
    
    double db = 20.0 * log10(rms);
    
    // 如果分贝值大于阈值，认为有人在说话
    bool speaking = db > threshold;
    
    // 可选：输出调试信息（可以注释掉以降低日志量）
    // qDebug() << "VAD检测 - RMS:" << rms << ", dB:" << db << ", 是否说话:" << speaking;
    
    return speaking;
}

QString WhisperASR::processAudio(const QByteArray &audioData, int sampleRate)
{
    if (!m_initialized || m_context == nullptr || audioData.isEmpty()) {
        qDebug() << "WhisperASR not initialized or empty audio data";
        return QString();
    }

    // 将PCM16字节数据转换为int16数组
    int sampleCount = audioData.size() / 2;  // PCM16 = 2 bytes per sample
    if (sampleCount == 0) {
        return QString();
    }
    
    const int16_t *samples = reinterpret_cast<const int16_t*>(audioData.constData());
    return processPCM16ToFloat(samples, sampleCount, sampleRate);
}

QString WhisperASR::processFloatAudio(const float *audioData, int len, int sampleRate)
{
    if (!m_initialized || m_context == nullptr || audioData == nullptr || len == 0) {
        return QString();
    }

    // 检查是否需要重采样
    if (sampleRate != 16000) {
        qDebug() << "Resampling from" << sampleRate << "Hz to 16000Hz";
        QByteArray resampledData = resampleTo16kHz(audioData, len, sampleRate);
        const float *newData = reinterpret_cast<const float*>(resampledData.constData());
        int newLen = resampledData.size() / sizeof(float);
        return processFloatAudio(newData, newLen, 16000);
    }

    QMutexLocker locker(&whisperMutex);
    
    // 运行推理
    int result = whisper_full(m_context, m_params, audioData, len);
    
    if (result != 0) {
        qDebug() << "Whisper processing failed with code:" << result;
        return QString();
    }

    // 提取结果
    QString resultText;
    int numSegments = whisper_full_n_segments(m_context);
    
    for (int i = 0; i < numSegments; i++) {
        const char *text = whisper_full_get_segment_text(m_context, i);
        if (text) {
            resultText += QString::fromUtf8(text);
        }
    }
    
    resultText = resultText.trimmed();
    
    // 发出信号
    if (!resultText.isEmpty() && resultText.length() >= m_minResultLength) {
        qDebug() << "Recognized text:" << resultText;
        emit textRecognized(resultText);
    }
    
    return resultText;
}

QString WhisperASR::processPCM16ToFloat(const int16_t *audioData, int samples, int sampleRate)
{
    if (!m_initialized || m_context == nullptr || audioData == nullptr || samples == 0) {
        return QString();
    }

    // 将PCM16转换为float数组
    float *audioFloat = new float[samples];
    
    for (int i = 0; i < samples; i++) {
        audioFloat[i] = static_cast<float>(audioData[i]) / 32768.0f;
    }
    
    QString result = processFloatAudio(audioFloat, samples, sampleRate);
    delete[] audioFloat;
    return result;
}

QByteArray WhisperASR::resampleTo16kHz(const float *audioData, int samples, int originalSampleRate)
{
    // 简单的线性重采样实现
    if (originalSampleRate == 16000) {
        QByteArray result(samples * sizeof(float), 0);
        memcpy(result.data(), audioData, samples * sizeof(float));
        return result;
    }
    
    int targetSamples = samples * 16000 / originalSampleRate;
    QByteArray result(targetSamples * sizeof(float), 0);
    float *output = reinterpret_cast<float*>(result.data());
    
    for (int i = 0; i < targetSamples; i++) {
        double srcIndex = i * static_cast<double>(originalSampleRate) / 16000.0;
        int srcIdx = static_cast<int>(srcIndex);
        double frac = srcIndex - srcIdx;
        
        if (srcIdx < samples - 1) {
            output[i] = audioData[srcIdx] * (1.0 - frac) + audioData[srcIdx + 1] * frac;
        } else if (srcIdx < samples) {
            output[i] = audioData[srcIdx];
        } else {
            output[i] = 0.0f;
        }
    }
    
    return result;
}

void WhisperASR::cleanup()
{
    if (m_context) {
        QMutexLocker locker(&whisperMutex);
        whisper_free(m_context);
        m_context = nullptr;
    }
    
    // 释放strdup分配的语言字符串
    if (m_params.language != nullptr) {
        free(const_cast<char*>(m_params.language));
        m_params.language = nullptr;
    }
    
    // 释放strdup分配的initial_prompt字符串
    if (m_params.initial_prompt != nullptr) {
        free(const_cast<char*>(m_params.initial_prompt));
        m_params.initial_prompt = nullptr;
    }
    
    m_initialized = false;
    qDebug() << "WhisperASR cleaned up";
}
