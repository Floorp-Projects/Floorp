#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include "nsIWidget.h"

#include <QtGui/QApplication>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsWidget>

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
    virtual bool sceneEvent(QEvent* aEvent);
 
    bool SetCursor(const QPixmap& aPixmap, int, int);

private:
    nsWindow *mReceiver;
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
    MozQGraphicsView(QGraphicsScene* aScene, QWidget * aParent = nsnull)
     : QGraphicsView (aScene, aParent)
     , mTopLevelWidget(0)
    { }

    void setTopLevel(MozQWidget* aTopLevel)
    {
        mTopLevelWidget = aTopLevel;
    }

protected:
    virtual void resizeEvent(QResizeEvent* aEvent)
    {
        if (mTopLevelWidget) {
            // transfer new size to graphics widget
            mTopLevelWidget->setGeometry( 0.0, 0.0,
                static_cast<qreal>(aEvent->size().width()),
                static_cast<qreal>(aEvent->size().height()));
        }
        QGraphicsView::resizeEvent(aEvent);
    }

    virtual void closeEvent (QCloseEvent* aEvent)
    {
        if (mTopLevelWidget) {
            // close graphics widget instead, this view will be discarded
            // automatically
            QApplication::postEvent(mTopLevelWidget, new QCloseEvent(*aEvent));
            aEvent->ignore();
        } else {
            QGraphicsView::closeEvent(aEvent);
        }
    }

private:
    MozQWidget* mTopLevelWidget;
};

#endif
