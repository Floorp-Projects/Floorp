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

#include "nsWindow.h"
#include "nsIFontMetrics.h"
#include "nsIFontCache.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"

#include "nsXtEventHandler.h"

#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"

#include "stdio.h"

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

NS_IMPL_QUERY_INTERFACE(nsWindow, kIWidgetIID)
NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)

void nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
}

void nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
} 

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------
void nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWindow::RemoveTooltips()
{
}


//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow(nsISupports *aOuter)
{
  // XXX Til can deal with ColorMaps!
  SetForegroundColor(1);
  SetBackgroundColor(2);

  mToolkit = nsnull ;

}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
}



//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
void nsWindow::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{

  Widget mainWindow, frame;
  Widget parentWidget;

  if (nsnull == mToolkit) {
    if (nsnull != aToolkit) {
      mToolkit = (nsToolkit*)aToolkit;
      mToolkit->AddRef();
    }
    else {
      if (nsnull != aParent) {
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
  
  // save the event callback function
  mEventCallback = aHandleEventFunction;
  
  // keep a reference to the toolkit object
  if (aContext) {
    mContext = aContext;
    mContext->AddRef();
  }
  else {
    nsresult  res;
    
    // XXX Move this! - For some reason Registering in another DLL (shell) isn't working
        
#define GFXWIN_DLL "libgfxunix.so"

    static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
    
    NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    
    
    
    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);
    
    res = NSRepository::CreateInstance(kDeviceContextCID, 
				       nsnull, 
				       kDeviceContextIID, 
				       (void **)&mContext);
    
    if (NS_OK == res)
      mContext->Init();
  }
  
  if (aParent)
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  else
    parentWidget = (Widget) aInitData ;
  
  if (!aParent)
    mainWindow = ::XtVaCreateManagedWidget("mainWindow",
					   xmMainWindowWidgetClass,
					   parentWidget, 
  					   XmNwidth, aRect.width,
  					   XmNheight, aRect.height,
  					   nsnull);
  
  frame = ::XtVaCreateManagedWidget("frame",
				    xmDrawingAreaWidgetClass,
				    (aParent) ? parentWidget : mainWindow,
				    XmNwidth, aRect.width,
				    XmNheight, aRect.height,
				    nsnull);

  if (!aParent)
    XmMainWindowSetAreas (mainWindow, nsnull, nsnull, nsnull, nsnull, frame);

  mWidget = frame ;
    
  if (aParent) {
    aParent->AddChild(this);
  }

  // setup the event Handlers
  XtAddEventHandler(mWidget, 
		    ExposureMask, 
		    PR_FALSE, 
		    nsXtWidget_ExposureMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget, 
		    ButtonPressMask, 
		    PR_FALSE, 
		    nsXtWidget_ButtonPressMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget, 
		    ButtonReleaseMask, 
		    PR_FALSE, 
		    nsXtWidget_ButtonReleaseMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget, 
		    ButtonMotionMask, 
		    PR_FALSE, 
		    nsXtWidget_ButtonMotionMask_EventHandler,
		    this);


  // Force cursor to default setting
  mCursor = eCursor_select;
  SetCursor(eCursor_standard);

}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
void nsWindow::Create(nsNativeWindow aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIToolkit *aToolkit,
                         nsWidgetInitData *aInitData)
{
}


//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
void nsWindow::Destroy()
{
}


//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
  return nsnull;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsWindow::GetChildren()
{
    return nsnull;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsWindow::AddChild(nsIWidget* aChild)
{
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsWindow::RemoveChild(nsIWidget* aChild)
{
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
void nsWindow::Show(PRBool bState)
{
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
void nsWindow::Move(PRUint32 aX, PRUint32 aY)
{
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
void nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
void nsWindow::Resize(PRUint32 aX,
                      PRUint32 aY,
                      PRUint32 aWidth,
                      PRUint32 aHeight,
                      PRBool   aRepaint)
{
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
void nsWindow::Enable(PRBool bState)
{
}

    
//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
void nsWindow::SetFocus(void)
{
}

    
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
void nsWindow::GetBounds(nsRect &aRect)
{
  XWindowAttributes attrs ;

  Display * d = ::XtDisplay(mWidget);
  Window w = ::XtWindow(mWidget);

  XGetWindowAttributes(d, w, &attrs);

  aRect.x = attrs.x ;
  aRect.y = attrs.y ;
  aRect.width = attrs.width ;
  aRect.height = attrs.height;
  
}

    
//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsWindow::GetForegroundColor(void)
{
  return (mForeground);
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
void nsWindow::SetForegroundColor(const nscolor &aColor)
{
  mForeground = aColor;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsWindow::GetBackgroundColor(void)
{
  return (mBackground);
}

    
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
void nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  mBackground = aColor ;
}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
    NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
    return nsnull;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
void nsWindow::SetFont(const nsFont &aFont)
{
}

    
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsWindow::GetCursor()
{
}

    
//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

void nsWindow::SetCursor(nsCursor aCursor)
{
}
    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
void nsWindow::Invalidate(PRBool aIsSynchronous)
{
}


//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {

        case NS_NATIVE_WINDOW:
	  {
            return (void*)XtWindow(mWidget);
	  }
	break;
        case NS_NATIVE_WIDGET:
	  {
            return (void*)(mWidget);
	  }
	break;
        case NS_NATIVE_GRAPHIC:
	  {
	    XGCValues values;
	    GC aGC;
  
	    aGC= ::XtGetGC(mWidget, 
			   nsnull,
			   &values);

            return (void*)aGC;
	  }
	break;
        case NS_NATIVE_COLORMAP:
        default:
            break;
    }

    return NULL;

}


//-------------------------------------------------------------------------
//
// Create a rendering context from this nsWindow
//
//-------------------------------------------------------------------------
nsIRenderingContext* nsWindow::GetRenderingContext()
{
  return nsnull ;
}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsWindow::GetToolkit()
{
}


//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
void nsWindow::SetColorMap(nsColorMap *aColorMap)
{
}

//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsWindow::GetDeviceContext() 
{ 
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
void nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
}





void nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle) 
{
} 

void nsWindow::SetTitle(const nsString& aTitle) 
{
} 


/**
 * Processes a mouse pressed event
 *
 **/
void nsWindow::AddMouseListener(nsIMouseListener * aListener)
{
}

/**
 * Processes a mouse pressed event
 *
 **/
void nsWindow::AddEventListener(nsIEventListener * aListener)
{
}

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
    case nsEventStatus_eIgnore:
      return(PR_FALSE);
    break;
    case nsEventStatus_eConsumeNoDefault:
      return(PR_TRUE);
    break;
    case nsEventStatus_eConsumeDoDefault:
      return(PR_FALSE);
    break;
    default:
      NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
      return(PR_FALSE);
    break;
  }
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchEvent(nsGUIEvent* event)
{
  PRBool result = PR_FALSE;
 
  if (nsnull != mEventCallback) {
    result = ConvertStatus((*mEventCallback)(event));
  }

    // Dispatch to event listener if event was not consumed
  if ((result != PR_TRUE) && (nsnull != mEventListener)) {
    return ConvertStatus(mEventListener->ProcessEvent(*event));
  }
  else {
    return(result); 
  }
}

/**
 * Processes an Expose Event
 *
 **/
void nsWindow::OnPaint(nsPaintEvent &event)
{
  nsresult result ;

  // call the event callback 
  if (mEventCallback) {

    nsRect rr ;

    GetBounds(rr);
    
    event.rect = &rr;

    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    
    if (NS_OK == NSRepository::CreateInstance(kRenderingContextCID, 
					      nsnull, 
					      kRenderingContextIID, 
					      (void **)&event.renderingContext))
      {
	event.renderingContext->Init(mContext, this);
	result = DispatchEvent(&event);
	NS_RELEASE(event.renderingContext);
      }
    else
      result = PR_FALSE;
  }

}


void nsWindow::BeginResizingChildren(void)
{
}

void nsWindow::EndResizingChildren(void)
{
}



