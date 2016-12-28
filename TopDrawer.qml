import QtQuick 2.3
import "StyleSheet.js" as Style
import "qrc:/Controls"

Rectangle {
    id: theTopDrawer
    color: Style.bgRed
    clip: true

    readonly property int iExpandedHeight: 160
    readonly property int iClosedHeight: 70

    width: 480 // Default
    height: isExpanded ? iExpandedHeight : iClosedHeight

    property bool isExpanded: false

    Behavior on height {
        PropertyAnimation { duration: 400; }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 5
        color:  Style.accentColor
    }

    Image {
        id: img_Expander
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 15
        anchors.rightMargin: 15
        width: 50
        height: 50
        source: "Images/Chevron Down-50.png"
        rotation: isExpanded ? (flipMouse.pressed ? 0 : 180) : (flipMouse.pressed ? 180 : 0)

        Behavior on rotation {
            PropertyAnimation {}
        }

        MouseArea {
            id: flipMouse
            anchors.centerIn: parent
            width: parent.width + 10
            height: parent.width + 10

            onClicked: {
                theTopDrawer.isExpanded = !theTopDrawer.isExpanded
            }
        }
    }

    Label {
        text: printer.curTemp.toFixed(2).replace(/\.?0+$/, "") + "/"
              + printer.targetTemp.toFixed(2).replace(/\.?0+$/, "") + "°C"
        anchors.verticalCenter: img_Expander.verticalCenter
        anchors.right: img_Expander.left
        anchors.rightMargin: 15
    }

    Label {
        text: printer.status
        anchors.verticalCenter: img_Expander.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 15
    }

    Item {
        // Hidden area
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: img_Expander.top
        anchors.bottomMargin: 15
        height: iExpandedHeight - iClosedHeight - 10 // -15+5

        Button {
            id: estpBtn
            text: "Emergency Stop"
            height: 50
            width: 180
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 15

            onClicked: {
                printer.emergencyStop()
            }
        }

        Button {
            text: printer.paused ? "Resume" : "Pause"
            height: 50
            width: 120
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: estpBtn.right
            anchors.leftMargin: 15

            onClicked: {
                printer.pauseResume()
            }
        }

        Button {
            text: "Home"
            height: 50
            width: 100
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 15

            onClicked: {
                printer.homeAll()
            }
        }
    }
}

