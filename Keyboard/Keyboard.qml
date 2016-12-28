import QtQuick 2.3
import 'qrc:/StyleSheet.js' as Style

Rectangle {
    readonly property int maxRows: 10
    readonly property int columnCount: 5
    readonly property int keySpacing: 3
    readonly property int buttonWidth: ((keysColumn.width) / maxRows) - keySpacing
    readonly property int buttonHeight: ((keysColumn.height) / columnCount) - keySpacing

    z: 100
    width: 480 //default
    height: keyboard.open ? keyboard.keyboardHeight : 0
    opacity: keyboard.open ? 1 : 0.5
    visible: (height != 0)
    clip: true

    anchors.left: parent.left
    anchors.right: parent.right

    color: "#80000000"

    Behavior on height {
        PropertyAnimation { duration: 500 }
    }

    Behavior on opacity {
        PropertyAnimation { duration: 500 }
    }

    Rectangle {
        id: rectTop
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 5
        color: Style.accentColor
        z: 101
    }

    Component {
        id: keyDelegate

        KeyboardButton {
            id: _btn
            text: keyboard.shifted ? key.toUpperCase() : key
            width: widthMulti ? buttonWidth * widthMulti : buttonWidth
            height: buttonHeight
            fontSize: 20
            onClicked: {
                functionIds[functionId](_btn)
            }
            imgSource: image
        }
    }

    readonly property var rowIds: [numRowModel, firstRowModel, secondRowModel, thirdRowModel, fourthRowModel]

    function charPressed(btn) {
        keyboard.pressKey(btn.text)
    }

    function shiftPressed(btn) {
        keyboard.pressShift()
    }

    function hidePressed(btn) {
        keyboard.forceClose()
    }

    function spacePressed(btn) {
        keyboard.pressSpace()
    }

    function enterPressed(btn) {
        keyboard.pressEnter()
    }

    function leftPressed(btn) {
        keyboard.pressLeft()
    }

    function rightPressed(btn) {
        keyboard.pressRight()
    }

    function backspacePressed(btn) {
        keyboard.pressBackspace()
    }

    readonly property var functionIds: [charPressed, shiftPressed, hidePressed, spacePressed, enterPressed, leftPressed, rightPressed, backspacePressed]

    ListModel {
        id: numRowModel
        ListElement { key: "1"; functionId: 0; widthMulti: 1; image: "null" }
        ListElement { key: "2"; image: "null"  }
        ListElement { key: "3"; image: "null"  }
        ListElement { key: "4"; image: "null"  }
        ListElement { key: "5"; image: "null"  }
        ListElement { key: "6"; image: "null"  }
        ListElement { key: "7"; image: "null"  }
        ListElement { key: "8"; image: "null"  }
        ListElement { key: "9"; image: "null"  }
        ListElement { key: "0"; image: "null"  }
    }

    ListModel {
        id: firstRowModel
        ListElement { key: "q"; functionId: 0; widthMulti: 1; image: "null" }
        ListElement { key: "w"; image: "null"  }
        ListElement { key: "e"; image: "null"  }
        ListElement { key: "r"; image: "null"  }
        ListElement { key: "t"; image: "null"  }
        ListElement { key: "y"; image: "null"  }
        ListElement { key: "u"; image: "null"  }
        ListElement { key: "i"; image: "null"  }
        ListElement { key: "o"; image: "null"  }
        ListElement { key: "p"; image: "null"  }
    }

    ListModel {
        id: secondRowModel
        ListElement { key: "-"; functionId: 0; widthMulti: 1; image: "null" }
        ListElement { key: "a"; functionId: 0; widthMulti: 1; image: "null" }
        ListElement { key: "s"; image: "null"  }
        ListElement { key: "d"; image: "null"  }
        ListElement { key: "f"; image: "null"  }
        ListElement { key: "g"; image: "null"  }
        ListElement { key: "h"; image: "null"  }
        ListElement { key: "j"; image: "null"  }
        ListElement { key: "k"; image: "null"  }
        ListElement { key: "l"; image: "null"  }
    }

    ListModel {
        id: thirdRowModel
        ListElement { key: "⇑"; functionId: 1; widthMulti: 1; image: "Shift Key.png" }
        ListElement { key: "z"; image: "null"  }
        ListElement { key: "x"; image: "null"  }
        ListElement { key: "c"; image: "null"  }
        ListElement { key: "v"; image: "null"  }
        ListElement { key: "b"; image: "null"  }
        ListElement { key: "n"; image: "null"  }
        ListElement { key: "n"; image: "null"  }
        ListElement { key: "m"; image: "null"  }
        ListElement { key: "⇐"; functionId: 7; image: "Backspace Key.png"  }
    }

    ListModel {
        id: fourthRowModel
        ListElement { key: "⬇"; functionId: 2; widthMulti: 1; image: "Close Key.png" }
        ListElement { key: "."; widthMulti: 0.75; image: "null"  }
        ListElement { key: "◀"; functionId: 5; widthMulti: 1; image: "Left Key.png"  }
        ListElement { key: "Space"; functionId: 3; widthMulti: 4.35; image: "null"  }
        ListElement { key: "▶"; functionId: 6; widthMulti: 1; image: "Right Key.png"  }
        ListElement { key: "⏎"; functionId: 4; widthMulti: 1.75; image: "Enter Key.png"  }
    }

    Component {
        id: rowDelegate

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: keySpacing

            Repeater {
                model: rowIds[rowId]
                delegate: keyDelegate
            }
        }
    }

    ListModel {
        id: alphabetSetModel
        ListElement { rowId: 0 }
        ListElement { rowId: 1 }
        ListElement { rowId: 2 }
        ListElement { rowId: 3 }
        ListElement { rowId: 4 }
    }

    Column {
        id: keysColumn
        spacing: keySpacing
        height: keyboard.keyboardHeight - (5 * keySpacing) // 5 seems to be the magic number (not sure if because column count)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: keySpacing

        Repeater {
            model: alphabetSetModel
            delegate: rowDelegate
        }
    }
}
