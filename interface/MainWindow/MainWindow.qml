import QtQuick 2.15
import QtQuick.Window 2.15   // 别忘了 import
import QtQuick.Controls 2.15
import Component 1.0

Window {          // ← 关键：用 Window 而不是 Rectangle
    id:mainWindow
    width: 800
    height: 480
    visible: true          // 必须显式设成 true
    color: "white"         // 想要边框自己再包一层 Rectangle
    flags: Qt.FramelessWindowHint  // 无边框窗口，无标题栏

    Image{
        id:mainBg
        source: "qrc:/UI/MainWindow/Icons/main_bg.jpg"
        anchors.fill: parent
    }

    TopArea{
        id:topArea
        anchors.top: parent.top
    }

    // PageManager{
    //     id:pageManager
    //     anchors.top:topArea.bottom
    // }
    // 使用单例的包装器
    Rectangle {
        id: pageManagerContainer
        anchors.top: topArea.bottom
        width: parent.width
        height: parent.height * 0.94
        color: "transparent"
        
        // 将 PageManager 的功能代理到这个容器
        Component.onCompleted: {
            // PageManager 是单例，可以直接访问其属性和方法
            PageManager.container = pageManagerContainer
            PageManager.showTargetPage(PageManager.mainWindow)
        }
    }
}

