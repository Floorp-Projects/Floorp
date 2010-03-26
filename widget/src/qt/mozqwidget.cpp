#include <QtGui/QApplication>
#include <QtGui/QCursor>
#include <QtGui/QInputContext>
#include <QtGui/QGraphicsSceneContextMenuEvent>
#include <QtGui/QGraphicsSceneDragDropEvent>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsSceneHoverEvent>
#include <QtGui/QGraphicsSceneWheelEvent>

#include <QtCore/QEvent>
#include <QtCore/QVariant>

#include "mozqwidget.h"
#include "nsWindow.h"


MozQWidget::MozQWidget(nsWindow* aReceiver, QGraphicsItem* aParent)
    : QGraphicsWidget(aParent),
      mReceiver(aReceiver)
{
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
     setFlag(QGraphicsItem::ItemAcceptsInputMethod);

     setAcceptTouchEvents(true);
     grabGesture(Qt::PinchGesture);
#endif
}

MozQWidget::~MozQWidget()
{
    if (mReceiver)
        mReceiver->QWidgetDestroyed();
}

void MozQWidget::paint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget /*= 0*/)
{
    mReceiver->DoPaint(aPainter, aOption);
}

void MozQWidget::activate()
{
    // ensure that the keyboard is hidden when we activate the window
    hideVKB();
    mReceiver->DispatchActivateEvent();
}

void MozQWidget::deactivate()
{
    // ensure that the keyboard is hidden when we deactivate the window
    hideVKB();
    mReceiver->DispatchDeactivateEvent();
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
}

void MozQWidget::focusOutEvent(QFocusEvent* aEvent)
{
    mReceiver->OnFocusOutEvent(aEvent);
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
    mReceiver->OnMoveEvent(aEvent);
}

void MozQWidget::keyPressEvent(QKeyEvent* aEvent)
{
    mReceiver->OnKeyPressEvent(aEvent);
}

void MozQWidget::keyReleaseEvent(QKeyEvent* aEvent)
{
    mReceiver->OnKeyReleaseEvent(aEvent);
}

void MozQWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* aEvent)
{
    mReceiver->mouseDoubleClickEvent(aEvent);
}

void MozQWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* aEvent)
{
    mReceiver->OnMotionNotifyEvent(aEvent);
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
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
    switch (event->type())
    {
    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
    {
        // Do not send this event to other handlers, this is needed
        // to be able to receive the gesture events
        PRBool handled = PR_FALSE;
        mReceiver->OnTouchEvent(static_cast<QTouchEvent *>(event),handled);
        return handled;
    }
    case (QEvent::Gesture):
    {
        PRBool handled = PR_FALSE;
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
    mReceiver->hideEvent(aEvent);
    QGraphicsWidget::hideEvent(aEvent);
}

void MozQWidget::showEvent(QShowEvent* aEvent)
{
    mReceiver->showEvent(aEvent);
    QGraphicsWidget::showEvent(aEvent);
}

bool MozQWidget::SetCursor(nsCursor aCursor)
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

    return NS_OK;
}

bool MozQWidget::SetCursor(const QPixmap& aCursor, int aHotX, int aHotY)
{
    QCursor bitmapCursor(aCursor, aHotX, aHotY);
    setCursor(bitmapCursor);

    return NS_OK;
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
    return QGraphicsWidget::inputMethodQuery(aQuery);
}

void MozQWidget::showVKB()
{
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
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
    }
#else
    LOG(("VKB not supported in Qt < 4.6\n"));
#endif
}

void MozQWidget::hideVKB()
{
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
    QInputContext *inputContext = qApp->inputContext();
    if (!inputContext) {
        NS_WARNING("Closing SIP: but no input context");
        return;
    }

    QEvent request(QEvent::CloseSoftwareInputPanel);
    inputContext->filterEvent(&request);
    inputContext->reset();
#else
    LOG(("VKB not supported in Qt < 4.6\n"));
#endif
}

bool MozQWidget::isVKBOpen()
{
    // There is no clear API in Pure QT about how to get OPEN/CLOSED vkb state
    // FIXME in bug 555019.
    return PR_FALSE;
}

