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

#ifndef nsXPFCCanvas_h___
#define nsXPFCCanvas_h___

#include "nsxpfc.h"
#include "nsAgg.h"
#include "nsxpfcCIID.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIWidget.h"
#include "nsIXPFCCanvas.h"
#include "nsILayout.h"
#include "nsIRenderingContext.h"
#include "nsColor.h"
#include "nsPoint.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCCommandReceiver.h"
#include "nsIXPFCSubject.h"
#include "nsIXMLParserObject.h"
#include "nsIImageGroup.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsIView.h"
#include "nsIModel.h"
#include "nsIXPFCCommand.h"
#include "nsXPFCDialogDataHandlerCommand.h"

CLASS_EXPORT_XPFC nsXPFCCanvas : public nsIXPFCCanvas,
                                 public nsIXPFCObserver,
                                 public nsIXMLParserObject,
                                 public nsIImageRequestObserver

{
public:
  nsXPFCCanvas(nsISupports* outer);

  NS_DECL_AGGREGATED

  NS_IMETHOD  Init();

  NS_IMETHOD  Init(nsNativeWidget aNativeParent, const nsRect& aBounds);
  NS_IMETHOD  Init(nsIView * aParent, const nsRect& aBounds);

  NS_IMETHOD  CreateIterator(nsIIterator ** aIterator) ;
  NS_IMETHOD  Layout() ;
  NS_IMETHOD_(nsILayout *)  GetLayout();
  NS_IMETHOD                SetLayout(nsILayout * aLayout) ;


  NS_IMETHOD_(nsString&) GetNameID();
  NS_IMETHOD             SetNameID(nsString& aString);

  NS_IMETHOD_(nsString&) GetLabel();
  NS_IMETHOD             SetLabel(nsString& aString);

  NS_IMETHOD_(nsIModel *)   GetModel();
  NS_IMETHOD                GetModelInterface(const nsIID &aModelIID, nsISupports * aInterface) ;
  NS_IMETHOD                SetModel(nsIModel * aModel);

  NS_IMETHOD  SetBounds(const nsRect& aBounds);
  NS_IMETHOD_(void) GetBounds(nsRect& aRect);

  NS_IMETHOD_(nsEventStatus) DefaultProcessing(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect);

  NS_IMETHOD_(nsEventStatus) OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD_(nsEventStatus) OnMove(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnGotFocus(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnLostFocus(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnLeftButtonDown(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnLeftButtonUp(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnLeftButtonDoubleClick(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnMiddleButtonDown(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMiddleButtonUp(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMiddleButtonDoubleClick(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnRightButtonDown(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnRightButtonUp(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnRightButtonDoubleClick(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnMouseEnter(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMouseExit(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMouseMove(nsGUIEvent *aEvent);

  NS_IMETHOD_(nsEventStatus) OnKeyUp(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnKeyDown(nsGUIEvent *aEvent);

  NS_IMETHOD_(void) AddChildCanvas(nsIXPFCCanvas * aChildCanvas, PRInt32 aPosition = -1);
  NS_IMETHOD_(void) RemoveChildCanvas(nsIXPFCCanvas * aChildCanvas);
  NS_IMETHOD_(void) Reparent(nsIXPFCCanvas * aParentCanvas);

  NS_IMETHOD_(nsEventStatus) PaintBackground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);
  NS_IMETHOD_(nsEventStatus) PaintForeground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);
  NS_IMETHOD_(nsEventStatus) PaintBorder(nsIRenderingContext& aRenderingContext,
                                         const nsRect& aDirtyRect);
  NS_IMETHOD_(nsEventStatus) PaintChildWidgets(nsIRenderingContext& aRenderingContext,
                                               const nsRect& aDirtyRect);

  NS_IMETHOD_(nsEventStatus) ResizeChildWidgets(nsGUIEvent *aEvent);

  NS_IMETHOD  SetOpacity(PRFloat64 aOpacity) ;
  NS_IMETHOD_(PRFloat64) GetOpacity() ;
  NS_IMETHOD  SetVisibility(PRBool aVisibility);
  NS_IMETHOD_(PRBool) GetVisibility();

  NS_IMETHOD  SetTabID(PRUint32 aTabID);
  NS_IMETHOD_(PRUint32) GetTabID();

  NS_IMETHOD  SetTabGroup(PRUint32 aTabGroup);
  NS_IMETHOD_(PRUint32) GetTabGroup();

  NS_IMETHOD_(nsIWidget *) GetWidget();
  NS_IMETHOD_(nsIView *) GetView();
  NS_IMETHOD_(nsIXPFCCanvas *) GetParent();
  NS_IMETHOD_(void) SetParent(nsIXPFCCanvas * aCanvas);

  NS_IMETHOD_(nscolor) GetBackgroundColor(void) ;
  NS_IMETHOD_(void)    SetBackgroundColor(const nscolor &aColor) ;

  NS_IMETHOD_(nscolor) GetForegroundColor(void) ;
  NS_IMETHOD_(void)    SetForegroundColor(const nscolor &aColor) ;

  NS_IMETHOD_(nscolor) GetBorderColor(void) ;
  NS_IMETHOD_(void)    SetBorderColor(const nscolor &aColor) ;

  NS_IMETHOD_(nscolor) Highlight(const nscolor &aColor);
  NS_IMETHOD_(nscolor) Dim(const nscolor &aColor);

  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromPoint(const nsPoint &aPoint);
  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromTab(PRUint32 aTabGroup, PRUint32 aTabID);

  NS_IMETHOD GetClassPreferredSize(nsSize& aSize);
  NS_IMETHOD SetFocus();

  NS_IMETHOD  GetPreferredSize(nsSize &aSize);
  NS_IMETHOD  GetMaximumSize(nsSize &aSize);
  NS_IMETHOD  GetMinimumSize(nsSize &aSize);

  NS_IMETHOD  SetPreferredSize(nsSize &aSize);
  NS_IMETHOD  SetMaximumSize(nsSize &aSize);
  NS_IMETHOD  SetMinimumSize(nsSize &aSize);

  NS_IMETHOD_(PRBool) HasPreferredSize();
  NS_IMETHOD_(PRBool) HasMinimumSize();
  NS_IMETHOD_(PRBool) HasMaximumSize();

  NS_IMETHOD_(PRFloat64) GetMajorAxisWeight();
  NS_IMETHOD_(PRFloat64) GetMinorAxisWeight();
  NS_IMETHOD  SetMajorAxisWeight(PRFloat64 aWeight);
  NS_IMETHOD  SetMinorAxisWeight(PRFloat64 aWeight);

  NS_IMETHOD_(PRUint32) GetChildCount();
  NS_IMETHOD DeleteChildren();
  NS_IMETHOD DeleteChildren(PRUint32 aCount);

  NS_IMETHOD SetCommand(nsString& aCommand);
  NS_IMETHOD_(nsString&) GetCommand();

  NS_IMETHOD_(PRBool) PaintRequested();

  // nsIXPFCObserver methods
  NS_IMETHOD_(nsEventStatus) Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand);


  // nsIXPFCCommandReceiver methods
  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand);

  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromName(nsString& aName);
  NS_IMETHOD CreateView() ;

  /**
   * Get the font for this canvas
   * @result nsFont, the canvas font
   */
  NS_IMETHOD_(nsFont&) GetFont() ;

  /**
   * Set the font for this canvas
   * @param aFont the nsFont for this canvas
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD SetFont(nsFont& aFont) ;

  /**
   * Get the font metrics for this canvas
   * @param nsIFontMetrics, the canvas font metrics
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD GetFontMetrics(nsIFontMetrics ** aFontMetrics) ;

  /**
   * Save canvas graphical state
   * @param aRenderingContext, rendering context to save state to
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD PushState(nsIRenderingContext& aRenderingContext) ;

  /**
   * Get and and set RenderingContext to this graphical state
   * @param aRenderingContext, rendering context to get previously saved state from
   * @return if PR_TRUE, indicates that the clipping region after
   *         popping state is empty, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) PopState(nsIRenderingContext& aRenderingContext) ;

  NS_IMETHOD FindLargestTabGroup(PRUint32& aTabGroup);
  NS_IMETHOD FindLargestTabID(PRUint32 aTabGroup, PRUint32& aTabID);
  NS_IMETHOD_(PRBool)   IsSplittable() ;
  NS_IMETHOD_(PRBool)   IsOverSplittableRegion(nsPoint& aPoint) ;
  NS_IMETHOD_(nsCursor) GetCursor(void) ;
  NS_IMETHOD_(void)     SetCursor(nsCursor aCursor) ;
  NS_IMETHOD_(nsCursor) GetDefaultCursor(nsPoint& aPoint) ;
  NS_IMETHOD_(nsSplittableOrientation)     GetSplittableOrientation(nsPoint& aPoint) ;
  NS_IMETHOD BroadcastCommand(nsIXPFCCommand& aCommand);
  NS_IMETHOD SendCommand();

  // nsIImageRequestObserver
  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);

  NS_IMETHOD_(nsEventStatus) ProcessCommand(nsIXPFCCommand * aCommand) ;


#if defined(DEBUG) && defined(XP_PC)
  NS_IMETHOD  DumpCanvas(FILE * f, PRUint32 indent) ;
#endif

  NS_IMETHOD LoadView(const nsCID &aViewClassIID, 
                      const nsCID * aWidgetClassIID = nsnull,
                      nsIView * aParent = nsnull,
                      nsWidgetInitData * aInitData = nsnull,
                      nsNativeWidget aNativeWidget = nsnull);

protected:
  ~nsXPFCCanvas();
  NS_IMETHOD CreateImageGroup();
  NS_IMETHOD CreateDefaultLayout();
  NS_IMETHOD_(nsIImageRequest *) RequestImage(nsString aUrl);


public:
	PRBool Create(const nsRect& rect, 
                  nsIView * aParent);

private:
  
  nsILayout *     mLayout;
  nsIArray *     mChildWidgets ;
  nscolor         mBackgroundColor;
  nscolor         mForegroundColor;
  nscolor         mBorderColor;
  nsSize          mPreferredSize;
  nsSize          mMaximumSize;
  nsSize          mMinimumSize;
  nsString        mNameID;
  nsString        mLabel;
  PRFloat64       mWeightMajor;
  PRFloat64       mWeightMinor;
  PRFloat64       mOpacity;
  PRBool          mVisibility;
  nsFont          mFont;
  PRUint32        mTabID;
  PRUint32        mTabGroup;
  nsIModel *      mModel;
  nsString        mCommand;
  nsXPFCDialogDataHandlerCommand* mDataCommand;

protected:

  nsIRenderingContext *mRenderingContext;
  nsIXPFCCanvas       *mParent;
  nsIImageRequest     *mImageRequest;
  nsIImageGroup       *mImageGroup;
  nsRect          mBounds;

public:
  nsIView           *mView;

};



#endif /* nsXPFCCanvas_h___ */
