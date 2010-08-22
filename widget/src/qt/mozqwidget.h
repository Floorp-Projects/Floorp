#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include <QtGui/QApplication>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsWidget>

#ifdef MOZ_ENABLE_MEEGOTOUCH
#include <QtGui/QGraphicsSceneResizeEvent>
#include <MSceneWindow>
#include <QTimer>
#include <mstatusbar.h>
#endif

#include "nsIWidget.h"
#include "prenv.h"

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
    void requestVKB(int aTimeout);
    void hideVKB();
    bool isVKBOpen();

public slots:
    void showVKB();

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

    virtual void wheelEvent(QGraphicsSceneWheelEvent* aEvent);
    virtual void paint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget = 0);
    virtual void resizeEvent(QGraphicsSceneResizeEvent* aEvent);
    virtual void closeEvent(QCloseEvent* aEvent);
    virtual void hideEvent(QHideEvent* aEvent);
    virtual void showEvent(QShowEvent* aEvent);
    virtual bool event(QEvent* aEvent);

    bool SetCursor(const QPixmap& aPixmap, int, int);

private:
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
     , mStatusBar(nsnull)
    {
        mTopLevelWidget->setParentItem(this);
        mTopLevelWidget->installEventFilter(this);
        mStatusBar = new MStatusBar();
        mStatusBar->appear();
        connect(mStatusBar, SIGNAL(appeared()), this, SLOT(CheckTopLevelSize()));
        connect(mStatusBar, SIGNAL(disappeared()), this, SLOT(CheckTopLevelSize()));
    }

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent* aEvent) {
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

        //false == let event processing continue
        return false;
    }

private slots:
    void CheckTopLevelSize() {
        if (mTopLevelWidget) {
            qreal xpos = 0;
            qreal ypos = 0;
            qreal width = mCurrentSize.width();
            qreal height = mCurrentSize.height();

            //If statusbar is visible, move toplevel widget down
            if (mStatusBar->isVisible()) {
                ypos = mStatusBar->size().height();
                height -= ypos;
            }

            // transfer new size to graphics widget if changed
            QRectF r = mTopLevelWidget->geometry();
            if (r != QRectF(xpos, ypos, width, height))
                mTopLevelWidget->setGeometry(xpos, ypos, width, height);
        }
    }

private:
    MozQWidget* mTopLevelWidget;
    MStatusBar* mStatusBar;
    QSizeF mCurrentSize;
};

/**
    This is a helper class to synchronize the MWindow window with
    its contained QGraphicsWidget for things like resizing and closing
    by the user.
*/
class MozMGraphicsView : public MWindow
{

public:
    MozMGraphicsView(MozQWidget* aTopLevel, QWidget* aParent = nsnull)
     : MWindow(aParent)
     , mEventHandler(this)
     , mTopLevelWidget(aTopLevel)
    {
        MozMSceneWindow *page = new MozMSceneWindow(aTopLevel);
        if (page)
            page->appear(this);
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
        if (!mEventHandler.handleCloseEvent(aEvent, mTopLevelWidget))
            MWindow::closeEvent(aEvent);
    }

private:
    MozQGraphicsViewEvents mEventHandler;
    MozQWidget* mTopLevelWidget;
};

#endif /* MOZ_ENABLE_MEEGOTOUCH */

#endif
