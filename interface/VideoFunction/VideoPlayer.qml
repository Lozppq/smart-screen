import QtQuick 2.15
import QtQuick.Controls 2.15
import VideoFrame 1.0

Rectangle {
    property string videoPath: ""
    property int totalDuration: 0
    property int currentDuration: 0
    color: "white"

    Rectangle {
        id: videoArea
        width: parent.width
        height: parent.height * 0.8
        color: "black"
        anchors.top: parent.top

        Item {
            id: videoContainer
            anchors.centerIn: parent
            
            // 存储视频宽高比
            property real videoAspectRatio: 0
            
            // width 基于宽高比计算
            width: {
                if (videoAspectRatio > 0) {
                    var containerWidth = parent.width
                    var containerHeight = parent.height
                    var widthByHeight = containerHeight * videoAspectRatio
                    return Math.min(widthByHeight, containerWidth)
                }
                return parent.width
            }
            
            height: parent.height

            VideoFrame {
                id: videoFrame
                anchors.fill: parent
                
                // 监听图片变化，更新宽高比
                onCurrentImageChanged: {
                    var width = videoFrame.getWidth()
                    var height = videoFrame.getHeight()
                    currentDuration = videoFrame.getDuration()
                    progressSlider.value = currentDuration / totalDuration * 100
                    if(currentDuration >= totalDuration){
                        playButton.isPlaying = false
                        currentDuration = 0
                        progressSlider.value = 0
                        videoFrame.seekTo(0)
                    }
                    if(width > 0 && height > 0) {
                        videoContainer.videoAspectRatio = width / height
                    }
                    else {
                        videoContainer.videoAspectRatio = 0
                    }
                }
            }
        }
    }

    Rectangle {
        id: controlArea
        width: parent.width
        height: parent.height * 0.2
        color: "white"
        anchors.bottom: parent.bottom

        Column {
            anchors.fill: parent
            anchors.margins: 10
            
            // 进度条和时间显示
            Row {
                width: parent.width
                height: parent.height * 0.4
                spacing: 5
                
                // 当前时间
                Text {
                    id: currentTimeText
                    text: secondsToString(currentDuration)
                    width: 50
                    height: parent.height
                    font.pixelSize: 14
                    color: "#333333"
                    verticalAlignment: Text.AlignVCenter
                }
                
                // 进度条
                Slider {
                    id: progressSlider
                    width: parent.width - currentTimeText.width - totalTimeText.width - 20
                    height: parent.height
                    from: 0
                    to: 100
                    value: 0
                    
                    background: Rectangle {
                        x: progressSlider.leftPadding
                        y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 4
                        width: progressSlider.availableWidth
                        height: implicitHeight
                        radius: 2
                        color: "#e0e0e0"
                        
                        Rectangle {
                            width: progressSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#2f80ed"
                            radius: 2
                        }
                    }
                    
                    handle: Rectangle {
                        x: progressSlider.leftPadding + progressSlider.visualPosition * (progressSlider.availableWidth - width)
                        y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: progressSlider.pressed ? "#1d55b3" : "#2f80ed"
                        border.width: 2
                        border.color: "#ffffff"
                    }

                    onPressedChanged: {
                        if(pressed){
                            if(videoFrame && playButton.isPlaying){
                                playButton.isPlaying = false
                                videoFrame.setPlaying(false)
                            }
                        }
                        else if(videoFrame && !playButton.isPlaying){
                            playButton.isPlaying = true
                            videoFrame.seekTo(currentDuration)
                            videoFrame.setPlaying(true)
                        }
                    }
                    
                    // 拖拽进度条时跳转播放位置
                    onMoved: {
                        // TODO: 调用后端接口跳转到指定位置
                        // if (videoFrame) {
                        //     videoFrame.seekTo(value)
                        // }
                        currentDuration = value * totalDuration / 100
                    }

                }
                
                // 总时长
                Text {
                    id: totalTimeText
                    text: secondsToString(totalDuration)
                    width: 50
                    height: parent.height
                    font.pixelSize: 14
                    color: "#333333"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                }
            }
            
            // 控制按钮行
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 20
                height: parent.height * 0.6
                
                // 播放/暂停按钮
                Button {
                    id: playButton
                    text: isPlaying ? "暂停" : "播放"
                    
                    background: Rectangle {
                        radius: 12
                        color: playButton.down ? "#1d55b3" : "#2f80ed"
                        border.width: 1
                        border.color: "#ffffff"
                    }

                    property bool isPlaying: false
                    
                    onClicked: {
                        isPlaying = !isPlaying
                        if (videoFrame) {
                            videoFrame.setPlaying(isPlaying)
                        }
                    }
                }

                // 播放倍速下拉框
                ComboBox {
                    id: speedComboBox
                    width: 80
                    // height: 32
                    
                    // 倍速选项
                    model: [
                        { text: "0.5x", value: 0.5 },
                        { text: "0.75x", value: 0.75 },
                        { text: "1.0x", value: 1.0 },
                        { text: "1.25x", value: 1.25 },
                        { text: "1.5x", value: 1.5 },
                        { text: "2.0x", value: 2.0 }
                    ]
                    
                    // 默认选择1.0x
                    currentIndex: 2
                    
                    // 自定义显示文本
                    textRole: "text"
                    
                    // 内边距
                    padding: 8
                    leftPadding: 12
                    rightPadding: 28
                    
                    // 自定义样式
                    background: Rectangle {
                        radius: 6
                        color: speedComboBox.hovered ? "#f0f0f0" : "#ffffff"
                        border.width: 1
                        border.color: speedComboBox.hovered ? "#4a90e2" : "#d1d5db"
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                        Behavior on border.color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    // 固定显示"倍速"两个字
                    contentItem: Text {
                        text: "倍速"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: "#1f2937"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 0
                    }
                    
                    // 下拉指示器 - 使用简单的三角形
                    indicator: Item {
                        x: speedComboBox.width - width - 8
                        y: (speedComboBox.height - height) / 2
                        width: 8
                        height: 6
                        
                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.strokeStyle = "#6b7280"
                                ctx.fillStyle = "#6b7280"
                                ctx.lineWidth = 1
                                ctx.beginPath()
                                ctx.moveTo(0, 0)
                                ctx.lineTo(width, 0)
                                ctx.lineTo(width / 2, height)
                                ctx.closePath()
                                ctx.fill()
                            }
                        }
                    }
                    
                    // 下拉列表样式
                    popup: Popup {
                        y: speedComboBox.height + 2
                        width: speedComboBox.width
                        implicitHeight: contentItem.implicitHeight + 8
                        padding: 4
                        
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: speedComboBox.popup.visible ? speedComboBox.delegateModel : null
                            currentIndex: speedComboBox.highlightedIndex
                            
                            ScrollIndicator.vertical: ScrollIndicator { }
                        }
                        
                        background: Rectangle {
                            radius: 6
                            color: "#ffffff"
                            border.width: 1
                            border.color: "#e5e7eb"
                        }
                    }
                    
                    // 列表项样式
                    delegate: ItemDelegate {
                        width: speedComboBox.width - 8
                        height: 32
                        
                        // 禁用默认的高亮效果，完全由自定义逻辑控制
                        highlighted: false
                        
                        background: Rectangle {
                            // 高亮当前选中的项和悬停的项
                            color: {
                                if (speedComboBox.currentIndex === index) {
                                    return "#e5e7eb"  // 选中项：浅灰色
                                } else if (parent.hovered) {
                                    return "#f3f4f6"  // 悬停项：更浅的灰色
                                } else {
                                    return "transparent"  // 普通项：透明
                                }
                            }
                            radius: 4
                        }
                        
                        contentItem: Text {
                            text: modelData.text
                            font.pixelSize: 13
                            font.weight: (speedComboBox.currentIndex === index) ? Font.Medium : Font.Normal
                            color: (speedComboBox.currentIndex === index) ? "#1f2937" : "#4b5563"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            leftPadding: 12
                        }
                    }
                    
                    // 当选择改变时，设置播放倍速
                    onCurrentIndexChanged: {
                        if (videoFrame && currentIndex >= 0) {
                            var speed = model[currentIndex].value
                            videoFrame.setPlaySpeed(speed)
                            console.log("Playback speed set to:", speed, "x")
                        }
                    }
                }
            }
        }
    }

    // 00:00:00字符串转秒数
    function stringToSeconds(timeString) {
        var parts = timeString.split(":")
        return parts[0] * 3600 + parts[1] * 60 + parts[2]
    }

    // 秒数转00:00:00字符串
    function secondsToString(seconds) {
        var hours = Math.floor(seconds / 3600)
        var minutes = Math.floor((seconds % 3600) / 60)
        var remainingSeconds = seconds % 60
        return hours.toString().padStart(2, "0") + ":" + minutes.toString().padStart(2, "0") + ":" + remainingSeconds.toString().padStart(2, "0")
    }

    Component.onCompleted: {
        // 页面加载时自动打开视频
        if (videoPath && videoFrame) {
            videoFrame.openVideo(videoPath)
            totalDuration = videoFrame.getTotalDuration()
        }
    }
}