/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsQtEventDispatcher.h"

#include "nsCommonWidget.h"

#include <qwidget.h>
#include <qevent.h>

nsQtEventDispatcher::nsQtEventDispatcher(nsCommonWidget *receiver,
                                         QWidget *watchedWidget,
                                         const char *name, bool paint )
    : QObject(watchedWidget, name),
      mReceiver(receiver),
      m_paint(paint)

{
    watchedWidget->installEventFilter(this);
}

bool
nsQtEventDispatcher::eventFilter(QObject *o, QEvent *e)
{
#if 0
    if (m_paint && e->type() == QEvent::Paint) {
        qDebug("MMM PAINT FROM %d", o);
        QPaintEvent *ev = (QPaintEvent*)(e);
        return mReceiver->paintEvent(ev);
    }
#endif

    switch(e->type()) {
    case QEvent::Accessibility:
    {
        qDebug("accessibility event received");
    }
    break;
    case QEvent::MouseButtonPress:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        return mReceiver->mousePressEvent(ms);
    }
    break;
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        return mReceiver->mouseReleaseEvent(ms);
    }
    break;
    case QEvent::MouseButtonDblClick:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        return mReceiver->mouseDoubleClickEvent(ms);
    }
    break;
    case QEvent::MouseMove:
    {
        QMouseEvent *ms = (QMouseEvent*)(e);
        return mReceiver->mouseMoveEvent(ms);
    }
    break;
    case QEvent::KeyPress:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        return mReceiver->keyPressEvent(kev);
    }
    break;
    case QEvent::KeyRelease:
    {
        QKeyEvent *kev = (QKeyEvent*)(e);
        return mReceiver->keyReleaseEvent(kev);
    }
    break;
    case QEvent::IMStart:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        return mReceiver->imStartEvent(iev);
    }
    break;
    case QEvent::IMCompose:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        return mReceiver->imComposeEvent(iev);
    }
    break;
    case QEvent::IMEnd:
    {
        QIMEvent *iev = (QIMEvent*)(e);
        return mReceiver->imEndEvent(iev);
    }
    break;
    case QEvent::FocusIn:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        return mReceiver->focusInEvent(fev);
    }
    break;
    case QEvent::FocusOut:
    {
        QFocusEvent *fev = (QFocusEvent*)(e);
        return mReceiver->focusOutEvent(fev);
    }
    break;
    case QEvent::Enter:
    {
        return mReceiver->enterEvent(e);
    }
    break;
    case QEvent::Leave:
    {
        return mReceiver->enterEvent(e);
    }
    break;
    case QEvent::Paint:
    {
        QPaintEvent *ev = (QPaintEvent*)(e);
        mReceiver->paintEvent(ev);
        return TRUE;
    }
    break;
    case QEvent::Move:
    {
        QMoveEvent *mev = (QMoveEvent*)(e);
        return mReceiver->moveEvent(mev);
    }
    break;
    case QEvent::Resize:
    {
        QResizeEvent *rev = (QResizeEvent*)(e);
        return mReceiver->resizeEvent(rev);
    }
        break;
    case QEvent::Show:
    {
        QShowEvent *sev = (QShowEvent*)(e);
        return mReceiver->showEvent(sev);
    }
    break;
    case QEvent::Hide:
    {
        QHideEvent *hev = (QHideEvent*)(e);
        return mReceiver->hideEvent(hev);
    }
        break;
    case QEvent::Close:
    {
        QCloseEvent *cev = (QCloseEvent*)(e);
        return mReceiver->closeEvent(cev);
    }
    break;
    case QEvent::Wheel:
    {
        QWheelEvent *wev = (QWheelEvent*)(e);
        return mReceiver->wheelEvent(wev);
    }
    break;
    case QEvent::ContextMenu:
    {
        QContextMenuEvent *cev = (QContextMenuEvent*)(e);
        return mReceiver->contextMenuEvent(cev);
    }
    break;
    case QEvent::DragEnter:
    {
        QDragEnterEvent *dev = (QDragEnterEvent*)(e);
        return mReceiver->dragEnterEvent(dev);
    }
        break;
    case QEvent::DragMove:
    {
        QDragMoveEvent *dev = (QDragMoveEvent*)(e);
        return mReceiver->dragMoveEvent(dev);
    }
    break;
    case QEvent::DragLeave:
    {
        QDragLeaveEvent *dev = (QDragLeaveEvent*)(e);
        return mReceiver->dragLeaveEvent(dev);
    }
    break;
    case QEvent::Drop:
    {
        QDropEvent *dev = (QDropEvent*)(e);
        return mReceiver->dropEvent(dev);
    }
    break;
    default:
        break;
    }

    return FALSE;
}

