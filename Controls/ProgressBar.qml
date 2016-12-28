import QtQuick 2.3
import "qrc:/StyleSheet.js" as Style

DimmableControl {
    id: mainRect
    color: Style.bgRed
    border.color: Style.accentColor
    border.width: 2

    // Default
    width: 100
    height: 60

    property real total: 100;
    property real value: 50;

    onValueChanged: {
        if (value > total) {
            value = total
        }
        else if (value < 0) {
            value = 0
        }

        growRect.width = (mainRect.width - 14) / total * value
    }

    Rectangle {
        id: growRect
        anchors.left: parent.left
        anchors.leftMargin: 7
        anchors.top: parent.top
        anchors.topMargin: 7
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
        width: growRect.width = (mainRect.width - 14) / total * value
        color: Style.barColor

        Behavior on width {
            PropertyAnimation {}
        }
    }

    Text {
        text: (value / total * 100) + '%'
        font.family: 'Nevis'
        anchors.centerIn: parent
        font.pixelSize: 20
    }
}

