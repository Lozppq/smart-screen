#include "EkhoTTS.h"
#include <QMutexLocker>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <string>
#include "../play/AudioOutput.h"

EkhoTTS::EkhoTTS()
    : m_ekho(nullptr)
    , m_initialized(false)
    , m_sampleRate(0)
    , m_swrContext(nullptr)
    , m_playQueueIndex(-1)
{

}

EkhoTTS::~EkhoTTS()
{
    QMutexLocker locker(&m_mutex);
    cleanupResampler();
    if (m_ekho) {
        delete m_ekho;
        m_ekho = nullptr;
    }
}

EkhoTTS *EkhoTTS::getInstance()
{
    static EkhoTTS instance;
    return &instance;
}

bool EkhoTTS::initialize(const QString &voice)
{
    QMutexLocker locker(&m_mutex);

    // 如果已经初始化，检查是否需要切换语音
    if (m_initialized && m_ekho) {
        std::string currentVoice = m_ekho->getVoice();
        std::string newVoice = voice.isEmpty() ? "Mandarin" : voice.toStdString();
        
        if (currentVoice == newVoice) {
            qDebug() << "EkhoTTS 已经初始化，语音相同，无需重新设置";
            return true;
        }
        
        // 语音不同，需要重新创建实例
        cleanupResampler();
        delete m_ekho;
        m_ekho = nullptr;
        m_initialized = false;
    }

    try {
        // 设置 ekho-data 目录路径
        QByteArray envPath = qgetenv("EKHO_DATA_PATH");
        if (envPath.isEmpty()) {
            // 尝试查找 ekho-data 目录
            QStringList possiblePaths = {
                QCoreApplication::applicationDirPath() + "/../thirdParty/ekho/share/ekho-data",
                QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../share/smart-screen/thirdParty/ekho/share/ekho-data"),
                "/mnt/hgfs/share/smart-screen/thirdParty/ekho/share/ekho-data",
                QDir::currentPath() + "/thirdParty/ekho/share/ekho-data"
            };
            
            for (const QString &path : possiblePaths) {
                QDir testDir(path);
                if (testDir.exists() && testDir.exists("pinyin.voice")) {
                    qputenv("EKHO_DATA_PATH", QDir::cleanPath(path).toUtf8());
                    qDebug() << "设置 EKHO_DATA_PATH 环境变量为:" << path;
                    break;
                }
            }
        }

        std::string voiceStr = voice.isEmpty() ? "Mandarin" : voice.toStdString();
        m_ekho = new ekho::Ekho();
        if (!m_ekho) {
            return false;
        }

        m_ekho->setSampleRate(0);  // 使用语音文件原始采样率
        m_ekho->setChannels(1);
        int voiceResult = m_ekho->setVoice(voiceStr);
        if (voiceResult != 0) {
            if (voiceStr != "pinyin" && m_ekho->setVoice("pinyin") == 0) {
                voiceStr = "pinyin";
            } else {
                delete m_ekho;
                m_ekho = nullptr;
                return false;
            }
        }
        m_ekho->setEnglishSpeed(0);
        m_ekho->setPitch(0);
        m_ekho->setSpeed(0);
        m_ekho->setOverlap(2048);
        m_ekho->setVolume(0);
        m_ekho->setRate(0);

        m_sampleRate = m_ekho->getSampleRate();
        if (m_sampleRate != 44100 && m_sampleRate > 0) {
            initializeResampler();
        }
        m_initialized = true;
        start();
        return true;
    } catch (const std::exception &e) {
        qDebug() << "初始化 Ekho 时发生异常:" << e.what();
        if (m_ekho) {
            delete m_ekho;
            m_ekho = nullptr;
        }
        return false;
    } catch (...) {
        qDebug() << "初始化 Ekho 时发生未知异常";
        if (m_ekho) {
            delete m_ekho;
            m_ekho = nullptr;
        }
        return false;
    }
}

void EkhoTTS::addTextToQueue(const QString &text)
{
    QMutexLocker locker(&m_textQueueMutex);
    m_textQueue.enqueue(text);
}

void EkhoTTS::run()
{
    m_playQueueIndex = AudioOutput::getInstance()->addThreadIdToPlayQueue(reinterpret_cast<qintptr>(QThread::currentThreadId()));
    while (true) {
        QString text;
        {
            QMutexLocker locker(&m_textQueueMutex);
            if (m_textQueue.isEmpty()) {
                msleep(1);
                continue;
            }
            text = m_textQueue.dequeue();
        }
        QByteArray audioData = synthesize(text);
        if (!audioData.isEmpty()) {
            AudioOutput::getInstance()->addAudioDataToQueue(m_playQueueIndex, audioData);
        }
    }
    AudioOutput::getInstance()->removeThreadIdFromPlayQueue(m_playQueueIndex);
    m_playQueueIndex = -1;
}

QByteArray EkhoTTS::synthesize(const QString &text)
{
    if (!m_initialized || text.isEmpty() || !m_ekho) {
        return QByteArray();
    }

    QMutexLocker locker(&m_mutex);

    try {
        std::string textStr = text.toStdString();
        int pcmSize = 0;
        short* pcm = m_ekho->synth3(textStr, pcmSize);
        
        if (!pcm || pcmSize <= 0) {
            return QByteArray();
        }
        
        QByteArray audioData(reinterpret_cast<const char*>(pcm), pcmSize * sizeof(short));
        delete[] pcm;
        
        int currentSampleRate = m_ekho->getSampleRate();
        if (currentSampleRate != 44100 && m_swrContext != nullptr) {
            QByteArray resampledData = resampleChunk(reinterpret_cast<const short*>(audioData.constData()), pcmSize);
            if (!resampledData.isEmpty()) {
                return resampledData;
            }
        }
        
        return audioData;
    } catch (...) {
        return QByteArray();
    }
}

bool EkhoTTS::initializeResampler()
{
    if (m_swrContext != nullptr) {
        cleanupResampler();
    }

    m_swrContext = swr_alloc();
    if (!m_swrContext) {
        return false;
    }

    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;

    int ret = swr_alloc_set_opts2(&m_swrContext,
                                  &out_ch_layout, AV_SAMPLE_FMT_S16, 44100,
                                  &in_ch_layout, AV_SAMPLE_FMT_S16, m_sampleRate,
                                  0, nullptr);

    if (ret < 0 || swr_init(m_swrContext) < 0) {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
        return false;
    }

    return true;
}

void EkhoTTS::cleanupResampler()
{
    if (m_swrContext != nullptr) {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
    }
}

QByteArray EkhoTTS::resampleChunk(const short *inputData, int inputSamples)
{
    if (m_swrContext == nullptr || inputData == nullptr || inputSamples <= 0) {
        return QByteArray();
    }

    int64_t outSamples = av_rescale_rnd(inputSamples, 44100, m_sampleRate, AV_ROUND_UP);
    int outBufferSize = av_samples_get_buffer_size(nullptr, 1, outSamples, AV_SAMPLE_FMT_S16, 1);
    uint8_t *outBuffer = (uint8_t*)av_malloc(outBufferSize);
    if (!outBuffer) {
        return QByteArray();
    }

    const uint8_t *inData[1] = { (const uint8_t*)inputData };
    uint8_t *outData[1] = { outBuffer };
    int outSamplesActual = swr_convert(m_swrContext, outData, outSamples, inData, inputSamples);

    QByteArray result;
    if (outSamplesActual > 0) {
        int outputBytes = outSamplesActual * sizeof(int16_t);
        result = QByteArray((const char*)outBuffer, outputBytes);
    }

    av_free(outBuffer);
    return result;
}
