TEMPLATE = app

QT += qml quick serialport
CONFIG += c++11

SOURCES += main.cpp \
    ChopperEngine/chopperengine.cpp \
    ChopperEngine/clipper.cpp \
    Misc/filebrowser.cpp \
    Misc/globalsettings.cpp \
    Misc/qtsettings.cpp \
    Rendering/comborendering.cpp \
    Rendering/fborenderer.cpp \
    Rendering/gcodeimporting.cpp \
    Rendering/glhelper.cpp \
    Rendering/gridrendering.cpp \
    Rendering/loadedgl.cpp \
    Rendering/stlexporting.cpp \
    Rendering/stlimporting.cpp \
    Rendering/stlrendering.cpp \
    Rendering/structures.cpp \
    Rendering/toolpathrendering.cpp \
    Keyboard/keyboard.cpp \
    Printer/printer.cpp \
    ChopperEngine/mesh.cpp \
    ChopperEngine/progressor.cpp \
    ChopperEngine/linewriter.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

HEADERS += \
    ChopperEngine/chopperengine.h \
    ChopperEngine/clipper.hpp \
    ChopperEngine/pmvector.h \
    Misc/delegate.h \
    Misc/filebrowser.h \
    Misc/globalsettings.h \
    Misc/qtsettings.h \
    Misc/strings.h \
    Rendering/comborendering.h \
    Rendering/fborenderer.h \
    Rendering/gcodeimporting.h \
    Rendering/glhelper.h \
    Rendering/gridgeneration.h \
    Rendering/gridrendering.h \
    Rendering/loadedgl.h \
    Rendering/mathhelper.h \
    Rendering/stlexporting.h \
    Rendering/stlimporting.h \
    Rendering/stlrendering.h \
    Rendering/structures.h \
    Rendering/toolpathrendering.h \
    Keyboard/keyboard.h \
    Printer/gcode.h \
    Printer/printer.h \
    ChopperEngine/toolpath.h \
    ChopperEngine/meshinfo.h \
    ChopperEngine/mesh.h \
    ChopperEngine/progressor.h \
    ChopperEngine/linewriter.h

DEFINES += QT_APPLICATION

CONFIG(rpi):DEFINES += \
    GLES    \
    ROTATE_SCREEN \
    REAL_PRINTER
