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

#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include <QtGui/QApplication>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsWidget>

#include "nsIWidget.h"
#include "prenv.h"

#include "nsIObserverService.h"
#include "mozilla/Services.h"

#ifdef MOZ_ENABLE_QTMOBILITY
#include "mozqorientationsensorfilter.h"
#endif

#ifdef MOZ_ENABLE_MEEGOTOUCH
#include <QtGui/QGraphicsSceneResizeEvent>
#include <MSceneWindow>
#include <MScene>
#include <QTimer>
#include <MInputMethodState>
#include <MApplication>
#endif

class QEvent;
class QPixmap;
class QWidget;

class nsWindow;

class MozQWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    MozQWidget(nsWindow* aReceiver, QGraphicsItem *aParent);

    ~MozQWidget();

    /**
     * Mozilla helper.
     */
    void setModal(bool);
    bool SetCursor(nsCursor aCursor);
    void dropReceiver() { mReceiver = 0x0; };
    nsWindow* getReceiver() { return mReceiver; };

    void activate();
    void deactivate();

    QVariant inputMethodQuery(Qt::InputMethodQuery aQuery) const;

    /**
     * VirtualKeyboardIntegration
     */
    static void requestVKB(int aTimeout = 0, QObject* aWidget = 0);
    static void hideVKB();
    static bool isVKBOpen();

public slots:
    static void showVKB();

#ifdef MOZ_ENABLE_QTMOBILITY
    void orientationChanged();
#endif

protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* aEvent);
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent* aEvent);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent* aEvent);
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent* aEvent);
    virtual void dropEvent(QGraphicsSceneDragDropEvent* aEvent);
    virtual void focusInEvent(QFocusEvent* aEvent);
    virtual void focusOutEvent(QFocusEvent* aEvent);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* aEvent);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* aEvent);
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* aEvent);
    virtual void keyPressEvent(QKeyEvent* aEvent);
    virtual void keyReleaseEvent(QKeyEvent* aEvent);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* aEvent);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* aEvent);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* aEvent);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* aEvent);
    virtual void inputMethodEvent(QInputMethodEvent* aEvent);

    virtual void wheelEvent(QGraphicsSceneWheelEvent* aEvent);
    virtual void paint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget = 0);
    virtual void resizeEvent(QGraphicsSceneResizeEvent* aEvent);
    virtual void closeEvent(QCloseEvent* aEvent);
    virtual void hideEvent(QHideEvent* aEvent);
    virtual void showEvent(QShowEvent* aEvent);
    virtual bool event(QEvent* aEvent);

    bool SetCursor(const QPixmap& aPixmap, int, int);

private:
    void sendPressReleaseKeyEvent(int key, const QChar* letter = 0, bool autorep = false, ushort count = 1);
    nsWindow *mReceiver;
};

class MozQGraphicsViewEvents
{
public:

    MozQGraphicsViewEvents(QGraphicsView* aView)
     : mView(aView)
    { }

    void handleEvent(QEvent* aEvent, MozQWidget* aTopLevel)
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

    void handleResizeEvent(QResizeEvent* aEvent, MozQWidget* aTopLevel)
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

    bool handleCloseEvent(QCloseEvent* aEvent, MozQWidget* aTopLevel)
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
    MozQGraphicsView(MozQWidget* aTopLevel, QWidget * aParent = nsnull)
     : QGraphicsView (new QGraphicsScene(), aParent)
     , mEventHandler(this)
     , mTopLevelWidget(aTopLevel)
    {
        scene()->addItem(aTopLevel);
        setMouseTracking(true);
        setFrameShape(QFrame::NoFrame);
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

private:
    MozQGraphicsViewEvents mEventHandler;
    MozQWidget* mTopLevelWidget;
};

#ifdef MOZ_ENABLE_MEEGOTOUCH
class MozMSceneWindow : public MSceneWindow
{
    Q_OBJECT
public:
    MozMSceneWindow(MozQWidget* aTopLevel)
     : MSceneWindow(aTopLevel->parentItem())
     , mTopLevelWidget(aTopLevel)
    {
        mTopLevelWidget->setParentItem(this);
        mTopLevelWidget->installEventFilter(this);
        MInputMethodState* inputMethodState = MInputMethodState::instance();
        if (inputMethodState) {
            connect(inputMethodState, SIGNAL(inputMethodAreaChanged(const QRect&)),
                    this, SLOT(VisibleScreenAreaChanged(const QRect&)));
        }
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
            QRegion region(scene()->views()[0]->rect());
            region -= rect;
            QRectF bounds = mapRectFromScene(region.boundingRect());

            nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
            if (observerService) {
                QString rect = QString("{\"left\": %1, \"top\": %2, \"right\": %3, \"bottom\": %4}")
                                       .arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height());
                observerService->NotifyObservers(nsnull, "softkb-change", rect.utf16());
            }
        }
    }

private:
    MozQWidget* mTopLevelWidget;
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
    MozMGraphicsView(MozQWidget* aTopLevel, QWidget* aParent = nsnull)
     : MWindow(aParent)
     , mEventHandler(this)
     , mTopLevelWidget(aTopLevel)
    {
        MozMSceneWindow* page = new MozMSceneWindow(aTopLevel);
        page->appear(this);
        QObject::connect(this, SIGNAL(switcherEntered()), this, SLOT(onSwitcherEntered()));
        QObject::connect(this, SIGNAL(switcherExited()), this, SLOT(onSwitcherExited()));
    }

public Q_SLOTS:
    void onSwitcherEntered() {
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (!os)
            return;
        os->NotifyObservers(nsnull, "application-background", nsnull);
    }
    void onSwitcherExited() {
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (!os)
            return;
        os->NotifyObservers(nsnull, "application-foreground", nsnull);
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
    MozQWidget* mTopLevelWidget;
};

#endif /* MOZ_ENABLE_MEEGOTOUCH */
#endif
