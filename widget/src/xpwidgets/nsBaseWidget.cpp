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

#include "nsBaseWidget.h"
#include "nsIAppShell.h"
#include "nsIDeviceContext.h"
#include "nsCOMPtr.h"
#include "nsIMenuListener.h"
#include "nsIEnumerator.h"
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

#ifdef NS_DEBUG
#include "nsIServiceManager.h"
#include "nsIPref.h"
#endif

// nsBaseWidget
NS_IMPL_ISUPPORTS1(nsBaseWidget, nsIWidget)


// nsBaseWidget::Enumerator
NS_IMPL_ISUPPORTS2(nsBaseWidget::Enumerator, nsIBidirectionalEnumerator, nsIEnumerator)


//-------------------------------------------------------------------------
//
// nsBaseWidget constructor
//
//-------------------------------------------------------------------------
nsBaseWidget::nsBaseWidget()
:	mClientData(nsnull)
,	mEventCallback(nsnull)
,	mContext(nsnull)
,	mAppShell(nsnull)
,	mToolkit(nsnull)
,	mMouseListener(nsnull)
,	mEventListener(nsnull)
,	mMenuListener(nsnull)
,	mCursor(eCursor_standard)
,	mBorderStyle(eBorderStyle_none)
,	mIsShiftDown(PR_FALSE)
,	mIsControlDown(PR_FALSE)
,	mIsAltDown(PR_FALSE)
,	mIsDestroying(PR_FALSE)
,	mOnDestroyCalled(PR_FALSE)
,	mBounds(0,0,0,0)
,	mVScrollbar(nsnull)
{
    NS_NewISupportsArray(getter_AddRefs(mChildren));
    
    NS_INIT_REFCNT();
}


//-------------------------------------------------------------------------
//
// nsBaseWidget destructor
//
//-------------------------------------------------------------------------
nsBaseWidget::~nsBaseWidget()
{
	NS_IF_RELEASE(mMenuListener);
	NS_IF_RELEASE(mVScrollbar);
}

//-------------------------------------------------------------------------
//
// Basic create.
//
//-------------------------------------------------------------------------
void nsBaseWidget::BaseCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    if (nsnull == mToolkit) {
        if (nsnull != aToolkit) {
            mToolkit = (nsIToolkit*)aToolkit;
            NS_ADDREF(mToolkit);
        }
        else {
            if (nsnull != aParent) {
                mToolkit = (nsIToolkit*)(aParent->GetToolkit()); // the call AddRef's, we don't have to
            }
            // it's some top level window with no toolkit passed in.
            // Create a default toolkit with the current thread
            else {
               static NS_DEFINE_CID(kToolkitCID, NS_TOOLKIT_CID);

               nsresult res;
               res = nsComponentManager::CreateInstance(kToolkitCID, nsnull, NS_GET_IID(nsIToolkit), (void **)&mToolkit);
               if (NS_OK != res)
                  NS_ASSERTION(PR_FALSE, "Can not create a toolkit in nsBaseWidget::Create");
               if (mToolkit)
	               mToolkit->Init(PR_GetCurrentThread());
            }
        }

    }

    mAppShell = aAppShell;
    NS_IF_ADDREF(mAppShell);

    // save the event callback function
    mEventCallback = aHandleEventFunction;

    // keep a reference to the device context
    if (aContext) {
        mContext = aContext;
        NS_ADDREF(mContext);
    }
    else {
      nsresult  res;

      static NS_DEFINE_CID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

      res = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull, NS_GET_IID(nsIDeviceContext), (void **)&mContext);

      if (NS_OK == res)
        mContext->Init(nsnull);
    }

    if (nsnull != aInitData) {
      PreCreateWidget(aInitData);
    }

    if (aParent) {
      aParent->AddChild(this);
    }
}

NS_IMETHODIMP nsBaseWidget::CaptureMouse(PRBool aCapture)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Accessor functions to get/set the client data
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseWidget::GetClientData(void*& aClientData)
{
  aClientData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetClientData(void* aClientData)
{
  mClientData = aClientData;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsBaseWidget
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::Destroy()
{
  // disconnect from the parent
  nsIWidget *parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
    NS_RELEASE(parent);
  }

	NS_IF_RELEASE(mVScrollbar);

  // disconnect listeners.
  NS_IF_RELEASE(mMouseListener);
  NS_IF_RELEASE(mEventListener);
  NS_IF_RELEASE(mMenuListener);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this nsBaseWidget parent
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetParent(void)
{
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Get this nsBaseWidget's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsBaseWidget::GetChildren()
{
  nsIEnumerator* children = nsnull;

  PRUint32 itemCount = 0;
  mChildren->Count(&itemCount);
  if ( itemCount ) {
    children = new Enumerator(*this);
    NS_IF_ADDREF(children);
  }
  return children;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::AddChild(nsIWidget* aChild)
{
  mChildren->AppendElement(aChild);
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::RemoveChild(nsIWidget* aChild)
{
  mChildren->RemoveElement(aChild);
}
    
//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetForegroundColor(void)
{
    return mForeground;
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetForegroundColor(const nscolor &aColor)
{
    mForeground = aColor;
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetBackgroundColor(void)
{
    return mBackground;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetBackgroundColor(const nscolor &aColor)
{
    mBackground = aColor;
    return NS_OK;
}
     
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsBaseWidget::GetCursor()
{
   return mCursor;
}

NS_METHOD nsBaseWidget::SetCursor(nsCursor aCursor)
{
  mCursor = aCursor; 
  return NS_OK;
}
    
//-------------------------------------------------------------------------
//
// Create a rendering context from this nsBaseWidget
//
//-------------------------------------------------------------------------
nsIRenderingContext* nsBaseWidget::GetRenderingContext()
{
  nsIRenderingContext *renderingCtx = NULL;
  nsresult  res;

  static NS_DEFINE_CID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);

  res = nsComponentManager::CreateInstance(kRenderingContextCID, nsnull,
                                           NS_GET_IID(nsIRenderingContext),
                                           (void **)&renderingCtx);

  if (NS_OK == res)
    renderingCtx->Init(mContext, this);

  NS_ASSERTION(NULL != renderingCtx, "Null rendering context");
  
  return renderingCtx;
}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsBaseWidget::GetToolkit()
{
  NS_IF_ADDREF(mToolkit);
  return mToolkit;
}


//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsBaseWidget::GetDeviceContext() 
{
  NS_IF_ADDREF(mContext);
  return mContext; 
}

//-------------------------------------------------------------------------
//
// Return the App Shell
//
//-------------------------------------------------------------------------

nsIAppShell *nsBaseWidget::GetAppShell()
{
    NS_IF_ADDREF(mAppShell);
    return mAppShell;
}


//-------------------------------------------------------------------------
//
// Destroy the window
//
//-------------------------------------------------------------------------
void nsBaseWidget::OnDestroy()
{
    // release references to device context, toolkit, and app shell
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mToolkit);
    NS_IF_RELEASE(mAppShell);
}


//-------------------------------------------------------------------------
//
// Constructor
//
//-------------------------------------------------------------------------

nsBaseWidget::Enumerator::Enumerator(nsBaseWidget & inParent)
  : mCurrentPosition(0), mParent(inParent)
{
  NS_INIT_REFCNT();
}


//-------------------------------------------------------------------------
//
// Destructor
//
//-------------------------------------------------------------------------
nsBaseWidget::Enumerator::~Enumerator()
{   
}


//enumerator interfaces
NS_IMETHODIMP
nsBaseWidget::Enumerator::Next()
{
  PRUint32 itemCount = 0;
  mParent.mChildren->Count(&itemCount);
  if (mCurrentPosition < itemCount - 1 )
    mCurrentPosition ++;
  else
    return NS_ERROR_FAILURE;
  return NS_OK;
}


 
NS_IMETHODIMP
nsBaseWidget::Enumerator::Prev()
{
  if (mCurrentPosition > 0 )
    mCurrentPosition --;
  else
    return NS_ERROR_FAILURE;
  return NS_OK;
}



NS_IMETHODIMP
nsBaseWidget::Enumerator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;

  PRUint32 itemCount = 0;
  mParent.mChildren->Count(&itemCount);
  if ( mCurrentPosition < itemCount ) {
    nsISupports* widget = mParent.mChildren->ElementAt(mCurrentPosition);
//  NS_IF_ADDREF(widget);		already addref'd in nsSupportsArray::ElementAt()
    *aItem = widget;
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}



NS_IMETHODIMP
nsBaseWidget::Enumerator::First()
{
  PRUint32 itemCount = 0;
  mParent.mChildren->Count(&itemCount);
  if ( itemCount ) {
    mCurrentPosition = 0;
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}



NS_IMETHODIMP
nsBaseWidget::Enumerator::Last()
{
  PRUint32 itemCount = 0;
  mParent.mChildren->Count(&itemCount);
  if ( itemCount ) {
    mCurrentPosition = itemCount - 1;
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}



NS_IMETHODIMP
nsBaseWidget::Enumerator::IsDone()
{
  PRUint32 itemCount = 0;
  mParent.mChildren->Count(&itemCount);

  if ((mCurrentPosition == itemCount-1) || (itemCount == 0) ){ //empty lists always return done
    return NS_OK;
  }
  else {
    return NS_COMFALSE;
  }
  return NS_OK;
}


NS_METHOD nsBaseWidget::SetWindowType(nsWindowType aWindowType) 
{
  mWindowType = aWindowType;
  return NS_OK;
}


NS_METHOD nsBaseWidget::SetBorderStyle(nsBorderStyle aBorderStyle)
{
  mBorderStyle = aBorderStyle;
  return NS_OK;
}


NS_METHOD nsBaseWidget::SetTitle(const nsString& aTitle) 
{
  return NS_OK;
} 

/**
* Processes a mouse pressed event
*
**/
NS_METHOD nsBaseWidget::AddMouseListener(nsIMouseListener * aListener)
{
  NS_PRECONDITION(mMouseListener == nsnull, "Null mouse listener");
  NS_IF_RELEASE(mMouseListener);
  NS_ADDREF(aListener);
  mMouseListener = aListener;
  return NS_OK;
}

/**
* Processes a mouse pressed event
*
**/
NS_METHOD nsBaseWidget::AddEventListener(nsIEventListener * aListener)
{
  NS_PRECONDITION(mEventListener == nsnull, "Null mouse listener");
  NS_IF_RELEASE(mEventListener);
  NS_ADDREF(aListener);
  mEventListener = aListener;
  return NS_OK;
}

/**
* Add a menu listener
* This interface should only be called by the menu services manager
* This will AddRef() the menu listener
* This will Release() a previously set menu listener
*
**/

NS_METHOD nsBaseWidget::AddMenuListener(nsIMenuListener * aListener)
{
  NS_IF_RELEASE(mMenuListener);
  NS_IF_ADDREF(aListener);
  mMenuListener = aListener;
  return NS_OK;
}


/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetClientBounds(nsRect &aRect)
{
  return GetBounds(aRect);
}

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}


/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetBoundsAppUnits(nsRect &aRect, float aAppUnits)
{
  aRect = mBounds;
  // Convert to twips
  aRect.x      = nscoord((PRFloat64)aRect.x * aAppUnits);
  aRect.y      = nscoord((PRFloat64)aRect.y * aAppUnits);
  aRect.width  = nscoord((PRFloat64)aRect.width * aAppUnits); 
  aRect.height = nscoord((PRFloat64)aRect.height * aAppUnits);
  return NS_OK;
}

/**
* 
*
**/
NS_METHOD nsBaseWidget::SetBounds(const nsRect &aRect)
{
  mBounds = aRect;

  return NS_OK;
}



/**
* Calculates the border width and height  
*
**/
NS_METHOD nsBaseWidget::GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
  nsRect rectWin;
  nsRect rect;
  GetBounds(rectWin);
  GetClientBounds(rect);

  aWidth  = (rectWin.width - rect.width) / 2;
  aHeight = (rectWin.height - rect.height) / 2;

  return NS_OK;
}


/**
* Calculates the border width and height  
*
**/
void nsBaseWidget::DrawScaledRect(nsIRenderingContext& aRenderingContext, const nsRect & aRect, float aScale, float aAppUnits)
{
  nsRect rect = aRect;

  float x = (float)rect.x;
  float y = (float)rect.y;
  float w = (float)rect.width;
  float h = (float)rect.height;
  float twoAppUnits = aAppUnits * 2.0f;

  for (int i=0;i<int(aScale);i++) {
    rect.x      = nscoord(x);
    rect.y      = nscoord(y);
    rect.width  = nscoord(w);
    rect.height = nscoord(h);
    aRenderingContext.DrawRect(rect);
    x += aAppUnits; 
    y += aAppUnits;
    w -= twoAppUnits; 
    h -= twoAppUnits;
  }
}

/**
* Calculates the border width and height  
*
**/
void nsBaseWidget::DrawScaledLine(nsIRenderingContext& aRenderingContext, 
                                  nscoord aSX, 
                                  nscoord aSY, 
                                  nscoord aEX, 
                                  nscoord aEY, 
                                  float   aScale, 
                                  float   aAppUnits,
                                  PRBool  aIsHorz)
{
  float sx = (float)aSX;
  float sy = (float)aSY;
  float ex = (float)aEX;
  float ey = (float)aEY;

  for (int i=0;i<int(aScale);i++) {
    aSX = nscoord(sx);
    aSY = nscoord(sy);
    aEX = nscoord(ex);
    aEY = nscoord(ey);
    aRenderingContext.DrawLine(aSX, aSY, aEX, aEY);
    if (aIsHorz) {
      sy += aAppUnits; 
      ey += aAppUnits;
    } else {
      sx += aAppUnits; 
      ex += aAppUnits;
    }
  }
}

/**
* Paints default border (XXX - this should be done by CSS)
*
**/
NS_METHOD nsBaseWidget::Paint(nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect)
{
  nsRect rect;
  float  appUnits;
  float  scale;
  nsIDeviceContext * context;
  aRenderingContext.GetDeviceContext(context);

  context->GetCanonicalPixelScale(scale);
  context->GetDevUnitsToAppUnits(appUnits);

  GetBoundsAppUnits(rect, appUnits);
  aRenderingContext.SetColor(NS_RGB(0,0,0));

  DrawScaledRect(aRenderingContext, rect, scale, appUnits);

  NS_RELEASE(context);
  return NS_OK;
}

NS_METHOD nsBaseWidget::SetVerticalScrollbar(nsIWidget * aWidget)
{
  NS_IF_RELEASE(mVScrollbar);
  mVScrollbar = aWidget;
  NS_IF_ADDREF(mVScrollbar);
  return NS_OK;
}

NS_METHOD nsBaseWidget::EnableDragDrop(PRBool aEnable)
{
  return NS_OK;
}

NS_METHOD nsBaseWidget::SetModal(void)
{
  return NS_ERROR_FAILURE;
}

#ifdef NS_DEBUG
//////////////////////////////////////////////////////////////
//
// Convert a GUI event message code to a string.
// Makes it a lot easier to debug events.
//
// See gtk/nsWidget.cpp and windows/nsWindow.cpp
// for a DebugPrintEvent() function that uses
// this.
//
//////////////////////////////////////////////////////////////
/* static */ nsAutoString
nsBaseWidget::debug_GuiEventToString(nsGUIEvent * aGuiEvent)
{
  NS_ASSERTION(nsnull != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName = "UNKNOWN";

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName = _name ; break

  switch(aGuiEvent->message)
  {
    _ASSIGN_eventName(NS_BLUR_CONTENT,"NS_BLUR_CONTENT");
    _ASSIGN_eventName(NS_CONTROL_CHANGE,"NS_CONTROL_CHANGE");
    _ASSIGN_eventName(NS_CREATE,"NS_CREATE");
    _ASSIGN_eventName(NS_DESTROY,"NS_DESTROY");
    _ASSIGN_eventName(NS_DRAGDROP_GESTURE,"NS_DND_GESTURE");
    _ASSIGN_eventName(NS_DRAGDROP_DROP,"NS_DND_DROP");
    _ASSIGN_eventName(NS_DRAGDROP_ENTER,"NS_DND_ENTER");
    _ASSIGN_eventName(NS_DRAGDROP_EXIT,"NS_DND_EXIT");
    _ASSIGN_eventName(NS_DRAGDROP_OVER,"NS_DND_OVER");
    _ASSIGN_eventName(NS_FOCUS_CONTENT,"NS_FOCUS_CONTENT");
    _ASSIGN_eventName(NS_FORM_SELECTED,"NS_FORM_SELECTED");
    _ASSIGN_eventName(NS_FORM_CHANGE,"NS_FORM_CHANGE");
    _ASSIGN_eventName(NS_FORM_RESET,"NS_FORM_RESET");
    _ASSIGN_eventName(NS_FORM_SUBMIT,"NS_FORM_SUBMIT");
    _ASSIGN_eventName(NS_GOTFOCUS,"NS_GOTFOCUS");
    _ASSIGN_eventName(NS_IMAGE_ABORT,"NS_IMAGE_ABORT");
    _ASSIGN_eventName(NS_IMAGE_ERROR,"NS_IMAGE_ERROR");
    _ASSIGN_eventName(NS_IMAGE_LOAD,"NS_IMAGE_LOAD");
    _ASSIGN_eventName(NS_KEY_DOWN,"NS_KEY_DOWN");
    _ASSIGN_eventName(NS_KEY_PRESS,"NS_KEY_PRESS");
    _ASSIGN_eventName(NS_KEY_UP,"NS_KEY_UP");
    _ASSIGN_eventName(NS_LOSTFOCUS,"NS_LOSTFOCUS");
    _ASSIGN_eventName(NS_MENU_SELECTED,"NS_MENU_SELECTED");
    _ASSIGN_eventName(NS_MOUSE_ENTER,"NS_MOUSE_ENTER");
    _ASSIGN_eventName(NS_MOUSE_EXIT,"NS_MOUSE_EXIT");
    _ASSIGN_eventName(NS_MOUSE_LEFT_BUTTON_DOWN,"NS_MOUSE_LEFT_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_LEFT_BUTTON_UP,"NS_MOUSE_LEFT_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_LEFT_CLICK,"NS_MOUSE_LEFT_CLICK");
    _ASSIGN_eventName(NS_MOUSE_LEFT_DOUBLECLICK,"NS_MOUSE_LEFT_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_BUTTON_DOWN,"NS_MOUSE_MIDDLE_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_BUTTON_UP,"NS_MOUSE_MIDDLE_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_CLICK,"NS_MOUSE_MIDDLE_CLICK");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_DOUBLECLICK,"NS_MOUSE_MIDDLE_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MOVE,"NS_MOUSE_MOVE");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_BUTTON_DOWN,"NS_MOUSE_RIGHT_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_BUTTON_UP,"NS_MOUSE_RIGHT_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_CLICK,"NS_MOUSE_RIGHT_CLICK");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_DOUBLECLICK,"NS_MOUSE_RIGHT_DBLCLICK");
    _ASSIGN_eventName(NS_MOVE,"NS_MOVE");
    _ASSIGN_eventName(NS_PAGE_LOAD,"NS_PAGE_LOAD");
    _ASSIGN_eventName(NS_PAGE_UNLOAD,"NS_PAGE_UNLOAD");
    _ASSIGN_eventName(NS_PAINT,"NS_PAINT");
    _ASSIGN_eventName(NS_MENU_CREATE,"NS_MENU_CREATE");
    _ASSIGN_eventName(NS_MENU_DESTROY,"NS_MENU_DESTROY");
    _ASSIGN_eventName(NS_MENU_ACTION, "NS_MENU_ACTION");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_NEXT,"NS_SB_LINE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_PREV,"NS_SB_LINE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_NEXT,"NS_SB_PAGE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_PREV,"NS_SB_PAGE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_POS,"NS_SB_POS");
    _ASSIGN_eventName(NS_SIZE,"NS_SIZE");

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %d",aGuiEvent->message);
      
      eventName = buf;
    }
    break;
  }
  
  return nsAutoString(eventName);
}
//////////////////////////////////////////////////////////////
//
// The idea here is to get the prefs service once and cache it.
// The reason being that this code gets called from OnPaint() 
// which we dont want to slow down.
//
// So, gPrefs will leak when the beast shutdowns.
//
// debug_CleanupCrapSoThatBruceAndPurifyAreHappy() can be called from
// the widget dll unloading hook to cleanup...
//
// But then again, its only debug code, so...
//
//////////////////////////////////////////////////////////////
static nsIPref * gPrefs = nsnull;

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static nsIPref *
_GetPrefService()
{
  if (!gPrefs)
  {
    nsresult rv = nsServiceManager::GetService(kPrefCID, 
                                               NS_GET_IID(nsIPref),
                                               (nsISupports**) &gPrefs);
    
    NS_ASSERTION(NS_SUCCEEDED(rv),"Could not get prefs service.");
    NS_ASSERTION(nsnull != gPrefs,"Prefs services is null.");
  }

  return gPrefs;
}
//////////////////////////////////////////////////////////////
static PRBool
_GetBoolPref(const char * aPrefName)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  PRBool value = PR_FALSE;

  nsIPref * prefs = _GetPrefService();

  if (prefs)
    prefs->GetBoolPref(aPrefName,&value);

  return value;
}
//////////////////////////////////////////////////////////////
static PRInt32
_GetPrintCount()
{
  static PRInt32 sCount = 0;
  
  return ++sCount;
}
//////////////////////////////////////////////////////////////
/* static */ PRBool
nsBaseWidget::debug_WantPaintFlashing()
{
  return _GetBoolPref("nglayout.debug.paint_flashing");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpEvent(FILE *                aFileOut,
                              nsIWidget *           aWidget,
                              nsGUIEvent *          aGuiEvent,
                              const nsCAutoString & aWidgetName,
                              PRInt32               aWindowID)
{
  // NS_PAINT is handled by debug_DumpPaintEvent()
  if (aGuiEvent->message == NS_PAINT)
    return;

  if (aGuiEvent->message == NS_MOUSE_MOVE)
  {
    if (!_GetBoolPref("nglayout.debug.motion_event_dumping"))
      return;
  }
  
  if (aGuiEvent->message == NS_MOUSE_ENTER || 
      aGuiEvent->message == NS_MOUSE_EXIT)
  {
    if (!_GetBoolPref("nglayout.debug.crossing_event_dumping"))
      return;
  }

  if (!_GetBoolPref("nglayout.debug.event_dumping"))
    return;
  
  fprintf(aFileOut,
          "%4d %-26s widget=%-8p name=%-12s id=%-8p pos=%d,%d\n",
          _GetPrintCount(),
          (const char *) nsCAutoString(debug_GuiEventToString(aGuiEvent)),
          (void *) aWidget,
          (const char *) aWidgetName,
          (void *) (aWindowID ? aWindowID : 0x0),
          aGuiEvent->point.x,
          aGuiEvent->point.y);
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   nsPaintEvent *        aPaintEvent,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID)
{
  NS_ASSERTION(nsnull != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nsnull != aWidget,"cmon, the widget is null");
  NS_ASSERTION(nsnull != aPaintEvent,"cmon, the paint event is null");

  if (!_GetBoolPref("nglayout.debug.paint_dumping"))
    return;
  
  fprintf(aFileOut,
          "%4d PAINT      widget=%p name=%-12s id=%-8p rect=", 
          _GetPrintCount(),
          (void *) aWidget,
          (const char *) aWidgetName,
          (void *) aWindowID);
  
  if (aPaintEvent->rect) 
  {
    fprintf(aFileOut,
            "%3d,%-3d %3d,%-3d",
            aPaintEvent->rect->x, 
            aPaintEvent->rect->y,
            aPaintEvent->rect->width, 
            aPaintEvent->rect->height);
  }
  else
  {
    fprintf(aFileOut,"none");
  }
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsRect *        aRect,
                                   PRBool                aIsSynchronous,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID)
{
  if (!_GetBoolPref("nglayout.debug.invalidate_dumping"))
    return;

  NS_ASSERTION(nsnull != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nsnull != aWidget,"cmon, the widget is null");

  fprintf(aFileOut,
          "%4d Invalidate widget=%p name=%-12s id=%-8p",
          _GetPrintCount(),
          (void *) aWidget,
          (const char *) aWidgetName,
          (void *) aWindowID);

  if (aRect) 
  {
    fprintf(aFileOut,
            " rect=%3d,%-3d %3d,%-3d",
            aRect->x, 
            aRect->y,
            aRect->width, 
            aRect->height);
  }
  else
  {
    fprintf(aFileOut,
            " rect=%-15s",
            "none");
  }

  fprintf(aFileOut,
          " sync=%s",
          (const char *) (aIsSynchronous ? "yes" : "no "));
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_CleanupCrapSoThatBruceAndPurifyAreHappy()
{
  if (gPrefs)
  {
    nsServiceManager::ReleaseService(kPrefCID, gPrefs);

    gPrefs = nsnull;
  }
}
//////////////////////////////////////////////////////////////

#endif // NS_DEBUG

