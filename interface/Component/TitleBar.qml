import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle{
    id:root
    width: parent.width
    height: 50
    color: "#2f80ed"
    property string title: ""
    signal returnClicked()

    ReturnArea{
        id:returnArea
        width: 30
        height: 30
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.topMargin: 10
        MouseArea{
            anchors.fill: parent
            onClicked: {
                root.returnClicked();
            }
        }
    }

    Text{
        id:titleText
        text: qsTr(root.title)
        color: "white"
        font.pixelSize: 20
        anchors.centerIn: parent
    }
}