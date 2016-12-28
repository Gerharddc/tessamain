import QtQuick 2.3
import "qrc:/StyleSheet.js" as Style

DimmableControl {
    id: textRect
    color: 'transparent'

    property alias fontSize: txt.font.pixelSize
    property alias text: txt.text

    // Default
    width: txt.width
    height: txt.height

    Text {
        anchors.centerIn: parent
        id: txt
        font.family: 'Nevis'
        color: Style.textColor
        text: 'Label'
        font.pixelSize: 20
    }

    isActive: false
}

