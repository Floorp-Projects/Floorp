/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsGUIEvent.h"

#include <Gestalt.h>
#include <Appearance.h>

#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIImageManager.h"
#include "nsGfxCIID.h"

//#define MAC_PL_EVENT_TWEAKING

// Class IDs...
static NS_DEFINE_CID(kEventQueueCID,  NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,  NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_IID(kImageManagerCID, NS_IMAGEMANAGER_CID);

static nsMacNSPREventQueueHandler*	gEventQueueHandler = nsnull;

//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsMacNSPREventQueueHandler::nsMacNSPREventQueueHandler(): Repeater()
{
	mRefCnt = 0;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsMacNSPREventQueueHandler::~nsMacNSPREventQueueHandler()
{
	StopRepeating();
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacNSPREventQueueHandler::StartPumping()
{
	++mRefCnt;
	NS_LOG_ADDREF(this, mRefCnt, "nsMacNSPREventQueueHandler", sizeof(*this));
	StartRepeating();
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
PRBool nsMacNSPREventQueueHandler::StopPumping()
{
	if (mRefCnt > 0) {
		--mRefCnt;
		NS_LOG_RELEASE(this, mRefCnt, "nsMacNSPREventQueueHandler");
		if (mRefCnt == 0) {
			StopRepeating();
			return PR_TRUE;
		}
	}

	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacNSPREventQueueHandler::RepeatAction(const EventRecord& inMacEvent)
{
	ProcessPLEventQueue();
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacNSPREventQueueHandler::ProcessPLEventQueue()
{
	nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID);
	if (eventQService)
	{
		nsCOMPtr<nsIEventQueue> queue;
		eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
	
#ifdef MAC_PL_EVENT_TWEAKING
		// just handle one event at a time
		if (queue)
		{
			PRBool plEventAvail = PR_FALSE;
			queue->EventAvailable(plEventAvail);
			if (plEventAvail)
			{
				PLEvent* thisEvent;
				if (NS_SUCCEEDED(queue->GetEvent(&thisEvent)))
				{
					queue->HandleEvent(thisEvent);
				}
			}
		}

#else
		// the old way
		if (queue)
			queue->ProcessPendingEvents();
#endif	
	}

}


#pragma mark -

NS_IMPL_THREADSAFE_ISUPPORTS1(nsToolkit, nsIToolkit);


// assume we begin as the fg app
bool nsToolkit::sInForeground = true;


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit() : mInited(false)
{
	NS_INIT_REFCNT();
	if (gEventQueueHandler == nsnull)
		gEventQueueHandler = new nsMacNSPREventQueueHandler;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
	/* StopPumping decrements a refcount on gEventQueueHandler; a prelude toward
	   stopping event handling. This is not something you want to do unless you've
	   bloody well started event handling and incremented the refcount. That's
	   done in the Init method, not the constructor, and that's what mInited is about.
	*/
	if (mInited && gEventQueueHandler) {
		if (gEventQueueHandler->StopPumping()) {
			delete gEventQueueHandler;
			gEventQueueHandler = nsnull;
		}
	}
    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsToolkit::Init(PRThread */*aThread*/)
{
	if (gEventQueueHandler)
		gEventQueueHandler->StartPumping();
	mInited = true;
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
bool nsToolkit::HasAppearanceManager()
{

#define APPEARANCE_MIN_VERSION	0x0110		// we require version 1.1
	
	static bool inited = false;
	static bool hasAppearanceManager = false;

	if (inited)
		return hasAppearanceManager;
	inited = true;

	SInt32 result;
	if (::Gestalt(gestaltAppearanceAttr, &result) != noErr)
		return false;		// no Appearance Mgr

	if (::Gestalt(gestaltAppearanceVersion, &result) != noErr)
		return false;		// still version 1.0

	hasAppearanceManager = (result >= APPEARANCE_MIN_VERSION);

	return hasAppearanceManager;
}


void 
nsToolkit :: AppInForeground ( )
{
  sInForeground = true;
}


void 
nsToolkit :: AppInBackground ( )
{
  sInForeground = false;
} 


bool
nsToolkit :: IsAppInForeground ( )
{
  return sInForeground;
}


//-------------------------------------------------------------------------
//
// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  nsIToolkit* toolkit = nsnull;
  nsresult rv = NS_OK;
  PRStatus status;

  // Create the TLS index the first time through...
  if (0 == gToolkitTLSIndex) {
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
    if (PR_FAILURE == status) {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);

    //
    // Create a new toolkit for this thread...
    //
    if (!toolkit) {
      toolkit = new nsToolkit();

      if (!toolkit) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        NS_ADDREF(toolkit);
        toolkit->Init(PR_GetCurrentThread());
        //
        // The reference stored in the TLS is weak.  It is removed in the
        // nsToolkit destructor...
        //
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
      }
    } else {
      NS_ADDREF(toolkit);
    }
    *aResult = toolkit;
  }

  return rv;
}

#pragma mark -

Handle nsMacMemoryCushion::sMemoryReserve;

nsMacMemoryCushion::nsMacMemoryCushion()
: mBufferHandle(nsnull)
{
}

nsMacMemoryCushion::~nsMacMemoryCushion()
{
  ::SetGrowZone(nsnull);
  if (sMemoryReserve)
    ::DisposeHandle(sMemoryReserve);

  if (mBufferHandle)
    ::DisposeHandle(mBufferHandle);
}


OSErr nsMacMemoryCushion::Init(Size bufferSize, Size reserveSize)
{
  sMemoryReserve = ::NewHandle(reserveSize);
  if (sMemoryReserve == nsnull)
    return ::MemError();
  
  mBufferHandle = ::NewHandle(bufferSize);
  if (mBufferHandle == nsnull)
    return ::MemError();

  // make this purgable
  ::HPurge(mBufferHandle);

	::SetGrowZone(NewGrowZoneProc(GrowZoneProc));
	return noErr;
}


void nsMacMemoryCushion::RepeatAction(const EventRecord &aMacEvent)
{
  if (!RecoverMemoryBuffer(kMemoryBufferSize))
  {
    // NS_ASSERTION(0, "Failed to recallocate memory buffer. Flushing caches");
    // until imglib implements nsIMemoryPressureObserver (bug 46337)
    // manually flush the imglib cache here
    nsCOMPtr<nsIImageManager> imageManager = do_GetService(kImageManagerCID);
    if (imageManager)
    {
      imageManager->FlushCache(1);    // flush everything
    }
  }

  if (!RecoverMemoryReserve(kMemoryReserveSize))
  {
    // NS_ASSERTION(0, "Failed to recallocate memory reserve. Flushing caches");
    nsMemory::HeapMinimize(PR_TRUE);
  }
}


Boolean nsMacMemoryCushion::RecoverMemoryReserve(Size reserveSize)
{
  if (!sMemoryReserve) return true;     // not initted yet
  if (*sMemoryReserve != nsnull) return true;   // everything is OK
  
  ::ReallocateHandle(sMemoryReserve, reserveSize);
  if (::MemError() != noErr || !*sMemoryReserve) return false;
  return true;
}

Boolean nsMacMemoryCushion::RecoverMemoryBuffer(Size bufferSize)
{
  if (!mBufferHandle) return true;     // not initted yet
  if (*mBufferHandle != nsnull) return true;   // everything is OK
    
  ::ReallocateHandle(mBufferHandle, bufferSize);
  if (::MemError() != noErr || !*mBufferHandle) return false;

  // make this purgable
  ::HPurge(mBufferHandle);
  return true;
}

pascal long nsMacMemoryCushion::GrowZoneProc(Size amountNeeded)
{
  long    freedMem = 0;
  
  if (sMemoryReserve && *sMemoryReserve && sMemoryReserve != ::GZSaveHnd())
  {
    freedMem = ::GetHandleSize(sMemoryReserve);
    ::EmptyHandle(sMemoryReserve);
  }
  
  return freedMem;
}

