import QtQuick 2.3
import "qrc:/StyleSheet.js" as Style

DimmableControl {
    id: rectMain
    color: enabled ? Style.bgRed : Style.disabledRed
    border.color: Style.accentColor
    border.width: 2

    // Default
    width: 100
    height: 60

    property alias text: _text.text
    property alias fontSize: _text.font.pixelSize
    property alias isPressed: _mouseArea.pressed

    signal clicked()
    signal pressed()
    signal canceled()
    signal released()

    focus: _mouseArea.pressed

    // For some reason this binding has to be made the long way in order to stick
    Binding {
        target: rectMain
        property: "isActive"
        value: rectMain.focus
    }

    Text {
        id: _text
        anchors.centerIn: parent
        font.family: 'Nevis'
        color: Style.textColor
        text: 'Button'
        font.pixelSize: 20
    }

    MouseArea {
        id: _mouseArea
        anchors.fill: parent

        onClicked: {
            rectMain.clicked()
        }

        onPressed: {
            rectMain.pressed()
            rectMain.color = Style.pressedRed
        }

        onCanceled: {
            rectMain.canceled()
            rectMain.color = Style.bgRed
            isActive = false
        }

        onReleased: {
            rectMain.released()
            rectMain.color = Style.bgRed
        }
    }
}

