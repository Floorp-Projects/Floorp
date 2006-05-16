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

// kWNETransitionEventList
//
// This list encompasses all Carbon events that can be converted into
// EventRecords.  Not all will necessarily be called; not all will necessarily
// be handled.  Some items here may be redundant in that handlers are already
// installed elsewhere.  This may need a good audit.
//
static const EventTypeSpec kWNETransitionEventList[] = {
  { kEventClassMouse,       kEventMouseDown },
  { kEventClassMouse,       kEventMouseUp },
  { kEventClassMouse,       kEventMouseMoved },
  { kEventClassMouse,       kEventMouseDragged },
  { kEventClassKeyboard,    kEventRawKeyDown },
  { kEventClassKeyboard,    kEventRawKeyUp },
  { kEventClassKeyboard,    kEventRawKeyRepeat },
  { kEventClassWindow,      kEventWindowUpdate },
  { kEventClassWindow,      kEventWindowActivated },
  { kEventClassWindow,      kEventWindowDeactivated },
  { kEventClassWindow,      kEventWindowCursorChange },
  { kEventClassApplication, kEventAppActivated },
  { kEventClassApplication, kEventAppDeactivated },
  { kEventClassAppleEvent,  kEventAppleEvent },
  { kEventClassControl,     kEventControlTrack },
};

// nsAppShell implementation

nsAppShell::nsAppShell()
: mWNETransitionEventHandler(NULL)
, mCFRunLoop(NULL)
, mCFRunLoopSource(NULL)
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

  if (mWNETransitionEventHandler)
    ::RemoveEventHandler(mWNETransitionEventHandler);
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
  mMacPump = new nsMacMessagePump(static_cast<nsToolkit*>(toolkit));
  if (!mMacPump.get() || !nsMacMemoryCushion::EnsureMemoryCushion())
    return NS_ERROR_OUT_OF_MEMORY;

  OSStatus err = ::InstallApplicationEventHandler(
                               ::NewEventHandlerUPP(WNETransitionEventHandler),
                               GetEventTypeCount(kWNETransitionEventList),
                               kWNETransitionEventList,
                               (void*)this,
                               &mWNETransitionEventHandler);
  NS_ENSURE_TRUE(err == noErr, NS_ERROR_UNEXPECTED);

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

  if (!aMayWait) {
    // Only process a single event.
#if 0
    EventQueueRef carbonEventQueue = ::GetCurrentEventQueue();

    // This requires Mac OS X 10.3 or later... we cannot use it until the
    // builds systems are upgraded --darin
    if (EventRef carbonEvent =
         ::AcquireFirstMatchingEventInQueue(carbonEventQueue,
                                            0,
                                            NULL,
                                            kEventQueueOptionsNone)) {
      ::SendEventToEventTarget(carbonEvent, ::GetEventDispatcherTarget());
      ::RemoveEventFromQueue(carbonEventQueue, carbonEvent);
      ::ReleaseEvent(carbonEvent);
    }
#endif
    EventRef carbonEvent;
    OSStatus err = ::ReceiveNextEvent(0, nsnull, kEventDurationNoWait, PR_TRUE,
                                      &carbonEvent);
    if (err == noErr && carbonEvent) {
      ::SendEventToEventTarget(carbonEvent, ::GetEventDispatcherTarget());
      ::ReleaseEvent(carbonEvent);
      eventProcessed = PR_TRUE;
    }
  }
  else {
    // XXX(darin): It seems to me that we should be using ReceiveNextEvent here
    // as well to ensure that we only wait for and process a single event.

    // Run the loop until interrupted by ::QuitApplicationEventLoop().
    ::RunApplicationEventLoop();

    eventProcessed = PR_TRUE;
  }

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

  if (self->RunWasCalled()) {
    // We own the run loop.  Interrupt it.  It will be started again later
    // (unless exiting) by nsBaseAppShell.  Trust me, I'm a doctor.
    ::QuitApplicationEventLoop();
  }

  self->NativeEventCallback();

  NS_RELEASE(self);
}

// WNETransitionEventHandler
//
// Transitional WaitNextEvent handler.  Accepts Carbon events from
// kWNETransitionEventList, converts them into EventRecords, and
// dispatches them through the path they would have gone if they
// had been received as EventRecords from WaitNextEvent.
//
// protected static
pascal OSStatus
nsAppShell::WNETransitionEventHandler(EventHandlerCallRef aHandlerCallRef,
                                      EventRef            aEvent,
                                      void*               aUserData)
{
  nsAppShell* self = NS_STATIC_CAST(nsAppShell*, aUserData);

  EventRecord eventRecord;
  ::ConvertEventRefToEventRecord(aEvent, &eventRecord);

  PRBool handled = self->mMacPump->DispatchEvent(PR_TRUE, &eventRecord);

  if (handled)
    return noErr;

  return eventNotHandledErr;
}
