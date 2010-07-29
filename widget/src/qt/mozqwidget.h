#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include <QtGui/QApplication>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsWidget>

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
    void showVKB();
    void hideVKB();
    bool isVKBOpen();

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

#endif
