import QtQuick 2.3
import "qrc:/StyleSheet.js" as Style

DimmableControl {
    id: mainRect
    color: enabled ? Style.bgRed : Style.disabledRed
    border.color: Style.accentColor
    border.width: 2

    // Default
    width: 100
    height: 60

    property int total: 360;
    property int value: 0;
    property int snapInterval: 15
    property int snapThreshold: 10
    property bool toggled: false
    property string nameA: 'On'
    property string nameB: 'Off'

    isActive: _mouseArea.pressed
    focus: _mouseArea.pressed

    MouseArea {
        id: _mouseArea
        anchors.fill: parent
        anchors.margins: 7

        onClicked: {
            toggled = !toggled
        }

        Rectangle {
            id: mover
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: _mouseArea.pressed ? (parent.width / 2 - width / 2) : (toggled ? (parent.width - width) : 0)
            width: height
            color: Style.barColor
            z: 10

            Behavior on x {
                NumberAnimation {}
            }
        }

        Text {
            text: toggled ? nameA : nameB
            font.family: 'Nevis'
            x: toggled ? 7 : (parent.width - width - 7)
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 20

            Behavior on x {
                NumberAnimation {}
            }
        }
    }
}
