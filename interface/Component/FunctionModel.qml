import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: functionModel
    anchors.fill: parent
    color: "transparent"
    
    // 组件属性，可以在使用时设置
    property string functionName: ""
    property string functionIcon: ""
    
    Rectangle{
        id: iconRect
        width:parent.width
        height: parent.width
        color: "transparent"
        anchors.top: parent.top

        // 放置图片
        Image{
            id: iconImage
            source: functionIcon
            anchors.fill: parent
        }
    }
    
    Rectangle{
        id: nameRect
        width:parent.width
        height: parent.height - parent.width
        color: "transparent"
        anchors.bottom: parent.bottom
        // 放置文字
        Text{
            id: nameText
            text: functionName
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 16
            color: "white"
        }
    }

    
}

