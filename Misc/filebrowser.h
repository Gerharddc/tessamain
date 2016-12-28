#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <QObject>
#include <QList>
#include <QVariantList>
#include <QDir>
#include <QVariantMap>

class FileBrowser : public QObject
{
    Q_OBJECT

private:
    QVariantList m_fileModel;
    bool isRootDir = true;
    QDir curDir;
    const char * const rootDir = "/home/Simon";
    void GetFilesForDir();

public:
    explicit FileBrowser(QObject *parent = 0);

    Q_INVOKABLE void getRootDirectory();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void selectItem(QString cd);

    Q_PROPERTY(QVariantList fileModel READ fileModel NOTIFY fileModelChanged)
    QVariantList fileModel() { return m_fileModel; }

signals:
    void fileModelChanged();

public slots:
};

#endif // FILEBROWSER_H
