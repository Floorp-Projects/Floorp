/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Nokia.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef MOZIQWIDGET_H
#define MOZIQWIDGET_H

#include <QApplication>
#include <QGraphicsWidget>
#include <QGraphicsView>
#include <QtOpenGL/QGLWidget>

#include "nsCOMPtr.h"

#ifdef MOZ_ENABLE_MEEGOTOUCH
#include <MSceneWindow>
#include <MInputMethodState>
#include <QtGui/QGraphicsSceneResizeEvent>
#include <QTimer>
#endif

class nsWindow;

class IMozQWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    /**
     * Mozilla helper.
     */
    virtual void setModal(bool) {}
    virtual void dropReceiver() { };
    virtual nsWindow* getReceiver() { return NULL; };

    virtual void activate() {}
    virtual void deactivate() {}

    /**
     * VirtualKeyboardIntegration
     */
    virtual bool isVKBOpen() { return false; }

    virtual void NotifyVKB(const QRect& rect) {}
    virtual void SwitchToForeground() {}
    virtual void SwitchToBackground() {}
};

class MozQGraphicsViewEvents
{
public:

    MozQGraphicsViewEvents(QGraphicsView* aView)
     : mView(aView)
    { }

    void handleEvent(QEvent* aEvent, IMozQWidget* aTopLevel)
    {
        if (!aEvent)
            return;
        if (aEvent->type() == QEvent::WindowActivate) {
            if (aTopLevel)
                aTopLevel->activate();
        }

        if (aEvent->type() == QEvent::WindowDeactivate) {
            if (aTopLevel)
                aTopLevel->deactivate();
        }
    }

    void handleResizeEvent(QResizeEvent* aEvent, IMozQWidget* aTopLevel)
    {
        if (!aEvent)
            return;
        if (aTopLevel) {
            // transfer new size to graphics widget
            aTopLevel->setGeometry(0.0, 0.0,
                static_cast<qreal>(aEvent->size().width()),
                static_cast<qreal>(aEvent->size().height()));
            // resize scene rect to vieport size,
            // to avoid extra scrolling from QAbstractScrollable
            if (mView)
                mView->setSceneRect(mView->viewport()->rect());
        }
    }

    bool handleCloseEvent(QCloseEvent* aEvent, IMozQWidget* aTopLevel)
    {
        if (!aEvent)
            return false;
        if (aTopLevel) {
            // close graphics widget instead, this view will be discarded
            // automatically
            QApplication::postEvent(aTopLevel, new QCloseEvent(*aEvent));
            aEvent->ignore();
            return true;
        }

        return false;
    }

private:
    QGraphicsView* mView;
};

/**
    This is a helper class to synchronize the QGraphicsView window with
    its contained QGraphicsWidget for things like resizing and closing
    by the user.
*/
class MozQGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    MozQGraphicsView(QWidget * aParent = nsnull)
     : QGraphicsView (new QGraphicsScene(), aParent)
     , mEventHandler(this)
     , mTopLevelWidget(NULL)
     , mGLWidget(0)
    {
        setMouseTracking(true);
        setFrameShape(QFrame::NoFrame);
    }

    void SetTopLevel(IMozQWidget* aTopLevel, QWidget* aParent)
    {
        scene()->addItem(aTopLevel);
        mTopLevelWidget = aTopLevel;
    }

    void setGLWidgetEnabled(bool aEnabled)
    {
        if (aEnabled) {
            mGLWidget = new QGLWidget();
            setViewport(mGLWidget);
        } else {
            delete mGLWidget;
            mGLWidget = 0;
            setViewport(new QWidget());
        }
    }

protected:

    virtual bool event(QEvent* aEvent)
    {
        mEventHandler.handleEvent(aEvent, mTopLevelWidget);
        return QGraphicsView::event(aEvent);
    }

    virtual void resizeEvent(QResizeEvent* aEvent)
    {
        mEventHandler.handleResizeEvent(aEvent, mTopLevelWidget);
        QGraphicsView::resizeEvent(aEvent);
    }

    virtual void closeEvent (QCloseEvent* aEvent)
    {
        if (!mEventHandler.handleCloseEvent(aEvent, mTopLevelWidget))
            QGraphicsView::closeEvent(aEvent);
    }

    virtual void paintEvent(QPaintEvent* aEvent)
    {
        if (mGLWidget) {
            mGLWidget->makeCurrent();
        }
        QGraphicsView::paintEvent(aEvent);
    }

private:
    MozQGraphicsViewEvents mEventHandler;
    IMozQWidget* mTopLevelWidget;
    QGLWidget* mGLWidget;
};

#ifdef MOZ_ENABLE_MEEGOTOUCH
class MozMSceneWindow : public MSceneWindow
{
    Q_OBJECT
public:
    MozMSceneWindow(IMozQWidget* aTopLevel)
     : MSceneWindow(aTopLevel->parentItem())
     , mTopLevelWidget(aTopLevel)
    {
        MInputMethodState* inputMethodState = MInputMethodState::instance();
        if (inputMethodState) {
            connect(inputMethodState, SIGNAL(inputMethodAreaChanged(const QRect&)),
                    this, SLOT(VisibleScreenAreaChanged(const QRect&)));
        }
    }

    void SetTopLevel(IMozQWidget* aTopLevel)
    {
        mTopLevelWidget = aTopLevel;
        mTopLevelWidget->setParentItem(this);
        mTopLevelWidget->installEventFilter(this);
    }

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent* aEvent)
    {
        mCurrentSize = aEvent->newSize();
        MSceneWindow::resizeEvent(aEvent);
        CheckTopLevelSize();
    }

    virtual bool eventFilter(QObject* watched, QEvent* e)
    {
        if (e->type() == QEvent::GraphicsSceneResize ||
            e->type() == QEvent::GraphicsSceneMove) {

            //Do this in next event loop, or we are in recursion!
            QTimer::singleShot(0, this, SLOT(CheckTopLevelSize()));
        }

        return false;
    }

private slots:
    void CheckTopLevelSize()
    {
        if (mTopLevelWidget) {
            qreal xpos = 0;
            qreal ypos = 0;
            qreal width = mCurrentSize.width();
            qreal height = mCurrentSize.height();

            // transfer new size to graphics widget if changed
            QRectF r = mTopLevelWidget->geometry();
            if (r != QRectF(xpos, ypos, width, height)) {
                mTopLevelWidget->setGeometry(xpos, ypos, width, height);
            }
        }
    }

    void VisibleScreenAreaChanged(const QRect& rect) {
        if (mTopLevelWidget) {
            mTopLevelWidget->NotifyVKB(rect);
        }
    }

private:
    IMozQWidget* mTopLevelWidget;
    QSizeF mCurrentSize;
};

/**
  This is a helper class to synchronize the MWindow window with
  its contained QGraphicsWidget for things like resizing and closing
  by the user.
*/
class MozMGraphicsView : public MWindow
{
    Q_OBJECT
public:
    MozMGraphicsView(QWidget* aParent = nsnull)
     : MWindow(aParent)
     , mEventHandler(this)
     , mTopLevelWidget(NULL)
     , mSceneWin(NULL)
    {
        QObject::connect(this, SIGNAL(switcherEntered()), this, SLOT(onSwitcherEntered()));
        QObject::connect(this, SIGNAL(switcherExited()), this, SLOT(onSwitcherExited()));
        setFrameShape(QFrame::NoFrame);
    }

    void SetTopLevel(IMozQWidget* aTopLevel, QWidget* aParent)
    {
        if (!mSceneWin) {
            mSceneWin = new MozMSceneWindow(aTopLevel);
            mSceneWin->appear(this);
        }
        mSceneWin->SetTopLevel(aTopLevel);
        mTopLevelWidget = aTopLevel;
    }

public Q_SLOTS:
    void onSwitcherEntered() {
        if (mTopLevelWidget) {
            mTopLevelWidget->SwitchToBackground();
        }
    }
    void onSwitcherExited() {
        if (mTopLevelWidget) {
            mTopLevelWidget->SwitchToForeground();
        }
    }

protected:
    virtual bool event(QEvent* aEvent) {
        mEventHandler.handleEvent(aEvent, mTopLevelWidget);
        return MWindow::event(aEvent);
    }

    virtual void resizeEvent(QResizeEvent* aEvent)
    {
        setSceneRect(viewport()->rect());
        MWindow::resizeEvent(aEvent);
    }

    virtual void closeEvent (QCloseEvent* aEvent)
    {
        if (!mEventHandler.handleCloseEvent(aEvent, mTopLevelWidget)) {
            MWindow::closeEvent(aEvent);
        }
    }

private:
    MozQGraphicsViewEvents mEventHandler;
    IMozQWidget* mTopLevelWidget;
    MozMSceneWindow* mSceneWin;
};

#endif /* MOZ_ENABLE_MEEGOTOUCH */
#endif
