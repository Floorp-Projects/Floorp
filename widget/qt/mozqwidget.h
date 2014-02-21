/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include "nsIWidget.h"

#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE
class QPainter;
class QExposeEvent;
class QResizeEvent;
QT_END_NAMESPACE

namespace mozilla {
namespace widget {

class nsWindow;

class MozQWidget : public QWindow
{
    Q_OBJECT
public:
    explicit MozQWidget(nsWindow* aReceiver, QWindow* aParent = 0);
    ~MozQWidget();

    virtual void render(QPainter* painter);

    virtual nsWindow* getReceiver() { return mReceiver; };
    virtual void dropReceiver() { mReceiver = nullptr; };
    virtual void SetCursor(nsCursor aCursor);

public Q_SLOTS:
    void renderLater();
    void renderNow();

protected:
    virtual bool event(QEvent* event);
    virtual void exposeEvent(QExposeEvent* event);
    virtual void focusInEvent(QFocusEvent* event);
    virtual void focusOutEvent(QFocusEvent* event);
    virtual void hideEvent(QHideEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void moveEvent(QMoveEvent* event);
    virtual void resizeEvent(QResizeEvent* event);
    virtual void showEvent(QShowEvent* event);
    virtual void tabletEvent(QTabletEvent* event);
    virtual void touchEvent(QTouchEvent* event);
    virtual void wheelEvent(QWheelEvent* event);

private:
    nsWindow* mReceiver;
    bool mUpdatePending;
    nsWindowType mWindowType;
};

} // namespace widget
} // namespace mozilla

#endif // MOZQWIDGET_H

