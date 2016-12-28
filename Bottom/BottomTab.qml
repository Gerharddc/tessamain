import QtQuick 2.3
import 'qrc:/StyleSheet.js' as Style

Rectangle {
    id: tabRect

    property alias tabText: tabTextBlock.text
    property int tabNum: 0

    signal tabClicked()

    color: mouseArea.pressed ? Style.pressedRed : 'transparent'

    Text {
        z: 10
        id: tabTextBlock
        anchors.centerIn: parent
        text: 'Model'
        font.family: 'Nevis'
        font.pixelSize: 20
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: {
            tabRect.tabClicked()
        }
    }
}

