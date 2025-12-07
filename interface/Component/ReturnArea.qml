import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle{
    id:root
    color: "transparent"

    Button{
        id:returnButton
        width: parent.width
        height: parent.height
        anchors.centerIn: parent

        background: Rectangle{
            color: "transparent"
        }
        
        contentItem: Image {
            source: "qrc:/UI/Component/Icons/main_return.jpg"
            fillMode: Image.PreserveAspectFit
            anchors.fill: parent
        }
    }
}

