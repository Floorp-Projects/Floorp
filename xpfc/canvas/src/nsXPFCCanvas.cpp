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

#include "nscore.h"
#include "nsAgg.h"
#include "nsxpfcCIID.h"
#include "nsXPFCCanvas.h"
#include "nsIVector.h"
#include "nsIIterator.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIWidget.h"
#include "nsIDeviceContext.h"
#include "nsWidgetsCID.h"
#include "nsColor.h"
#include "nsBoxLayout.h"
#include "nsIXPFCCommand.h"
#include "nsIXMLParserObject.h"
#include "nsBoxLayout.h"
#include "nsxpfcCIID.h"
#include "nsxpfcstrings.h"
#include "nsXPFCMethodInvokerCommand.h"
#include "nsIWebViewerContainer.h"
#include "nsXPFCToolkit.h"
#include "nsIButton.h"
#include "nsITabWidget.h"
#include "nspr.h"
#include "nsViewsCID.h"
#include "nsIViewManager.h"
#include "nsXPFCError.h"

#define DEFAULT_WIDTH  100
#define DEFAULT_HEIGHT 100

#define DELTA 5

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCCanvasCID, NS_XPFC_CANVAS_CID);
static NS_DEFINE_IID(kCBoxLayoutCID,   NS_BOXLAYOUT_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kIXMLParserObjectIID, NS_IXML_PARSER_OBJECT_IID);

static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);
static NS_DEFINE_IID(kXPFCCommandReceiverIID, NS_IXPFC_COMMANDRECEIVER_IID);
static NS_DEFINE_IID(kXPFCCommandCID, NS_XPFC_COMMAND_CID);

static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIViewIID,               NS_IVIEW_IID);
static NS_DEFINE_IID(kViewCID,                NS_VIEW_CID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);


nsXPFCCanvas :: nsXPFCCanvas(nsISupports* outer) :     
                  mFont("Times", 
                         NS_FONT_STYLE_NORMAL,
                         NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_NORMAL,
                         0,
                         12)

{
  NS_INIT_AGGREGATED(outer);

  mLayout = nsnull;
  mParent = nsnull;
  mBackgroundColor = NS_RGB(192,192,192);
  mBorderColor     = NS_RGB(192,192,192);
  mForegroundColor = NS_RGB(0,0,0);

  SetPreferredSize(nsSize(-1,-1));
  SetMaximumSize(nsSize(-1,-1));
  SetMinimumSize(nsSize(-1,-1));

  mWeightMajor = (PRFloat64)(0.0);
  mWeightMinor = (PRFloat64)(0.0);

  mChildWidgets = nsnull;

  // XXX: We should probably auto-generate unique names here
  mNameID  = "Canvas";
  mLabel   = "Default Label";
  mCommand = "";

  mOpacity = 1.0;
  mVisibility = PR_TRUE;

  mBounds.x = 0 ;
  mBounds.y = 0 ;
  mBounds.width = 0 ;
  mBounds.height = 0 ;

  mTabID = 0;
  mTabGroup = 0;

  mImageGroup = nsnull;
  mImageRequest = nsnull;

  mView = nsnull;
  mModel = nsnull;

}

nsXPFCCanvas :: ~nsXPFCCanvas()
{
  DeleteChildren();

  mParent = nsnull;

  NS_IF_RELEASE(mLayout);
  NS_IF_RELEASE(mChildWidgets);
  NS_IF_RELEASE(mModel);

  if (mImageGroup != nsnull)
  {
    mImageGroup->Interrupt();
    NS_IF_RELEASE(mImageRequest);
    NS_RELEASE(mImageGroup);
  }

  if (nsnull != mView)
    mView->Destroy();

}

NS_IMPL_AGGREGATED(nsXPFCCanvas)

nsresult nsXPFCCanvas::AggregatedQueryInterface(const nsIID &aIID, 
                                               void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  static NS_DEFINE_IID(kCXPFCCanvasIID,   NS_XPFC_CANVAS_CID);
  static NS_DEFINE_IID(kIXPFCCanvasIID,   NS_IXPFC_CANVAS_IID);
  static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID,         kIXPFCCanvasIID);

    if (aIID.Equals(kClassIID)) {
        *aInstancePtr = (void*)((nsIXPFCCanvas*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kCXPFCCanvasIID)) {
        *aInstancePtr = (void*)((nsIXPFCCanvas*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIXPFCCanvasIID)) {
        *aInstancePtr = (void*)((nsIXPFCCanvas*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIXMLParserObjectIID)) {
        *aInstancePtr = (void*)((nsIXMLParserObject*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports *)&fAggregated);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kXPFCObserverIID)) {                                          
      *aInstancePtr = (void*)(nsIXPFCObserver *) this;   
      AddRef();                                                            
      return NS_OK;                                                        
    }                                                                      
    if (aIID.Equals(kIImageObserverIID)) {
      *aInstancePtr = (void*)(nsIImageRequestObserver*)this;
      AddRef();
      return NS_OK;
    }
    if (aIID.Equals(kXPFCCommandReceiverIID)) {                                          
      *aInstancePtr = (void*)(nsIXPFCCommandReceiver *) this;   
      AddRef();                                                            
      return NS_OK;                                                        
    }                                                                      

    return NS_NOINTERFACE;
}

nsresult nsXPFCCanvas :: SetNameID(nsString& aString)
{
  mNameID = aString;
  return NS_OK;
}

nsString& nsXPFCCanvas :: GetNameID()
{
  return mNameID;
}

nsresult nsXPFCCanvas :: SetLabel(nsString& aString)
{
  mLabel = aString;
  return NS_OK;
}

nsString& nsXPFCCanvas :: GetLabel()
{
  return mLabel;
}

nsIXPFCCanvas * nsXPFCCanvas::CanvasFromName(nsString& aName)
{
  /*
   * If I am the canvas, just return
   */

  if (aName.EqualsIgnoreCase(mNameID))
    return this;

  /*
   * Ask our children
   */

  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;
  nsIXPFCCanvas * child_canvas ;
  nsString child;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    child = canvas->GetNameID();

    if (child.EqualsIgnoreCase(aName)) {
      NS_RELEASE(iterator);
      return canvas;
    }

    child_canvas = canvas->CanvasFromName(aName);

    if (child_canvas != nsnull) {
      NS_RELEASE(iterator);
      return child_canvas;
    }

    iterator->Next();
  }

  NS_RELEASE(iterator);

  /*
   * Nobody bit...
   */

  return nsnull;
}

nsresult nsXPFCCanvas::FindLargestTabGroup(PRUint32& aTabGroup)
{

  if (aTabGroup == -1)
    aTabGroup = 0;

  if (GetTabGroup() > aTabGroup)
    aTabGroup = GetTabGroup();

  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;
  PRUint32 child_group = 0;


  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    child_group = canvas->GetTabGroup();

    if (child_group > aTabGroup)
      aTabGroup = child_group;

    canvas->FindLargestTabGroup(aTabGroup);

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK;
}

nsresult nsXPFCCanvas::FindLargestTabID(PRUint32 aTabGroup, PRUint32& aTabID)
{

  if (aTabID == -1)
    aTabID = 1;

  if (GetTabGroup() == aTabGroup)
  {
    if (GetTabID() > aTabID)
      aTabID = GetTabID();
  }

  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    if (canvas->GetTabGroup() == aTabGroup)
    {
      if (canvas->GetTabID() > aTabID)
        aTabID = canvas->GetTabID();
    }

    canvas->FindLargestTabID(aTabGroup,aTabID);


    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK;
}

nsresult nsXPFCCanvas :: SetFocus()
{
  if (GetWidget())
    GetWidget()->SetFocus();

  return NS_OK;
}

nsresult nsXPFCCanvas :: CreateImageGroup()
{
  nsresult res = NS_OK;

  if (nsnull != mImageGroup)
    return NS_OK;

  res = NS_NewImageGroup(&mImageGroup);

  if (NS_OK == res) 
  {
    nsIDeviceContext * deviceCtx = nsnull;

    gXPFCToolkit->GetViewManager()->GetDeviceContext(deviceCtx);    

    mImageGroup->Init(deviceCtx,nsnull);

    NS_IF_RELEASE(deviceCtx);
  }

  return res;
}


nsIImageRequest * nsXPFCCanvas :: RequestImage(nsString aUrl)
{
    char * url = aUrl.ToNewCString();

    nsIImageRequest * request ;

    //nscolor bgcolor = NS_RGB(0, 0, 0);

    request = mImageGroup->GetImage(url,
                                   (nsIImageRequestObserver *)this,
                                   nsnull,//&bgcolor,
                                   mBounds.width,
                                   mBounds.height, 
                                   0);

    delete url;

    return request;
}


nsresult nsXPFCCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  PRInt32 error = 0;

  if (aKey.EqualsIgnoreCase(XPFC_STRING_NAME) || aKey.EqualsIgnoreCase(XPFC_STRING_ID)) {

    SetNameID(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_LABEL)) {

    SetLabel(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_TABID)) {

    SetTabID(aValue.ToInteger(&error));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_TABGROUP)) {

    SetTabGroup(aValue.ToInteger(&error));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_KEYBOARDFOCUS)) {

    gXPFCToolkit->GetCanvasManager()->SetFocusedCanvas((nsIXPFCCanvas *)this);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_LAYOUT)) {

    // XXX: Layout should implement this interface.
    //      Then, put functionality in the core layout class
    //      to identify the type of layout object needed.

    if (aValue.EqualsIgnoreCase(XPFC_STRING_XBOX)) {
      ((nsBoxLayout *)GetLayout())->SetLayoutAlignment(eLayoutAlignment_horizontal);
    } else if (aValue.EqualsIgnoreCase(XPFC_STRING_YBOX)) {
      ((nsBoxLayout *)GetLayout())->SetLayoutAlignment(eLayoutAlignment_vertical);
    }

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_BACKGROUNDCOLOR)) {

    nscolor color;
    char * cstring = aValue.ToNewCString();

    if (NS_HexToRGB(cstring,&color) == PR_TRUE)
      SetBackgroundColor(color);

    delete cstring;

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_FOREGROUNDCOLOR)) {

    nscolor color;
    char * cstring = aValue.ToNewCString();

    if (NS_HexToRGB(cstring,&color) == PR_TRUE)
      SetForegroundColor(color);

    delete cstring;

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_HEIGHT)) {

    // XXX: This should be major/minor axis oriented
    nsSize size;
    GetPreferredSize(size);
    SetPreferredSize(nsSize(size.width,aValue.ToInteger(&error)));
    
  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_WIDTH)) {

    nsSize size;
    GetPreferredSize(size);
    SetPreferredSize(nsSize(aValue.ToInteger(&error),size.height));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_BACKGROUNDIMAGE)) {

    /*
     * Request to load the image
     */

    CreateImageGroup();

    mImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_WEIGHTMAJOR)) {

    SetMajorAxisWeight(aValue.ToFloat(&error));

    if (GetMajorAxisWeight() == 0)
    {
      nsSize size;
      nsSize sizeclass;

      GetPreferredSize(size);
      GetClassPreferredSize(sizeclass);

      if (size.width == -1)
        size.width = sizeclass.width;
      if (size.height == -1)
        size.height = sizeclass.height;

      SetPreferredSize(size);
    }

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_WEIGHTMINOR)) {

    SetMinorAxisWeight(aValue.ToFloat(&error));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_HGAP)) {

    ((nsBoxLayout *)GetLayout())->SetHorizontalGap(aValue.ToInteger(&error));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_VGAP)) {

    ((nsBoxLayout *)GetLayout())->SetVerticalGap(aValue.ToInteger(&error));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_COMMAND)) {
    
    SetCommand(aValue);

  } 

  /*
  * Check for tab#
   */

  if (aKey.Compare("tab", PR_TRUE, 3) == 0)
  {
    /*
     * Make sure everything after the "tab" is a valid number
     */
    nsString temp = aKey.Cut(0,3);
    PRInt32 theInt=temp.ToInteger(&error);

    // XXX: Temp hack til clean up widget interface
    if (error == NS_OK && theInt == 1)
    {
      nsString strings[3];

      strings[0] = "Tab1";
      strings[1] = "Tab2";
      strings[2] = "Tab3";

      static NS_DEFINE_IID(kInsTabWidgetIID, NS_ITABWIDGET_IID);

      nsITabWidget * tab = nsnull;
      nsIWidget * widget = GetWidget();
      nsresult res = widget->QueryInterface(kInsTabWidgetIID,(void**)&tab);
      NS_RELEASE(widget);

      if (NS_OK == res)
      {
        tab->SetTabs(3,strings);
        NS_RELEASE(tab);
      }

      return NS_OK;

    }
  }

  return NS_OK;
}


nsresult nsXPFCCanvas :: LoadView(const nsCID &aViewClassIID, 
                                  const nsCID * aWidgetClassIID,
                                  nsIView * aParent,
                                  nsWidgetInitData * aInitData,
                                  nsNativeWidget aNativeWidget)
{
  nsresult res = NS_OK;

  nsRect bounds;

  GetBounds(bounds);

  res = nsRepository::CreateInstance(aViewClassIID, 
                                    nsnull,
                                    kIViewIID, 
                                    (void**)&mView);

  if (NS_OK != res)
    return res;

  gXPFCToolkit->GetCanvasManager()->RegisterView((nsIXPFCCanvas*)this,mView);

  nsIView * view = nsnull;

  if (aParent != nsnull)
    view = aParent;
  else if (GetParent() != nsnull)
    view = GetParent()->GetView();

  nsViewClip clip ;

  mView->Init(gXPFCToolkit->GetViewManager(),
              bounds,
              view,
              aWidgetClassIID,
              aInitData,
              aNativeWidget);

  return res;
}

nsresult nsXPFCCanvas :: CreateView()
{
  
  nsresult res = NS_OK;

  static NS_DEFINE_IID(kCWidgetCID, NS_CHILD_CID);

  res = LoadView(kViewCID, &kCWidgetCID);

  return res;
}

// XXX change API to be XPCOM-ish - caller needs to release!
nsIView * nsXPFCCanvas :: GetView()
{

  if (nsnull != mView)
    return (mView);    

  /*
   * If we have no parent, this usually means it is during parsing.
   * However, a canvas needs to know who it's native parent widget
   * is, so since we know it is at the root grab it there.
   *
   * We do this because the Init() method of canvas variants require
   * knowing the parent native widget in order to do proper initialization.
   *
   * IFF we start offering a ReParent method somewhere, we'll need to deal 
   * with the change in native hierarchy (yuck).
   *
   */

  if (GetParent() == nsnull) {

    nsIXPFCCanvas * root = nsnull;

    gXPFCToolkit->GetRootCanvas(&root);

    if (root == this)
    {
      NS_RELEASE(root);
      return nsnull;
    }

    if (root == nsnull)
      return nsnull;

    nsIView * view = root->GetView();

    NS_RELEASE(root);
    return (view);

  }

  return (GetParent()->GetView());

}

// XXX change API to be XPCOM-ish - caller needs to release!
nsIWidget * nsXPFCCanvas :: GetWidget()
{

  nsIWidget * widget  = nsnull;
  nsIView * view = GetView();

  while(widget == nsnull && view != nsnull)
  {
    view->GetWidget(widget);

    if (widget)
      break;

    view->GetParent(view);

  }
  
  return (widget);
}

nsILayout * nsXPFCCanvas :: GetLayout()
{
    return mLayout;
}

nsresult nsXPFCCanvas :: SetLayout(nsILayout * aLayout)
{
  mLayout = aLayout;
  return NS_OK;
}

nsIModel * nsXPFCCanvas :: GetModel()
{
  if (nsnull != mModel)
    return mModel;

  if (nsnull != GetParent())
    return (GetParent()->GetModel());

  return nsnull;
}

nsresult nsXPFCCanvas :: GetModelInterface(const nsIID &aModelIID, nsISupports * aInterface)
{
  nsIModel * model = GetModel();

  if (nsnull == model)
    return (NS_NOINTERFACE);

  nsresult res = model->QueryInterface(aModelIID, (void**) &aInterface);

  return res;
}


nsresult nsXPFCCanvas :: SetModel(nsIModel * aModel)
{
  NS_IF_RELEASE(mModel);
  mModel = aModel;
  NS_ADDREF(mModel);
  return NS_OK;
}

nsresult nsXPFCCanvas :: Init()
{

  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);

  nsresult res ;

#ifdef NS_WIN32
  #define XPFC_DLL "xpfc10.dll"
#else
  #define XPFC_DLL "libxpfc10.so"
#endif

  static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);

  nsRepository::RegisterFactory(kCVectorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  if (nsnull == mChildWidgets)
  {
    res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mChildWidgets);

    if (NS_OK != res)
      return res ;

    mChildWidgets->Init();
  }

  CreateDefaultLayout();

  return res ;
}

nsresult nsXPFCCanvas :: CreateDefaultLayout()
{
  nsresult res = NS_OK;
 
  if (mLayout == nsnull)
  { 
    res = nsRepository::CreateInstance(kCBoxLayoutCID, 
                                       nsnull, 
                                       kCBoxLayoutCID, 
                                       (void **)&mLayout);

    if (NS_OK != res)
      return res ;

    ((nsBoxLayout *)mLayout)->Init(this);
  }
  return res;
}

nsresult nsXPFCCanvas :: Init(nsNativeWidget aNativeParent, const nsRect& aBounds)
{

  nsresult res = Init();

  SetBounds(aBounds) ;

  static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);

  LoadView(kCViewCID, &kCChildCID, nsnull, nsnull, aNativeParent);

  return res ;
}

nsresult nsXPFCCanvas :: Init(nsIView * aParent, const nsRect& aBounds)
{

  nsresult res = Init();

  Create(aBounds, aParent);

  SetBounds(aBounds) ;

  return res ;
}



PRBool nsXPFCCanvas::Create(const nsRect& rect, 
                            nsIView * aParent) 
{
  static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);

  nsWidgetInitData initData ;

  initData.clipChildren = PR_FALSE;

  LoadView(kCViewCID, &kCChildCID, aParent, &initData);

  return PR_TRUE;
}



nsresult nsXPFCCanvas :: CreateIterator(nsIIterator ** aIterator)
{
  if (mChildWidgets) {
    mChildWidgets->CreateIterator(aIterator);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsXPFCCanvas :: Layout()
{
  if (mLayout)
    return (mLayout->Layout());
  else
    return NS_OK;
}

nsresult nsXPFCCanvas :: SetBounds(const nsRect &aBounds)
{

  mBounds.x = aBounds.x ;
  mBounds.y = aBounds.y ;
  mBounds.width = aBounds.width ;
  mBounds.height = aBounds.height ;

  if (mView != nsnull)
  {
    gXPFCToolkit->GetViewManager()->MoveViewTo(mView, mBounds.x, mBounds.y);
    gXPFCToolkit->GetViewManager()->ResizeView(mView, mBounds.width, mBounds.height);

    nsIWidget * widget = nsnull;

    mView->GetWidget(widget);

    if (widget)
    {
      mBounds.x = 0 ;
      mBounds.y = 0 ;
    }

    NS_IF_RELEASE(widget);
  }

  return NS_OK;
}

void nsXPFCCanvas :: GetBounds(nsRect &aRect)
{

  aRect.x = mBounds.x ;
  aRect.y = mBounds.y ;
  aRect.width = mBounds.width ;
  aRect.height = mBounds.height ;

  return ;
}

nsEventStatus nsXPFCCanvas :: DefaultProcessing(nsGUIEvent *aEvent)
{
  return (nsEventStatus_eIgnore);
}

nsEventStatus nsXPFCCanvas :: OnPaint(nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{

  if (mVisibility == PR_TRUE)
  {
    PushState(aRenderingContext);

    PaintBackground(aRenderingContext, aDirtyRect);
    PaintBorder(aRenderingContext, aDirtyRect);
    PaintForeground(aRenderingContext, aDirtyRect);

    PopState(aRenderingContext);
  }


  PaintChildWidgets(aRenderingContext, aDirtyRect);


  return (DefaultProcessing(nsnull));

}

nsEventStatus nsXPFCCanvas :: OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  mBounds.x = aX ;
  mBounds.y = aY ;
  mBounds.width = aWidth ;
  mBounds.height = aHeight ;

  if (mView != nsnull)
  {
    nsRect bounds = mBounds;

    bounds.x = 0;
    bounds.y = 0;

    gXPFCToolkit->GetViewManager()->UpdateView(mView, bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC) ;
  }

  mLayout->Layout();

  return (DefaultProcessing(nsnull));
}

nsEventStatus nsXPFCCanvas :: OnLeftButtonDown(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnLeftButtonUp(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnLeftButtonDoubleClick(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnMiddleButtonDown(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnMiddleButtonUp(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnMiddleButtonDoubleClick(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnRightButtonDown(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnRightButtonUp(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnRightButtonDoubleClick(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnMove(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnGotFocus(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas :: OnLostFocus(nsGUIEvent *aEvent)
{
  return (DefaultProcessing(aEvent));
}

void nsXPFCCanvas :: AddChildCanvas(nsIXPFCCanvas * aChildCanvas,PRInt32 aPosition)
{
  if (aPosition >= 0)
    mChildWidgets->Insert(aPosition,aChildCanvas);
  else
    mChildWidgets->Append(aChildCanvas);

  aChildCanvas->SetParent(this);

  NS_ADDREF(aChildCanvas);

  return ;
}

void nsXPFCCanvas :: RemoveChildCanvas(nsIXPFCCanvas * aChildCanvas)
{
  mChildWidgets->Remove(aChildCanvas);
  aChildCanvas->SetParent(nsnull);
  NS_IF_RELEASE(aChildCanvas);
  return ;
}

void nsXPFCCanvas :: Reparent(nsIXPFCCanvas * aParentCanvas)
{
  if (GetParent() != nsnull)
    GetParent()->RemoveChildCanvas(this);

  aParentCanvas->AddChildCanvas(this);

  return ;
}

nsIXPFCCanvas * nsXPFCCanvas :: GetParent()
{
  return (mParent);
}

void nsXPFCCanvas :: SetParent(nsIXPFCCanvas * aCanvas)
{
  mParent = aCanvas;

  /*
   * Update ye-ole view system
   */

  if (mView)
    mView->SetParent(aCanvas->GetView());

  return ;
}

nscolor nsXPFCCanvas :: GetBackgroundColor(void)
{
  return mBackgroundColor;
}

void nsXPFCCanvas :: SetBackgroundColor(const nscolor &aColor)
{
  mBackgroundColor = aColor;

  if (GetWidget() != nsnull)
      GetWidget()->SetBackgroundColor(aColor);
}

nscolor nsXPFCCanvas :: GetBorderColor(void)
{
  return mBorderColor;
}

void nsXPFCCanvas :: SetBorderColor(const nscolor &aColor)
{
  mBorderColor = aColor;
}

nscolor nsXPFCCanvas :: GetForegroundColor(void)
{
  return mForegroundColor;
}

void nsXPFCCanvas :: SetForegroundColor(const nscolor &aColor)
{
  mForegroundColor = aColor;

  if (GetWidget() != nsnull)
      GetWidget()->SetForegroundColor(aColor);
}

nscolor nsXPFCCanvas :: Highlight(const nscolor &aColor)
{
  PRIntn r, g, b, max, over;

  r = NS_GET_R(aColor);
  g = NS_GET_G(aColor);
  b = NS_GET_B(aColor);

  //10% of max color increase across the board
  r += 25;
  g += 25;
  b += 25;

  //figure out which color is largest
  if (r > g)
  {
    if (b > r)
      max = b;
    else
      max = r;
  }
  else
  {
    if (b > g)
      max = b;
    else
      max = g;
  }

  //if we overflowed on this max color, increase
  //other components by the overflow amount
  if (max > 255)
  {
    over = max - 255;

    if (max == r)
    {
      g += over;
      b += over;
    }
    else if (max == g)
    {
      r += over;
      b += over;
    }
    else
    {
      r += over;
      g += over;
    }
  }

  //clamp
  if (r > 255)
    r = 255;
  if (g > 255)
    g = 255;
  if (b > 255)
    b = 255;

  return NS_RGBA(r, g, b, NS_GET_A(aColor));
}

nscolor nsXPFCCanvas :: Dim(const nscolor &aColor)
{

  PRIntn r, g, b, max;

  r = NS_GET_R(aColor);
  g = NS_GET_G(aColor);
  b = NS_GET_B(aColor);

  //10% of max color decrease across the board
  r -= 25;
  g -= 25;
  b -= 25;

  //figure out which color is largest
  if (r > g)
  {
    if (b > r)
      max = b;
    else
      max = r;
  }
  else
  {
    if (b > g)
      max = b;
    else
      max = g;
  }

  //if we underflowed on this max color, decrease
  //other components by the underflow amount
  if (max < 0)
  {
    if (max == r)
    {
      g += max;
      b += max;
    }
    else if (max == g)
    {
      r += max;
      b += max;
    }
    else
    {
      r += max;
      g += max;
    }
  }

  //clamp
  if (r < 0)
    r = 0;
  if (g < 0)
    g = 0;
  if (b < 0)
    b = 0;

  return NS_RGBA(r, g, b, NS_GET_A(aColor));
}


nsEventStatus nsXPFCCanvas :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                              const nsRect& aDirtyRect)
{

  nsRect rect;

  // Paint The Background
  GetBounds(rect);
  nsIImage *img;

  if ((nsnull == mImageRequest) || (nsnull == (img = mImageRequest->GetImage())))
  {
    aRenderingContext.SetColor(GetBackgroundColor());
    aRenderingContext.FillRect(rect);
  } else 
  {
    aRenderingContext.DrawImage(img, mBounds.x, mBounds.y);
    NS_RELEASE(img);
  }

  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsXPFCCanvas :: PaintBorder(nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect)
{
  nsRect rect;

  GetBounds(rect);

  rect.x++; rect.y++; rect.width-=2; rect.height-=2;

  aRenderingContext.SetColor(Highlight(GetBorderColor()));
  aRenderingContext.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
  aRenderingContext.DrawLine(rect.x,rect.y,rect.x,rect.y+rect.height);

  aRenderingContext.SetColor(Dim(GetBorderColor()));
  aRenderingContext.DrawLine(rect.x+rect.width,rect.y,rect.x+rect.width,rect.y+rect.height);
  aRenderingContext.DrawLine(rect.x,rect.y+rect.height,rect.x+rect.width,rect.y+rect.height);

  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsXPFCCanvas :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                              const nsRect& aDirtyRect)
{
  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsXPFCCanvas :: PaintChildWidgets(nsIRenderingContext& aRenderingContext,
                                                const nsRect& aDirtyRect)
{
  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas ;
  nsRect rect;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsEventStatus_eConsumeNoDefault;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    canvas->GetBounds(rect);

    if ((rect.width != 0) && (rect.height != 0) /*&& (rect.Contains(aDirtyRect) == PR_TRUE)*/)
      canvas->OnPaint(aRenderingContext,aDirtyRect);

    iterator->Next();
  }

  NS_RELEASE(iterator);
  
  return nsEventStatus_eConsumeNoDefault;
  
}

nsEventStatus nsXPFCCanvas :: ResizeChildWidgets(nsGUIEvent *aEvent)
{
  nsIIterator * iterator;
  nsIXPFCCanvas * widget ;
  nsresult res;


  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsEventStatus_eConsumeNoDefault;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    widget = (nsIXPFCCanvas *) iterator->CurrentItem();

    nscoord x = ((nsSizeEvent*)aEvent)->windowSize->x;
    nscoord y = ((nsSizeEvent*)aEvent)->windowSize->y;
    nscoord w = ((nsSizeEvent*)aEvent)->windowSize->width;
    nscoord h = ((nsSizeEvent*)aEvent)->windowSize->height;

    widget->OnResize(x,y,w,h);

    iterator->Next();
  }
  
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsXPFCCanvas :: HandleEvent(nsGUIEvent *aEvent)
{

  if (aEvent->message == NS_PAINT || aEvent->message == NS_SIZE)
  {
    switch(aEvent->message) 
    {
      case NS_PAINT:
      {
        //if (mVisibility == PR_TRUE)

        // Optimization:: check to see that atleast one child in
        //                the hierarchy wants to be painted.

        if (PaintRequested() == PR_TRUE)
        {

          nsEventStatus es ;
          nsRect rect;
          nsDrawingSurface ds;
          nsIRenderingContext * ctx = ((nsPaintEvent*)aEvent)->renderingContext;

          rect.x = ((nsPaintEvent *)aEvent)->rect->x;
          rect.y = ((nsPaintEvent *)aEvent)->rect->y;
          rect.width = ((nsPaintEvent *)aEvent)->rect->width;
          rect.height = ((nsPaintEvent *)aEvent)->rect->height;

          ds = ctx->CreateDrawingSurface(&rect);
          ctx->SelectOffScreenDrawingSurface(ds);

          es = OnPaint((*((nsPaintEvent*)aEvent)->renderingContext),(*((nsPaintEvent*)aEvent)->rect));

          ctx->CopyOffScreenBits(rect);
          ctx->DestroyDrawingSurface(ds);

          return es;
        } else {

          // Invalidate all children with native widgets
          nsIIterator * iterator;
          nsXPFCCanvas * canvas ;

          nsresult res = CreateIterator(&iterator);

          if (NS_OK != res)
            return (DefaultProcessing(aEvent));

          iterator->Init();

          while(!(iterator->IsDone()))
          {
            canvas = (nsXPFCCanvas *) iterator->CurrentItem();

            if (canvas->mView != nsnull)
            {
              nsRect bounds;
              canvas->mView->GetBounds(bounds);
              bounds.x = 0;
              bounds.y = 0;
              gXPFCToolkit->GetViewManager()->UpdateView(canvas->mView, bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC) ;
            }

            iterator->Next();
          }

          NS_IF_RELEASE(iterator);

        }

        return (DefaultProcessing(aEvent));

      }
      break;

      case NS_SIZE:
      {
        nscoord x = ((nsSizeEvent*)aEvent)->windowSize->x;
        nscoord y = ((nsSizeEvent*)aEvent)->windowSize->y;
        nscoord w = ((nsSizeEvent*)aEvent)->windowSize->width;
        nscoord h = ((nsSizeEvent*)aEvent)->windowSize->height;

        OnResize(x,y,w,h);
      }
      break;

      case NS_TABCHANGE:
      {
        // TBD...
        int iiii = 0;
      }
      break;

    }
    return (DefaultProcessing(aEvent));
  }

  if (aEvent->message == NS_DESTROY)
    return (DefaultProcessing(aEvent));

  /*
   * XXX: Make this a HandleInputEvent...
   */

  nsIXPFCCanvas * canvas ;

  canvas = CanvasFromPoint(aEvent->point);

  if (canvas) {
    switch(aEvent->message) {

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      {
        gXPFCToolkit->GetCanvasManager()->SetPressedCanvas(canvas);
        canvas->OnLeftButtonDown(aEvent);
      }
      break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      {
        gXPFCToolkit->GetCanvasManager()->SetPressedCanvas(nsnull);
        canvas->OnLeftButtonUp(aEvent);
      }
      break;

      case NS_MOUSE_RIGHT_BUTTON_DOWN:
      {
        gXPFCToolkit->GetCanvasManager()->SetPressedCanvas(canvas);
        canvas->OnLeftButtonDown(aEvent);
      }
      break;

      case NS_MOUSE_RIGHT_BUTTON_UP:
      {
        gXPFCToolkit->GetCanvasManager()->SetPressedCanvas(nsnull);
        canvas->OnLeftButtonUp(aEvent);
      }
      break;

      case NS_MOUSE_MOVE:
      {
        if ((gXPFCToolkit->GetCanvasManager()->GetMouseOverCanvas() != nsnull) && (gXPFCToolkit->GetCanvasManager()->GetMouseOverCanvas() != canvas))
        {
          gXPFCToolkit->GetCanvasManager()->GetMouseOverCanvas()->OnMouseExit(aEvent);
          canvas->OnMouseEnter(aEvent);
        }
        gXPFCToolkit->GetCanvasManager()->SetMouseOverCanvas(canvas);
        canvas->OnMouseMove(aEvent);
      }
      break;

      case NS_MOUSE_ENTER:
      {
        canvas->OnMouseEnter(aEvent);
      }
      break;
      
      case NS_MOUSE_EXIT:
      {
        canvas->OnMouseExit(aEvent);
      }
      break;

      case NS_KEY_UP:
      {
        canvas->OnKeyUp(aEvent);
      }
      break;

      case NS_KEY_DOWN:
      {
        canvas->OnKeyDown(aEvent);
      }
      break;


    }
  }

  return (DefaultProcessing(aEvent));
  
}

PRBool nsXPFCCanvas::PaintRequested()
{
  PRBool bRequested = mVisibility;
  nsIIterator * iterator;
  nsXPFCCanvas * canvas ;

  nsresult res = CreateIterator(&iterator);

  if (NS_OK != res)
    return bRequested;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsXPFCCanvas *) iterator->CurrentItem();

    if (canvas->mView == nsnull)
    {
      
      if(canvas->GetVisibility() == PR_TRUE)
      {
        bRequested = PR_TRUE;
        break;
      } else {

        if (canvas->PaintRequested() == PR_TRUE)
        {
          bRequested = PR_TRUE;
          break;
        }
        
      }

    }

    iterator->Next();
  }

  NS_IF_RELEASE(iterator);

  return bRequested;
}

nsIXPFCCanvas * nsXPFCCanvas :: CanvasFromPoint(const nsPoint &aPoint)
{
  nsresult res;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas ;
  nsIXPFCCanvas * candidate = nsnull;
  nsIXPFCCanvas * winner = nsnull;
  nsRect rect;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();
  
    canvas->GetBounds(rect);

    if (rect.Contains(aPoint))
    {
      candidate = canvas;
      break;
    }

    iterator->Next();
  }
  
  if (candidate == nsnull)
  {
    winner = this ;
  } else 
  {
    nsIXPFCCanvas * child = candidate->CanvasFromPoint(aPoint);

    if (child == nsnull)
      winner = candidate;
    else
      winner = candidate->CanvasFromPoint(aPoint);
  }

  NS_RELEASE(iterator);

  return winner;

}

/*
 * -1 for group or id means find the last one in that collection
 */

nsIXPFCCanvas * nsXPFCCanvas :: CanvasFromTab(PRUint32 aTabGroup, PRUint32 aTabID)
{

  if (aTabGroup == -1)
    FindLargestTabGroup(aTabGroup);

  if (aTabID == -1)
    FindLargestTabID(aTabGroup,aTabID);

  /*
   * If I am the canvas, just return
   */

  if ((GetTabGroup() == aTabGroup) && (GetTabID() == aTabID))
    return this;

  /*
   * Ask our children
   */

  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;
  nsIXPFCCanvas * child_canvas ;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    if (canvas->GetTabGroup() == aTabGroup && canvas->GetTabID() == aTabID)
    {
      NS_RELEASE(iterator);
      return canvas;
    }

    child_canvas = canvas->CanvasFromTab(aTabGroup,aTabID);

    if (child_canvas != nsnull) {
      NS_RELEASE(iterator);
      return child_canvas;
    }

    iterator->Next();
  }

  NS_RELEASE(iterator);

  /*
   * Nobody bit...
   */

  return nsnull;

}

PRFloat64 nsXPFCCanvas :: GetMajorAxisWeight()
{
  return (mWeightMajor);
}

PRFloat64 nsXPFCCanvas :: GetMinorAxisWeight()
{
  return (mWeightMinor);
}

nsresult nsXPFCCanvas :: SetMajorAxisWeight(PRFloat64 aWeight)
{
  mWeightMajor = aWeight;
  return NS_OK;
}

nsresult nsXPFCCanvas :: SetMinorAxisWeight(PRFloat64 aWeight)
{
  mWeightMinor = aWeight;
  return NS_OK;
}

/*
 * Our preferred size is the preferred (max) of all children
 */

nsresult nsXPFCCanvas :: GetClassPreferredSize(nsSize& aSize)
{
  nsresult res;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas ;
  nsSize temp;

  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return (res);

  iterator->Init();

  aSize.width  = 0;
  aSize.height = 0;

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();
  
    canvas->GetClassPreferredSize(temp);

    if (temp.width > aSize.width)
      aSize.width = temp.width;
    if (temp.height > aSize.height)
      aSize.height = temp.height;

    iterator->Next();
  }
  
  NS_RELEASE(iterator);


  if (aSize.width  == 0)
    aSize.width  = DEFAULT_WIDTH;
  if (aSize.height == 0)
    aSize.height = DEFAULT_HEIGHT;

  return (NS_OK);
}

nsresult nsXPFCCanvas :: GetPreferredSize(nsSize &aSize)
{
  aSize = mPreferredSize ;
  return NS_OK;
}
nsresult nsXPFCCanvas :: GetMaximumSize(nsSize &aSize)
{
  aSize = mMaximumSize ;
  return NS_OK;
}
nsresult nsXPFCCanvas :: GetMinimumSize(nsSize &aSize)
{
  aSize = mMinimumSize ;
  return NS_OK;
}

nsresult nsXPFCCanvas :: SetPreferredSize(nsSize &aSize)
{
  mPreferredSize = aSize;
  return NS_OK;
}
nsresult nsXPFCCanvas :: SetMaximumSize(nsSize &aSize)
{
  mMaximumSize = aSize;
  return NS_OK;
}
nsresult nsXPFCCanvas :: SetMinimumSize(nsSize &aSize)
{
  mMinimumSize = aSize;
  return NS_OK;
}

PRBool nsXPFCCanvas :: HasPreferredSize()
{
  if (mPreferredSize.width == -1 && mPreferredSize.height == -1)
    return PR_FALSE;

  return PR_TRUE;
}

PRBool nsXPFCCanvas :: HasMinimumSize()
{
  if (mMinimumSize.width == -1 && mMinimumSize.height == -1)
    return PR_FALSE;

  return PR_TRUE;
}

PRBool nsXPFCCanvas :: HasMaximumSize()
{
  if (mMaximumSize.width == -1 && mMaximumSize.height == -1)
    return PR_FALSE;

  return PR_TRUE;
}

PRUint32 nsXPFCCanvas :: GetChildCount()
{
  nsresult res ;
  nsIIterator * iterator ;
  PRUint32 count = 0 ;

  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return res;

  iterator->Init();

  count = iterator->Count();

  NS_IF_RELEASE(iterator);

  return (count);  
}

nsresult nsXPFCCanvas :: DeleteChildren()
{
  return (DeleteChildren(GetChildCount()));
}

nsresult nsXPFCCanvas :: DeleteChildren(PRUint32 aCount)
{
  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * widget ;
  PRUint32 index = 0;

  // Iterate through the children ... backwards
  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return res;

  iterator->Init();

  iterator->Last();

  while(aCount != index)
  {
    widget = (nsIXPFCCanvas *) iterator->CurrentItem();

    if (widget != nsnull)
      widget->DeleteChildren();

    if (mChildWidgets != nsnull)
      mChildWidgets->Remove(widget);

    NS_IF_RELEASE(widget);

    index++;

    iterator->Previous();
  }
  
  NS_IF_RELEASE(iterator);

  return NS_OK;
  
}

#if defined(DEBUG) && defined(XP_PC)
nsresult nsXPFCCanvas :: DumpCanvas(FILE * f, PRUint32 indent)
{
  nsRect bounds;

  for (PRInt32 i = indent; --i >= 0; ) fputs("  ", f);

  GetBounds(bounds);

  fprintf(f, "Canvas [x,y,width,height] = [%d,%d,%d,%d]\n",bounds.x, bounds.y, bounds.width, bounds.height);

  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * widget ;

  // Iterate through the children
  res = CreateIterator(&iterator);

  if (NS_OK == res) {

    iterator->Init();

    while(!(iterator->IsDone()))
    {
      widget = (nsIXPFCCanvas *) iterator->CurrentItem();

      widget->DumpCanvas(f, indent+1);

      iterator->Next();
    }

    NS_RELEASE(iterator);

  }
  

  for (i = indent; --i >= 0; ) fputs("  ", f);
  fputs(">\n", f);

  return 0;
}
#endif



nsEventStatus nsXPFCCanvas::Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand)
{

  /*
   * Update our internal structure based on this update
   */

  return (Action(aCommand));
}

/*
 *  If we cannot process the command, pass it up the food chain
 */

nsEventStatus nsXPFCCanvas::ProcessCommand(nsIXPFCCommand * aCommand)
{
  if (GetParent())
    return (GetParent()->ProcessCommand(aCommand));
 
  // XXX: Until we figure out the relationship of embeddable widgets
  //      and containing windows, pass directly to our owner...

  nsIWebViewerContainer * container = gXPFCToolkit->GetCanvasManager()->GetWebViewerContainer();

  if (container)
    return (container->ProcessCommand(aCommand));

  return (nsEventStatus_eIgnore); 
}

nsEventStatus nsXPFCCanvas::Action(nsIXPFCCommand * aCommand)
{

  /*
   * If this is a MethodInvoker Command, invoke it.
   */

  nsresult res;

  nsXPFCMethodInvokerCommand * methodinvoker_command = nsnull;
  static NS_DEFINE_IID(kXPFCMethodInvokerCommandCID, NS_XPFC_METHODINVOKER_COMMAND_CID);                 

  res = aCommand->QueryInterface(kXPFCMethodInvokerCommandCID,(void**)&methodinvoker_command);

  if (NS_OK == res)
  {

    if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_SETBACKGROUNDCOLOR)
     || methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_SETFOREGROUNDCOLOR)
     || methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_SETBORDERCOLOR))
    {
      nscolor color;

      char * ccolor = methodinvoker_command->mParams.ToNewCString();

      NS_HexToRGB(ccolor, &color);
      
      if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_SETBACKGROUNDCOLOR))
        SetBackgroundColor(color);
      else if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_SETFOREGROUNDCOLOR))
        SetForegroundColor(color);
      else if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_SETBORDERCOLOR))
        SetBorderColor(color);

      delete ccolor;

      NSRESULT_TO_NSTRING(NS_OK,methodinvoker_command->mReply);
    }
    else if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_GETBACKGROUNDCOLOR)
          || methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_GETFOREGROUNDCOLOR)
          || methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_GETBORDERCOLOR))
    {
      nscolor color;
      
      if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_GETBACKGROUNDCOLOR))
        color = GetBackgroundColor();
      else if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_GETFOREGROUNDCOLOR))
        color = GetForegroundColor();
      else if (methodinvoker_command->mMethod.EqualsIgnoreCase(XPFC_STRING_GETBORDERCOLOR))
        color = GetBorderColor();

      nsString reply_string("#");

      nscolor r = NS_GET_R(color);
      nscolor g = NS_GET_G(color);
      nscolor b = NS_GET_B(color);

      reply_string.Append(r,16);
      if (0 == r) reply_string.Append(r,16);
      reply_string.Append(g,16);
      if (0 == g) reply_string.Append(g,16);
      reply_string.Append(b,16);
      if (0 == b) reply_string.Append(b,16);

      methodinvoker_command->mReply = reply_string;
    }

    NS_RELEASE(methodinvoker_command);
  }

  /*
   * Just paint ourselves for now
   */

  nsRect bounds;

  GetView()->GetBounds(bounds);

  bounds.x = 0;
  bounds.y = 0;

  gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);

  return nsEventStatus_eIgnore;
}

nsCursor nsXPFCCanvas::GetCursor()
{
  nsIWidget * widget = GetWidget();
  nsCursor cursor = eCursor_standard;

  if (widget != nsnull)
  {
    cursor = widget->GetCursor();
    NS_RELEASE(widget);
  }

  return (cursor);
}

nsCursor nsXPFCCanvas::GetDefaultCursor(nsPoint& aPoint)
{
  nsCursor cursor = eCursor_standard;

  if (IsSplittable() == PR_TRUE)
  {
    if (IsOverSplittableRegion(aPoint) == PR_TRUE)
    {
      nsSplittableOrientation orientation = GetSplittableOrientation(aPoint) ;

      switch(orientation)
      {
        case nsSplittableOrientation_eastwest:
          cursor = eCursor_sizeWE;
          break;

        case nsSplittableOrientation_northsouth:
          cursor = eCursor_sizeNS;
          break;

      }
    }
  } 

  return (cursor);
}

void nsXPFCCanvas::SetCursor(nsCursor aCursor)
{
  nsIWidget * widget = GetWidget();

  if (widget != nsnull)
  {
    widget->SetCursor(aCursor);
    NS_RELEASE(widget);
  }

  return ;

}

nsEventStatus nsXPFCCanvas::OnMouseMove(nsGUIEvent *aEvent)
{

  SetCursor(GetDefaultCursor(((nsEvent*)aEvent)->point));

  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas::OnMouseEnter(nsGUIEvent *aEvent)
{  

  SetCursor(GetDefaultCursor(((nsEvent*)aEvent)->point));

  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas::OnMouseExit(nsGUIEvent *aEvent)
{  
  SetCursor(GetDefaultCursor(((nsEvent*)aEvent)->point));

  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas::OnKeyUp(nsGUIEvent *aEvent)
{  
  return (DefaultProcessing(aEvent));
}

nsEventStatus nsXPFCCanvas::OnKeyDown(nsGUIEvent *aEvent)
{  

  if (((nsKeyEvent*)aEvent)->keyCode == NS_VK_TAB)
  {
    nsIXPFCCanvas * canvas = nsnull;
    nsIXPFCCanvas * parent = nsnull ;
    nsIXPFCCanvas * temp   = GetParent() ;

    while (temp != nsnull)
    {
      parent = temp;
      temp = parent->GetParent();
    }

    if (parent == nsnull)
      parent = this;

    if (((nsKeyEvent*)aEvent)->isShift == PR_TRUE)
    {

      if (GetTabID() != 1)
        canvas = parent->CanvasFromTab(GetTabGroup(), GetTabID()-1);

      if (canvas == nsnull && GetTabGroup() != 0)
        canvas = parent->CanvasFromTab(GetTabGroup()-1, -1);

      if (canvas == nsnull)
        canvas = parent->CanvasFromTab(-1, -1);

      if (canvas != nsnull)
        gXPFCToolkit->GetCanvasManager()->SetFocusedCanvas(canvas);
    } else
    {
      canvas = parent->CanvasFromTab(GetTabGroup(), GetTabID()+1);

      if (canvas == nsnull)
        canvas = parent->CanvasFromTab(GetTabGroup()+1, 1);

      if (canvas == nsnull)
        canvas = parent->CanvasFromTab(0, 1);

      if (canvas != nsnull)
        gXPFCToolkit->GetCanvasManager()->SetFocusedCanvas(canvas);
    }
    
  }
  return (DefaultProcessing(aEvent));
}

nsresult nsXPFCCanvas::SetOpacity(PRFloat64 aOpacity)
{
  mOpacity = aOpacity;
  return NS_OK;
}

PRFloat64 nsXPFCCanvas::GetOpacity()
{
  return mOpacity;
}

nsresult nsXPFCCanvas::SetVisibility(PRBool aVisibility)
{
  mVisibility = aVisibility;
  return NS_OK;
}

PRBool nsXPFCCanvas::GetVisibility()
{
  return mVisibility;
}

nsresult nsXPFCCanvas::SetTabID(PRUint32 aTabID)
{
  mTabID = aTabID;
  return NS_OK;
}

PRUint32 nsXPFCCanvas::GetTabID()
{
  return mTabID;
}

nsresult nsXPFCCanvas::SetTabGroup(PRUint32 aTabGroup)
{
  mTabGroup = aTabGroup;
  return NS_OK;
}

PRUint32 nsXPFCCanvas::GetTabGroup()
{
  return mTabGroup;
}

nsresult nsXPFCCanvas::SetFont(nsFont& aFont)
{
  mFont = aFont;
  return NS_OK;
}

nsFont& nsXPFCCanvas::GetFont()
{
  return mFont;
}

nsresult nsXPFCCanvas::GetFontMetrics(nsIFontMetrics ** aFontMetrics)
{
  nsIDeviceContext * context = nsnull;

  if (GetWidget())
    context = GetWidget()->GetDeviceContext();
  
  if (nsnull != context)
    context->GetMetricsFor(mFont,*aFontMetrics);

  NS_IF_RELEASE(context);

  return NS_OK;
}

nsresult nsXPFCCanvas::PushState(nsIRenderingContext& aRenderingContext)
{
  aRenderingContext.PushState();

  aRenderingContext.SetFont(mFont);

  return NS_OK;
}

PRBool nsXPFCCanvas::PopState(nsIRenderingContext& aRenderingContext)
{
  return (aRenderingContext.PopState());
}

void nsXPFCCanvas::Notify(nsIImageRequest *aImageRequest,
                          nsIImage *aImage,
                          nsImageNotification aNotificationType,
                          PRInt32 aParam1, PRInt32 aParam2,
                          void *aParam3)
{
  nsRect bounds;

  GetView()->GetBounds(bounds);

  if (aNotificationType == nsImageNotification_kImageComplete)
  {
    bounds.x = 0;
    bounds.y = 0;
    gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);
  }
  return ;
}

void nsXPFCCanvas::NotifyError(nsIImageRequest *aImageRequest,
                               nsImageError aErrorType)
{
  return ;
}




nsresult nsXPFCCanvas :: SetCommand(nsString& aCommand)
{
  mCommand = aCommand;
  return NS_OK;
}

nsString& nsXPFCCanvas :: GetCommand()
{
  return (mCommand);
}

PRBool nsXPFCCanvas :: IsSplittable()
{
  return (PR_TRUE);
}

PRBool nsXPFCCanvas :: IsOverSplittableRegion(nsPoint& aPoint)
{
  /*
   * For now, if we are 5 pixels within our border, return true
   */

  nsRect bounds;
  PRBool bSplittable = PR_FALSE;

  GetBounds(bounds);

  if (bounds.Contains(aPoint) == PR_FALSE)
    return bSplittable;


  if (aPoint.x <= bounds.x+DELTA) {

    bSplittable = PR_TRUE;
  
  } else if( aPoint.x >= bounds.XMost()-DELTA) {

    bSplittable = PR_TRUE;

  } else if (aPoint.y <= bounds.y+DELTA) {

    bSplittable = PR_TRUE;
  
  } else if( aPoint.y <= bounds.YMost()-DELTA) {

    bSplittable = PR_TRUE;

  }

  return (bSplittable);
}

nsSplittableOrientation nsXPFCCanvas :: GetSplittableOrientation(nsPoint& aPoint)
{

  nsRect bounds;
  nsSplittableOrientation orientation = nsSplittableOrientation_none;

  GetBounds(bounds);

  if (aPoint.x <= bounds.x+DELTA) {

    orientation = nsSplittableOrientation_eastwest;
  
  } else if( aPoint.x >= bounds.XMost()-DELTA) {

    orientation = nsSplittableOrientation_eastwest;

  } else if (aPoint.y <= bounds.y+DELTA) {

    orientation = nsSplittableOrientation_northsouth;
  
  } else if( aPoint.y >= bounds.YMost()-DELTA) {

    orientation = nsSplittableOrientation_northsouth;

  }

  return (orientation);
}

nsresult nsXPFCCanvas :: SendCommand()
{
  if (mCommand.Length() > 0)
  {
    nsIWebViewerContainer * webViewerContainer = nsnull;
    gXPFCToolkit->GetApplicationShell()->GetWebViewerContainer(&webViewerContainer);

    if (webViewerContainer != nsnull)
    {
      webViewerContainer->LoadURL(mCommand,nsnull);
      NS_RELEASE(webViewerContainer);
    }
  }
  return NS_OK;
}

nsresult nsXPFCCanvas :: BroadcastCommand(nsIXPFCCommand& aCommand)
{
  nsresult res ;
  nsIIterator * iterator ;
  nsIXPFCCanvas * canvas;
  nsEventStatus status ;

  /*
   * Call this container's Action Method. We should change the action
   * method to return something about ignoring and consuming commands
   */

  status = Action(&aCommand);

  if (nsEventStatus_eConsumeNoDefault == status)
    return NS_OK;

  /*
   * Iterate through the children and pass it on
   */

  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return NS_OK;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    canvas->BroadcastCommand(aCommand);

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK;
}

