#include "mozqwidget.h"

#include "nsCommonWidget.h"

#include <qevent.h>

MozQWidget::MozQWidget(nsCommonWidget *receiver, QWidget *parent,
                       const char *name, WFlags f)
    : QWidget(parent, name, f),
      mReceiver(receiver)
{
}

bool MozQWidget::event(QEvent *e)
{
    bool ignore = false;
    switch(e->type()) {
    case QEvent::Accessibility:
    {
        qDebug("accessibility event received");
    }
    break;
    case QEvent::MouseButtonPress:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        ignore = mReceiver->mousePressEvent(ms);
    }
    break;
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        ignore = mReceiver->mouseReleaseEvent(ms);
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
        ignore = mReceiver->mouseMoveEvent(ms);
    }
    break;
    case QEvent::KeyPress:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        ignore = mReceiver->keyPressEvent(kev);
    }
    break;
    case QEvent::KeyRelease:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        ignore = mReceiver->keyReleaseEvent(kev);
    }
    break;
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
    case QEvent::FocusIn:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        mReceiver->focusInEvent(fev);
        return TRUE;
    }
    break;
    case QEvent::FocusOut:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        mReceiver->focusOutEvent(fev);
        return TRUE;
    }
    break;
    case QEvent::Enter:
    {
        ignore = mReceiver->enterEvent(e);
    }
    break;
    case QEvent::Leave:
    {
        ignore = mReceiver->enterEvent(e);
    }
    break;
    case QEvent::Paint:
    {
        QPaintEvent *ev = (QPaintEvent*)(e);
        mReceiver->paintEvent(ev);
    }
    break;
    case QEvent::Move:
    {
        QMoveEvent *mev = (QMoveEvent*)(e);
        ignore = mReceiver->moveEvent(mev);
    }
    break;
    case QEvent::Resize:
    {
        QResizeEvent *rev = (QResizeEvent*)(e);
        ignore = mReceiver->resizeEvent(rev);
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
        ignore = mReceiver->closeEvent(cev);
    }
    break;
    case QEvent::Wheel:
    {
        QWheelEvent *wev = (QWheelEvent*)(e);
        ignore = mReceiver->wheelEvent(wev);
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
        ignore = mReceiver->dragEnterEvent(dev);
    }
        break;
    case QEvent::DragMove:
    {
        QDragMoveEvent *dev = (QDragMoveEvent*)(e);
        ignore = mReceiver->dragMoveEvent(dev);
    }
    break;
    case QEvent::DragLeave:
    {
        QDragLeaveEvent *dev = (QDragLeaveEvent*)(e);
        ignore = mReceiver->dragLeaveEvent(dev);
    }
    break;
    case QEvent::Drop:
    {
        QDropEvent *dev = (QDropEvent*)(e);
        ignore = mReceiver->dropEvent(dev);
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
        setWFlags(Qt::WShowModal);
    else
        clearWFlags(Qt::WShowModal);
}
