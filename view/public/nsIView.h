/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIView_h___
#define nsIView_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsNativeWidget.h"
#include "nsIWidget.h"
#include "nsWidgetInitData.h"
#include "nsIFrame.h"

class nsIViewManager;
class nsViewManager;
class nsView;
class nsWeakView;
class nsIWidget;

// Enumerated type to indicate the visibility of a layer.
// hide - the layer is not shown.
// show - the layer is shown irrespective of the visibility of 
//        the layer's parent.
enum nsViewVisibility {
  nsViewVisibility_kHide = 0,
  nsViewVisibility_kShow = 1
};

#define NS_IVIEW_IID    \
  { 0x697948d2, 0x3f10, 0x407d, \
    { 0xb8, 0x94, 0x9f, 0x36, 0xd2, 0x11, 0xdb, 0xf1 } }

// Public view flags

// Indicates that the view is using auto z-indexing
#define NS_VIEW_FLAG_AUTO_ZINDEX          0x0004

// Indicates that the view is a floating view.
#define NS_VIEW_FLAG_FLOATING             0x0008

// If set it indicates that this view should be
// displayed above z-index:auto views if this view 
// is z-index:auto also
#define NS_VIEW_FLAG_TOPMOST              0x0010

//----------------------------------------------------------------------

/**
 * View interface
 *
 * Views are NOT reference counted. Use the Destroy() member function to
 * destroy a view.
 *
 * The lifetime of the view hierarchy is bounded by the lifetime of the
 * view manager that owns the views.
 *
 * Most of the methods here are read-only. To set the corresponding properties
 * of a view, go through nsIViewManager.
 */

class nsIView
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IVIEW_IID)

  /**
   * Find the view for the given widget, if there is one.
   * @return the view the widget belongs to, or null if the widget doesn't
   * belong to any view.
   */
  static nsIView* GetViewFor(nsIWidget* aWidget);

  /**
   * Get the view manager which "owns" the view.
   * This method might require some expensive traversal work in the future. If you can get the
   * view manager from somewhere else, do that instead.
   * @result the view manager
   */
  nsIViewManager* GetViewManager() const
  { return reinterpret_cast<nsIViewManager*>(mViewManager); }

  /**
   * Destroy the view.
   *
   * The view destroys its child views, and destroys and releases its
   * widget (if it has one).
   *
   * Also informs the view manager that the view is destroyed by calling
   * SetRootView(NULL) if the view is the root view and calling RemoveChild()
   * otherwise.
   */
  void Destroy();

  /**
   * Called to get the position of a view.
   * The specified coordinates are relative to the parent view's origin, but
   * are in appunits of this.
   * This is the (0, 0) origin of the coordinate space established by this view.
   * @param x out parameter for x position
   * @param y out parameter for y position
   */
  nsPoint GetPosition() const {
    // Call ExternalIsRoot here so that we can get to it from other
    // components
    NS_ASSERTION(!ExternalIsRoot() || (mPosX == 0 && mPosY == 0),
                 "root views should always have explicit position of (0,0)");
    return nsPoint(mPosX, mPosY);
  }

  /**
   * Set the position of a view. This does not cause any invalidation. It
   * does reposition any widgets in this view or its descendants.
   */
  virtual void SetPosition(nscoord aX, nscoord aY) = 0;
  
  /**
   * Called to get the dimensions and position of the view's bounds.
   * The view's bounds (x,y) are relative to the origin of the parent view, but
   * are in appunits of this.
   * The view's bounds (x,y) might not be the same as the view's position,
   * if the view has content above or to the left of its origin.
   * @param aBounds out parameter for bounds
   */
  nsRect GetBounds() const { return mDimBounds; }

  /**
   * The bounds of this view relative to this view. So this is the same as
   * GetBounds except this is relative to this view instead of the parent view.
   */
  nsRect GetDimensions() const {
    nsRect r = mDimBounds; r.MoveBy(-mPosX, -mPosY); return r;
  }

  /**
   * Get the offset between the coordinate systems of |this| and aOther.
   * Adding the return value to a point in the coordinate system of |this|
   * will transform the point to the coordinate system of aOther.
   *
   * The offset is expressed in appunits of |this|. So if you are getting the
   * offset between views in different documents that might have different
   * appunits per devpixel ratios you need to be careful how you use the
   * result.
   *
   * If aOther is null, this will return the offset of |this| from the
   * root of the viewmanager tree.
   * 
   * This function is fastest when aOther is an ancestor of |this|.
   *
   * NOTE: this actually returns the offset from aOther to |this|, but
   * that offset is added to transform _coordinates_ from |this| to aOther.
   */
  nsPoint GetOffsetTo(const nsIView* aOther) const;

  /**
   * Get the offset between the origin of |this| and the origin of aWidget.
   * Adding the return value to a point in the coordinate system of |this|
   * will transform the point to the coordinate system of aWidget.
   *
   * The offset is expressed in appunits of |this|.
   */
  nsPoint GetOffsetToWidget(nsIWidget* aWidget) const;

  /**
   * Takes a point aPt that is in the coordinate system of |this|'s parent view
   * and converts it to be in the coordinate system of |this| taking into
   * account the offset and any app unit per dev pixel ratio differences.
   */
  nsPoint ConvertFromParentCoords(nsPoint aPt) const;

  /**
   * Called to query the visibility state of a view.
   * @result current visibility state
   */
  nsViewVisibility GetVisibility() const { return mVis; }

  /**
   * Get whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   * @result true if the view floats, false otherwise.
   */
  bool GetFloating() const { return (mVFlags & NS_VIEW_FLAG_FLOATING) != 0; }

  /**
   * Called to query the parent of the view.
   * @result view's parent
   */
  nsIView* GetParent() const { return reinterpret_cast<nsIView*>(mParent); }

  /**
   * The view's first child is the child which is earliest in document order.
   * @result first child
   */
  nsIView* GetFirstChild() const { return reinterpret_cast<nsIView*>(mFirstChild); }

  /**
   * Called to query the next sibling of the view.
   * @result view's next sibling
   */
  nsIView* GetNextSibling() const { return reinterpret_cast<nsIView*>(mNextSibling); }
  void SetNextSibling(nsIView *aSibling) {
    mNextSibling = reinterpret_cast<nsView*>(aSibling);
  }

  /**
   * Set the view's frame.
   */
  void SetFrame(nsIFrame* aRootFrame) { mFrame = aRootFrame; }

  /**
   * Retrieve the view's frame.
   */
  nsIFrame* GetFrame() const { return mFrame; }

  /**
   * Get the nearest widget in this view or a parent of this view and
   * the offset from the widget's origin to this view's origin
   * @param aOffset - if non-null the offset from this view's origin to the
   * widget's origin (usually positive) expressed in appunits of this will be
   * returned in aOffset.
   * @return the widget closest to this view; can be null because some view trees
   * don't have widgets at all (e.g., printing), but if any view in the view tree
   * has a widget, then it's safe to assume this will not return null
   * XXX Remove this 'virtual' when gfx+widget are merged into gklayout;
   * Mac widget depends on this method, which is BOGUS!
   */
  virtual nsIWidget* GetNearestWidget(nsPoint* aOffset) const;

  /**
   * Create a widget to associate with this view.  This variant of
   * CreateWidget*() will look around in the view hierarchy for an
   * appropriate parent widget for the view.
   *
   * @param aWidgetInitData data used to initialize this view's widget before
   *        its create is called.
   * @return error status
   */
  nsresult CreateWidget(nsWidgetInitData *aWidgetInitData = nsnull,
                        bool aEnableDragDrop = true,
                        bool aResetVisibility = true);

  /**
   * Create a widget for this view with an explicit parent widget.
   * |aParentWidget| must be nonnull.  The other params are the same
   * as for |CreateWidget()|.
   */
  nsresult CreateWidgetForParent(nsIWidget* aParentWidget,
                                 nsWidgetInitData *aWidgetInitData = nsnull,
                                 bool aEnableDragDrop = true,
                                 bool aResetVisibility = true);

  /**
   * Create a popup widget for this view.  Pass |aParentWidget| to
   * explicitly set the popup's parent.  If it's not passed, the view
   * hierarchy will be searched for an appropriate parent widget.  The
   * other params are the same as for |CreateWidget()|, except that
   * |aWidgetInitData| must be nonnull.
   */
  nsresult CreateWidgetForPopup(nsWidgetInitData *aWidgetInitData,
                                nsIWidget* aParentWidget = nsnull,
                                bool aEnableDragDrop = true,
                                bool aResetVisibility = true);

  /**
   * Destroys the associated widget for this view.  If this method is
   * not called explicitly, the widget when be destroyed when its
   * view gets destroyed.
   */
  void DestroyWidget();

  /**
   * Attach/detach a top level widget from this view. When attached, the view
   * updates the widget's device context and allows the view to begin receiving
   * gecko events. The underlying base window associated with the widget will
   * continues to receive events it expects.
   *
   * An attached widget will not be destroyed when the view is destroyed,
   * allowing the recycling of a single top level widget over multiple views.
   *
   * @param aWidget The widget to attach to / detach from.
   */
  nsresult AttachToTopLevelWidget(nsIWidget* aWidget);
  nsresult DetachFromTopLevelWidget();

  /**
   * Returns a flag indicating whether the view owns it's widget
   * or is attached to an existing top level widget.
   */
  bool IsAttachedToTopLevel() const { return mWidgetIsTopLevel; }

  /**
   * In 4.0, the "cutout" nature of a view is queryable.
   * If we believe that all cutout view have a native widget, this
   * could be a replacement.
   * @param aWidget out parameter for widget that this view contains,
   *        or nsnull if there is none.
   */
  nsIWidget* GetWidget() const { return mWindow; }

  /**
   * Returns true if the view has a widget associated with it.
   */
  bool HasWidget() const { return mWindow != nsnull; }

  /**
   * Make aWidget direct its events to this view.
   * The caller must call DetachWidgetEventHandler before this view
   * is destroyed.
   */
  EVENT_CALLBACK AttachWidgetEventHandler(nsIWidget* aWidget);
  /**
   * Stop aWidget directing its events to this view.
   */
  void DetachWidgetEventHandler(nsIWidget* aWidget);

#ifdef DEBUG
  /**
   * Output debug info to FILE
   * @param out output file handle
   * @param aIndent indentation depth
   * NOTE: virtual so that debugging tools not linked into gklayout can access it
   */
  virtual void List(FILE* out, PRInt32 aIndent = 0) const;
#endif // DEBUG

  /**
   * @result true iff this is the root view for its view manager
   */
  bool IsRoot() const;

  virtual bool ExternalIsRoot() const;

  void SetDeletionObserver(nsWeakView* aDeletionObserver);

  nsIntRect CalcWidgetBounds(nsWindowType aType);

  bool IsEffectivelyVisible();

  // This is an app unit offset to add when converting view coordinates to
  // widget coordinates.  It is the offset in view coordinates from widget
  // origin (unlike views, widgets can't extend above or to the left of their
  // origin) to view origin expressed in appunits of this.
  nsPoint ViewToWidgetOffset() const { return mViewToWidgetOffset; }

protected:
  friend class nsWeakView;
  nsViewManager     *mViewManager;
  nsView            *mParent;
  nsIWidget         *mWindow;
  nsView            *mNextSibling;
  nsView            *mFirstChild;
  nsIFrame          *mFrame;
  PRInt32           mZIndex;
  nsViewVisibility  mVis;
  // position relative our parent view origin but in our appunits
  nscoord           mPosX, mPosY;
  // relative to parent, but in our appunits
  nsRect            mDimBounds;
  // in our appunits
  nsPoint           mViewToWidgetOffset;
  float             mOpacity;
  PRUint32          mVFlags;
  nsWeakView*       mDeletionObserver;
  bool              mWidgetIsTopLevel;

  virtual ~nsIView() {}

private:
  nsView* Impl();
  const nsView* Impl() const;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIView, NS_IVIEW_IID)

// nsWeakViews must *not* be used in heap!
class nsWeakView
{
public:
  nsWeakView(nsIView* aView) : mPrev(nsnull), mView(aView)
  {
    if (mView) {
      mView->SetDeletionObserver(this);
    }
  }

  ~nsWeakView()
  {
    if (mView) {
      NS_ASSERTION(mView->mDeletionObserver == this,
                   "nsWeakViews deleted in wrong order!");
      // Clear deletion observer temporarily.
      mView->SetDeletionObserver(nsnull);
      // Put back the previous deletion observer.
      mView->SetDeletionObserver(mPrev);
    }
  }

  bool IsAlive() { return !!mView; }

  nsIView* GetView() { return mView; }

  void SetPrevious(nsWeakView* aWeakView) { mPrev = aWeakView; }

  void Clear()
  {
    if (mPrev) {
      mPrev->Clear();
    }
    mView = nsnull;
  }
private:
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
  static void operator delete(void*, size_t) {}
  nsWeakView* mPrev;
  nsIView*    mView;
};

#endif
