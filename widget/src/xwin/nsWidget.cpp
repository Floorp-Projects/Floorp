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

#include "nsWidget.h"
#include "nsIFontMetrics.h"
#include "nsIFontCache.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsStringUtil.h"
#include <windows.h>
#include "nsGfxCIID.h"
#include "resource.h"
#include <commctrl.h>

#include "prtime.h"

BOOL nsWidget::sIsRegistered = FALSE;

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);



// DoCreateTooltip - creates a tooltip control and adds some tools  
//     to it. 
// Returns the handle of the tooltip control if successful or NULL
//     otherwise. 
// hwndOwner - handle of the owner window 
// 

void nsWidget::AddTooltip(HWND hwndOwner,nsRect& aRect) 
{ 
}

void nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
}

void nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
} 

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWidget::SetTooltips(PRUint32 aNumberOfTips,const nsRect* aTooltipAreas)
{
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWidget::UpdateTooltips(const nsRect* aNewTips)
{

}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWidget::RemoveTooltips()
{
}

//-------------------------------------------------------------------------
//
// Convert nsEventStatus value to a windows boolean
//
//-------------------------------------------------------------------------

PRBool nsWidget::ConvertStatus(nsEventStatus aStatus)
{
}

//-------------------------------------------------------------------------
//
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------

void nsWidget::InitEvent(nsGUIEvent& event, PRUint32 aEventType)
{
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

PRBool nsWidget::DispatchEvent(nsGUIEvent* event)
{
  PRBool result = PR_FALSE;
 
  return(result); 
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWidget::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event;
  InitEvent(event, aMsg);
  return(DispatchEvent(&event));
}



//-------------------------------------------------------------------------
//
// nsWidget constructor
//
//-------------------------------------------------------------------------
nsWidget::nsWidget(nsISupports *aOuter) : nsObject(aOuter)
{
    /*mWnd           = 0;
    mPrevWndProc   = NULL;
    mChildren      = NULL;
    mEventCallback = NULL;
    mToolkit       = NULL;
    mMouseListener = NULL;
    mEventListener = NULL;
    mBackground    = ::GetSysColor(COLOR_WINDOW);
    mBrush         = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
    mForeground    = ::GetSysColor(COLOR_WINDOWTEXT);
    mPalette       = NULL;
    mCursor        = eCursor_standard;
    mBorderStyle   = eBorderStyle_none;
    mIsShiftDown   = PR_FALSE;
    mIsControlDown = PR_FALSE;
    mIsAltDown     = PR_FALSE;
    mIsDestroying = PR_FALSE;
    mTooltip       = NULL;
    mDeferredPositioner = NULL;*/
}


//-------------------------------------------------------------------------
//
// nsWidget destructor
//
//-------------------------------------------------------------------------
nsWidget::~nsWidget()
{
    //mIsDestroying = PR_TRUE;
    //Destroy();
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsWidget::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsObject::QueryObject(aIID, aInstancePtr);

    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kIWidgetIID)) {
        *aInstancePtr = (void*) ((nsIWidget*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
void nsWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      void *aInitData)
{
   /* if (NULL == mToolkit) {
        if (NULL != aToolkit) {
            mToolkit = (nsToolkit*)aToolkit;
            mToolkit->AddRef();
        }
        else {
            if (NULL != aParent) {
                mToolkit = (nsToolkit*)(aParent->GetToolkit()); // the call AddRef's, we don't have to
            }
            // it's some top level window with no toolkit passed in.
            // Create a default toolkit with the current thread
            else {
                mToolkit = new nsToolkit();
                mToolkit->AddRef();
                mToolkit->Init(PR_GetCurrentThread());
            }
        }

    }

    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    if (!mToolkit->IsGuiThread()) {
        DWORD args[5];
        args[0] = (DWORD)aParent;
        args[1] = (DWORD)&aRect;
        args[2] = (DWORD)aHandleEventFunction;
        args[3] = (DWORD)aContext;
        args[4] = (DWORD)aToolkit;
        args[5] = (DWORD)aInitData;
        MethodInfo info(this, nsWidget::CREATE, 6, args);
        mToolkit->CallMethod(&info);
        return;
    }

    // save the event callback function
    mEventCallback = aHandleEventFunction;

    // keep a reference to the toolkit object
    if (aContext) {
        mContext = aContext;
        mContext->AddRef();
    }
    else {
      nsresult  res;

      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
      static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

      res = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&mContext);

      if (NS_OK == res)
        mContext->Init();
    }

    if (nsnull != aInitData) {
      PreCreateWidget(aInitData);
    }

    mWnd = ::CreateWindowEx(WindowExStyle(),
                            WindowClass(),
                            "",
                            WindowStyle(),
                            aRect.x,
                            aRect.y,
                            aRect.width,
                            aRect.height,
                            (aParent) ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW):
                                        (HWND)NULL,
                            NULL,
                            nsToolkit::mDllInstance,
                            NULL);
    
    VERIFY(mWnd);
    if (aParent) {
        aParent->AddChild(this);
    }

    // Force cursor to default setting
    mCursor = eCursor_select;
    SetCursor(eCursor_standard);

    // call the event callback to notify about creation

    DispatchStandardEvent(NS_CREATE);
    SubclassWindow(TRUE);*/
}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
void nsWidget::Create(nsNativeWindow aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIToolkit *aToolkit,
                         void *aInitData)
{

/*    if (NULL == mToolkit) {
        if (NULL != aToolkit) {
            mToolkit = (nsToolkit*)aToolkit;
            mToolkit->AddRef();
        }
        else {
            mToolkit = new nsToolkit();
            mToolkit->AddRef();
            mToolkit->Init(PR_GetCurrentThread());
        }

    }

    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    if (!mToolkit->IsGuiThread()) {
        DWORD args[5];
        args[0] = (DWORD)aParent;
        args[1] = (DWORD)&aRect;
        args[2] = (DWORD)aHandleEventFunction;
        args[3] = (DWORD)aContext;
        args[4] = (DWORD)aToolkit;
        args[5] = (DWORD)aInitData;
        MethodInfo info(this, nsWidget::CREATE_NATIVE, 5, args);
        mToolkit->CallMethod(&info);
        return;
    }

    // save the event callback function
    mEventCallback = aHandleEventFunction;

    // keep a reference to the toolkit object
    if (aContext) {
        mContext = aContext;
        mContext->AddRef();
    }
    else {
      nsresult  res;

      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
      static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

      res = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&mContext);

      if (NS_OK == res)
        mContext->Init();
    }

    if (nsnull != aInitData) {
      PreCreateWidget(aInitData);
    }

    mWnd = ::CreateWindowEx(WindowExStyle(),
                            WindowClass(),
                            "",
                            WindowStyle(),
                            aRect.x,
                            aRect.y,
                            aRect.width,
                            aRect.height,
                            aParent,
                            NULL,
                            nsToolkit::mDllInstance,
                            NULL);
    
    VERIFY(mWnd);

    // call the event callback to notify about creation
    DispatchStandardEvent(NS_CREATE);
    SubclassWindow(TRUE);*/
}


//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------
void nsWidget::Destroy()
{
/*    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    if (mToolkit != nsnull && !mToolkit->IsGuiThread()) {
        MethodInfo info(this, nsWidget::DESTROY);
        mToolkit->CallMethod(&info);
        return;
    }

    //if we were in the middle deferred window positioning
    if (mDeferredPositioner) {
      VERIFY(::EndDeferWindowPos(mDeferredPositioner));
    }

    // destroy the nsWidget 
    if (mWnd) {
        // destroy the nsWidget
        mEventCallback = nsnull;  // prevent the widget from causing additional events
        VERIFY(::DestroyWindow(mWnd));
    }

    if (mPalette) {
        VERIFY(::DeleteObject(mPalette));
    }

      // Destroy the tooltip control
    if (mTooltip) {
      VERIFY(::DestroyWindow(mTooltip));
    }*/
}


//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWidget::GetParent(void)
{
    nsIWidget* widget = NULL;

    return widget;
}


//-------------------------------------------------------------------------
//
// Get this nsWidget's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsWidget::GetChildren()
{
    if (mChildren) {
        mChildren->Reset();
        //mChildren->AddRef();

        Enumerator * children = new Enumerator();
        nsISupports   * next = mChildren->Next();
        if (next) {
          nsIWidget *widget;
          if (NS_OK == next->QueryInterface(kIWidgetIID, (void**)&widget)) {
            children->Append(widget);
            //NS_RELEASE(widget);
          }
        }

        return (nsIEnumerator*)children;
    }

    return NULL;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsWidget::AddChild(nsIWidget* aChild)
{
    if (!mChildren) {
        mChildren = new Enumerator();
    }

    mChildren->Append(aChild);
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsWidget::RemoveChild(nsIWidget* aChild)
{
    if (mChildren) {
        mChildren->Remove(aChild);
    }
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
void nsWidget::Show(PRBool bState)
{
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
void nsWidget::Move(PRUint32 aX, PRUint32 aY)
{
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
void nsWidget::Resize(PRUint32 aWidth, PRUint32 aHeight)
{
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
void nsWidget::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight)
{
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
void nsWidget::Enable(PRBool bState)
{
}

    
//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
void nsWidget::SetFocus(void)
{
}

    
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
void nsWidget::GetBounds(nsRect &aRect)
{
}

    
//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsWidget::GetForegroundColor(void)
{
    return mForeground;
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
void nsWidget::SetForegroundColor(const nscolor &aColor)
{
    mForeground = aColor;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsWidget::GetBackgroundColor(void)
{
    return mBackground;
}

    
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
void nsWidget::SetBackgroundColor(const nscolor &aColor)
{
    mBackground = aColor;

}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWidget::GetFont(void)
{
    NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
    return NULL;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
void nsWidget::SetFont(const nsFont &aFont)
{
  /*  nsIFontCache* fontCache = mContext->GetFontCache();
    nsIFontMetrics* metrics = fontCache->GetMetricsFor(aFont);
    HFONT hfont = metrics->GetFontHandle();

      // Draw in the new font
    ::SendMessage(mWnd, WM_SETFONT, (WPARAM)hfont, (LPARAM)0); 
    NS_RELEASE(metrics);
    NS_RELEASE(fontCache);*/
}

    
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsWidget::GetCursor()
{
    return mCursor;
}

    
//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------


void nsWidget::SetCursor(nsCursor aCursor)
{
  // Only change cursor if it's changing
/*  if (aCursor != mCursor) {
    HCURSOR newCursor = NULL;

    switch(aCursor) {
    case eCursor_select:
      newCursor = ::LoadCursor(NULL, IDC_IBEAM);
      break;
      
    case eCursor_wait:
      newCursor = ::LoadCursor(NULL, IDC_WAIT);
      break;

    case eCursor_hyperlink: {
      HMODULE hm = ::GetModuleHandle(DLLNAME(NS_DLLNAME));
      newCursor = ::LoadCursor(hm, MAKEINTRESOURCE(IDC_SELECTANCHOR));
      break;
    }

    case eCursor_standard:
      newCursor = ::LoadCursor(NULL, IDC_ARROW);
      break;

    default:
      NS_ASSERTION(0, "Invalid cursor type");
      break;
    }

    if (NULL != newCursor) {
      mCursor = aCursor;
      HCURSOR oldCursor = ::SetCursor(newCursor);
    }
  }*/
}
    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
void nsWidget::Invalidate(PRBool aIsSynchronous)
{
}


//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWidget::GetNativeData(PRUint32 aDataType)
{
  /*  switch(aDataType) {
        case NS_NATIVE_WINDOW:
            return (void*)mWnd;
        case NS_NATIVE_GRAPHIC:
            return (void*)::GetDC(mWnd);
        case NS_NATIVE_COLORMAP:
        default:
            break;
    }*/

    return NULL;
}


//-------------------------------------------------------------------------
//
// Create a rendering context from this nsWidget
//
//-------------------------------------------------------------------------
nsIRenderingContext* nsWidget::GetRenderingContext()
{
nsRect  bounds;

    nsIRenderingContext *renderingCtx = NULL;
    /*if (mWnd) {
        HDC aDC = ::GetDC(mWnd);
        nsresult  res;

        static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
        static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

        res = NSRepository::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&renderingCtx);

        if (NS_OK == res)
          renderingCtx->Init(mContext, (nsDrawingSurface)aDC);

        NS_ASSERTION(NULL != renderingCtx, "Null rendering context");
    }*/

    return renderingCtx;
}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsWidget::GetToolkit()
{
    //if (NULL != mToolkit) {
    //    mToolkit->AddRef();
   // }

    return mToolkit;
}


//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
void nsWidget::SetColorMap(nsColorMap *aColorMap)
{
 /*   if (mPalette != NULL) {
        ::DeleteObject(mPalette);
    }

    PRUint8 *map = aColorMap->Index;
    LPLOGPALETTE pLogPal = (LPLOGPALETTE) new char[2 * sizeof(WORD) +
                                              aColorMap->NumColors * sizeof(PALETTEENTRY)];
	pLogPal->palVersion = 0x300;
	pLogPal->palNumEntries = aColorMap->NumColors;
	for(int i = 0; i < aColorMap->NumColors; i++) 
    {
		pLogPal->palPalEntry[i].peRed = *map++;
		pLogPal->palPalEntry[i].peGreen = *map++;
		pLogPal->palPalEntry[i].peBlue = *map++;
		pLogPal->palPalEntry[i].peFlags = 0;
	}
	mPalette = ::CreatePalette(pLogPal);
	delete pLogPal;

    NS_ASSERTION(mPalette != NULL, "Null palette");
    if (mPalette != NULL) {
        HDC hDC = ::GetDC(mWnd);
        HPALETTE hOldPalette = ::SelectPalette(hDC, mPalette, TRUE);
        ::RealizePalette(hDC);
        ::SelectPalette(hDC, hOldPalette, TRUE);
        ::ReleaseDC(mWnd, hDC);
    }*/
}

//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsWidget::GetDeviceContext() 
{ 
    if (mContext) {
        mContext->AddRef();
    }

    return mContext; 
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
void nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
 /* RECT  trect;

  if (nsnull != aClipRect)
  {
    trect.left = aClipRect->x;
    trect.top = aClipRect->y;
    trect.right = aClipRect->XMost();
    trect.bottom = aClipRect->YMost();
  }

  ::ScrollWindowEx(mWnd, aDx, aDy, (nsnull != aClipRect) ? &trect : NULL, NULL,
                   NULL, NULL, SW_INVALIDATE);
  ::UpdateWindow(mWnd);*/
}

//-------------------------------------------------------------------------
//
// Relay mouse events to the tooltip control
//
//-------------------------------------------------------------------------

void nsWidget::RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam)
{
  printf("relaying event\n");
#if 0
  MSG msg;
  msg.hwnd = mWnd;	 
  msg.message = aMsg; 
  msg.wParam = wParam;
  msg.lParam = lParam;
  msg.time = ::GetMessageTime();
  DWORD pos = ::GetMessagePos();
  POINT pt;
  msg.pt.x = LOWORD(pos);
  msg.pt.y = HIWORD(pos);  
  ::SendMessage(mTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPMSG) &msg);  
#endif
}





//-------------------------------------------------------------------------
//
// Constructor
//
//-------------------------------------------------------------------------
#define INITIAL_SIZE        2

nsWidget::Enumerator::Enumerator()
{
    mRefCnt = 1;
    mArraySize = INITIAL_SIZE;
    mChildrens = (nsIWidget**)new DWORD[mArraySize];
    memset(mChildrens, 0, sizeof(DWORD) * mArraySize);
    mCurrentPosition = 0;
}


//-------------------------------------------------------------------------
//
// Destructor
//
//-------------------------------------------------------------------------
nsWidget::Enumerator::~Enumerator()
{   
    if (mChildrens) {
        //for (int i = 0; mChildrens[i] && i < mArraySize; i++) {
        //    NS_RELEASE(mChildrens[i]);
        //}

        delete[] mChildrens;
    }
}

//
// nsISupports implementation macro
//
NS_IMPL_ISUPPORTS(nsWidget::Enumerator, NS_IENUMERATOR_IID);

//-------------------------------------------------------------------------
//
// Get enumeration next element. Return null at the end
//
//-------------------------------------------------------------------------
nsISupports* nsWidget::Enumerator::Next()
{
    if (mCurrentPosition < mArraySize && mChildrens[mCurrentPosition]) {
        mChildrens[mCurrentPosition]->AddRef();
        return mChildrens[mCurrentPosition++];
    }

    return NULL;
}


//-------------------------------------------------------------------------
//
// Reset enumerator internal pointer to the beginning
//
//-------------------------------------------------------------------------
void nsWidget::Enumerator::Reset()
{
    mCurrentPosition = 0;
}


//-------------------------------------------------------------------------
//
// Append an element 
//
//-------------------------------------------------------------------------
void nsWidget::Enumerator::Append(nsIWidget* aWidget)
{
    NS_PRECONDITION(aWidget, "Null widget");
    if (aWidget) {
        int pos;
        for (pos = 0; pos < mArraySize && mChildrens[pos]; pos++);
        if (pos == mArraySize) {
            GrowArray();
        }
        mChildrens[pos] = aWidget;
        aWidget->AddRef();
    }
}


//-------------------------------------------------------------------------
//
// Remove an element 
//
//-------------------------------------------------------------------------
void nsWidget::Enumerator::Remove(nsIWidget* aWidget)
{
    int pos;
    for(pos = 0; mChildrens[pos] && (mChildrens[pos] != aWidget); pos++);
    if (mChildrens[pos] == aWidget) {
        NS_RELEASE(aWidget);
        memcpy(mChildrens + pos, mChildrens + pos + 1, mArraySize - pos - 1);
    }

}


//-------------------------------------------------------------------------
//
// Grow the size of the children array
//
//-------------------------------------------------------------------------
void nsWidget::Enumerator::GrowArray()
{
    mArraySize <<= 1;
    nsIWidget **newArray = (nsIWidget**)new DWORD[mArraySize];
    memset(newArray, 0, sizeof(DWORD) * mArraySize);
    memcpy(newArray, mChildrens, (mArraySize>>1) * sizeof(DWORD));
    mChildrens = newArray;
}


DWORD nsWidget::GetBorderStyle(nsBorderStyle aBorderStyle)
{
  /*switch(aBorderStyle)
  {
    case eBorderStyle_none:
      return(0);
    break;

    case eBorderStyle_dialog:
     return(WS_DLGFRAME | DS_3DLOOK);
    break;

    default:
      NS_ASSERTION(0, "unknown border style");
      return(WS_OVERLAPPEDWINDOW);
  }*/
}


void nsWidget::SetBorderStyle(nsBorderStyle aBorderStyle) 
{
  mBorderStyle = aBorderStyle; 
} 

void nsWidget::SetTitle(const nsString& aTitle) 
{
  /*NS_ALLOC_STR_BUF(buf, aTitle, 256);
  ::SendMessage(mWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
  NS_FREE_STR_BUF(buf);*/
} 


/**
 * Processes a mouse pressed event
 *
 **/
//void nsWidget::AddMouseListener(nsIMouseListener * aListener)
//{
//  NS_PRECONDITION(mMouseListener == nsnull, "Null mouse listener");
//  mMouseListener = aListener;
//}

/**
 * Processes a mouse pressed event
 *
 **/
//void nsWidget::AddEventListener(nsIEventListener * aListener)
//{
//  NS_PRECONDITION(mEventListener == nsnull, "Null mouse listener");
//  mEventListener = aListener;
//}

//PRBool nsWidget::AutoErase()
//{
//  return(PR_FALSE);
//}



