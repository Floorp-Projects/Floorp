#include "mozqwidget.h"
#include "nsWindow.h"
#include <qevent.h>

MozQWidget::MozQWidget(nsWindow *receiver, QWidget *parent,
                       const char *name, int f)
    : QWidget(parent, (Qt::WindowFlags)f),
      mReceiver(receiver)
{
}

bool MozQWidget::event(QEvent *e)
{
    bool ignore = false;
    if (!mReceiver) return !ignore;
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
        ignore = mReceiver->OnButtonPressEvent(ms);
    }
    break;
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        ignore = mReceiver->OnButtonReleaseEvent(ms);
    }
    break;
    case QEvent::MouseButtonDblClick:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        ignore = mReceiver->mouseDoubleClickEvent(ms);
    }
    break;
    case QEvent::MouseMove:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        ignore = mReceiver->OnMotionNotifyEvent(ms);
    }
    break;
    case QEvent::KeyPress:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        ignore = mReceiver->OnKeyPressEvent(kev);
    }
    break;
    case QEvent::KeyRelease:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        ignore = mReceiver->OnKeyReleaseEvent(kev);
    }
    break;
/*
    case QEvent::IMStart:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        ignore = mReceiver->imStartEvent(iev);
    }
    break;
    case QEvent::IMCompose:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        ignore = mReceiver->imComposeEvent(iev);
    }
    break;
    case QEvent::IMEnd:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        ignore = mReceiver->imEndEvent(iev);
    }
    break;
*/
    case QEvent::FocusIn:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        mReceiver->OnContainerFocusInEvent(fev);
        return TRUE;
    }
    break;
    case QEvent::FocusOut:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        mReceiver->OnContainerFocusOutEvent(fev);
        return TRUE;
    }
    break;
    case QEvent::Enter:
    {
        ignore = mReceiver->OnEnterNotifyEvent(e);
    }
    break;
    case QEvent::Leave:
    {
        ignore = mReceiver->OnLeaveNotifyEvent(e);
    }
    break;
    case QEvent::Paint:
    {
        QPaintEvent *ev = (QPaintEvent*)(e);
        mReceiver->OnExposeEvent(ev);
    }
    break;
    case QEvent::Move:
    {
        QMoveEvent *mev = (QMoveEvent*)(e);
        ignore = mReceiver->OnConfigureEvent(mev);
    }
    break;
    case QEvent::Resize:
    {
        QResizeEvent *rev = (QResizeEvent*)(e);
        ignore = mReceiver->OnSizeAllocate(rev);
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
        ignore = mReceiver->hideEvent(hev);
    }
        break;
    case QEvent::Close:
    {
        QCloseEvent *cev = (QCloseEvent*)(e);
        ignore = mReceiver->OnDeleteEvent(cev);
    }
    break;
    case QEvent::Wheel:
    {
        QWheelEvent *wev = (QWheelEvent*)(e);
        ignore = mReceiver->OnScrollEvent(wev);
    }
    break;
    case QEvent::ContextMenu:
    {
        QContextMenuEvent *cev = (QContextMenuEvent*)(e);
        ignore = mReceiver->contextMenuEvent(cev);
    }
    break;
    case QEvent::DragEnter:
    {
        QDragEnterEvent *dev = (QDragEnterEvent*)(e);
        ignore = mReceiver->OnDragEnter(dev);
    }
        break;
    case QEvent::DragMove:
    {
        QDragMoveEvent *dev = (QDragMoveEvent*)(e);
        ignore = mReceiver->OnDragMotionEvent(dev);
    }
    break;
    case QEvent::DragLeave:
    {
        QDragLeaveEvent *dev = (QDragLeaveEvent*)(e);
        ignore = mReceiver->OnDragLeaveEvent(dev);
    }
    break;
    case QEvent::Drop:
    {
        QDropEvent *dev = (QDropEvent*)(e);
        ignore = mReceiver->OnDragDropEvent(dev);
    }
    break;
    default:
        break;
    }

    QWidget::event(e);

    return !ignore;
}

void MozQWidget::setModal(bool modal)
{
    if (modal)
        setWindowModality(Qt::ApplicationModal);
    else
        setWindowModality(Qt::NonModal);
}
