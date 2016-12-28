#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QObject>
#include <QQuickItem>
#include <QQuickView>

class Keyboard : public QObject
{
    Q_OBJECT

private:
    bool m_open = false;
    void emitKey(int key, QString keyText = NULL);
    int m_uiOffset = 0;
    QQuickItem *qmlKeyboard = NULL;
    const int m_keyboardHeight = 350;
    QQuickItem *focusedItem = NULL;
    bool m_shifted = false;
    QQuickView *viewObj;

public:
    explicit Keyboard(QQuickView *view, QObject *parent = 0);

    Q_INVOKABLE void pressKey(QString c);
    Q_INVOKABLE void pressRight();
    Q_INVOKABLE void pressLeft();
    Q_INVOKABLE void pressEnter();
    Q_INVOKABLE void pressSpace();
    Q_INVOKABLE void pressBackspace();
    Q_INVOKABLE void pressShift();

    Q_INVOKABLE void requestOpen(QQuickItem *item);
    Q_INVOKABLE void requestClose(QQuickItem *item);
    Q_INVOKABLE void forceClose();

    void setQmlKeyboard(QQuickItem *item);

    Q_PROPERTY(bool open READ open WRITE setOpen NOTIFY openChanged)
    void setOpen(bool a);
    bool open();

    Q_PROPERTY(int keyboardHeight READ keyboardHeight NOTIFY keyboardHeightChanged)
    int keyboardHeight() { return m_keyboardHeight; }

    Q_PROPERTY(int uiOffset READ uiOffset NOTIFY uiOffsetChanged)
    int uiOffset() { return m_uiOffset; }

    Q_PROPERTY(bool shifted READ shifted WRITE setShifted NOTIFY shiftedChanged)
    bool shifted() { return m_shifted; }
    void setShifted(bool a);

signals:
   void openChanged();
   void keyboardHeightChanged();
   void uiOffsetChanged();
   void shiftedChanged();
};

#endif // KEYBOARD_H
