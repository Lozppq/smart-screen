#ifndef EKHOTTS_H
#define EKHOTTS_H

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QThread>
#include <QQueue>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mem.h>
#include <libavutil/mathematics.h>
}

#include <ekho.h>

/**
 * @brief EkhoTTS 类用于通过文本合成中文语音音频数据
 * 
 * 使用 ekho 库实现文本到语音的转换，返回 PCM 音频数据
 */
class EkhoTTS : public QThread
{
public:
    static EkhoTTS *getInstance();

    /**
     * @brief 初始化 ekho 引擎
     * @param voice 语音名称，默认为 "Mandarin"（中文普通话）
     * @return 初始化成功返回 true，失败返回 false
     */
    bool initialize(const QString &voice = QStringLiteral("Mandarin"));

    /**
     * @brief 将文本转换为中文语音音频数据（同步调用）
     * @param text 要合成的文本
     * @return 返回 PCM 音频数据（44100Hz, 16bit, 单声道），失败返回空 QByteArray
     */
    QByteArray synthesize(const QString &text);

    /**
     * @brief 将文本添加到队列中
     * @param text 要合成的文本
     */
    void addTextToQueue(const QString &text);

private:
    explicit EkhoTTS();
    ~EkhoTTS();

    void run() override;

    // 禁止拷贝
    EkhoTTS(const EkhoTTS &) = delete;
    EkhoTTS &operator=(const EkhoTTS &) = delete;

    /**
     * @brief 初始化重采样上下文（如果采样率不是 44100Hz）
     */
    bool initializeResampler();

    /**
     * @brief 清理重采样上下文
     */
    void cleanupResampler();

    /**
     * @brief 重采样音频数据到 44100Hz, 16bit, 单声道
     * @param inputData 输入音频数据（原始采样率）
     * @param inputSamples 输入样本数
     * @return 重采样后的音频数据（44100Hz, 16bit, 单声道），失败返回空 QByteArray
     */
    QByteArray resampleChunk(const short *inputData, int inputSamples);


    QMutex m_mutex;
    ekho::Ekho *m_ekho;  // ekho 引擎实例
    bool m_initialized;   // 是否已初始化
    int m_sampleRate;     // ekho 的原始采样率
    SwrContext *m_swrContext;  // 重采样上下文（如果采样率不是 44100Hz）
    // 一个文本队列，用于存储需要合成的文本
    QQueue<QString> m_textQueue;
    // 互斥锁，用于保护文本队列
    QMutex m_textQueueMutex;
    // 音频队列索引
    int m_playQueueIndex;
};

#endif // EKHOTTS_H

