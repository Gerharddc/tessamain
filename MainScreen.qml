import QtQuick 2.3
import "StyleSheet.js" as Style
import "Controls"
import "Keyboard"
import "Bottom"
import FBORenderer 1.0

Rectangle {
    id: rootRect
    visible: true
    width: 480
    height: 800
    color: Style.bgMain

    function addStl() {
        qmlFileBrowser.open()
    }

    TopDrawer {
        id: topDrawer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 100;
    }

    FileBrowser {
        id: qmlFileBrowser
        height: parent.height - topDrawer.iClosedHeight
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 90
    }

    Item {
        anchors.fill: parent
        visible: !qmlFileBrowser.visible

        Keyboard {
            objectName: "keyboard"
            anchors.left: parent.left
            anchors.bottom: parent.bottom
        }

        BottomDrawer {
            id: bottomDrawer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            anchors.bottomMargin: keyboard.uiOffset

            Behavior on anchors.bottomMargin {
                PropertyAnimation { }
            }
        }

        Renderer {
            id: renderer
            width: parent.width
            height: parent.height - bottomDrawer.iClosedHeight
            anchors.bottom: parent.bottom
            anchors.bottomMargin: bottomDrawer.isExpanded ? 0 : bottomDrawer.iClosedHeight
            Behavior on anchors.bottomMargin {
                PropertyAnimation {}
            }

            // Move the view down a bit after creation
            Component.onCompleted: {
                //renderer.panView(0, renderer.height / 3);
            }

            meshOpacity: (bottomDrawer.activeTabNum == 0) ? 1 : ((bottomDrawer.activeTabNum == 1) ? 0.5 : 0)
            tpOpacity: (bottomDrawer.activeTabNum == 2) ? 1 : ((bottomDrawer.activeTabNum == 1) ? 0.5 : 0)

            Behavior on meshOpacity {
                PropertyAnimation {}
            }

            Behavior on tpOpacity {
                PropertyAnimation {}
            }

            Behavior on curMeshPos {
                PropertyAnimation {
                    id: curMeshPosAnimation
                }
            }

            Behavior on curMeshScale {
                PropertyAnimation {
                    id: curMeshScaleAnimation
                }
            }

            Behavior on curMeshRot {
                PropertyAnimation {
                    id: curMeshRotAnimation
                }
            }

            Toggle {
                id: toggler
                anchors.top: parent.top
                anchors.topMargin: 7 + topDrawer.iClosedHeight
                anchors.left: parent.left
                anchors.leftMargin: 7
                width: 150
                isDimmable: true
                opacity: bottomDrawer.isExpanded ? 0.0 : 0.6
                nameA: 'Pan'
                nameB: 'Rotate'
                visible: !bottomDrawer.isExpanded
                z: 200
            }

            Button {
                anchors.top: toggler.top
                anchors.right: parent.right
                anchors.rightMargin: 7
                opacity: bottomDrawer.isExpanded ? 0.0 : 0.6
                isDimmable: true
                text: "Reset view"
                width: 150
                z: 200

                onClicked: {
                    renderer.resetView(true)
                }
            }

            PinchArea {
                anchors.fill: parent

                property real lastScale: 1

                onPinchStarted: {
                    lastScale = pinch.scale
                }

                onPinchUpdated: {
                    var scale = pinch.scale / lastScale
                    lastScale = pinch.scale
                    renderer.zoomView(scale)
                }

                MouseArea {
                    id: mouser
                    anchors.fill: parent
                    //scrollGestureEnabled: true // Qt 5.5

                    property real lastX: 0
                    property real lastY: 0
                    property bool started: false
                    property bool click: true

                    onPressed: {
                        lastX = mouseX
                        lastY = mouseY
                        started = true
                        click = true
                    }

                    onReleased: {
                        started = false

                        // Normal onclicked includes drags
                        if (click)
                            renderer.testMouseIntersection(lastX, lastY)
                    }

                    onPositionChanged: {
                        var deltaX = mouseX - lastX
                        var deltaY = mouseY - lastY

                        if ((deltaX != 0) || (deltaY != 0))
                        {
                            if (toggler.toggled)
                                renderer.panView(deltaX, deltaY);
                            else
                                renderer.rotateView(deltaX, deltaY);
                        }

                        lastX = mouseX
                        lastY = mouseY
                        click = false
                    }

                    onWheel: {
                        if (wheel.angleDelta.y > 0)
                            renderer.zoomView(1.1)
                        else if (wheel.angleDelta.y < 0)
                            renderer.zoomView(0.9)
                    }
                }
            }
        }
    }
}

