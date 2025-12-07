#ifndef VOICECOMMANDMATCHER_H
#define VOICECOMMANDMATCHER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QPair>

/**
 * @brief VoiceCommandMatcher - 语音命令匹配类
 * 
 * 将语音识别结果与预设的命令列表进行匹配，找到最匹配的命令
 * 
 * 使用方法：
 * @code
 * VoiceCommandMatcher *matcher = new VoiceCommandMatcher();
 * matcher->registerCommand("打开音乐", 1, "music");
 * matcher->registerCommand("播放视频", 2, "video");
 * 
 * // 连接语音识别信号
 * connect(WhisperASR::getInstance(), &WhisperASR::textRecognized,
 *         matcher, &VoiceCommandMatcher::matchCommand);
 * 
 * // 监听匹配结果
 * connect(matcher, &VoiceCommandMatcher::commandMatched,
 *         [](int commandId, const QString &tag) {
 *             qDebug() << "匹配到命令:" << commandId << tag;
 *         });
 * @endcode
 */
class VoiceCommandMatcher : public QObject
{
    Q_OBJECT

public:
    explicit VoiceCommandMatcher(QObject *parent = nullptr);
    ~VoiceCommandMatcher();

    /**
     * @brief 注册一个语音命令
     * @param command 命令文本（如"打开音乐"）
     * @param commandId 命令ID（用于标识不同的命令）
     * @param tag 命令标签（可选，用于描述命令）
     * @param aliases 命令的别名列表（可选，如["播放音乐", "开启音乐"]）
     */
    void registerCommand(const QString &command, int commandId, 
                        const QString &tag = QString(), 
                        const QStringList &aliases = QStringList());

    /**
     * @brief 批量注册命令
     * @param commands 命令列表，格式：(命令文本, 命令ID, 标签)
     */
    void registerCommands(const QVector<QPair<QString, QPair<int, QString>>> &commands);

    /**
     * @brief 匹配语音识别结果
     * @param recognizedText 语音识别得到的文本
     * @return 匹配到的命令ID，如果未匹配返回-1
     */
    int matchCommand(const QString &recognizedText);

    /**
     * @brief 获取匹配到的命令信息
     * @param recognizedText 语音识别得到的文本
     * @return (命令ID, 标签) 如果未匹配返回(-1, "")
     */
    QPair<int, QString> matchCommandWithInfo(const QString &recognizedText);

    /**
     * @brief 设置匹配模式
     * @param exactMatch true=完全匹配，false=模糊匹配（默认）
     */
    void setExactMatch(bool exactMatch) { m_exactMatch = exactMatch; }

    /**
     * @brief 设置模糊匹配的相似度阈值（0.0-1.0）
     * @param threshold 阈值，默认0.6，相似度低于此值不匹配
     */
    void setSimilarityThreshold(double threshold) { m_similarityThreshold = threshold; }

    /**
     * @brief 清除所有注册的命令
     */
    void clearCommands();

signals:
    /**
     * @brief 匹配到命令时发出的信号
     * @param commandId 命令ID
     * @param tag 命令标签
     * @param recognizedText 识别的文本
     * @param matchedCommand 匹配到的命令文本
     */
    void commandMatched(int commandId, const QString &tag, 
                       const QString &recognizedText, 
                       const QString &matchedCommand);

private:
    /**
     * @brief 计算两个字符串的相似度（0.0-1.0）
     * 使用简单的包含匹配和字符相似度
     */
    double calculateSimilarity(const QString &str1, const QString &str2);

    /**
     * @brief 字符串包含匹配
     */
    bool containsMatch(const QString &text, const QString &pattern);

    /**
     * @brief 移除标点符号和空格
     */
    QString normalizeText(const QString &text);

    struct CommandInfo {
        QString command;        // 原始命令文本
        int commandId;          // 命令ID
        QString tag;            // 命令标签
        QStringList aliases;    // 别名列表
    };

    QVector<CommandInfo> m_commands;    // 注册的命令列表
    bool m_exactMatch;                  // 是否精确匹配
    double m_similarityThreshold;       // 相似度阈值
};

#endif // VOICECOMMANDMATCHER_H
