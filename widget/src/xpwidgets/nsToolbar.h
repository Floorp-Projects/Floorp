/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsToolbar_h___
#define nsToolbar_h___

#include "nsIToolbar.h" //*** not for long
#include "nsIContentConnector.h"
#include "nsWindow.h"
#include "nsIToolbarItem.h"
#include "nsDataModelWidget.h"


class ToolbarLayoutInfo;
class nsIImageGroup;
class nsToolbarDataModel;


//
// pinkerton's notes 
//
// The only access to the toolbars should be through the DOM. As a result,
// we don't need a separate toolbar interface to the outside world besides the
// minimum required to be loaded by the loader (nsILoader or something). The
// |nsIToolbar| interface will probably go away soon.
//

//------------------------------------------------------------
class nsToolbar : public nsDataModelWidget,
                  public nsIToolbar,  //*** not for long
                  public nsIContentConnector,
                  public nsIToolbarItem
{
public:
    nsToolbar();
    virtual ~nsToolbar();

    NS_DECL_ISUPPORTS

	// nsIContentConnector Interface --------------------------------
	NS_IMETHOD SetContentRoot(nsIContent* pContent);
	NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

	// nsIToolbar interface that won't be around much longer....
    NS_IMETHOD AddItem(nsIToolbarItem* anItem, PRInt32 aLeftGap, PRBool aStretchable);
    NS_IMETHOD InsertItemAt(nsIToolbarItem* anItem, 
                            PRInt32         aLeftGap, 
                            PRBool          aStretchable, 
                            PRInt32         anIndex);
    NS_IMETHOD GetItemAt(nsIToolbarItem*& anItem, PRInt32 anIndex);
    NS_IMETHOD DoLayout();
    NS_IMETHOD SetHorizontalLayout(PRBool aDoHorizontalLayout);
    NS_IMETHOD SetHGap(PRInt32 aGap);
    NS_IMETHOD SetVGap(PRInt32 aGap);
    NS_IMETHOD SetMargin(PRInt32 aMargin);
    NS_IMETHOD SetLastItemIsRightJustified(const PRBool & aState);
    NS_IMETHOD SetNextLastItemIsStretchy(const PRBool & aState);
    NS_IMETHOD SetBorderType(nsToolbarBorderType aBorderType);
    NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                       const nsRect& aDirtyRect);
	virtual void HandleDataModelEvent(int event, nsHierarchicalDataItem* pItem) ;

    // nsIToolbarItem Interface  --------------------------------
    NS_IMETHOD Repaint(PRBool aIsSynchronous);
    NS_IMETHOD GetBounds(nsRect &aRect);
    NS_IMETHOD SetVisible(PRBool aState);
    //NS_IMETHOD IsVisible(PRBool & aState);
    NS_IMETHOD SetLocation(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD SetBounds(PRUint32 aWidth,
                        PRUint32 aHeight,
                        PRBool   aRepaint);
    NS_IMETHOD SetBounds(PRUint32 aX,
                         PRUint32 aY,
                         PRUint32 aWidth,
                         PRUint32 aHeight,
                         PRBool   aRepaint);
    //NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);


    // Overriding nsWindow & nsIToolbarItem
    NS_IMETHOD IsVisible(PRBool & aIsVisible);
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);

    NS_IMETHOD            Resize(PRUint32 aWidth,
                                   PRUint32 aHeight,
                                   PRBool   aRepaint);

    NS_IMETHOD            Resize(PRUint32 aX,
                                   PRUint32 aY,
                                   PRUint32 aWidth,
                                   PRUint32 aHeight,
                                   PRBool   aRepaint);

  NS_IMETHOD SetWrapping(PRBool aDoWrap);
  NS_IMETHOD GetWrapping(PRBool & aDoWrap);

  NS_IMETHOD GetPreferredConstrainedSize(PRInt32& aSuggestedWidth, PRInt32& aSuggestedHeight, 
                                         PRInt32& aWidth,          PRInt32& aHeight);
  // Override the widget creation method
  NS_IMETHOD Create(nsIWidget *aParent,
                            const nsRect &aRect,
                            EVENT_CALLBACK aHandleEventFunction,
                            nsIDeviceContext *aContext,
                            nsIAppShell *aAppShell,
                            nsIToolkit *aToolkit,
                            nsWidgetInitData *aInitData);

protected:
  void GetMargins(PRInt32 &aX, PRInt32 &aY);
  void DoHorizontalLayout(const nsRect& aTBRect);
  void DoVerticalLayout(const nsRect& aTBRect);

  // General function for painting a background image.
  void PaintBackgroundImage(nsIRenderingContext& drawCtx, 
						  nsIImage* bgImage, const nsRect& constraintRect,
						  int xSrcOffset = 0, int ySrcOffset = 0);

  //*** these should be smart pointers ***
  nsToolbarDataModel* mDataModel;   // The data source from which everything to draw is obtained.

  //*** This will all be stored in the DOM
  ToolbarLayoutInfo ** mItems;
  PRInt32              mNumItems;

  PRBool mLastItemIsRightJustified;
  PRBool mNextLastItemIsStretchy;

  PRInt32 mMargin;
  PRInt32 mWrapMargin;
  PRInt32 mHGap;
  PRInt32 mVGap;

  PRBool  mBorderType;

  PRBool  mWrapItems;
  PRBool  mDoHorizontalLayout;

};

#endif /* nsToolbar_h___ */
