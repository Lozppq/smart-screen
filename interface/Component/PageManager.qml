import QtQuick 2.15
import QtQuick.Controls 2.15

pragma Singleton
Item {

    // 1. 定义页面类型枚举（或用字符串常量，这里用枚举更规范）
    property int mainWindow: 0
    property int musicPage: 1
    property int videoPage: 2
    property int settingPage: 3

    // 2. 当前激活的页面（默认主页）
    property int currentPage: mainWindow
    property int lastPage: mainWindow

    // 3. 页面映射表：枚举值 → 对应的Loader组件
    property var pageMap: [
        functionArea,
        musicPage,
        videoPage,
    ]

    property Item container: null

    Loader{
        id:functionArea
        source: "qrc:/UI/MainWindow/FunctionArea.qml"
        parent: container
        anchors.fill: parent
        active: false
    }

    Loader{
        id:musicPage
        source: "qrc:/UI/MusicFunction/MainMusic.qml"
        parent: container
        anchors.fill: parent
        active: false
    }

    Loader{
        id:videoPage
        source: "qrc:/UI/VideoFunction/MainVideo.qml"
        parent: container
        anchors.fill: parent
        active: false
    }

    Connections{
        target: QmlBridgeToCpp
        function onSendMessageToQmlSignal(appId, messageId, data){
            
        }
    }

    function showTargetPage(targetPage){
        pageMap[currentPage].visible = false

        pageMap[targetPage].visible = true
        pageMap[targetPage].active = true

        lastPage = currentPage
        currentPage = targetPage
    }

    function closeCurrentPage(){
        if(currentPage !== 0){
            showTargetPage(lastPage)
        }
    }
}

