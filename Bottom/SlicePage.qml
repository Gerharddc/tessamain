import QtQuick 2.3
import "qrc:/Controls"

BottomPage {
    id: slicePage
    contentHeight: settingsModel.count * 90 + 150

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        Label {
            id: txtStatus
            text: 'Satus: ' + renderer.slicerStatus
            anchors.top: parent.top
            anchors.topMargin: 5
            anchors.left: parent.left
            anchors.right: parent.right
            isDimmable: true
        }

        Button {
            id: btnSlice
            text: renderer.slicerRunning ? "Stop slicing" : "Start slicing"
            anchors.top: txtStatus.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 15
            isDimmable: true
            enabled: renderer.meshCount > 0

            onClicked: {
                renderer.sliceMeshes()
            }
        }

        ListModel {
            id: settingsModel
            ListElement { title: "Bed Width"; setting: "bedWidth"; }
            ListElement { title: "Bed Length"; setting: "bedLength"; }
            ListElement { title: "Bed Height"; setting: "bedHeight"; }
            ListElement { title: "Infill Density"; setting: "infillDensity"; }
            ListElement { title: "Layer Height"; setting: "layerHeight"; }
            ListElement { title: "Skirt Line Count"; setting: "skirtLineCount"; }
            ListElement { title: "Skirt Distance"; setting: "skirtDistance"; }
            ListElement { title: "Print Speed"; setting: "printSpeed"; }
            ListElement { title: "Infill Speed"; setting: "infillSpeed"; }
            ListElement { title: "Top/Bottom Speed"; setting: "topBottomSpeed"; }
            ListElement { title: "First Line Speed"; setting: "firstLineSpeed"; }
            ListElement { title: "Travel Speed"; setting: "travelSpeed"; }
            ListElement { title: "Retraction Speed"; setting: "retractionSpeed"; }
            ListElement { title: "Retraction Distance"; setting: "retractionDistance"; }
            ListElement { title: "Shell Thickness"; setting: "shellThickness"; }
            ListElement { title: "Top Bottom Thickness"; setting: "topBottomThickness"; }
            ListElement { title: "Print Temperature"; setting: "printTemperature"; }
        }

        Component {
            id: settingsDelegate

            Item {
                width: parent.width
                height: childrenRect.height

                Label {
                    id: setLabel
                    text: title + ":"
                    width: parent.width
                    isDimmable: true
                }

                TextBox {
                    id: setTBox
                    anchors.top: setLabel.bottom
                    anchors.topMargin: 5
                    width: parent.width
                    isDimmable: true

                    Binding on text {
                        when: !setTBox.isActive
                        value: settings[setting].toFixed(2).replace(/\.?0+$/, "")
                    }

                    Binding {
                        target: settings
                        property: setting
                        value: setTBox.text
                    }
                }
            }
        }

        Column {
            id: settingsColumn
            spacing: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: btnSlice.bottom
            anchors.topMargin: 15

            Repeater {
                model: settingsModel
                delegate: settingsDelegate
            }
        }
    }
}
