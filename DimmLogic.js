.pragma library

var inactiveOpacity = 0.2
var activeItem = null

// We need to store bindable properties in a qml object
var bindable = Qt.createQmlObject(
            "import QtQuick 2.3\n" +
            "QtObject {\n" +
            "   property bool dimmInactives: false\n" +
            "   signal focusItemChanged(Item focusItem)" +
            "}", Qt.application, 'binadbleObject')

function respondItemActive(item) {
    if (item.isActive) {
        activeItem = item
        bindable.focusItemChanged(activeItem)
        bindable.dimmInactives = true
    }
    else {
        if (bindable.dimmInactives && activeItem === item) {
            bindable.dimmInactives = false;
            activeItem = null
        }
    }
}

function registerFocusHandler(handler) {
    bindable.focusItemChanged.connect(handler)
}

// The object is merely a dummy object to steal the focus
function forceNoActive(object) {
    object.forceActiveFocus()
    activeItem = null
    bindable.dimmInactives = false
    bindable.focusItemChanged(object)
}

