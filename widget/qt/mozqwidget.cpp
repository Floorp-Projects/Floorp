/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QtCore/QCoreApplication>
#include <QtGui/QResizeEvent>

#include "mozqwidget.h"
#include "nsWindow.h"

using namespace mozilla::widget;

MozQWidget::MozQWidget(nsWindow* aReceiver, QWindow* aParent)
  : QWindow(aParent)
  , mReceiver(aReceiver)
  , mUpdatePending(false)
{
    mWindowType = mReceiver->WindowType();
}

MozQWidget::~MozQWidget()
{
}

void MozQWidget::render(QPainter* painter)
{
    Q_UNUSED(painter);
}

void MozQWidget::renderLater()
{
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }

    if (!mUpdatePending) {
        mUpdatePending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void MozQWidget::renderNow()
{
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }

    mReceiver->OnPaint();
}

bool MozQWidget::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        mUpdatePending = false;
        renderNow();
        return true;
    default:
        return QWindow::event(event);
    }
}

void MozQWidget::exposeEvent(QExposeEvent* event)
{
    Q_UNUSED(event);
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }
    LOG(("MozQWidget::%s [%p] flags:%x\n", __FUNCTION__, (void *)this, flags()));
    renderNow();

}

void MozQWidget::resizeEvent(QResizeEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->resizeEvent(event);
    QWindow::resizeEvent(event);
}

void MozQWidget::focusInEvent(QFocusEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->focusInEvent(event);
    QWindow::focusInEvent(event);
}

void MozQWidget::focusOutEvent(QFocusEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->focusOutEvent(event);
    QWindow::focusOutEvent(event);
}

void MozQWidget::hideEvent(QHideEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->hideEvent(event);
    QWindow::hideEvent(event);
}

void MozQWidget::keyPressEvent(QKeyEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->keyPressEvent(event);
    QWindow::keyPressEvent(event);
}

void MozQWidget::keyReleaseEvent(QKeyEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->keyReleaseEvent(event);
    QWindow::keyReleaseEvent(event);
}

void MozQWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mouseDoubleClickEvent(event);
    QWindow::mouseDoubleClickEvent(event);
}

void MozQWidget::mouseMoveEvent(QMouseEvent* event)
{
    mReceiver->mouseMoveEvent(event);
    QWindow::mouseMoveEvent(event);
}

void MozQWidget::mousePressEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mousePressEvent(event);
    QWindow::mousePressEvent(event);
}

void MozQWidget::mouseReleaseEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mouseReleaseEvent(event);
    QWindow::mouseReleaseEvent(event);
}

void MozQWidget::moveEvent(QMoveEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->moveEvent(event);
    QWindow::moveEvent(event);
}

void MozQWidget::showEvent(QShowEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->showEvent(event);
    QWindow::showEvent(event);
}

void MozQWidget::wheelEvent(QWheelEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->wheelEvent(event);
    QWindow::wheelEvent(event);
}

void MozQWidget::tabletEvent(QTabletEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::tabletEvent(event);
}

void MozQWidget::touchEvent(QTouchEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::touchEvent(event);
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
