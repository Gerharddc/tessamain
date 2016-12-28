import QtQuick 2.3
import "qrc:/StyleSheet.js" as Style

// This is a simpler button to improve the keyboard's performance and use less memory
// all the fancy features have been stripped out

Rectangle {
    id: rectMain
    border.color: Style.accentColor
    border.width: 2
    color: _mouseArea.pressed ? Style.pressedRed : Style.bgRed

    // Default
    width: 100
    height: 50

    property alias text: _text.text
    property alias fontSize: _text.font.pixelSize
    property string imgSource: ""

    signal clicked()

    Text {
        id: _text
        anchors.centerIn: parent
        font.family: "Nevis"
        color: Style.textColor
        text: 'Button'
        font.pixelSize: 25
        visible: (imgSource == "null")
    }

    Image {
        id: _img
        anchors.fill: parent
        source: (imgSource != "null") ? imgSource : ""
        visible: (imgSource != "null")
        fillMode: Image.PreserveAspectFit
    }

    MouseArea {
        id: _mouseArea
        anchors.fill: parent

        onClicked: {
            rectMain.clicked()
        }
    }
}

