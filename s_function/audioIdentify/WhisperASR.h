#ifndef WHISPERASR_H
#define WHISPERASR_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QThread>
#include <QMutex>

extern "C" {
#include <whisper.h>
}

/**
 * @brief WhisperASR - 独立的语音识别类
 * 
 * 使用OpenAI Whisper模型进行语音识别，支持中文识别
 * 
 * 使用方法：
 * @code
 * WhisperASR *asr = new WhisperASR();
 * asr->initialize("path/to/model.bin", "zh");
 * QString text = asr->processAudio(audioData);
 * @endcode
 */
class WhisperASR : public QThread
{
    Q_OBJECT

public:
    explicit WhisperASR(QObject *parent = nullptr);
    ~WhisperASR();

    static WhisperASR *getInstance();

    /**
     * @brief 初始化Whisper模型
     * @param modelPath 模型文件路径（相对或绝对路径）
     * @param language 语言代码，"zh"表示中文，"auto"表示自动检测
     * @return 成功返回true，失败返回false
     */
    bool initialize(const QString &modelPath, const QString &language = "zh");

    /**
     * @brief 处理PCM16格式音频数据进行语音识别
     * @param audioData PCM16格式的音频数据（单声道，16kHz采样率）
     * @param sampleRate 音频采样率，默认16kHz
     * @return 识别结果文本
     * @note 如果采样率不是16kHz，内部会进行重采样
     */
    QString processAudio(const QByteArray &audioData, int sampleRate = 16000);

    /**
     * @brief 处理音频数据进行语音识别（使用浮点数组）
     * @param audioData 浮点数组，范围为-1.0到1.0
     * @param len 数据长度（样本数）
     * @param sampleRate 音频采样率，默认16kHz
     * @return 识别结果文本
     * @note 如果采样率不是16kHz，内部会进行重采样
     */
    QString processFloatAudio(const float *audioData, int len, int sampleRate = 16000);

    /**
     * @brief 处理PCM16数据并转换为float（内部方法）
     */
    QString processPCM16ToFloat(const int16_t *audioData, int samples, int sampleRate = 16000);

    /**
     * @brief 设置是否输出详细日志
     */
    void setVerbose(bool verbose);

    /**
     * @brief 释放资源
     */
    void cleanup();

    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief 设置识别结果的最小长度
     * @param minLength 最小字符长度，默认1
     */
    void setMinResultLength(int minLength) { m_minResultLength = minLength; }

    void addAudioData(const QByteArray &audioData);

    /**
     * @brief 检测音频数据中是否有人说话（VAD - Voice Activity Detection）
     * @param audioData PCM16格式的音频数据
     * @param threshold 音量阈值（分贝），默认-30dB，低于此值认为是静音
     * @return 如果检测到有人说话返回true，否则返回false
     */
    bool isSpeaking(const QByteArray &audioData, double threshold = -35.0);

signals:
    /**
     * @brief 识别结果信号
     * @param text 识别的文本
     */
    void textRecognized(const QString &text);

private:
    /**
     * @brief 重采样音频到16kHz（如果需要）
     */
    QByteArray resampleTo16kHz(const float *audioData, int samples, int originalSampleRate);

    void run() override;
    QByteArray m_audioDataList;
    bool m_initialized;
    whisper_context *m_context;
    whisper_full_params m_params;
    bool m_verbose;
    int m_minResultLength;
    QMutex m_audioDataListMutex;
};

#endif // WHISPERASR_H
