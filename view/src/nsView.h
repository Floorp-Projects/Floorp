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
#include "nsIWidget.h"
#include "nsRect.h"
#include "nsCRT.h"
#include "nsIFactory.h"
#include "nsIViewObserver.h"
#include "nsEvent.h"
#include <stdio.h>

//mmptemp

class nsIRegion;
class nsIRenderingContext;
class nsIViewManager;
class nsViewManager;
class nsZPlaceholderView;

class nsView : public nsIView
{
public:
  nsView();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /**
   * Called to indicate that the specified rect of the view
   * needs to be drawn via the rendering context. The rect
   * is specified in view coordinates.
   * @param rc rendering context to paint into
   * @param rect damage area
   * @param aPaintFlags see nsIView.h for flag definitions
   * @return PR_TRUE if the entire clip region has been eliminated, else PR_FALSE
   */
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &aResult);
  /**
   * Called to indicate that the specified region of the view
   * needs to be drawn via the rendering context. The region
   * is specified in view coordinates.
   * @param rc rendering context to paint into
   * @param region damage area
   * @param aPaintFlags see nsIView.h for flag definitions
   * @return PR_TRUE if the entire clip region has been eliminated, else PR_FALSE
   */
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsIRegion& region,
                    PRUint32 aPaintFlags, PRBool &aResult);

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
   * or to the left of its origin position.
   */
  virtual void SetDimensions(const nsRect &aRect, PRBool aPaint = PR_TRUE);
  void GetDimensions(nsRect &aRect) const { aRect = mDimBounds; aRect.x -= mPosX; aRect.y -= mPosY; }
  void GetDimensions(nsSize &aSize) const { aSize.width = mDimBounds.width; aSize.height = mDimBounds.height; }

  /**
   * This checks whether the view is a placeholder for some view that has
   * been reparented to a different geometric parent.
   */
  virtual PRBool IsZPlaceholderView() const { return PR_FALSE; }

  /**
   * Called to set the clip of the children of this view.
   * The clip is relative to the origin of the view.
   * All of the children of this view will be clipped using
   * the specified rectangle
   */
  void SetClipChildrenToRect(const nsRect* aRect) {
    if (!aRect) {
      delete mClipRect;
      mClipRect = nsnull;
    } else {
      if (mClipRect) {
        *mClipRect = *aRect;
      } else {
        mClipRect = new nsRect(*aRect);
      }
    }
  }
  void SetClipChildrenToBounds(PRBool aDoClip) {
    mVFlags = (mVFlags & ~NS_VIEW_FLAG_CLIP_CHILDREN_TO_BOUNDS)
      | (aDoClip ? NS_VIEW_FLAG_CLIP_CHILDREN_TO_BOUNDS : 0);
  }
  void SetClipPlaceholdersToBounds(PRBool aDoClip) {
    mVFlags = (mVFlags & ~NS_VIEW_FLAG_CLIP_PLACEHOLDERS_TO_BOUNDS)
      | (aDoClip ? NS_VIEW_FLAG_CLIP_PLACEHOLDERS_TO_BOUNDS : 0);
  }

  /**
   * Called to get the dimensions and position of the clip for the children of this view.
   */
  const nsRect* GetClipChildrenToRect() const
  { return mClipRect; }
  PRBool GetClipChildrenToBounds(PRBool aPlaceholders) const
  { return (mVFlags & (aPlaceholders ? NS_VIEW_FLAG_CLIP_PLACEHOLDERS_TO_BOUNDS : NS_VIEW_FLAG_CLIP_CHILDREN_TO_BOUNDS)) != 0; }

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
  /**
   * Note: This didn't exist in 4.0. Called to set the opacity of a view. 
   * A value of 0.0 means completely transparent. A value of 1.0 means
   * completely opaque.
   * @param opacity new opacity value
   */
  NS_IMETHOD  SetOpacity(float opacity);
  /**
   * Used set the transparency status of the content in a view. see
   * HasTransparency().
   * @param aTransparent PR_TRUE if there are transparent areas, PR_FALSE otherwise.
   */
  NS_IMETHOD  SetContentTransparency(PRBool aTransparent);
  /**
   * Gets the dirty region associated with this view. Used by the view
   * manager.
   */
  nsresult GetDirtyRegion(nsIRegion*& aRegion);
  /**
   * Set the widget associated with this view.
   * @param aWidget widget to associate with view. It is an error
   *        to associate a widget with more than one view. To disassociate
   *        a widget from a view, use nsnull. If there are no more references
   *        to the widget that may have been associated with the view, it will
   *        be destroyed.
   * @return error status
   */
  NS_IMETHOD  SetWidget(nsIWidget *aWidget);

  /**
   * @return the view's dimensions after clipping by ancestors is applied
   * (the rect is relative to the view's origin)
   */
  nsRect GetClippedRect();

  /**
   * Sync your widget size and position with the view
   */
  NS_IMETHOD SynchWidgetSizePosition();


  // Helper function to get the view that's associated with a widget
  static nsView*  GetViewFor(nsIWidget* aWidget);

  // Helper function to get mouse grabbing off this view (by moving it to the
  // parent, if we can)
  void DropMouseGrabbing();

public:
  // NOT in nsIView, so only available in view module
  nsZPlaceholderView* GetZParent() const { return mZParent; }
  // These are also present in nsIView, but these versions return nsView and nsViewManager
  // instead of nsIView and nsIViewManager.
  nsView* GetFirstChild() const { return mFirstChild; }
  nsView* GetNextSibling() const { return mNextSibling; }
  nsView* GetParent() const { return mParent; }
  nsViewManager* GetViewManager() const { return mViewManager; }
  // These are superceded by a better interface in nsIView
  PRInt32 GetZIndex() const { return mZIndex; }
  PRBool GetZIndexIsAuto() const { return (mVFlags & NS_VIEW_FLAG_AUTO_ZINDEX) != 0; }
  // This is a better interface than GetDimensions(nsRect&) above
  nsRect GetDimensions() const { nsRect r = mDimBounds; r.MoveBy(-mPosX, -mPosY); return r; }
  // These are defined exactly the same in nsIView, but for now they have to be redeclared
  // here because of stupid C++ method hiding rules

  void InsertChild(nsView *aChild, nsView *aSibling);
  void RemoveChild(nsView *aChild);

  void SetParent(nsView *aParent) { mParent = aParent; }
  void SetZParent(nsZPlaceholderView *aZParent) { mZParent = aZParent; }
  void SetNextSibling(nsView *aSibling) { mNextSibling = aSibling; }

  PRUint32 GetViewFlags() const { return mVFlags; }
  void SetViewFlags(PRUint32 aFlags) { mVFlags = aFlags; }

  void SetTopMost(PRBool aTopMost) { aTopMost ? mVFlags |= NS_VIEW_FLAG_TOPMOST : mVFlags &= ~NS_VIEW_FLAG_TOPMOST; }
  PRBool IsTopMost() { return((mVFlags & NS_VIEW_FLAG_TOPMOST) != 0); }

  // Don't use this method when you want to adjust an nsPoint.
  // Just write "pt += view->GetPosition();"
  // When everything's converted to nsPoint, this can go away.
  void ConvertToParentCoords(nscoord* aX, nscoord* aY) const { *aX += mPosX; *aY += mPosY; }
  // Don't use this method when you want to adjust an nsPoint.
  // Just write "pt -= view->GetPosition();"
  // When everything's converted to nsPoint, this can go away.
  void ConvertFromParentCoords(nscoord* aX, nscoord* aY) const { *aX -= mPosX; *aY -= mPosY; }
  void ResetWidgetPosition(PRBool aRecurse);
  void SetPositionIgnoringChildWidgets(nscoord aX, nscoord aY);
  nsresult LoadWidget(const nsCID &aClassIID);

  virtual ~nsView();

protected:
  nsZPlaceholderView*mZParent;

  // mClipRect is relative to the view's origin.
  nsRect*         mClipRect;
  nsIRegion*      mDirtyRegion;
  PRPackedBool    mChildRemoved;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif
