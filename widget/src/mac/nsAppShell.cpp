/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Darin Fisher <darin@meer.net>
 *   Mark Mentovai <mark@moxienet.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Runs the main native Carbon run loop, interrupting it as needed to process
 * Gecko events.
 */

#include "nsAppShell.h"
#include "nsIToolkit.h"
#include "nsToolkit.h"
#include "nsMacMessagePump.h"

#include <Carbon/Carbon.h>

enum {
  kEventClassMoz = 'MOZZ',
  kEventMozNull  = 0,
};

// nsAppShell implementation

NS_IMETHODIMP
nsAppShell::ResumeNative(void)
{
  nsresult retval = nsBaseAppShell::ResumeNative();
  if (NS_SUCCEEDED(retval) && (mSuspendNativeCount == 0))
    ScheduleNativeEventCallback();
  return retval;
}

nsAppShell::nsAppShell()
: mCFRunLoop(NULL)
, mCFRunLoopSource(NULL)
, mRunningEventLoop(PR_FALSE)
{
}

nsAppShell::~nsAppShell()
{
  if (mCFRunLoopSource) {
    ::CFRunLoopRemoveSource(mCFRunLoop, mCFRunLoopSource,
                            kCFRunLoopCommonModes);
    ::CFRelease(mCFRunLoopSource);
  }

  if (mCFRunLoop)
    ::CFRelease(mCFRunLoop);
}

// Init
//
// Set up the transitional WaitNextEvent handler and the CFRunLoopSource
// used to interrupt the main Carbon event loop.
//
// public
nsresult
nsAppShell::Init()
{
  // The message pump is only used for its EventRecord dispatcher.  It is
  // used by the transitional WaitNextEvent handler.

  nsresult rv = NS_GetCurrentToolkit(getter_AddRefs(mToolkit));
  if (NS_FAILED(rv))
    return rv;

  nsIToolkit *toolkit = mToolkit.get();
  mMacPump = new nsMacMessagePump(NS_STATIC_CAST(nsToolkit*, toolkit));
  if (!mMacPump.get())
    return NS_ERROR_OUT_OF_MEMORY;

  // Add a CFRunLoopSource to the main native run loop.  The source is
  // responsible for interrupting the run loop when Gecko events are ready.

  // Silly Carbon, why do you require a cast here?
  mCFRunLoop = (CFRunLoopRef)::GetCFRunLoopFromEventLoop(::GetMainEventLoop());
  NS_ENSURE_STATE(mCFRunLoop);
  ::CFRetain(mCFRunLoop);

  CFRunLoopSourceContext context;
  bzero(&context, sizeof(context));
  // context.version = 0;
  context.info = this;
  context.perform = ProcessGeckoEvents;
  
  mCFRunLoopSource = ::CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
  NS_ENSURE_STATE(mCFRunLoopSource);

  ::CFRunLoopAddSource(mCFRunLoop, mCFRunLoopSource, kCFRunLoopCommonModes);

  return nsBaseAppShell::Init();
}

// ScheduleNativeEventCallback
//
// Called (possibly on a non-main thread) when Gecko has an event that
// needs to be processed.  The Gecko event needs to be processed on the
// main thread, so the native run loop must be interrupted.
//
// protected virtual
void
nsAppShell::ScheduleNativeEventCallback()
{
  // This will invoke ProcessGeckoEvents on the main thread.

  NS_ADDREF_THIS();
  ::CFRunLoopSourceSignal(mCFRunLoopSource);
  ::CFRunLoopWakeUp(mCFRunLoop);
}

// ProcessNextNativeEvent
//
// If aMayWait is false, process a single native event.  If it is true, run
// the native run loop until stopped by ProcessGeckoEvents.
//
// Returns true if more events are waiting in the native event queue.
//
// protected virtual
PRBool
nsAppShell::ProcessNextNativeEvent(PRBool aMayWait)
{
  PRBool eventProcessed = PR_FALSE;
  PRBool wasRunningEventLoop = mRunningEventLoop;

  mRunningEventLoop = aMayWait;
  EventTimeout waitUntil = kEventDurationNoWait;
  if (aMayWait)
    waitUntil = kEventDurationForever;

  do {
    EventRef carbonEvent;
    OSStatus err = ::ReceiveNextEvent(0, nsnull, waitUntil, PR_TRUE,
                                      &carbonEvent);
    if (err == noErr) {
      ::SendEventToEventTarget(carbonEvent, ::GetEventDispatcherTarget());
      ::ReleaseEvent(carbonEvent);
      eventProcessed = PR_TRUE;
    }
  } while (mRunningEventLoop);

  mRunningEventLoop = wasRunningEventLoop;

  return eventProcessed;
}

// ProcessGeckoEvents
//
// The "perform" target of mCFRunLoop, called when mCFRunLoopSource is
// signalled from ScheduleNativeEventCallback.
//
// Arrange for Gecko events to be processed.  They will either be processed
// after the main run loop returns (if we own the run loop) or on
// NativeEventCallback (if an embedder owns the loop).
//
// protected static
void
nsAppShell::ProcessGeckoEvents(void* aInfo)
{
  nsAppShell* self = NS_STATIC_CAST(nsAppShell*, aInfo);

  if (self->mRunningEventLoop) {
    self->mRunningEventLoop = PR_FALSE;

    // The run loop is sleeping.  ::ReceiveNextEvent() won't return until
    // it's given a reason to wake up.  Awaken it by posting a bogus event.
    EventRef bogusEvent;
    OSStatus err = ::CreateEvent(nsnull, kEventClassMoz, kEventMozNull, 0,
                                 kEventAttributeNone, &bogusEvent);
    if (err == noErr) {
      ::PostEventToQueue(::GetMainEventQueue(), bogusEvent,
                         kEventPriorityStandard);
      ::ReleaseEvent(bogusEvent);
    }
  }

  if (self->mSuspendNativeCount <= 0)
    self->NativeEventCallback();

  NS_RELEASE(self);
}
