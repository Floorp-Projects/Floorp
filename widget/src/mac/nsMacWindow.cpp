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

#include <LowMem.h>

#include "nsWindow.h"
#include "nsMacWindow.h"
#include "nsMacEventHandler.h"

// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif


const short kWindowTitleBarHeight = 22;
const short kWindowMarginWidth = 6;
const short kDialogTitleBarHeight = 26;
const short kDialogMarginWidth = 6;

NS_IMPL_ADDREF(nsMacWindow);
NS_IMPL_RELEASE(nsMacWindow);

//-------------------------------------------------------------------------
//
// nsMacWindow constructor
//
//-------------------------------------------------------------------------
nsMacWindow::nsMacWindow() : nsWindow()
	, mWindowMadeHere(PR_FALSE)
	, mIsDialog(PR_FALSE)
	,	mMacEventHandler(new nsMacEventHandler(this))
{
	NS_INIT_REFCNT();
	strcpy(gInstanceClassName, "nsMacWindow");
}


//-------------------------------------------------------------------------
//
// nsMacWindow destructor
//
//-------------------------------------------------------------------------
nsMacWindow::~nsMacWindow()
{
	if (mWindowPtr)
	{
		nsRefData* theRefData = (nsRefData*)::GetWRefCon(mWindowPtr);

		if (mWindowMadeHere)
			::CloseWindow(mWindowPtr);
		else
			::SetWRefCon(mWindowPtr, theRefData->GetUserData());	// restore the refCon if we did not create the window
		mWindowPtr = nsnull;

		delete theRefData;
	}
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsresult nsMacWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    static NS_DEFINE_IID(kIWindowIID, NS_IWINDOW_IID);	//еее
    if (aIID.Equals(kIWindowIID)) {
        *aInstancePtr = (void*) ((nsIWidget*)(nsISupports*)this);
        AddRef();
        return NS_OK;
    }

	return nsWindow::QueryInterface(aIID,aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsMacWindow::StandardCreate(nsIWidget *aParent,
	                      const nsRect &aRect,
	                      EVENT_CALLBACK aHandleEventFunction,
	                      nsIDeviceContext *aContext,
	                      nsIAppShell *aAppShell,
	                      nsIToolkit *aToolkit,
	                      nsWidgetInitData *aInitData,
	                      nsNativeWidget aNativeParent)
{
	// build the main native window
	if (aNativeParent == nsnull)
	{
			Rect			wRect;
			short			wDefProcID;
			Boolean		goAwayFlag;

		nsRectToMacRect(aRect, wRect);

		if (mIsDialog)
		{
			wDefProcID = movableDBoxProc;
			goAwayFlag = false;
			::OffsetRect(&wRect, kDialogMarginWidth, kDialogTitleBarHeight + ::LMGetMBarHeight());
		}
		else
		{
			wDefProcID = 0;
			goAwayFlag = true;
			::OffsetRect(&wRect, kWindowMarginWidth, kWindowTitleBarHeight + ::LMGetMBarHeight());
		}

		mWindowPtr = ::NewCWindow(nil, &wRect, "\p-", false, wDefProcID, (GrafPort*)-1, goAwayFlag, (long)nsnull);
		mWindowMadeHere = PR_TRUE;
	}
	else
	{
		mWindowPtr = (WindowPtr)aNativeParent;
		mWindowMadeHere = PR_FALSE;
	}

	// set the refData
	nsRefData* theRefData = new nsRefData();	
	if (theRefData == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
	theRefData->SetNSMacWindow(this);
	theRefData->SetUserData(::GetWRefCon(mWindowPtr));	// save the actual refCon in case we did not create the window
	::SetWRefCon(mWindowPtr, (long)theRefData);

	// reset the coordinates to (0,0) because it's the top level widget
	nsRect bounds(0, 0, aRect.width, aRect.height);

	// init base class
	nsWindow::StandardCreate(aParent, bounds, aHandleEventFunction, 
														aContext, aAppShell, aToolkit, aInitData);


	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create a nsMacWindow using a native window provided by the application
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Create(nsNativeWidget aNativeParent,		// this is a windowPtr
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
	return(StandardCreate(nsnull, aRect, aHandleEventFunction,
													aContext, aAppShell, aToolkit, aInitData,
														aNativeParent));
}

//-------------------------------------------------------------------------
//
// Hide or show this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Show(PRBool bState)
{
  nsWindow::Show(bState);
	
  // we need to make sure we call ::Show/HideWindow() to generate the 
  // necessary activate/deactivate events. Calling ::ShowHide() is
  // not adequate (pinkerton).
  if ( bState )
  {
    ::ShowWindow(mWindowPtr);
    ::SelectWindow(mWindowPtr);
  }
  else
    ::HideWindow(mWindowPtr);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Move(PRUint32 aX, PRUint32 aY)
{
	Rect screenRect = (**::GetGrayRgn()).rgnBBox;

	short windowWidth = mWindowPtr->portRect.right - mWindowPtr->portRect.left;
	if (((PRInt32)aX) < screenRect.left - windowWidth)
		aX = screenRect.left - windowWidth;
	else if (((PRInt32)aX) > screenRect.right)
		aX = screenRect.right;

	if (((PRInt32)aY) < screenRect.top)
		aY = screenRect.top;
	else if (((PRInt32)aY) > screenRect.bottom)
		aY = screenRect.bottom;

	// propagate the event in global coordinates
	nsWindow::Move(aX, aY);

	// reset the coordinates to (0,0) because it's the top level widget
	mBounds.x = 0;
	mBounds.y = 0;

	// move the window if it has not been moved yet
	// (ie. if this function isn't called in response to a DragWindow event)
	Point macPoint;
	macPoint = topLeft(mWindowPtr->portRect);
	::LocalToGlobal(&macPoint);
	if ((macPoint.h != aX) || (macPoint.v != aY)) {
		::MoveWindow(mWindowPtr, aX, aY, false);
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
	// move the window if it has not been moved yet
	// (ie. if this function isn't called in response to a GrowWindow event)
	Rect macRect = mWindowPtr->portRect;
	if (((macRect.right - macRect.left) != aWidth)
		|| ((macRect.bottom - macRect.top) != aHeight)) {
		::SizeWindow(mWindowPtr, aWidth, aHeight, aRepaint);
	}
	nsWindow::Resize(aWidth, aHeight, aRepaint);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::OnPaint(nsPaintEvent &event)
{
										// nothing to draw here
  return PR_FALSE;	// don't dispatch the update event
}

//-------------------------------------------------------------------------
//
// Set this window's title
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::SetTitle(const nsString& aTitle)
{
	Str255 title;
	title[0] = aTitle.Length();
	aTitle.ToCString((char*)title + 1, sizeof(title) - 1);
	::SetWTitle(mWindowPtr, title);
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Handle OS events
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::HandleOSEvent(
												EventRecord&		aOSEvent)
{
	PRBool retVal;
	if (mMacEventHandler.get())
		retVal = mMacEventHandler->HandleOSEvent(aOSEvent);
	else
		retVal = PR_FALSE;
	return retVal;
}

//-------------------------------------------------------------------------
//
// Handle Menu commands
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::HandleMenuCommand(
												EventRecord&		aOSEvent,
												long						aMenuResult)
{
	PRBool retVal;
	if (mMacEventHandler.get())
		retVal = mMacEventHandler->HandleMenuCommand(aOSEvent, aMenuResult);
	else
		retVal = PR_FALSE;
	return retVal;
}
