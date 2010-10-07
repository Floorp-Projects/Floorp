/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
  nsView(nsViewManager* aViewManager = nsnull,
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
  virtual void SetDimensions(const nsRect &aRect, PRBool aPaint = PR_TRUE,
                             PRBool aResizeWidget = PR_TRUE);
  void GetDimensions(nsRect &aRect) const { aRect = mDimBounds; aRect.x -= mPosX; aRect.y -= mPosY; }
  void GetDimensions(nsSize &aSize) const { aSize.width = mDimBounds.width; aSize.height = mDimBounds.height; }

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
  void SetZIndex(PRBool aAuto, PRInt32 aZIndex, PRBool aTopMost);

  /**
   * Set/Get whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   * @result PR_TRUE if the view floats, PR_FALSE otherwise.
   */
  NS_IMETHOD  SetFloating(PRBool aFloatingView);

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
                        PRBool aEnableDragDrop,
                        PRBool aResetVisibility);

  // See nsIView::CreateWidgetForParent.
  nsresult CreateWidgetForParent(nsIWidget* aParentWidget,
                                 nsWidgetInitData *aWidgetInitData,
                                 PRBool aEnableDragDrop,
                                 PRBool aResetVisibility);

  // See nsIView::CreateWidgetForPopup.
  nsresult CreateWidgetForPopup(nsWidgetInitData *aWidgetInitData,
                                nsIWidget* aParentWidget,
                                PRBool aEnableDragDrop,
                                PRBool aResetVisibility);

  // NOT in nsIView, so only available in view module
  // These are also present in nsIView, but these versions return nsView and nsViewManager
  // instead of nsIView and nsIViewManager.
  nsView* GetFirstChild() const { return mFirstChild; }
  nsView* GetNextSibling() const { return mNextSibling; }
  nsView* GetParent() const { return mParent; }
  nsViewManager* GetViewManager() const { return mViewManager; }
  // These are superseded by a better interface in nsIView
  PRInt32 GetZIndex() const { return mZIndex; }
  PRBool GetZIndexIsAuto() const { return (mVFlags & NS_VIEW_FLAG_AUTO_ZINDEX) != 0; }
  // This is a better interface than GetDimensions(nsRect&) above
  nsRect GetDimensions() const { nsRect r = mDimBounds; r.MoveBy(-mPosX, -mPosY); return r; }
  // Same as GetBounds but converts to parent appunits if they are different.
  nsRect GetBoundsInParentUnits() const;
  // These are defined exactly the same in nsIView, but for now they have to be redeclared
  // here because of stupid C++ method hiding rules

  PRBool HasNonEmptyDirtyRegion() {
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

  void SetTopMost(PRBool aTopMost) { aTopMost ? mVFlags |= NS_VIEW_FLAG_TOPMOST : mVFlags &= ~NS_VIEW_FLAG_TOPMOST; }
  PRBool IsTopMost() { return((mVFlags & NS_VIEW_FLAG_TOPMOST) != 0); }

  nsPoint ConvertFromParentCoords(nsPoint aPt) const;
  void ResetWidgetBounds(PRBool aRecurse, PRBool aMoveOnly, PRBool aInvalidateChangedSize);
  void SetPositionIgnoringChildWidgets(nscoord aX, nscoord aY);
  void AssertNoWindow();

  void NotifyEffectiveVisibilityChanged(PRBool aEffectivelyVisible);

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
  void DoResetWidgetBounds(PRBool aMoveOnly, PRBool aInvalidateChangedSize);

  nsRegion*    mDirtyRegion;

private:
  void InitializeWindow(PRBool aEnableDragDrop, PRBool aResetVisibility);
};

#endif
