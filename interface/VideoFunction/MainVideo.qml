import QtQuick 2.15
import QtQuick.Controls 2.15
import Component 1.0

Rectangle {
    anchors.fill: parent
    color: "white"

    property string playTitle: ""

    function backup(){
        if(videoStackView.depth > 1){
            videoStackView.pop()
        }
        else{
            PageManager.showTargetPage(PageManager.mainWindow)
        }
    }

    TitleBar{
        id:titleBar
        anchors.top: parent.top
        width: parent.width
        height: 50
        title: playTitle == "" ? "视频列表" : playTitle
        onReturnClicked: {
            backup()
        }
    }

    StackView {
        id: videoStackView
        width: parent.width
        height: parent.height - titleBar.height
        anchors.top: titleBar.bottom
        initialItem: videoListPage  // 初始显示列表页面

        // 如果当前页面是视频播放页面，则显示视频播放页面
        onCurrentItemChanged: {
            if(currentItem && currentItem.videoPath){
                var paths = currentItem.videoPath.split("/")
                playTitle = paths[paths.length - 1]
                console.log("playTitle: " + playTitle)
            }
            else{
                playTitle = ""
            }
        }
    }

    // 视频列表页面
    Component {
        id: videoListPage
        
        VideoListPage {
            currentPath: VideoFunction ? VideoFunction.currentPath : ""
            onVideoPathClicked: {
                videoStackView.push(videoPlayerPage, {
                    "videoPath": videoPath
                })
            }
            onDirPathClicked: {
                videoStackView.push(videoListPage, {
                    "currentPath": dirPath
                })
            }
        }
    }

    // 视频播放页面
    Component {
        id: videoPlayerPage
        
        VideoPlayer {
        }
    }
}