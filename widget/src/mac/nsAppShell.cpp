/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIWidget.h"
#include "nsMacMessageSink.h"
#include "nsMacMessagePump.h"
#include "nsToolKit.h"
#include <Quickdraw.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#ifndef XP_MACOSX
#include <Traps.h>
#endif
#include <Events.h>
#include <Menus.h>

#include <stdlib.h>

#ifndef XP_MACOSX
#include "macstdlibextras.h"
#endif
PRBool nsAppShell::mInitializedToolbox = PR_FALSE;


//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAppShell, nsIAppShell)

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
	nsresult rv;
	rv = NS_GetCurrentToolkit(getter_AddRefs(mToolkit));
	if (NS_FAILED(rv))
		return rv;
	mMacSink.reset(new nsMacMessageSink());
	nsIToolkit* toolkit = mToolkit.get();
	mMacPump.reset(new nsMacMessagePump(static_cast<nsToolkit*>(toolkit), mMacSink.get()));
  mMacMemoryCushion.reset(new nsMacMemoryCushion());

  if (!mMacSink.get() || !mMacPump.get() || !mMacMemoryCushion.get())
    return NS_ERROR_OUT_OF_MEMORY;
  
  OSErr err = mMacMemoryCushion->Init(nsMacMemoryCushion::kMemoryBufferSize, nsMacMemoryCushion::kMemoryReserveSize);
  if (err != noErr)
    return NS_ERROR_FAILURE;
  
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Run(void)
{
	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

  mMacMemoryCushion->StartRepeating();
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
NS_IMETHODIMP nsAppShell::Exit(void)
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
// respond to notifications that an event queue has come or gone
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue * aQueue, PRBool aListen)
{ // unnecessary; handled elsewhere
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Prepare to process events
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spinup(void)
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
NS_IMETHODIMP nsAppShell::Spindown(void)
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

#if TARGET_CARBON
  mInitializedToolbox = PR_TRUE;
#else 
  // The toolbox initialization code has moved to NSStdLib (InitializeToolbox)
  
  if (!mInitializedToolbox)
  {
    InitializeMacToolbox();
    mInitializedToolbox = PR_TRUE;
  }
#endif
  mRefCnt = 0;
  mExitCalled = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
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

NS_METHOD
nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

	mMacPump->DispatchEvent(aRealEvent, (EventRecord *) aEvent);
	return NS_OK;
}
