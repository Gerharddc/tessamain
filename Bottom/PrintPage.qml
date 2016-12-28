import QtQuick 2.3
import "qrc:/Controls"

BottomPage {
    id: printPage
    contentHeightPlus: 100

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        Button {
            id: btnPrint
            text: printer.printing ? "Stop printing" : "Print model"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 5
            isDimmable: true
            enabled: renderer.toolPathLoaded

            onClicked: {
                if (printer.printing)
                    printer.stopPrint()
                else
                    renderer.printToolpath()
            }
        }

        Button {
            id: btnHAll
            text: "Home all"
            anchors.top: btnPrint.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 15
            isDimmable: true
            enabled: !printer.printing

            onClicked: {
                printer.homeAll()
            }
        }

        Row {
            id: rowHome
            spacing: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: btnHAll.bottom
            anchors.topMargin: 15

            Button {
                width: (parent.width - 20) / 3
                text: "Home X"
                isDimmable: true
                enabled: !printer.printing

                onClicked: {
                    printer.homeX()
                }
            }

            Button {
                width: (parent.width - 20) / 3
                text: "Home Y"
                isDimmable: true
                enabled: !printer.printing

                onClicked: {
                    printer.homeY()
                }
            }

            Button {
                width: (parent.width - 20) / 3
                text: "Home Z"
                isDimmable: true
                enabled: !printer.printing

                onClicked: {
                    printer.homeZ()
                }
            }
        }

        Row {
            id: rowMove
            spacing: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: rowHome.bottom
            anchors.topMargin: 25

            Item {
                width: (parent.width - 20) / 3
                height: childrenRect.height

                Label {
                    id: txtX
                    text: 'X Dist:'
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    isDimmable: true
                }

                TextBox {
                    id: tboxX
                    anchors.top: txtX.bottom
                    anchors.topMargin: 5
                    anchors.left: parent.left
                    anchors.right: parent.right
                    isDimmable: true
                    enabled: !printer.printing
                    text: "0"
                }
            }

            Item {
                width: (parent.width - 20) / 3
                height: childrenRect.height

                Label {
                    id: txtY
                    text: 'Y Dist:'
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    isDimmable: true
                }

                TextBox {
                    id: tboxY
                    anchors.top: txtY.bottom
                    anchors.topMargin: 5
                    anchors.left: parent.left
                    anchors.right: parent.right
                    isDimmable: true
                    enabled: !printer.printing
                    text: "0"
                }
            }

            Item {
                width: (parent.width - 20) / 3
                height: childrenRect.height

                Label {
                    id: txtZ
                    text: 'Z Dist:'
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    isDimmable: true
                }

                TextBox {
                    id: tboxZ
                    anchors.top: txtZ.bottom
                    anchors.topMargin: 5
                    anchors.left: parent.left
                    anchors.right: parent.right
                    isDimmable: true
                    enabled: !printer.printing
                    text: "0"
                }
            }
        }

        Row {
            id: btnMove
            spacing: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: rowMove.bottom
            anchors.topMargin: 10

            Button {
                width: (parent.width - 20) / 3
                text: "Move X"
                isDimmable: true
                enabled: !printer.printing

                onClicked: {
                    printer.moveX(tboxX.text)
                }
            }

            Button {
                width: (parent.width - 20) / 3
                text: "Move Y"
                isDimmable: true
                enabled: !printer.printing

                onClicked: {
                    printer.moveY(tboxY.text)
                }
            }

            Button {
                width: (parent.width - 20) / 3
                text: "Move Z"
                isDimmable: true
                enabled: !printer.printing

                onClicked: {
                    printer.moveZ(tboxZ.text)
                }
            }
        }

        /*Button {
            id: btnMove
            text: "Move all"
            anchors.top: rowMove.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 10
            isDimmable: true
            enabled: !printer.printing

            onClicked: {
                printer.move(tboxX.text, tboxY.text, tboxZ.text)
            }
        }*/

        Label {
            id: lblMove
            anchors.top: btnMove.bottom
            anchors.topMargin: 15
            isDimmable: true
            text: "Target temparture:"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Item {
            id: itmTemp
            anchors.top: lblMove.bottom
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.right: parent.right
            height: childrenRect.height

            TextBox {
                id: tboxTemp
                isDimmable: true
                enabled: !printer.printing
                width: (parent.width - 10) / 2
                anchors.left: parent.left
                height: tglTemp.height

                Binding on text {
                    when: !tboxTemp.isActive
                    value: printer.targetTemp.toFixed(2).replace(/\.?0+$/, "")
                }
            }

            Binding {
                target: printer
                property: "targetTemp"
                value: tboxTemp.text
            }

            Toggle {
                id: tglTemp
                nameA: 'Heating'
                nameB: 'Not heating'
                isDimmable: true
                enabled: !printer.printing
                anchors.right: parent.right
                anchors.left: tboxTemp.right
                anchors.leftMargin: 10
                toggled: printer.heating
            }

            Binding {
                target: printer
                property: "heating"
                value: tglTemp.toggled
            }
        }

        Item {
            id: itmExt
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: itmTemp.bottom
            anchors.topMargin: 30
            height: childrenRect.height

            TextBox {
                id: tboxExt
                anchors.left: parent.left
                isDimmable: true
                enabled: !printer.printing
                width: (parent.width - 10) / 2
                text: "0"
                height: btnExt.height
            }

            Button {
                id: btnExt
                anchors.right: parent.right
                anchors.left: tboxExt.right
                anchors.leftMargin: 10
                text: "Extrude"
                enabled: !printer.printing
                onClicked: {
                    printer.extrude(tboxExt.text)
                }
            }
        }

        Item {
            id: itmFan
            anchors.top: itmExt.bottom
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.right: parent.right
            height: childrenRect.height

            Toggle {
                id: tglAutoFan
                nameA: "Autofan on"
                nameB: "Autofan off"
                isDimmable: true
                width: (parent.width - 10) / 2
                anchors.left: parent.left
                toggled: printer.autoFan
            }

            Binding {
                target: printer
                property: "autoFan"
                value: tglAutoFan.toggled
            }

            Toggle {
                id: tglFan
                nameA: 'Fan on'
                nameB: 'Fan off'
                isDimmable: true
                anchors.right: parent.right
                anchors.left: tglAutoFan.right
                anchors.leftMargin: 10

                Binding on toggled {
                    value: printer.fanning
                }
            }

            Binding {
                target: printer
                property: "fanning"
                value: tglFan.toggled
            }
        }
    }
}
