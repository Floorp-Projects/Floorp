/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsView_h___
#define nsView_h___

#include "nsIView.h"
#include "nsRegion.h"
#include "nsRect.h"
#include "nsCRT.h"
#include "nsIFactory.h"
#include "nsEvent.h"
#include <stdio.h>

//mmptemp

class nsIViewManager;
class nsViewManager;

class nsView : public nsIView
{
public:
  nsView(nsViewManager* aViewManager = nullptr,
         nsViewVisibility aVisibility = nsViewVisibility_kShow);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /**
   * Called to indicate that the position of the view has been changed.
   * The specified coordinates are in the parent view's coordinate space.
   * @param x new x position
   * @param y new y position
   */
  virtual void SetPosition(nscoord aX, nscoord aY);
  /**
   * Called to indicate that the dimensions of the view have been changed.
   * The x and y coordinates may be < 0, indicating that the view extends above
   * or to the left of its origin position. The term 'dimensions' indicates it
   * is relative to this view.
   */
  virtual void SetDimensions(const nsRect &aRect, bool aPaint = true,
                             bool aResizeWidget = true);

  /**
   * Called to indicate that the visibility of a view has been
   * changed.
   * @param visibility new visibility state
   */
  NS_IMETHOD  SetVisibility(nsViewVisibility visibility);

  /**
   * Called to indicate that the z-index of a view has been changed.
   * The z-index is relative to all siblings of the view.
   * @param aAuto Indicate that the z-index of a view is "auto". An "auto" z-index
   * means that the view does not define a new stacking context,
   * which means that the z-indicies of the view's children are
   * relative to the view's siblings.
   * @param zindex new z depth
   */
  void SetZIndex(bool aAuto, PRInt32 aZIndex, bool aTopMost);

  /**
   * Set/Get whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   * @result true if the view floats, false otherwise.
   */
  NS_IMETHOD  SetFloating(bool aFloatingView);

  // Helper function to get the view that's associated with a widget
  static nsView* GetViewFor(nsIWidget* aWidget) {
    return static_cast<nsView*>(nsIView::GetViewFor(aWidget));
  }

  // Helper function to get mouse grabbing off this view (by moving it to the
  // parent, if we can)
  void DropMouseGrabbing();

public:
  // See nsIView::CreateWidget.
  nsresult CreateWidget(nsWidgetInitData *aWidgetInitData,
                        bool aEnableDragDrop,
                        bool aResetVisibility);

  // See nsIView::CreateWidgetForParent.
  nsresult CreateWidgetForParent(nsIWidget* aParentWidget,
                                 nsWidgetInitData *aWidgetInitData,
                                 bool aEnableDragDrop,
                                 bool aResetVisibility);

  // See nsIView::CreateWidgetForPopup.
  nsresult CreateWidgetForPopup(nsWidgetInitData *aWidgetInitData,
                                nsIWidget* aParentWidget,
                                bool aEnableDragDrop,
                                bool aResetVisibility);

  // See nsIView::DestroyWidget
  void DestroyWidget();

  // NOT in nsIView, so only available in view module
  // These are also present in nsIView, but these versions return nsView and nsViewManager
  // instead of nsIView and nsIViewManager.
  nsView* GetFirstChild() const { return mFirstChild; }
  nsView* GetNextSibling() const { return mNextSibling; }
  nsView* GetParent() const { return mParent; }
  nsViewManager* GetViewManager() const { return mViewManager; }
  // These are superseded by a better interface in nsIView
  PRInt32 GetZIndex() const { return mZIndex; }
  bool GetZIndexIsAuto() const { return (mVFlags & NS_VIEW_FLAG_AUTO_ZINDEX) != 0; }
  // Same as GetBounds but converts to parent appunits if they are different.
  nsRect GetBoundsInParentUnits() const;

  // These are defined exactly the same in nsIView, but for now they have to be redeclared
  // here because of stupid C++ method hiding rules

  bool HasNonEmptyDirtyRegion() {
    return mDirtyRegion && !mDirtyRegion->IsEmpty();
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

  void InsertChild(nsView *aChild, nsView *aSibling);
  void RemoveChild(nsView *aChild);

  void SetParent(nsView *aParent) { mParent = aParent; }
  void SetNextSibling(nsView *aSibling) { mNextSibling = aSibling; }

  PRUint32 GetViewFlags() const { return mVFlags; }
  void SetViewFlags(PRUint32 aFlags) { mVFlags = aFlags; }

  void SetTopMost(bool aTopMost) { aTopMost ? mVFlags |= NS_VIEW_FLAG_TOPMOST : mVFlags &= ~NS_VIEW_FLAG_TOPMOST; }
  bool IsTopMost() { return((mVFlags & NS_VIEW_FLAG_TOPMOST) != 0); }

  void ResetWidgetBounds(bool aRecurse, bool aForceSync);
  void AssertNoWindow();

  void NotifyEffectiveVisibilityChanged(bool aEffectivelyVisible);

  // Update the cached RootViewManager for all view manager descendents,
  // If the hierarchy is being removed, aViewManagerParent points to the view
  // manager for the hierarchy's old parent, and will have its mouse grab
  // released if it points to any view in this view hierarchy.
  void InvalidateHierarchy(nsViewManager *aViewManagerParent);

  virtual ~nsView();

  nsPoint GetOffsetTo(const nsView* aOther) const;
  nsIWidget* GetNearestWidget(nsPoint* aOffset) const;
  nsPoint GetOffsetTo(const nsView* aOther, const PRInt32 aAPD) const;
  nsIWidget* GetNearestWidget(nsPoint* aOffset, const PRInt32 aAPD) const;

protected:
  // Do the actual work of ResetWidgetBounds, unconditionally.  Don't
  // call this method if we have no widget.
  void DoResetWidgetBounds(bool aMoveOnly, bool aInvalidateChangedSize);

  nsRegion*    mDirtyRegion;

private:
  void InitializeWindow(bool aEnableDragDrop, bool aResetVisibility);
};

#endif
