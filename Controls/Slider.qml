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

    property int total: 360;
    property int value: 0;
    property int snapInterval: 15
    property int snapThreshold: 10

    isActive: _mouseArea.pressed
    focus: _mouseArea.pressed

    onValueChanged: {
        if (value > total) {
            value = total
        }
        else if (value < 0) {
            value = 0
        }

        mover.x = value / total * (_mouseArea.width - mover.width)
    }

    MouseArea {
        id: _mouseArea
        anchors.fill: parent
        anchors.margins: 7

        Rectangle {
            id: mover
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: value / total * (_mouseArea.width - mover.width)
            width: height
            color: Style.barColor

            Behavior on x {
                NumberAnimation {}
            }
        }

        onMouseXChanged: {
            if (mouseX < (mover.width / 2)) {
                value = 0
            }
            else if (mouseX > (width - (mover.width / 2))) {
                value = total
            }
            else {
                var temp = Math.round((mouseX - (mover.width / 2)) / (width - mover.width) * total)
                var snap = false

                for (var i = 0; i < (total / snapInterval) && !snap; i++) {
                    if (Math.abs(snapInterval * i - temp) < snapThreshold) {
                        snap = true
                        temp = snapInterval * i
                    }
                }

                value = temp
            }
        }
    }

    Text {
        text: value + '/' + total
        font.family: 'Nevis'
        anchors.centerIn: parent
        font.pixelSize: 20
    }
}

