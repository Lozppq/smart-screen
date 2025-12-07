QT += quick qml sql multimedia

include(interface/interface.pri)
include(s_function/s_function.pri)
include(s_main/s_main.pri)

# FFmpeg 库配置
INCLUDEPATH += $$PWD/thirdParty/ffmpeg/include
FFMPEG_LIB_DIR = $$PWD/thirdParty/ffmpeg/lib
LIBS += -L$$FFMPEG_LIB_DIR/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale

# Whisper 库配置
INCLUDEPATH += $$PWD/thirdParty/whisper/include
WHISPER_LIB_DIR = $$PWD/thirdParty/whisper/lib
# 调整链接顺序：先链接依赖库（ggml相关），再链接使用它们的库（whisper）
LIBS += -L$$WHISPER_LIB_DIR -lggml-base -lggml-cpu -lggml -lwhisper

# espeak-ng 库配置
INCLUDEPATH += $$PWD/thirdParty/espeak-ng/include
ESPEAK_LIB_DIR = $$PWD/thirdParty/espeak-ng/lib
LIBS += -L$$ESPEAK_LIB_DIR -lespeak-ng

# ekho 库配置
INCLUDEPATH += $$PWD/thirdParty/ekho/include
EKHO_LIB_DIR = $$PWD/thirdParty/ekho/lib
EKHO_DATA_DIR = $$PWD/thirdParty/ekho/share/ekho-data
LIBS += -L$$EKHO_LIB_DIR -lekho

# 设置运行时库路径（RPATH），让程序运行时能找到动态库
# 由于构建目录可能在项目源目录之外，必须使用绝对路径
# $$PWD 会解析为 .pro 文件所在的目录（项目根目录）
# 使用 $$shell_path() 确保路径格式正确
QMAKE_RPATHDIR += $$shell_path($$FFMPEG_LIB_DIR)
QMAKE_RPATHDIR += $$shell_path($$WHISPER_LIB_DIR)
QMAKE_RPATHDIR += $$shell_path($$ESPEAK_LIB_DIR)
QMAKE_RPATHDIR += $$shell_path($$EKHO_LIB_DIR)
# 同时使用链接器标志作为双重保险
QMAKE_LFLAGS += -Wl,-rpath,$$shell_path($$FFMPEG_LIB_DIR)
QMAKE_LFLAGS += -Wl,-rpath,$$shell_path($$WHISPER_LIB_DIR)
QMAKE_LFLAGS += -Wl,-rpath,$$shell_path($$ESPEAK_LIB_DIR)
QMAKE_LFLAGS += -Wl,-rpath,$$shell_path($$EKHO_LIB_DIR)
