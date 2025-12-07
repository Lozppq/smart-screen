import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle{
    id:root
    width: parent.width
    height: parent.height * 0.06
    color: "transparent"

    // 直接通过 parent 访问 MainWindow 中的 pageManager 实例
    // property var pageManager: parent ? parent.pageManager : null

    // 时间显示
    Rectangle{
        width: parent.width * 0.22
        height: parent.height
        anchors.left: parent.left
        color: "transparent"


        Text{
            id:timeText
            text: getCurrentTimeString()
            font.pixelSize: 18
            color: "white"
            anchors.centerIn: parent
        }

        Timer{
            id:timeTimer
            interval: 1000
            repeat: true
            onTriggered: {
                timeText.text = getCurrentTimeString()
            }
        }
    }

    // 实例创建后开始计时
    Component.onCompleted: {
        timeTimer.start()
    }

    // 使用函数获取格式化时间
    function getCurrentTimeString() {
        var now = new Date();
        var year = now.getFullYear();
        var month = String(now.getMonth() + 1).padStart(2, '0');  // 月份从0开始，需要+1
        var day = String(now.getDate()).padStart(2, '0');
        var hours = String(now.getHours()).padStart(2, '0');
        var minutes = String(now.getMinutes()).padStart(2, '0');
        var seconds = String(now.getSeconds()).padStart(2, '0');
        
        return year + "-" + month + "-" + day + " " + hours + ":" + minutes + ":" + seconds;
    }
}
