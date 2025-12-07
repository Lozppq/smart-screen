import QtQuick 2.15
import QtQuick.Window 2.15   
import QtQuick.Controls 2.15
import Component 1.0

Rectangle {          
    anchors.fill:parent
    visible: true          // 必须显式设成 true
    color: "transparent"         // 想要边框自己再包一层 Rectangle

   // 通过属性接收 PageManager 的引用（由 Loader 的 onItemChanged 设置）
    // property var pageManager: null

    // 功能项配置数组
    property var functionItems: [
        { name: "音乐", icon: "qrc:/UI/Component/Icons/MusicFunction.jpg", pageId: -1 },
        { name: "视频", icon: "qrc:/UI/Component/Icons/VideoFunction.jpg", pageId: PageManager.videoPage },
        { name: "录音机", icon: "qrc:/UI/Component/Icons/MicrophoneFunction.jpg", pageId: -1 },
        { name: "设置", icon: "qrc:/UI/Component/Icons/SettingFunction.jpg", pageId: -1 },
        { name: "时钟", icon: "qrc:/UI/Component/Icons/ClockFunction.jpg", pageId: 0 },
        { name: "天气", icon: "qrc:/UI/Component/Icons/WeatherFunction.jpg", pageId: 0 },
        { name: "默认", icon: "qrc:/UI/Component/Icons/default.jpg", pageId: 0 }
    ]

    Grid{
        id: gridLayout
        anchors.fill: parent
        anchors.margins: 20  // 网格与边界的间距
        rows: 4
        columns: 10
        spacing: 20  // 网格项之间的间距

        Repeater{
            model: functionItems
            Rectangle{
                width: 50
                height: 70
                color: "transparent"
                radius: 8

                FunctionModel{
                    functionName: modelData.name
                    functionIcon: modelData.icon
                }
                MouseArea{
                    anchors.fill: parent
                    onClicked: {
                        PageManager.showTargetPage(modelData.pageId)
                    }
                }
            }
        }
    }



}

