import QtQuick 2.3
import "qrc:/StyleSheet.js" as Style
import "qrc:/DimmLogic.js" as DimmLogic

Item {
    id: theBottomDrawer
    z: 10
    clip: true
    
    readonly property int iClosedHeight: 70
    readonly property int iExpandedHeight: (800 - iClosedHeight + 5)
    readonly property int closeDuration: 500

    property bool isExpanded: false

    width: 480 // default
    height: isExpanded ? iExpandedHeight : iClosedHeight

    Behavior on height {
        PropertyAnimation
        {
            duration: closeDuration
        }
    }

    onIsExpandedChanged: {
        if (!isExpanded)
            keyboard.forceClose()
    }

    Rectangle {
        id: bottomBG
        color: Style.overlayGrey
        anchors.fill: parent

        opacity: isExpanded ? (DimmLogic.bindable.dimmInactives ? 0.2 : 0.5) : 1

        Behavior on opacity {
            PropertyAnimation {}
        }
    }

    Rectangle {
        id: rect_Chrome
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 5
        color:  Style.accentColor
    }

    Image {
        id: img_Expander
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 15
        anchors.rightMargin: 15
        width: 50
        height: 50
        source: "qrc:/Images/Chevron Down-50.png"
        rotation: isExpanded ? (flipMouse.pressed ? 180 : 0) : (flipMouse.pressed ? 0 : 180)

        Behavior on rotation {
            PropertyAnimation {}
        }

        MouseArea {
            id: flipMouse
            width: parent.width + 10
            height: parent.width + 10

            onClicked: {
                theBottomDrawer.isExpanded = !theBottomDrawer.isExpanded
            }
        }
    }

    property int activeTabNum: 0

    function enableTab(tabRef) {
        activeTabNum = tabRef.tabNum
    }

    onActiveTabNumChanged: {
        DimmLogic.forceNoActive(theBottomDrawer)
    }

    Row {
        id: tabBar
        z: 10

        anchors.left:parent.left
        anchors.right: img_Expander.left
        anchors.top: rect_Chrome.bottom
        anchors.rightMargin: 10
        height: iClosedHeight - 5
        spacing: 0

        BottomTab {
            id: modelTab
            tabText: 'Model'
            tabNum: 0
            height: parent.height
            width: parent.width / 3
            onTabClicked: enableTab(modelTab)
        }

        BottomTab {
            id: sliceTab
            tabText: 'Slice'
            tabNum: 1
            height: parent.height
            width: parent.width / 3
            onTabClicked: enableTab(sliceTab)
        }

        BottomTab {
            id: printTab
            tabText: 'Print'
            tabNum: 2
            height: parent.height
            width: parent.width / 3
            onTabClicked: enableTab(printTab)
        }
    }

    Rectangle {
        id: tabSlider
        anchors.top: tabBar.top
        anchors.bottom: tabBar.bottom
        width: modelTab.width
        anchors.left: tabBar.left
        anchors.leftMargin: activeTabNum * modelTab.width
        color: Style.bgRed
        z: 5
        //opacity: DimmLogic.hideInactives ? 0.5 : 1

        Behavior on anchors.leftMargin {
            PropertyAnimation {}
        }
    }

    Item {
        id: bottomPages
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: (iExpandedHeight - tabBar.height)
        visible: (opacity == 0) ? false : true
        opacity: isExpanded ? 1 : 0

        Behavior on opacity {
            PropertyAnimation
            {
                duration: closeDuration
            }
        }

        Item {
            id: pageContainer

            width: 3 * parent.width
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: -(activeTabNum * parent.width)

            Behavior on anchors.leftMargin {
                PropertyAnimation {}
            }

            ModelPage {
                id: modelPage
                width: bottomPages.width
                anchors.left: parent.left
            }

            SlicePage {
                id: slicePage
                width: bottomPages.width
                anchors.left: modelPage.right
            }

            PrintPage {
                id: printPage
                width: bottomPages.width
                anchors.left: slicePage.right
            }
        }
    }
}

