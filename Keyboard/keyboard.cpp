#include "keyboard.h"

#include <QKeyEvent>
#include <QCoreApplication>
#include <QKeySequence>
#include <QtGlobal>
#include <QDebug>
//#define VERBOS_KEYBOARD

Keyboard::Keyboard(QQuickView *view, QObject *parent) : QObject(parent)
{
    viewObj = view;
}

void Keyboard::setOpen(bool a)
{
    if (a != m_open) {
        m_open = a;

        emit openChanged();
    }
}

void Keyboard::setShifted(bool a)
{
    if (a != m_shifted) {
        m_shifted = a;

        emit shiftedChanged();
    }
}

bool Keyboard::open()
{
    return m_open;
}

void Keyboard::emitKey(int key, QString keyText)
{
    QQuickItem* receiver = qobject_cast<QQuickItem*>(viewObj->focusObject());

    if(!receiver) {
        qDebug() << "No receiver for keyboard";
        return;
    }

    if (keyText == NULL) {
        keyText = QKeySequence(key).toString();
    }

    QKeyEvent* pressEvent = new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier, keyText);
    QKeyEvent* releaseEvent = new QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(receiver, pressEvent);
    QCoreApplication::sendEvent(receiver, releaseEvent);

    setShifted(false);
}

void Keyboard::pressKey(QString c)
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing key: " << c[0];
    #endif

    emitKey(QKeySequence(c)[0], c);
}

void Keyboard::pressRight()
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing right";
    #endif

    emitKey(Qt::Key_Right);
}

void Keyboard::pressLeft()
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing left";
    #endif

    emitKey(Qt::Key_Left);
}

void Keyboard::pressEnter()
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing enter";
    #endif

    emitKey(Qt::Key_Enter);
}

void Keyboard::pressSpace()
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing space";
    #endif

    emitKey(Qt::Key_Space, " "); //With " " the word Space is entered
}

void Keyboard::pressBackspace()
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing backspace";
    #endif

    emitKey(Qt::Key_Backspace, " "); //With " " the word Space is entered
}

void Keyboard::pressShift()
{
#ifdef VERBOS_KEYBOARD
    qDebug() << "Pressing shift";
#endif

    setShifted(!shifted());
}

void Keyboard::requestOpen(QQuickItem *item)
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Requested open";
    #endif

    focusedItem = item;

    if (qmlKeyboard != NULL)
    {
        // The ui offset differs depending on if the keyboard is open or not
        QPointF newPos = item->mapToItem(qmlKeyboard, QPointF(0, item->height()));
        int newOffset = 0;

        if (m_open)
        {
            newOffset = m_uiOffset + newPos.y() + 5;
            if (newOffset < 0)
                newOffset = 0;
        }
        else
        {
            newOffset = newPos.y() + m_keyboardHeight;
            if (newOffset > 0)
                newOffset += 5;
            else newOffset = 0;
        }

        if (newOffset != m_uiOffset)
        {
            m_uiOffset = newOffset;
            emit uiOffsetChanged();
        }
    }

    setOpen(true);
}

void Keyboard::requestClose(QQuickItem *item)
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Requested close";
    #endif

    if (focusedItem == item)
    {
        setOpen(false);
        focusedItem = NULL;

        m_uiOffset = 0;
        emit uiOffsetChanged();
    }
}

void Keyboard::forceClose()
{
    #ifdef VERBOS_KEYBOARD
    qDebug() << "Requested force close";
    #endif

    // Working directly with the focusedItem does not work for some reason
    QQuickItem* focusee = qobject_cast<QQuickItem*>(viewObj->focusObject());
    if (focusee)
        focusee->setFocus(false);

    setOpen(false); // This is jsut redundancy as the previous statement should cause it anyway if the textbox is set uo correctly

    m_uiOffset = 0;
    emit uiOffsetChanged();
}

void Keyboard::setQmlKeyboard(QQuickItem *item)
{
    qmlKeyboard = item;
}

