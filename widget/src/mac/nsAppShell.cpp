/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

// 
// nsAppShell
//
// This file contains the default implementation of the application shell. Clients
// may either use this implementation or write their own. If you write your
// own, you must create a message sink to route events to. (The message sink
// interface may change, so this comment must be updated accordingly.)
//

#include "nsAppShell.h"
#include "nsIAppShell.h"

#include "nsIWidget.h"
#include "nsMacMessageSink.h"
#include "nsMacMessagePump.h"
#include "nsSelectionMgr.h"
#include "nsToolKit.h"
#include <Quickdraw.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Traps.h>
#include <Events.h>
#include <Menus.h>

#include <stdlib.h>

#include "macstdlibextras.h"

PRBool nsAppShell::mInitializedToolbox = PR_FALSE;


//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

NS_IMETHODIMP nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Create(int* argc, char ** argv)
{
	mToolKit = auto_ptr<nsToolkit>( new nsToolkit() );

  // Create the selection manager
  if (!mSelectionMgr)
      NS_NewSelectionMgr(&mSelectionMgr);

  mMacSink = new nsMacMessageSink();
  mMacPump = auto_ptr<nsMacMessagePump>( new nsMacMessagePump(mToolKit.get(), mMacSink) );

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
nsresult nsAppShell::Run()
{
	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

	mMacPump->StartRunning();
	mMacPump->DoMessagePump();

  //if (mDispatchListener)
    //mDispatchListener->AfterDispatch();

	if (mExitCalled)	// hack: see below
	{
		mRefCnt --;
		if (mRefCnt == 0)
			delete this;
	}

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit appshell
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Exit()
{
	if (mMacPump.get())
	{
		Spindown();
		mExitCalled = PR_TRUE;
		mRefCnt ++;			// hack: since the applications are likely to delete us
										// after calling this method (see nsViewerApp::Exit()),
										// we temporarily bump the refCnt to let the message pump
										// exit properly. The object will delete itself afterwards.
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Prepare to process events
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spinup()
{
	if (mMacPump.get())
	{
		mMacPump->StartRunning();
		return NS_OK;
	}
	return NS_ERROR_NOT_INITIALIZED;
}

//-------------------------------------------------------------------------
//
// Stop being prepared to process events.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spindown()
{
	if (mMacPump.get())
		mMacPump->StopRunning();
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{ 
  // The toolbox initialization code has moved to NSStdLib (InitializeToolbox)
  
  if (!mInitializedToolbox)
  {
    InitializeMacToolbox();
    mInitializedToolbox = PR_TRUE;
  }
  mRefCnt = 0;
  mExitCalled = PR_FALSE;
  mSelectionMgr = 0;
  mMacSink = 0;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  NS_IF_RELEASE(mSelectionMgr);
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) 
  	{
    //return mTopLevel;
  	}
  return nsnull;
}

NS_METHOD
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
	static EventRecord	theEvent;	// icky icky static (can't really do any better)

	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

	aRealEvent = mMacPump->GetEvent(theEvent);
	aEvent = &theEvent;
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// determine whether the given event is suitable for a modal window
//
//-------------------------------------------------------------------------
NS_METHOD
nsAppShell::EventIsForModalWindow(PRBool aRealEvent, void *aEvent,
nsIWidget *aWidget,
                                  PRBool *aForWindow)
{
	*aForWindow = PR_FALSE;
	EventRecord *theEvent = (EventRecord *) aEvent;

	if ( aRealEvent == PR_TRUE && theEvent->what != nullEvent ) {

		WindowPtr window = (WindowPtr) aWidget->GetNativeData(NS_NATIVE_DISPLAY);
		WindowPtr eventWindow = nsnull;
		PRInt16 where = ::FindWindow ( theEvent->where, &eventWindow );

		switch ( theEvent->what ) {
			// is it a mouse event?
			case mouseDown:
			case mouseUp:
				// is it in the given window?
				// (note we also let some events questionable for modal dialogs pass through.
				// but it makes sense that the draggability et.al. of a modal window should
				// be controlled by whether the window has a drag bar).
				if ( window == eventWindow &&
				     ( where == inContent || where == inDrag || where == inGrow ||
				       where == inGoAway || where == inZoomIn || where == inZoomOut ))
					*aForWindow = PR_TRUE;
				break;
			case keyDown:
			case keyUp:
			case autoKey:
				if ( window == eventWindow )
					*aForWindow = PR_TRUE;
				break;

			case diskEvt:
			    // I think dialogs might want to support floppy insertion, and it doesn't
			    // interfere with modality...
			case updateEvt:
				// always let update events through, because if we don't handle them, we're
				// doomed!
			case activateEvt:
				// certainly we have to let the obvious activate events through. hopefully
				// our consumption of other events will keep any unwanted activate events
				// from even getting this far
				*aForWindow = PR_TRUE;
				break;

			case osEvt:
				// check for mouseMoved or suspend/resume events. We especially need to
				// let suspend/resume events through in order to make sure the clipboard is
				// converted correctly.
				unsigned char eventType = (theEvent->message >> 24) & 0x00ff;
				if (eventType == mouseMovedMessage) {
					// I'm guessing we don't want to let these through unless the mouse is
					// in the modal dialog so we don't see rollover feedback in windows behind
					// the dialog.
					if ( where == inContent && window == eventWindow )
						*aForWindow = PR_TRUE;
				}
				if ( eventType == suspendResumeMessage )
					*aForWindow = PR_TRUE;
				break;
		} // case of which event type
	} else
		*aForWindow = PR_TRUE;

	return NS_OK;
} // EventIsForModalWindow

NS_METHOD
nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

	mMacPump->DispatchEvent(aRealEvent, (EventRecord *) aEvent);
	return NS_OK;
}

NS_METHOD
nsAppShell::GetSelectionMgr(nsISelectionMgr** aSelectionMgr)
{
  *aSelectionMgr = mSelectionMgr;
  NS_IF_ADDREF(mSelectionMgr);
  if (!mSelectionMgr)
    return NS_ERROR_NOT_INITIALIZED;
  return NS_OK;
}
