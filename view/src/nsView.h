/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsView_h___
#define nsView_h___

#include "nsIView.h"
#include "nsRect.h"
#include "nsCRT.h"
#include "nsIWidget.h"
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

// IID for the nsIClipView interface
#define NS_ICLIPVIEW_IID    \
{ 0x4cc36160, 0xd282, 0x11d2, \
{ 0x90, 0x67, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 } }

class nsIClipView : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICLIPVIEW_IID)
};

//Flag to determine whether the view will check if events can be handled
//by its children or just handle the events itself
#define NS_VIEW_FLAG_DONT_CHECK_CHILDREN  0x0001

// indicates that the view is or contains a placeholder view
#define NS_VIEW_FLAG_CONTAINS_PLACEHOLDER 0x0002

//the view is transparent
#define NS_VIEW_FLAG_TRANSPARENT          0x0004

//indicates that the view should not be bitblt'd when moved
//or scrolled and instead must be repainted
#define NS_VIEW_FLAG_DONT_BITBLT          0x0010

// indicates that the view is using auto z-indexing
#define NS_VIEW_FLAG_AUTO_ZINDEX          0x0020

// indicates that the view is a floating view.
#define NS_VIEW_FLAG_FLOATING             0x0040

// set if our widget resized. 
#define NS_VIEW_FLAG_WIDGET_RESIZED       0x0080

// set if our widget moved. 
#define NS_VIEW_FLAG_WIDGET_MOVED         0x0100
#define NS_VIEW_FLAG_CLIPCHILDREN         0x0200

// if set it indicates that this view should be 
// displayed above z-index:auto views if this view 
// is z-index:auto also
#define NS_VIEW_FLAG_TOPMOST              0x0400

class nsView : public nsIView
{
public:
  nsView();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIView
  NS_IMETHOD  Init(nsIViewManager* aManager,
      						 const nsRect &aBounds,
                   const nsIView *aParent,
      						 nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);
  NS_IMETHOD  GetViewManager(nsIViewManager *&aViewMgr) const;
  NS_IMETHOD  GetPosition(nscoord *x, nscoord *y) const;
  NS_IMETHOD  GetBounds(nsRect &aBounds) const;
  NS_IMETHOD  GetVisibility(nsViewVisibility &aVisibility) const;
  NS_IMETHOD  GetZIndex(PRBool &aAuto, PRInt32 &aZIndex, PRBool &aTopMost) const;
  PRInt32     GetZIndex() const { return mZIndex; }
  PRBool      GetZIndexIsAuto() const { return (mVFlags & NS_VIEW_FLAG_AUTO_ZINDEX) != 0; }
  NS_IMETHOD  GetFloating(PRBool &aFloatingView) const;
  NS_IMETHOD  GetParent(nsIView *&aParent) const;
  NS_IMETHOD  GetNextSibling(nsIView *&aNextSibling) const;
  NS_IMETHOD  GetOpacity(float &aOpacity) const;
  NS_IMETHOD  HasTransparency(PRBool &aTransparent) const;
  NS_IMETHOD  SetClientData(void *aData);
  NS_IMETHOD  GetClientData(void *&aData) const;
  NS_IMETHOD  GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget);
  NS_IMETHOD  CreateWidget(const nsIID &aWindowIID,
                           nsWidgetInitData *aWidgetInitData = nsnull,
                           nsNativeWidget aNative = nsnull,
                           PRBool aEnableDragDrop = PR_TRUE,
                           PRBool aResetVisibility = PR_TRUE);
  NS_IMETHOD  GetWidget(nsIWidget *&aWidget) const;
  NS_IMETHOD  HasWidget(PRBool *aHasWidget) const;
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  NS_IMETHOD  Destroy();
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
   * Called to indicate that the specified event should be handled
   * by the view. This method should return nsEventStatus_eConsumeDoDefault
   * or nsEventStatus_eConsumeNoDefault if the event has been handled.
   *
   * This is a hook giving the view a chance to handle the event specially.
   * By default we just bounce the event back to the view manager for display
   * list processing.
   *
   * @param event event to process
   * @result processing status
   */
  virtual nsEventStatus HandleEvent(nsViewManager* aVM, nsGUIEvent *aEvent, PRBool aCaptured);

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
  virtual PRBool IsZPlaceholderView() { return PR_FALSE; }

  /**
   * Called to set the clip of the children of this view.
   * The clip is relative to the origin of the view.
   * All of the children of this view will be clipped using
   * the specified rectangle
   */
  void SetClipChildren(PRBool aDoClip) {
    mVFlags = (mVFlags & ~NS_VIEW_FLAG_CLIPCHILDREN) | (aDoClip ? NS_VIEW_FLAG_CLIPCHILDREN : 0);
  }
  void SetChildClip(const nsRect &aRect) { mChildClip = aRect; }
  /**
   * Called to get the dimensions and position of the clip for the children of this view.
   */
  PRBool GetClipChildren() const { return (mVFlags & NS_VIEW_FLAG_CLIPCHILDREN) != 0; }
  void GetChildClip(nsRect &aRect) const { aRect = mChildClip; }

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
   * Return a rectangle containing the view's bounds adjusted for it's ancestors clipping
   * @param aClippedRect views bounds adjusted for ancestors clipping. If aEmpty is TRUE it
   * aClippedRect is set to an empty rect.
   * @param aIsClipped returns with PR_TRUE if view's rectangle is clipped by an ancestor
   * @param aEmpty returns with PR_TRUE if view's rectangle is 'clipped out'
   */
  NS_IMETHOD  GetClippedRect(nsRect& aClippedRect, PRBool& aIsClipped, PRBool& aEmpty) const;


  // XXX Temporary for Bug #19416
  NS_IMETHOD IgnoreSetPosition(PRBool aShouldIgnore);

  /**
   * Sync your widget size and position with the view
   */
  NS_IMETHOD SynchWidgetSizePosition();


  // Helper function to get the view that's associated with a widget
  static nsView*  GetViewFor(nsIWidget* aWidget);

   // Helper function to determine if the view instance is the root view
  PRBool IsRoot();

   // Helper function to determine if the view point is inside of a view
  PRBool PointIsInside(nsView& aView, nscoord x, nscoord y) const;

public: // NOT in nsIView, so only available in view module
  nsView* GetFirstChild() const { return mFirstChild; }
  nsView* GetNextSibling() const { return mNextSibling; }
  nsView* GetParent() const { return mParent; }
  nsZPlaceholderView* GetZParent() const { return mZParent; }
  nsViewManager* GetViewManager() const { return mViewManager; }
  nsViewVisibility GetVisibility() const { return mVis; }
  void* GetClientData() const { return mClientData; }
  PRBool GetFloating() const { return (mVFlags & NS_VIEW_FLAG_FLOATING) != 0; }

  PRInt32 GetChildCount() const { return mNumKids; }
  nsView* GetChild(PRInt32 aIndex) const;

  void InsertChild(nsView *aChild, nsView *aSibling);
  void RemoveChild(nsView *aChild);

  void SetParent(nsView *aParent) { mParent = aParent; }
  void SetZParent(nsZPlaceholderView *aZParent) { mZParent = aZParent; }
  void SetNextSibling(nsView *aSibling) { mNextSibling = aSibling; }

  PRUint32 GetViewFlags() const { return mVFlags; }
  void SetViewFlags(PRUint32 aFlags) { mVFlags = aFlags; }

  void SetTopMost(PRBool aTopMost) { aTopMost ? mVFlags |= NS_VIEW_FLAG_TOPMOST : mVFlags &= ~NS_VIEW_FLAG_TOPMOST; }
  PRBool IsTopMost() { return((mVFlags & NS_VIEW_FLAG_TOPMOST) != 0); }

  void ConvertToParentCoords(nscoord* aX, nscoord* aY) const { *aX += mPosX; *aY += mPosY; }
  void ConvertFromParentCoords(nscoord* aX, nscoord* aY) const { *aX -= mPosX; *aY -= mPosY; }

protected:
  virtual ~nsView();
  virtual nsresult LoadWidget(const nsCID &aClassIID);

protected:
  nsViewManager     *mViewManager;
  nsView            *mParent;
  nsIWidget         *mWindow;

  nsZPlaceholderView*mZParent;

  //XXX should there be pointers to last child so backward walking is fast?
  nsView            *mNextSibling;
  nsView            *mFirstChild;
  void              *mClientData;
  PRInt32           mZIndex;
  nsViewVisibility  mVis;
  PRInt32           mNumKids;
  nscoord           mPosX, mPosY;
  nsRect            mDimBounds; // relative to parent
  nsRect            mChildClip;
  float             mOpacity;
  PRUint32          mVFlags;
  nsIRegion*        mDirtyRegion;
  // Bug #19416
  PRPackedBool      mShouldIgnoreSetPosition;
  PRPackedBool      mChildRemoved;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif
