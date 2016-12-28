import QtQuick 2.3

Item {
    visible: true
    width: main.height
    height: main.width

    MainScreen {
        id: main
        rotation: 270
        anchors.centerIn: parent
    }
}

