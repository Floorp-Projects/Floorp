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

//mmptemp

class nsIPresContext;
class nsIViewManager;

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

  NS_IMETHOD  Destroy();
  NS_IMETHOD  GetViewManager(nsIViewManager *&aViewMgr) const;
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &aResult);
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsIRegion& region,
                    PRUint32 aPaintFlags, PRBool &aResult);
  NS_IMETHOD  HandleEvent(nsGUIEvent *event, 
                          PRUint32 aEventFlags,
                          nsEventStatus* aStatus,
                          PRBool aForceHandle,
                          PRBool& aHandled);
  NS_IMETHOD  SetPosition(nscoord x, nscoord y);
  NS_IMETHOD  GetPosition(nscoord *x, nscoord *y) const;
  NS_IMETHOD  SetDimensions(nscoord width, nscoord height, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  GetDimensions(nscoord *width, nscoord *height) const;
  NS_IMETHOD  SetBounds(const nsRect &aBounds, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  GetBounds(nsRect &aBounds) const;
  NS_IMETHOD  SetChildClip(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD  GetChildClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom) const;
  NS_IMETHOD  SetVisibility(nsViewVisibility visibility);
  NS_IMETHOD  GetVisibility(nsViewVisibility &aVisibility) const;
  NS_IMETHOD  SetZParent(nsIView *aZParent);
  NS_IMETHOD  GetZParent(nsIView *&aZParent) const;
  NS_IMETHOD  SetZIndex(PRInt32 aZIndex);
  NS_IMETHOD  GetZIndex(PRInt32 &aZIndex) const;
  NS_IMETHOD  SetAutoZIndex(PRBool aAutoZIndex);
  NS_IMETHOD  GetAutoZIndex(PRBool &aAutoZIndex) const;
  NS_IMETHOD  SetFloating(PRBool aFloatingView);
  NS_IMETHOD  GetFloating(PRBool &aFloatingView) const;
  NS_IMETHOD  SetParent(nsIView *aParent);
  NS_IMETHOD  GetParent(nsIView *&aParent) const;
  NS_IMETHOD  GetNextSibling(nsIView *&aNextSibling) const;
  NS_IMETHOD  SetNextSibling(nsIView* aNextSibling);
  NS_IMETHOD  InsertChild(nsIView *child, nsIView *sibling);
  NS_IMETHOD  RemoveChild(nsIView *child);
  NS_IMETHOD  GetChildCount(PRInt32 &aCount) const;
  NS_IMETHOD  GetChild(PRInt32 index, nsIView*& aChild) const;
  NS_IMETHOD  SetOpacity(float opacity);
  NS_IMETHOD  GetOpacity(float &aOpacity) const;
  NS_IMETHOD  HasTransparency(PRBool &aTransparent) const;
  NS_IMETHOD  SetContentTransparency(PRBool aTransparent);
  NS_IMETHOD  SetClientData(void *aData);
  NS_IMETHOD  GetClientData(void *&aData) const;
  NS_IMETHOD  GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget);
  NS_IMETHOD  GetDirtyRegion(nsIRegion*& aRegion) const;
  NS_IMETHOD  CreateWidget(const nsIID &aWindowIID,
                           nsWidgetInitData *aWidgetInitData = nsnull,
                           nsNativeWidget aNative = nsnull,
                           PRBool aEnableDragDrop = PR_TRUE);
  NS_IMETHOD  SetWidget(nsIWidget *aWidget);
  NS_IMETHOD  GetWidget(nsIWidget *&aWidget) const;
  NS_IMETHOD  HasWidget(PRBool *aHasWidget) const;
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD  SetViewFlags(PRUint32 aFlags);
  NS_IMETHOD  ClearViewFlags(PRUint32 aFlags);
  NS_IMETHOD  GetViewFlags(PRUint32 *aFlags) const;
  NS_IMETHOD  SetCompositorFlags(PRUint32 aFlags);
  NS_IMETHOD  GetCompositorFlags(PRUint32 *aFlags);
  NS_IMETHOD  GetExtents(nsRect *aExtents);
  NS_IMETHOD  GetClippedRect(nsRect& aClippedRect, PRBool& aIsClipped, PRBool& aEmpty) const;


  // XXX Temporary for Bug #19416
  NS_IMETHOD IgnoreSetPosition(PRBool aShouldIgnore);

  NS_IMETHOD SynchWidgetSizePosition();


  // Helper function to get the view that's associated with a widget
  static nsIView*  GetViewFor(nsIWidget* aWidget);

   // Helper function to determine if the view instance is the root view
  PRBool IsRoot();

   // Helper function to determine if the view point is inside of a view
  PRBool PointIsInside(nsIView& aView, nscoord x, nscoord y) const;

protected:
  virtual ~nsView();
  //
  virtual nsresult LoadWidget(const nsCID &aClassIID);

protected:
  nsIViewManager    *mViewManager;
  nsIView           *mParent;
  nsIWidget         *mWindow;

  nsIView           *mZParent;

  //XXX should there be pointers to last child so backward walking is fast?
  nsIView           *mNextSibling;
  nsIView           *mFirstChild;
  void              *mClientData;
  PRInt32           mZindex;
  nsViewVisibility  mVis;
  PRInt32           mNumKids;
  nsRect            mBounds;
  nsViewClip        mChildClip;
  float             mOpacity;
  PRUint32          mVFlags;
  nsIRegion*        mDirtyRegion;
  PRUint32			    mCompositorFlags;

  // Bug #19416
  PRBool            mShouldIgnoreSetPosition;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif
