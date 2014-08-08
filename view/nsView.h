/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsView_h__
#define nsView_h__

#include "nsCoord.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsRegion.h"
#include "nsCRT.h"
#include "nsWidgetInitData.h" // for nsWindowType
#include "nsIWidgetListener.h"
#include "mozilla/EventForwards.h"

class nsViewManager;
class nsIWidget;
class nsIFrame;

// Enumerated type to indicate the visibility of a layer.
// hide - the layer is not shown.
// show - the layer is shown irrespective of the visibility of 
//        the layer's parent.
enum nsViewVisibility {
  nsViewVisibility_kHide = 0,
  nsViewVisibility_kShow = 1
};

// Public view flags

// Indicates that the view is using auto z-indexing
#define NS_VIEW_FLAG_AUTO_ZINDEX          0x0004

// Indicates that the view is a floating view.
#define NS_VIEW_FLAG_FLOATING             0x0008

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
 * of a view, go through nsViewManager.
 */

class nsView MOZ_FINAL : public nsIWidgetListener
{
public:
  friend class nsViewManager;

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  /**
   * Get the view manager which "owns" the view.
   * This method might require some expensive traversal work in the future. If you can get the
   * view manager from somewhere else, do that instead.
   * @result the view manager
   */
  nsViewManager* GetViewManager() const { return mViewManager; }

  /**
   * Find the view for the given widget, if there is one.
   * @return the view the widget belongs to, or null if the widget doesn't
   * belong to any view.
   */
  static nsView* GetViewFor(nsIWidget* aWidget);

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
    NS_ASSERTION(!IsRoot() || (mPosX == 0 && mPosY == 0),
                 "root views should always have explicit position of (0,0)");
    return nsPoint(mPosX, mPosY);
  }

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
  nsPoint GetOffsetTo(const nsView* aOther) const;

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
  nsView* GetParent() const { return mParent; }

  /**
   * The view's first child is the child which is earliest in document order.
   * @result first child
   */
  nsView* GetFirstChild() const { return mFirstChild; }

  /**
   * Called to query the next sibling of the view.
   * @result view's next sibling
   */
  nsView* GetNextSibling() const { return mNextSibling; }

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
   */
  nsIWidget* GetNearestWidget(nsPoint* aOffset) const;

  /**
   * Create a widget to associate with this view.  This variant of
   * CreateWidget*() will look around in the view hierarchy for an
   * appropriate parent widget for the view.
   *
   * @param aWidgetInitData data used to initialize this view's widget before
   *        its create is called.
   * @return error status
   */
  nsresult CreateWidget(nsWidgetInitData *aWidgetInitData = nullptr,
                        bool aEnableDragDrop = true,
                        bool aResetVisibility = true);

  /**
   * Create a widget for this view with an explicit parent widget.
   * |aParentWidget| must be nonnull.  The other params are the same
   * as for |CreateWidget()|.
   */
  nsresult CreateWidgetForParent(nsIWidget* aParentWidget,
                                 nsWidgetInitData *aWidgetInitData = nullptr,
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
                                nsIWidget* aParentWidget = nullptr,
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
   *        or nullptr if there is none.
   */
  nsIWidget* GetWidget() const { return mWindow; }

  /**
   * Returns true if the view has a widget associated with it.
   */
  bool HasWidget() const { return mWindow != nullptr; }
  
  void SetForcedRepaint(bool aForceRepaint) { 
    mForcedRepaint = aForceRepaint; 
  }

  /**
   * Make aWidget direct its events to this view.
   * The caller must call DetachWidgetEventHandler before this view
   * is destroyed.
   */
  void AttachWidgetEventHandler(nsIWidget* aWidget);
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
  virtual void List(FILE* out, int32_t aIndent = 0) const;
#endif // DEBUG

  /**
   * @result true iff this is the root view for its view manager
   */
  bool IsRoot() const;

  nsIntRect CalcWidgetBounds(nsWindowType aType);

  // This is an app unit offset to add when converting view coordinates to
  // widget coordinates.  It is the offset in view coordinates from widget
  // origin (unlike views, widgets can't extend above or to the left of their
  // origin) to view origin expressed in appunits of this.
  nsPoint ViewToWidgetOffset() const { return mViewToWidgetOffset; }

  /**
   * Called to indicate that the position of the view has been changed.
   * The specified coordinates are in the parent view's coordinate space.
   * @param x new x position
   * @param y new y position
   */
  void SetPosition(nscoord aX, nscoord aY);

  /**
   * Called to indicate that the z-index of a view has been changed.
   * The z-index is relative to all siblings of the view.
   * @param aAuto Indicate that the z-index of a view is "auto". An "auto" z-index
   * means that the view does not define a new stacking context,
   * which means that the z-indicies of the view's children are
   * relative to the view's siblings.
   * @param zindex new z depth
   */
  void SetZIndex(bool aAuto, int32_t aZIndex);
  bool GetZIndexIsAuto() const { return (mVFlags & NS_VIEW_FLAG_AUTO_ZINDEX) != 0; }
  int32_t GetZIndex() const { return mZIndex; }

  void SetParent(nsView *aParent) { mParent = aParent; }
  void SetNextSibling(nsView *aSibling)
  {
    NS_ASSERTION(aSibling != this, "Can't be our own sibling!");
    mNextSibling = aSibling;
  }

  nsRegion* GetDirtyRegion() {
    if (!mDirtyRegion) {
      NS_ASSERTION(!mParent || GetFloating(),
                   "Only display roots should have dirty regions");
      mDirtyRegion = new nsRegion();
      NS_ASSERTION(mDirtyRegion, "Out of memory!");
    }
    return mDirtyRegion;
  }

  // nsIWidgetListener
  virtual nsIPresShell* GetPresShell() MOZ_OVERRIDE;
  virtual nsView* GetView() MOZ_OVERRIDE { return this; }
  virtual bool WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y) MOZ_OVERRIDE;
  virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight) MOZ_OVERRIDE;
  virtual bool RequestWindowClose(nsIWidget* aWidget) MOZ_OVERRIDE;
  virtual void WillPaintWindow(nsIWidget* aWidget) MOZ_OVERRIDE;
  virtual bool PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion) MOZ_OVERRIDE;
  virtual void DidPaintWindow() MOZ_OVERRIDE;
  virtual void DidCompositeWindow() MOZ_OVERRIDE;
  virtual void RequestRepaint() MOZ_OVERRIDE;
  virtual nsEventStatus HandleEvent(mozilla::WidgetGUIEvent* aEvent,
                                    bool aUseAttachedEvents) MOZ_OVERRIDE;

  virtual ~nsView();

  nsPoint GetOffsetTo(const nsView* aOther, const int32_t aAPD) const;
  nsIWidget* GetNearestWidget(nsPoint* aOffset, const int32_t aAPD) const;

private:
  explicit nsView(nsViewManager* aViewManager = nullptr,
                  nsViewVisibility aVisibility = nsViewVisibility_kShow);

  bool ForcedRepaint() { return mForcedRepaint; }

  // Do the actual work of ResetWidgetBounds, unconditionally.  Don't
  // call this method if we have no widget.
  void DoResetWidgetBounds(bool aMoveOnly, bool aInvalidateChangedSize);
  void InitializeWindow(bool aEnableDragDrop, bool aResetVisibility);

  bool IsEffectivelyVisible();

  /**
   * Called to indicate that the dimensions of the view have been changed.
   * The x and y coordinates may be < 0, indicating that the view extends above
   * or to the left of its origin position. The term 'dimensions' indicates it
   * is relative to this view.
   */
  void SetDimensions(const nsRect &aRect, bool aPaint = true,
                     bool aResizeWidget = true);

  /**
   * Called to indicate that the visibility of a view has been
   * changed.
   * @param visibility new visibility state
   */
  void SetVisibility(nsViewVisibility visibility);

  /**
   * Set/Get whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   * @result true if the view floats, false otherwise.
   */
  void SetFloating(bool aFloatingView);

  // Helper function to get mouse grabbing off this view (by moving it to the
  // parent, if we can)
  void DropMouseGrabbing();

  // Same as GetBounds but converts to parent appunits if they are different.
  nsRect GetBoundsInParentUnits() const;

  bool HasNonEmptyDirtyRegion() {
    return mDirtyRegion && !mDirtyRegion->IsEmpty();
  }

  void InsertChild(nsView *aChild, nsView *aSibling);
  void RemoveChild(nsView *aChild);

  void ResetWidgetBounds(bool aRecurse, bool aForceSync);
  void AssertNoWindow();

  void NotifyEffectiveVisibilityChanged(bool aEffectivelyVisible);

  // Update the cached RootViewManager for all view manager descendents,
  // If the hierarchy is being removed, aViewManagerParent points to the view
  // manager for the hierarchy's old parent, and will have its mouse grab
  // released if it points to any view in this view hierarchy.
  void InvalidateHierarchy(nsViewManager *aViewManagerParent);

  nsViewManager    *mViewManager;
  nsView           *mParent;
  nsIWidget        *mWindow;
  nsView           *mNextSibling;
  nsView           *mFirstChild;
  nsIFrame         *mFrame;
  nsRegion         *mDirtyRegion;
  int32_t           mZIndex;
  nsViewVisibility  mVis;
  // position relative our parent view origin but in our appunits
  nscoord           mPosX, mPosY;
  // relative to parent, but in our appunits
  nsRect            mDimBounds;
  // in our appunits
  nsPoint           mViewToWidgetOffset;
  uint32_t          mVFlags;
  bool              mWidgetIsTopLevel;
  bool              mForcedRepaint;
};

#endif
