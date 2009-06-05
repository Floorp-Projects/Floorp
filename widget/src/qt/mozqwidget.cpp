#include "mozqwidget.h"
#include "nsWindow.h"
#include <qevent.h>

MozQWidget::MozQWidget(nsWindow *receiver, QWidget *parent,
                       const char *name, int f)
    : QWidget(parent, (Qt::WindowFlags)f),
      mReceiver(receiver)
{
    setAttribute(Qt::WA_QuitOnClose, false);
}

MozQWidget::~MozQWidget()
{
    if (mReceiver)
        mReceiver->QWidgetDestroyed();
}

bool MozQWidget::event(QEvent *e)
{
    nsEventStatus status = nsEventStatus_eIgnore;
    bool handled = true;

    // always handle (delayed) delete requests triggered by
    // calling deleteLater() on this widget:
    if (e->type() == QEvent::DeferredDelete)
        return QObject::event(e);

    if (!mReceiver)
        return false;

    switch(e->type()) {
/*
    case QEvent::Accessibility:
    {
        qDebug("accessibility event received");
    }
    break;
*/
    case QEvent::MouseButtonPress:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        status = mReceiver->OnButtonPressEvent(ms);
    }
    break;
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        status = mReceiver->OnButtonReleaseEvent(ms);
    }
    break;
    case QEvent::MouseButtonDblClick:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        status = mReceiver->mouseDoubleClickEvent(ms);
    }
    break;
    case QEvent::MouseMove:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        status = mReceiver->OnMotionNotifyEvent(ms);
    }
    break;
    case QEvent::KeyPress:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        status = mReceiver->OnKeyPressEvent(kev);
    }
    break;
    case QEvent::KeyRelease:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        status = mReceiver->OnKeyReleaseEvent(kev);
    }
    break;
/*
    case QEvent::IMStart:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        status = mReceiver->imStartEvent(iev);
    }
    break;
    case QEvent::IMCompose:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        status = mReceiver->imComposeEvent(iev);
    }
    break;
    case QEvent::IMEnd:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        status = mReceiver->imEndEvent(iev);
    }
    break;
*/
    case QEvent::FocusIn:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        mReceiver->OnFocusInEvent(fev);
        return TRUE;
    }
    break;
    case QEvent::FocusOut:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        mReceiver->OnFocusOutEvent(fev);
        return TRUE;
    }
    break;
    case QEvent::Enter:
    {
        status = mReceiver->OnEnterNotifyEvent(e);
    }
    break;
    case QEvent::Leave:
    {
        status = mReceiver->OnLeaveNotifyEvent(e);
    }
    break;
    case QEvent::Paint:
    {
        QPaintEvent *ev = (QPaintEvent*)(e);
        status = mReceiver->OnPaintEvent(ev);
    }
    break;
    case QEvent::Move:
    {
        QMoveEvent *mev = (QMoveEvent*)(e);
        status = mReceiver->OnMoveEvent(mev);
    }
    break;
    case QEvent::Resize:
    {
        QResizeEvent *rev = (QResizeEvent*)(e);
        status = mReceiver->OnResizeEvent(rev);
    }
        break;
    case QEvent::Show:
    {
        QShowEvent *sev = (QShowEvent*)(e);
        mReceiver->showEvent(sev);
    }
    break;
    case QEvent::Hide:
    {
        QHideEvent *hev = (QHideEvent*)(e);
        status = mReceiver->hideEvent(hev);
    }
        break;
    case QEvent::Close:
    {
        QCloseEvent *cev = (QCloseEvent*)(e);
        status = mReceiver->OnCloseEvent(cev);
    }
    break;
    case QEvent::Wheel:
    {
        QWheelEvent *wev = (QWheelEvent*)(e);
        status = mReceiver->OnScrollEvent(wev);
    }
    break;
    case QEvent::ContextMenu:
    {
        QContextMenuEvent *cev = (QContextMenuEvent*)(e);
        status = mReceiver->contextMenuEvent(cev);
    }
    break;
    case QEvent::DragEnter:
    {
        QDragEnterEvent *dev = (QDragEnterEvent*)(e);
        status = mReceiver->OnDragEnter(dev);
    }
        break;
    case QEvent::DragMove:
    {
        QDragMoveEvent *dev = (QDragMoveEvent*)(e);
        status = mReceiver->OnDragMotionEvent(dev);
    }
    break;
    case QEvent::DragLeave:
    {
        QDragLeaveEvent *dev = (QDragLeaveEvent*)(e);
        status = mReceiver->OnDragLeaveEvent(dev);
    }
    break;
    case QEvent::Drop:
    {
        QDropEvent *dev = (QDropEvent*)(e);
        status = mReceiver->OnDragDropEvent(dev);
    }
    break;
    default:
        handled = false;
        break;
    }

    if (handled)
        return true;

    return QWidget::event(e);

#if 0
    // If we were going to ignore this event, pass it up to the parent
    // and return its value
    if (status == nsEventStatus_eIgnore)
        return QWidget::event(e);

    // Otherwise, we know we already consumed it -- we just need to know
    // whether we want default handling or not
    if (status == nsEventStatus_eConsumeDoDefault)
        QWidget::event(e);

    return true;
#endif
}

bool
MozQWidget::SetCursor(nsCursor aCursor)
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

    // qDebug("FIXME:>>>>>>Func:%s::%d, cursor:%i, aCursor:%i\n", __PRETTY_FUNCTION__, __LINE__, cursor, aCursor);
    // FIXME after reimplementation of whole nsWindow SetCursor cause lot of errors
    setCursor(cursor);

    return NS_OK;
}

void MozQWidget::setModal(bool modal)
{
    if (modal)
        setWindowModality(Qt::ApplicationModal);
    else
        setWindowModality(Qt::NonModal);
}

