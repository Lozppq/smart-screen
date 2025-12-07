#ifndef ESPEAKTTS_H
#define ESPEAKTTS_H

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

extern "C" {
#include <espeak-ng/speak_lib.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

#define VOICE_DIR "/mnt/hgfs/share/smart-screen/thirdParty/espeak-ng" // 语音包路径

/**
 * @brief EspeakTTS 类用于通过文本合成语音音频数据
 * 
 * 使用 espeak-ng 库实现文本到语音的转换，返回 PCM 音频数据
 */
class EspeakTTS
{
public:
    static EspeakTTS *getInstance();

    /**
     * @brief 初始化 espeak-ng 引擎
     * @param voice 语音名称，默认为 "cmn"（中文）
     * @return 初始化成功返回 true，失败返回 false
     */
    bool initialize(const QString &voice = QStringLiteral("cmn"));

    /**
     * @brief 将文本转换为语音音频数据（同步调用）
     * @param text 要合成的文本
     * @return 返回 PCM 音频数据（44100Hz, 16bit, 单声道），失败返回空 QByteArray
     */
    QByteArray synthesize(const QString &text);

private:
    explicit EspeakTTS();
    ~EspeakTTS();

    // 禁止拷贝
    EspeakTTS(const EspeakTTS &) = delete;
    EspeakTTS &operator=(const EspeakTTS &) = delete;

    /**
     * @brief espeak-ng 回调函数，用于接收音频数据
     */
    static int synthCallback(short *wav, int numsamples, espeak_EVENT *events);

    /**
     * @brief 实例回调函数（非静态成员函数包装）
     */
    int onSynthCallback(short *wav, int numsamples, espeak_EVENT *events);

    /**
     * @brief 初始化重采样上下文（如果采样率不是 44100Hz）
     */
    bool initializeResampler();

    /**
     * @brief 清理重采样上下文
     */
    void cleanupResampler();

    /**
     * @brief 实时重采样音频数据到 44100Hz
     * @param inputData 输入音频数据（原始采样率）
     * @param inputSamples 输入样本数
     * @return 重采样后的音频数据（44100Hz），失败返回空 QByteArray
     */
    QByteArray resampleChunk(const short *inputData, int inputSamples);

    QMutex m_mutex;
    QWaitCondition m_synthesisCondition;  // 用于等待合成完成
    QByteArray m_audioBuffer;  // 存储合成的音频数据（44100Hz）
    bool m_synthesisComplete;  // 合成是否完成
    int m_espeakSampleRate;    // eSpeak NG 的原始采样率
    bool m_initialized;         // 是否已初始化
    SwrContext *m_swrContext;  // 重采样上下文（如果采样率不是 44100Hz）
};

#endif // ESPEAKTTS_H
