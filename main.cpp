#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QFontDatabase>
#include <QQuickItem>

#include "Keyboard/keyboard.h"
#include "Rendering/fborenderer.h"
#include "Printer/printer.h"
#include "Misc/filebrowser.h"
#include "Misc/globalsettings.h"
#include "Misc/qtsettings.h"

#include <QFile>
#include <QString>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

#ifdef ANDROID
#define FILECOUNT 3
    QString* names = new QString[FILECOUNT];
    names[0] = "bin.stl";
    names[1] = "cube.vsh";
    names[2] = "cube.fsh";
    for (uint8_t i = 0; i < FILECOUNT; i++)
    {
        QFile dfile("assets:/" + names[i]);
        if (dfile.exists())
        {
            QString name = "./" + names[i];
            dfile.copy(name);
            QFile::setPermissions(name, QFile::ReadOwner);
        }
    }
    delete[] names;
#endif

    // Initialize
    GlobalSettings::LoadSettings();
    GlobalPrinter.Connect();

    qmlRegisterType<FBORenderer>("FBORenderer", 1, 0, "Renderer");

    QFontDatabase database;
    database.addApplicationFont(":/nevis.ttf");

    QQuickView view;

    Keyboard keyboard(&view);
    view.rootContext()->setContextProperty("keyboard", &keyboard);

    FileBrowser fb;
    view.rootContext()->setContextProperty("fileBrowser", &fb);
    view.rootContext()->setContextProperty("settings", &qtSettings);
    view.rootContext()->setContextProperty("printer", &GlobalPrinter);

#ifdef ROTATE_SCREEN
#define ROOTQML "qrc:/Rotated.qml"
    std::cout << "Running rotated" << std::endl;
#else
#define ROOTQML "qrc:/MainScreen.qml"
#endif

    view.setSource(QUrl(QStringLiteral(ROOTQML)));

    QObject *root = view.rootObject();
    QObject *qmlKeyboard = root->findChild<QObject*>("keyboard");
    keyboard.setQmlKeyboard(qobject_cast<QQuickItem*>(qmlKeyboard));

    view.show();

    //GlobalSettings::InfillLineDistance.Set(31.0f);
    //GlobalSettings::BedLength.Set(200.0f);

    auto result = app.exec();

    // Finialize
    GlobalSettings::StopSavingLoop();
    GlobalSettings::SaveSettings();

    return result;
}

