/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QInputContext>
#endif
#include <QtCore/QTimer>
// Solve conflict of qgl.h and GLDefs.h
#define GLdouble_defined 1
#include "mozqwidget.h"
#include "nsWindow.h"

#include "nsIObserverService.h"
#include "mozilla/Services.h"


#ifdef MOZ_ENABLE_QTMOBILITY
#include "mozqorientationsensorfilter.h"
#ifdef MOZ_X11
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
# undef KeyPress
# undef KeyRelease
# undef CursorShape
#endif //MOZ_X11
#endif //MOZ_ENABLE_QTMOBILITY

/*
  Pure Qt is lacking a clear API to get the current state of the VKB (opened
  or closed).
*/
static bool gKeyboardOpen = false;

/*
  In case we could not open the keyboard, we will try again when the focus
  event is sent.  This can happen if the keyboard is asked for before the
  window is focused. This global is used to track that case.
*/
static bool gFailedOpenKeyboard = false;
 
/*
  For websites that focus editable elements during other operations for a very
  short time, we add some decoupling to prevent the VKB from appearing and 
  reappearing for a very short time. This global is set when the keyboard should
  be opened and if it is still set when a timer runs out, the VKB is really
  shown.
*/
static bool gPendingVKBOpen = false;

/*
  Contains the last preedit String, this is needed in order to generate KeyEvents
*/
static QString gLastPreeditString;

MozQWidget::MozQWidget(nsWindow* aReceiver, QGraphicsItem* aParent)
    : mReceiver(aReceiver)
{
     setParentItem(aParent);
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
     setFlag(QGraphicsItem::ItemAcceptsInputMethod);
     setAcceptTouchEvents(true);
#endif
     setAcceptHoverEvents(true);
}

MozQWidget::~MozQWidget()
{
    if (mReceiver)
        mReceiver->QWidgetDestroyed();
}

void MozQWidget::paint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget /*= 0*/)
{
    if (mReceiver)
        mReceiver->DoPaint(aPainter, aOption, aWidget);
}

void MozQWidget::activate()
{
    // ensure that the keyboard is hidden when we activate the window
    hideVKB();
    mReceiver->DispatchActivateEventOnTopLevelWindow();
}

void MozQWidget::deactivate()
{
    // ensure that the keyboard is hidden when we deactivate the window
    hideVKB();
    mReceiver->DispatchDeactivateEventOnTopLevelWindow();
}

void MozQWidget::resizeEvent(QGraphicsSceneResizeEvent* aEvent)
{
    mReceiver->OnResizeEvent(aEvent);
}

void MozQWidget::contextMenuEvent(QGraphicsSceneContextMenuEvent* aEvent)
{
    mReceiver->contextMenuEvent(aEvent);
}

void MozQWidget::dragEnterEvent(QGraphicsSceneDragDropEvent* aEvent)
{
    mReceiver->OnDragEnter(aEvent);
}

void MozQWidget::dragLeaveEvent(QGraphicsSceneDragDropEvent* aEvent)
{
    mReceiver->OnDragLeaveEvent(aEvent);
}

void MozQWidget::dragMoveEvent(QGraphicsSceneDragDropEvent* aEvent)
{
    mReceiver->OnDragMotionEvent(aEvent);
}

void MozQWidget::dropEvent(QGraphicsSceneDragDropEvent* aEvent)
{
    mReceiver->OnDragDropEvent(aEvent);
}

void MozQWidget::focusInEvent(QFocusEvent* aEvent)
{
    mReceiver->OnFocusInEvent(aEvent);

    // The application requested the VKB during startup but did not manage
    // to open it, because there was no focused window yet so we do it now by
    // requesting the VKB without any timeout.
    if (gFailedOpenKeyboard)
        requestVKB(0, this);
}

#ifdef MOZ_ENABLE_QTMOBILITY
void MozQWidget::orientationChanged()
{
    if (!scene() || !scene()->views().size()) {
        return;
    }

    NS_ASSERTION(scene()->views().size() == 1, "Not exactly one view for our scene!");
    QTransform& transform = MozQOrientationSensorFilter::GetRotationTransform();
    QRect scrTrRect = transform.mapRect(scene()->views()[0]->rect());

    setTransformOriginPoint(scene()->views()[0]->size().width() / 2, scene()->views()[0]->size().height() / 2);
    scene()->views()[0]->setTransform(transform);
    int orientation = MozQOrientationSensorFilter::GetWindowRotationAngle();
    if (orientation == 0 || orientation ==  180) {
        setPos(0,0);
    } else {
        setPos(-(scrTrRect.size().width() - scrTrRect.size().height()) / 2,
               (scrTrRect.size().width() - scrTrRect.size().height()) / 2);
    }
    resize(scrTrRect.size());
    scene()->setSceneRect(QRectF(QPointF(0, 0), scrTrRect.size()));
#ifdef MOZ_X11
    Display* display = QX11Info::display();
    if (!display) {
        return;
    }

    Atom orientationAngleAtom = XInternAtom(display, "_MEEGOTOUCH_ORIENTATION_ANGLE", False);
    XChangeProperty(display, scene()->views()[0]->effectiveWinId(),
                    orientationAngleAtom, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)&orientation, 1);
#endif
}
#endif

void MozQWidget::focusOutEvent(QFocusEvent* aEvent)
{
    mReceiver->OnFocusOutEvent(aEvent);
    //OtherFocusReason most like means VKB was closed manual (done button)
    if (aEvent->reason() == Qt::OtherFocusReason && gKeyboardOpen) {
        hideVKB();
    }
}

void MozQWidget::hoverEnterEvent(QGraphicsSceneHoverEvent* aEvent)
{
    mReceiver->OnEnterNotifyEvent(aEvent);
}

void MozQWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent* aEvent)
{
    mReceiver->OnLeaveNotifyEvent(aEvent);
}

void MozQWidget::hoverMoveEvent(QGraphicsSceneHoverEvent* aEvent)
{
    mReceiver->OnMotionNotifyEvent(aEvent->pos(), aEvent->modifiers());
}

void MozQWidget::keyPressEvent(QKeyEvent* aEvent)
{
#if (MOZ_PLATFORM_MAEMO == 6)
    if (!gKeyboardOpen ||
       //those might get sended as KeyEvents, even in 'NormalMode'
       aEvent->key() == Qt::Key_Space ||
       aEvent->key() == Qt::Key_Return ||
       aEvent->key() == Qt::Key_Backspace) {
        mReceiver->OnKeyPressEvent(aEvent);
    }
#elif (MOZ_PLATFORM_MAEMO == 5)
    // Below removed to prevent invertion of upper and lower case
    // See bug 561234
    // mReceiver->OnKeyPressEvent(aEvent);
#else
    mReceiver->OnKeyPressEvent(aEvent);
#endif
}

void MozQWidget::keyReleaseEvent(QKeyEvent* aEvent)
{
#if (MOZ_PLATFORM_MAEMO == 6)
    if (!gKeyboardOpen ||
       //those might get sended as KeyEvents, even in 'NormalMode'
       aEvent->key() == Qt::Key_Space ||
       aEvent->key() == Qt::Key_Return ||
       aEvent->key() == Qt::Key_Backspace) {
        mReceiver->OnKeyReleaseEvent(aEvent);
    }
    return;
#elif (MOZ_PLATFORM_MAEMO == 5)
    // Below line should be removed when bug 561234 is fixed
    mReceiver->OnKeyPressEvent(aEvent);
#endif
    mReceiver->OnKeyReleaseEvent(aEvent);
}

void MozQWidget::inputMethodEvent(QInputMethodEvent* aEvent)
{
    QString currentPreeditString = aEvent->preeditString();
    QString currentCommitString = aEvent->commitString();

    //first check for some controllkeys send as text...
    if (currentCommitString == " ") {
        sendPressReleaseKeyEvent(Qt::Key_Space, currentCommitString.unicode());
    } else if (currentCommitString == "\n") {
        sendPressReleaseKeyEvent(Qt::Key_Return, currentCommitString.unicode());
    } else if (currentCommitString.isEmpty()) {
        //if its no controllkey than check if current Commit is empty
        //if yes than we have some preedit text here
        if (currentPreeditString.length() == 1 && gLastPreeditString.isEmpty()) {
            //Preedit text can change its entire look'a'like
            //check if length of new compared to the old is 1,
            //means that its a new startup
            sendPressReleaseKeyEvent(0, currentPreeditString.unicode());
        } else if (currentPreeditString.startsWith(gLastPreeditString)) {
            //Length was not 1 or not a new startup
            //check if the current preedit starts with the last one,
            //if so: Add new letters (note: this can be more then one new letter)
            const QChar * text = currentPreeditString.unicode();
            for (int i = gLastPreeditString.length(); i < currentPreeditString.length(); i++) {
                sendPressReleaseKeyEvent(0, &text[i]);
            }
        } else {
            //last possible case, we had a PreeditString which was now completely changed.
            //first, check if just one letter was removed (normal Backspace case!)
            //if so: just send the backspace
            QString tempLastPre = gLastPreeditString;
            tempLastPre.truncate(gLastPreeditString.length()-1);
            if (currentPreeditString == tempLastPre) {
                sendPressReleaseKeyEvent(Qt::Key_Backspace);
            } else if (currentPreeditString != tempLastPre) {
                //more than one character changed, so just renew everything
                //delete all preedit
                for (int i = 0; i < gLastPreeditString.length(); i++) {
                    sendPressReleaseKeyEvent(Qt::Key_Backspace);
                }
                //send new Preedit
                const QChar * text = currentPreeditString.unicode();
                for (int i = 0; i < currentPreeditString.length(); i++) {
                    sendPressReleaseKeyEvent(0, &text[i]);
                }
            }
        }
    } else if (gLastPreeditString != currentCommitString) {
        //User commited something
        if (currentCommitString.length() == 1 && gLastPreeditString.isEmpty()) {
            //if commit string ist one and there is no Preedit String
            //case i.e. when no error correction is enabled in the system (default meego.com)
            sendPressReleaseKeyEvent(0, currentCommitString.unicode());
        } else {
            //There is a Preedit, first remove it
            for (int i = 0; i < gLastPreeditString.length(); i++) {
                sendPressReleaseKeyEvent(Qt::Key_Backspace);
            }
            //Now push commited String into
            const QChar * text = currentCommitString.unicode();
            for (int i = 0; i < currentCommitString.length(); i++) {
                sendPressReleaseKeyEvent(0, &text[i]);
            }
        }
    }

    //save preedit for next round.
    gLastPreeditString = currentPreeditString;

    //pre edit is continues string of new chars pressed by the user.
    //if pre edit is changing rapidly without commit string first then user choose some overed text
    //if commitstring comes directly after, forget about it
    QGraphicsWidget::inputMethodEvent(aEvent);
}

void MozQWidget::sendPressReleaseKeyEvent(int key,
                                          const QChar* letter,
                                          bool autorep,
                                          ushort count)
{
    Qt::KeyboardModifiers modifiers  = Qt::NoModifier;
    if (letter && letter->isUpper()) {
        modifiers = Qt::ShiftModifier;
    }

    if (letter) {
        // Handle as TextEvent
        nsCompositionEvent start(true, NS_COMPOSITION_START, mReceiver);
        mReceiver->DispatchEvent(&start);

        nsTextEvent text(true, NS_TEXT_TEXT, mReceiver);
        QString commitString = QString(*letter);
        text.theText.Assign(commitString.utf16());
        mReceiver->DispatchEvent(&text);

        nsCompositionEvent end(true, NS_COMPOSITION_END, mReceiver);
        mReceiver->DispatchEvent(&end);
        return;
    }

    QKeyEvent press(QEvent::KeyPress, key, modifiers, QString(), autorep, count);
    mReceiver->OnKeyPressEvent(&press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, QString(), autorep, count);
    mReceiver->OnKeyReleaseEvent(&release);
}

void MozQWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* aEvent)
{
    // Qt sends double click event, but not second press event.
    mReceiver->OnButtonPressEvent(aEvent);
    mReceiver->OnMouseDoubleClickEvent(aEvent);
}

void MozQWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* aEvent)
{
    mReceiver->OnMotionNotifyEvent(aEvent->pos(), aEvent->modifiers());
}

void MozQWidget::mousePressEvent(QGraphicsSceneMouseEvent* aEvent)
{
    mReceiver->OnButtonPressEvent(aEvent);
}

void MozQWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* aEvent)
{
    mReceiver->OnButtonReleaseEvent(aEvent);
}

bool MozQWidget::event ( QEvent * event )
{
    // check receiver, since due to deleteLater() call it's possible, that
    // events pass loop after receiver's destroy and while widget is still alive
    if (!mReceiver)
        return QGraphicsWidget::event(event);

#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
    switch (event->type())
    {
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
    {
        // Do not send this event to other handlers, this is needed
        // to be able to receive the gesture events
        bool handled = false;
        mReceiver->OnTouchEvent(static_cast<QTouchEvent *>(event),handled);
        return handled;
    }
    case (QEvent::Gesture):
    {
        bool handled = false;
        mReceiver->OnGestureEvent(static_cast<QGestureEvent*>(event),handled);
        return handled;
    }
    default:
        break;
    }
#endif
    return QGraphicsWidget::event(event);
}

void MozQWidget::wheelEvent(QGraphicsSceneWheelEvent* aEvent)
{
    mReceiver->OnScrollEvent(aEvent);
}

void MozQWidget::closeEvent(QCloseEvent* aEvent)
{
    mReceiver->OnCloseEvent(aEvent);
}

void MozQWidget::hideEvent(QHideEvent* aEvent)
{
    if (mReceiver) {
        mReceiver->hideEvent(aEvent);
    }
    QGraphicsWidget::hideEvent(aEvent);
}

void MozQWidget::showEvent(QShowEvent* aEvent)
{
    if (mReceiver) {
        mReceiver->showEvent(aEvent);
    }
    QGraphicsWidget::showEvent(aEvent);
}

void MozQWidget::SetCursor(nsCursor aCursor)
{
    Qt::CursorShape cursor = Qt::ArrowCursor;
    switch(aCursor) {
    case eCursor_standard:
        cursor = Qt::ArrowCursor;
        break;
    case eCursor_wait:
        cursor = Qt::WaitCursor;
        break;
    case eCursor_select:
        cursor = Qt::IBeamCursor;
        break;
    case eCursor_hyperlink:
        cursor = Qt::PointingHandCursor;
        break;
    case eCursor_ew_resize:
        cursor = Qt::SplitHCursor;
        break;
    case eCursor_ns_resize:
        cursor = Qt::SplitVCursor;
        break;
    case eCursor_nw_resize:
    case eCursor_se_resize:
        cursor = Qt::SizeBDiagCursor;
        break;
    case eCursor_ne_resize:
    case eCursor_sw_resize:
        cursor = Qt::SizeFDiagCursor;
        break;
    case eCursor_crosshair:
    case eCursor_move:
        cursor = Qt::SizeAllCursor;
        break;
    case eCursor_help:
        cursor = Qt::WhatsThisCursor;
        break;
    case eCursor_copy:
    case eCursor_alias:
        break;
    case eCursor_context_menu:
    case eCursor_cell:
    case eCursor_grab:
    case eCursor_grabbing:
    case eCursor_spinning:
    case eCursor_zoom_in:
    case eCursor_zoom_out:

    default:
        break;
    }

    setCursor(cursor);
}

void MozQWidget::SetCursor(const QPixmap& aCursor, int aHotX, int aHotY)
{
    QCursor bitmapCursor(aCursor, aHotX, aHotY);
    setCursor(bitmapCursor);
}

void MozQWidget::setModal(bool modal)
{
#if QT_VERSION >= 0x040600
    setPanelModality(modal ? QGraphicsItem::SceneModal : QGraphicsItem::NonModal);
#else
    LOG(("Modal QGraphicsWidgets not supported in Qt < 4.6\n"));
#endif
}

QVariant MozQWidget::inputMethodQuery(Qt::InputMethodQuery aQuery) const
{
    // Additional MeeGo Touch queries, which do not depend on actually
    // having a focused input field.
    switch ((int) aQuery) {
    case 10001: // VisualizationPriorityQuery.
        // Tells if input method widget wants to have high priority
        // for visualization. Input methods should honor this and stay
        // out of widgets space.
        // Return false, eg. the input method can overlap QGraphicsWKView.
        return QVariant(false);
    case 10003: // ImCorrectionEnabledQuery.
        // Explicit correction enabling for text entries.
        return QVariant(false);
    case 10004: // ImModeQuery.
        // Retrieval mode: normal (0), direct [minics hardware keyboard] (1) or proxy (2)
        return QVariant::fromValue(0);
    case 10006: // InputMethodToolbarQuery.
        // Custom toolbar file name for text entry.
        return QVariant();
    }

    // Standard Qt queries dependent on having a focused web text input.
    switch (aQuery) {
    case Qt::ImFont:
        return QVariant(QFont());
    case Qt::ImMaximumTextLength:
        return QVariant(); // Means no limit.
    }

    // Additional MeeGo Touch queries dependent on having a focused web text input
    switch ((int) aQuery) {
    case 10002: // PreeditRectangleQuery.
        // Retrieve bounding rectangle for current preedit text.
        return QVariant(QRect());
    }

    return QVariant();
}

/**
  Request the VKB and starts a timer with the given timeout in milliseconds.
  If the request is not canceled when the timer runs out, the VKB is actually
  shown.
*/
void MozQWidget::requestVKB(int aTimeout, QObject* aWidget)
{
    if (!gPendingVKBOpen) {
        gPendingVKBOpen = true;

        if (aTimeout == 0 || !aWidget) {
            showVKB();
        } else {
            QTimer::singleShot(aTimeout, aWidget, SLOT(showVKB()));
        }
    }
}

void MozQWidget::showVKB()
{
    // skip showing of keyboard if not pending
    if (!gPendingVKBOpen) {
        return;
    }

    gPendingVKBOpen = false;

#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)) && (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    QWidget* focusWidget = qApp->focusWidget();

    if (focusWidget) {
        QInputContext *inputContext = qApp->inputContext();
        if (!inputContext) {
            NS_WARNING("Requesting SIP: but no input context");
            return;
        }

        QEvent request(QEvent::RequestSoftwareInputPanel);
        inputContext->filterEvent(&request);
        focusWidget->setAttribute(Qt::WA_InputMethodEnabled, true);
        inputContext->setFocusWidget(focusWidget);
        gKeyboardOpen = true;
        gFailedOpenKeyboard = false;
    }
    else
    {
        // No focused widget yet, so we have to open the VKB later on.
        gFailedOpenKeyboard = true;
    }
#else
    LOG(("VKB not supported in Qt < 4.6\n"));
#endif
}

void MozQWidget::hideVKB()
{
    if (gPendingVKBOpen) {
        // do not really open
        gPendingVKBOpen = false;
    }

    if (!gKeyboardOpen) {
        return;
    }

#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)) && (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    QInputContext *inputContext = qApp->inputContext();
    if (!inputContext) {
        NS_WARNING("Closing SIP: but no input context");
        return;
    }

    QEvent request(QEvent::CloseSoftwareInputPanel);
    inputContext->filterEvent(&request);
    inputContext->reset();
    gKeyboardOpen = false;
#else
    LOG(("VKB not supported in Qt < 4.6\n"));
#endif
}

bool MozQWidget::isVKBOpen()
{
    return gKeyboardOpen;
}

void
MozQWidget::NotifyVKB(const QRect& rect)
{
    QRegion region(scene()->views()[0]->rect());
    region -= rect;
    QRectF bounds = mapRectFromScene(region.boundingRect());
        nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (observerService) {
        QString rect = QString("{\"left\": %1, \"top\": %2, \"right\": %3, \"bottom\": %4}")
                               .arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height());
        observerService->NotifyObservers(nullptr, "softkb-change", rect.utf16());
    }
}

void MozQWidget::SwitchToForeground()
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os)
        return;
    os->NotifyObservers(nullptr, "application-foreground", nullptr);
}

void MozQWidget::SwitchToBackground()
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os)
        return;
    os->NotifyObservers(nullptr, "application-background", nullptr);
}

