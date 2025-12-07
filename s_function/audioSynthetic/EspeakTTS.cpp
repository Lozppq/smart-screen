#include "EspeakTTS.h"
#include <QMutexLocker>
#include <QDebug>
#include <QFileInfo>
#include <QFile>

EspeakTTS::EspeakTTS()
    : m_synthesisComplete(false)
    , m_espeakSampleRate(0)
    , m_initialized(false)
    , m_swrContext(nullptr)
{
}

EspeakTTS::~EspeakTTS()
{
    cleanupResampler();
}

EspeakTTS *EspeakTTS::getInstance()
{
    static EspeakTTS instance;
    return &instance;
}

bool EspeakTTS::initialize(const QString &voice)
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) {
        qDebug() << "EspeakTTS 已经初始化";
        return true;
    }

    // 初始化 espeak-ng，使用 RETRIEVAL 模式以获取音频数据
    int sampleRate = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 0, VOICE_DIR, 0);
    if (sampleRate < 0) {
        qDebug() << "初始化 eSpeak NG 失败";
        return false;
    }

    m_espeakSampleRate = sampleRate;
    qDebug() << "eSpeak NG 初始化成功，采样率:" << m_espeakSampleRate;

    // 设置回调函数
    espeak_SetSynthCallback(synthCallback);

    // 设置语音（eSpeak NG 的中文语音代码是 "cmn"，不是 "zh"）
    QString voiceName = voice.isEmpty() ? "cmn" : voice;
    espeak_ERROR voiceResult = espeak_SetVoiceByName(voiceName.toUtf8().constData());
    
    if (voiceResult != EE_OK) {
        qDebug() << "设置语音" << voiceName << "失败，错误码:" << voiceResult;
        // 尝试使用 "cmn" 作为备选
        if (voiceName != "cmn" && espeak_SetVoiceByName("cmn") == EE_OK) {
            qDebug() << "使用 cmn 语音成功";
        }
    } else {
        qDebug() << "设置语音" << voiceName << "成功";
    }

    // 如果采样率不是 44100Hz，初始化重采样器
    if (m_espeakSampleRate != 44100) {
        if (!initializeResampler()) {
            qDebug() << "初始化重采样器失败";
            return false;
        }
        qDebug() << "重采样器已初始化，将从" << m_espeakSampleRate << "Hz 重采样到 44100Hz";
    } else {
        qDebug() << "采样率已经是 44100Hz，无需重采样";
    }

    m_initialized = true;
    return true;
}

QByteArray EspeakTTS::synthesize(const QString &text)
{
    // 检查初始化和文本（不需要锁，因为 m_initialized 在初始化后不会改变）
    if (!m_initialized) {
        qDebug() << "EspeakTTS 未初始化";
        return QByteArray();
    }

    if (text.isEmpty()) {
        qDebug() << "文本为空";
        return QByteArray();
    }

    // 准备合成（在锁内清空缓冲区）
    {
        QMutexLocker locker(&m_mutex);
        m_audioBuffer.clear();
        m_synthesisComplete = false;
    }

    // 将文本转换为 UTF-8（几乎不会失败，移除冗余检查）
    QByteArray utf8Text = text.toUtf8();

    // 调用 espeak_Synth 进行合成（必须在锁外调用，让回调能执行）
    espeak_ERROR result = espeak_Synth(
        utf8Text.constData(),
        utf8Text.size() + 1,  // 包含结束符
        0,                     // position
        POS_CHARACTER,         // position_type
        0,                     // end_position
        espeakCHARS_UTF8,      // flags
        nullptr,               // unique_identifier
        nullptr                // user_data
    );

    if (result != EE_OK) {
        qDebug() << "espeak_Synth 调用失败，错误码:" << result;
        return QByteArray();
    }

    // 等待合成完成（必须在锁外调用）
    espeak_Synchronize();

    // 使用条件变量等待回调完成（更高效，最多等待 10 秒）
    QMutexLocker locker(&m_mutex);
    if (!m_synthesisComplete) {
        m_synthesisCondition.wait(&m_mutex, 10000);  // 10秒超时
    }

    // 检查是否超时且没有数据
    if (!m_synthesisComplete && m_audioBuffer.isEmpty()) {
        qDebug() << "合成超时且未获取到音频数据";
        return QByteArray();
    }

    // 直接返回音频数据（已经在回调中重采样为 44100Hz）
    QByteArray outputData = m_audioBuffer;
    locker.unlock();

    return outputData;
}

int EspeakTTS::synthCallback(short *wav, int numsamples, espeak_EVENT *events)
{
    // 直接使用单例实例，避免冗余的静态成员变量
    EspeakTTS *instance = getInstance();
    if (instance) {
        return instance->onSynthCallback(wav, numsamples, events);
    }
    return 0;
}

int EspeakTTS::onSynthCallback(short *wav, int numsamples, espeak_EVENT *events)
{
    // 如果有音频数据，先进行实时重采样（在锁外进行，避免阻塞）
    QByteArray audioDataToAppend;
    if (wav != nullptr && numsamples > 0) {
        if (m_swrContext != nullptr) {
            // 需要重采样
            audioDataToAppend = resampleChunk(wav, numsamples);
            if (audioDataToAppend.isEmpty()) {
                qDebug() << "重采样失败，跳过此块数据";
            }
        } else {
            // 采样率已经是 44100Hz，直接使用
            audioDataToAppend = QByteArray(reinterpret_cast<const char *>(wav), numsamples * sizeof(short));
        }
    }

    // 获取锁，追加数据并检查事件
    QMutexLocker locker(&m_mutex);
    
    // 追加音频数据
    if (!audioDataToAppend.isEmpty()) {
        m_audioBuffer.append(audioDataToAppend);
    }

    // 检查事件，判断合成是否完成
    bool shouldNotify = false;
    if (events != nullptr) {
        for (espeak_EVENT *event = events; event->type != 0; event++) {
            if (event->type == espeakEVENT_MSG_TERMINATED) {
                m_synthesisComplete = true;
                shouldNotify = true;
                break;
            }
        }
    } else if (wav == nullptr) {
        // wav 为 nullptr 表示合成完成
        m_synthesisComplete = true;
        shouldNotify = true;
    }

    // 如果合成完成，唤醒等待的线程
    if (shouldNotify) {
        m_synthesisCondition.wakeAll();
    }

    return 0;  // 返回 0 表示继续合成
}

bool EspeakTTS::initializeResampler()
{
    if (m_swrContext != nullptr) {
        cleanupResampler();
    }

    // 创建重采样上下文
    m_swrContext = swr_alloc();
    if (!m_swrContext) {
        qDebug() << "Failed to allocate swr context";
        return false;
    }

    // 设置输入格式：单声道，16bit，原始采样率
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;

    int ret = swr_alloc_set_opts2(&m_swrContext,
                                  &out_ch_layout, AV_SAMPLE_FMT_S16, 44100,  // 输出：44100Hz, 16bit, 单声道
                                  &in_ch_layout, AV_SAMPLE_FMT_S16, m_espeakSampleRate,  // 输入：原始采样率, 16bit, 单声道
                                  0, nullptr);

    if (ret < 0) {
        qDebug() << "Failed to set swr options";
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
        return false;
    }

    if (swr_init(m_swrContext) < 0) {
        qDebug() << "Failed to initialize swr context";
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
        return false;
    }

    return true;
}

void EspeakTTS::cleanupResampler()
{
    if (m_swrContext != nullptr) {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
    }
}

QByteArray EspeakTTS::resampleChunk(const short *inputData, int inputSamples)
{
    if (m_swrContext == nullptr || inputData == nullptr || inputSamples <= 0) {
        return QByteArray();
    }

    // 计算输出样本数
    int64_t outSamples = av_rescale_rnd(inputSamples, 44100, m_espeakSampleRate, AV_ROUND_UP);

    // 分配输出缓冲区
    int outBufferSize = av_samples_get_buffer_size(nullptr, 1, outSamples, AV_SAMPLE_FMT_S16, 1);
    uint8_t *outBuffer = (uint8_t*)av_malloc(outBufferSize);
    if (!outBuffer) {
        qDebug() << "Failed to allocate output buffer";
        return QByteArray();
    }

    // 准备输入数据指针
    const uint8_t *inData[1] = { (const uint8_t*)inputData };
    uint8_t *outData[1] = { outBuffer };

    // 执行重采样
    int outSamplesActual = swr_convert(m_swrContext, outData, outSamples, inData, inputSamples);

    QByteArray result;
    if (outSamplesActual > 0) {
        // 复制重采样后的数据
        int outputBytes = outSamplesActual * sizeof(int16_t);
        result = QByteArray((const char*)outBuffer, outputBytes);
    }

    // 清理资源
    av_free(outBuffer);

    return result;
}
