#include "VoiceCommandMatcher.h"
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>

VoiceCommandMatcher::VoiceCommandMatcher(QObject *parent)
    : QObject(parent)
    , m_exactMatch(false)
    , m_similarityThreshold(0.6)
{
}

VoiceCommandMatcher::~VoiceCommandMatcher()
{
}

void VoiceCommandMatcher::registerCommand(const QString &command, int commandId, 
                                         const QString &tag, 
                                         const QStringList &aliases)
{
    CommandInfo info;
    info.command = command;
    info.commandId = commandId;
    info.tag = tag;
    info.aliases = aliases;
    
    m_commands.append(info);
    qDebug() << "注册语音命令:" << command << "ID:" << commandId << "标签:" << tag;
}

void VoiceCommandMatcher::registerCommands(const QVector<QPair<QString, QPair<int, QString>>> &commands)
{
    for (const auto &cmd : commands) {
        registerCommand(cmd.first, cmd.second.first, cmd.second.second);
    }
}

int VoiceCommandMatcher::matchCommand(const QString &recognizedText)
{
    QPair<int, QString> result = matchCommandWithInfo(recognizedText);
    return result.first;
}

QPair<int, QString> VoiceCommandMatcher::matchCommandWithInfo(const QString &recognizedText)
{
    if (recognizedText.isEmpty() || m_commands.isEmpty()) {
        return qMakePair(-1, QString());
    }

    QString normalizedText = normalizeText(recognizedText);
    double bestSimilarity = 0.0;
    int bestMatchId = -1;
    QString bestMatchTag;
    QString bestMatchCommand;

    // 遍历所有注册的命令
    for (const CommandInfo &cmd : m_commands) {
        QString normalizedCmd = normalizeText(cmd.command);
        double similarity = 0.0;

        // 1. 完全匹配
        if (m_exactMatch) {
            if (normalizedText == normalizedCmd) {
                similarity = 1.0;
            }
        } else {
            // 2. 包含匹配（识别文本包含命令）
            if (containsMatch(normalizedText, normalizedCmd)) {
                similarity = 0.9;
            }
            // 3. 命令包含识别文本（部分匹配）
            else if (containsMatch(normalizedCmd, normalizedText)) {
                similarity = 0.8;
            }
            // 4. 相似度匹配
            else {
                similarity = calculateSimilarity(normalizedText, normalizedCmd);
            }

            // 5. 检查别名
            for (const QString &alias : cmd.aliases) {
                QString normalizedAlias = normalizeText(alias);
                if (containsMatch(normalizedText, normalizedAlias) || 
                    containsMatch(normalizedAlias, normalizedText)) {
                    similarity = qMax(similarity, 0.85);
                    break;
                }
                double aliasSimilarity = calculateSimilarity(normalizedText, normalizedAlias);
                similarity = qMax(similarity, aliasSimilarity);
            }
        }

        // 更新最佳匹配
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            bestMatchId = cmd.commandId;
            bestMatchTag = cmd.tag;
            bestMatchCommand = cmd.command;
        }
    }

    // 如果相似度达到阈值，返回匹配结果
    if (bestMatchId != -1 && bestSimilarity >= m_similarityThreshold) {
        qDebug() << "语音命令匹配成功 - 识别文本:" << recognizedText 
                 << "匹配命令:" << bestMatchCommand 
                 << "ID:" << bestMatchId 
                 << "相似度:" << bestSimilarity;
        
        emit commandMatched(bestMatchId, bestMatchTag, recognizedText, bestMatchCommand);
        return qMakePair(bestMatchId, bestMatchTag);
    }

    qDebug() << "语音命令未匹配 - 识别文本:" << recognizedText 
             << "最佳相似度:" << bestSimilarity;
    
    return qMakePair(-1, QString());
}

void VoiceCommandMatcher::clearCommands()
{
    m_commands.clear();
    qDebug() << "已清除所有语音命令";
}

double VoiceCommandMatcher::calculateSimilarity(const QString &str1, const QString &str2)
{
    if (str1.isEmpty() && str2.isEmpty()) {
        return 1.0;
    }
    if (str1.isEmpty() || str2.isEmpty()) {
        return 0.0;
    }

    // 简单的字符相似度计算（基于共同字符比例）
    QString s1 = str1;
    QString s2 = str2;
    
    int commonChars = 0;
    int totalChars = qMax(s1.length(), s2.length());

    // 计算共同字符数量
    for (int i = 0; i < s1.length(); i++) {
        if (s2.contains(s1[i])) {
            commonChars++;
            // 移除已匹配的字符（避免重复计算）
            int index = s2.indexOf(s1[i]);
            s2.remove(index, 1);
        }
    }

    return static_cast<double>(commonChars) / totalChars;
}

bool VoiceCommandMatcher::containsMatch(const QString &text, const QString &pattern)
{
    return text.contains(pattern);
}

QString VoiceCommandMatcher::normalizeText(const QString &text)
{
    // 移除标点符号和空格，转为小写
    QString normalized = text.toLower();
    
    // 移除常见标点符号
    normalized.remove(QRegularExpression("[，。！？、；：""''（）【】《》\\s]+"));
    
    // 移除空格
    normalized.replace(" ", "");
    
    return normalized;
}
