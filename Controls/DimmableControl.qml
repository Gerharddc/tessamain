import QtQuick 2.3
import "qrc:/DimmLogic.js" as DimmLogic

Rectangle {
    id: dimmable
    property bool isDimmable: false
    property bool isActive: false

    opacity: (isDimmable && DimmLogic.bindable.dimmInactives && !isActive) ? DimmLogic.inactiveOpacity : 1

    Behavior on opacity {
        PropertyAnimation {}
    }

    onIsActiveChanged: {
        DimmLogic.respondItemActive(dimmable)
    }
}

