import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    color: "transparent"

    property string currentPath: ""
    signal videoPathClicked(string videoPath)
    signal dirPathClicked(string dirPath)
    
    // 视频列表
    ScrollView {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        
        ListView {
            id: videoListView
            model: {
                if(!VideoFunction){
                    return [];
                }   
                var videoList = VideoFunction.videoList;
                var newVideoList = [];
                for(var i = 0; i < videoList.length; i++){
                    if(VideoFunction.isDirectSubPath(currentPath, videoList[i]["path"])){
                        newVideoList.push(videoList[i]);
                    }
                }
                return newVideoList;
            }
            
            delegate: Rectangle {
                width: videoListView.width
                height: 50
                color: mouseArea.containsMouse ? "#f0f0f0" : "white"
                
                property bool isDir: modelData.isDir || false
                property string videoPath: modelData.path || ""
                
                Row {
                    width: parent.width
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 20
                    
                    // 图标
                    Image {
                        source: parent.parent.isDir ? "qrc:/UI/VideoFunction/Icons/folder.jpg" : "qrc:/UI/VideoFunction/Icons/video.jpg"
                        width: 40
                        height: 40
                        fillMode: Image.PreserveAspectFit  // 保持宽高比，不拉伸变形
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    // 文件名
                    Text {
                        text: modelData.name || ""
                        font.pixelSize: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.5
                    }

                    // 文件修改时间
                    Text {
                        text: modelData.dateTime || ""
                        font.pixelSize: 16
                        color: "#666"
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.2
                    }
                    
                    // 文件大小（仅文件显示）
                    Text {
                        visible: !parent.parent.isDir && modelData.size !== undefined
                        text: formatSize(modelData.size)
                        font.pixelSize: 16
                        color: "#666"
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.1
                    }
                }
                
                // 分隔线
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: "#e0e0e0"
                }
                
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    
                    onClicked: {
                        if (parent.isDir) {
                            root.dirPathClicked(parent.videoPath)
                        } else{
                            root.videoPathClicked(parent.videoPath)
                        }
                    }
                }
            }
        }
    }
    
    // 格式化文件大小函数
    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
    }
}