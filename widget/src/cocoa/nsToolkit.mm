/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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

#include "nsToolkit.h"
#include "nsWidgetAtoms.h"

#include <Gestalt.h>
#include <Appearance.h>

#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"

// for some reason, this must come last. otherwise the appshell 
// component fails to instantiate correctly at runtime.
#undef DARWIN
#import <Cocoa/Cocoa.h>

//
// interface EventQueueHanlder
//
// An object that handles processing events for the PLEvent queue
// on each thread.
//
@interface EventQueueHandler : NSObject
{
  nsIEventQueue*  mMainThreadEventQueue;    // addreffed
  NSTimer*        mEventTimer;              // our timer [STRONG]
}

- (void)eventTimer:(NSTimer *)theTimer;

@end


static EventQueueHandler*  gEventQueueHandler = nsnull;


@implementation EventQueueHandler

//
// -init
//
// Do init stuff. Cache the EventQueue Service and kick off our repeater
// to process PLEvents. The toolkit owns the timer so that neither Mozilla
// nor embedding apps need to worry about polling to process these events.
//
- (id)init
{
  if ( (self = [super init]) )
  {
    nsCOMPtr<nsIEventQueueService> service = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
    if (!service) {
      [self release];
      return nil;
    }
    
    service->GetThreadEventQueue(NS_CURRENT_THREAD, &mMainThreadEventQueue);   // addref

    mEventTimer = [NSTimer scheduledTimerWithTimeInterval:0.005
                              target:self
                              selector:@selector(eventTimer:)
                              userInfo:nil
                              repeats:YES];
    if (!mMainThreadEventQueue || !mEventTimer) {
      [self release];
      return nil;
    }
  }
  
  return self;
}


//
// -dealloc
//
// When we're finally ready to go away, kill our timer and get rid of the
// EventQueue Service. Also set the global var to null so in case we're called
// into action again we'll start over and not try to re-use the deleted object.
//
- (void) dealloc
{
  [mEventTimer release];
  NS_IF_RELEASE(mMainThreadEventQueue);
  
  gEventQueueHandler = nsnull;
  
  [super dealloc];
}


//
// -eventTimer
//
// Called periodically to process PLEvents from the queue on the current thread
//

#define MAX_PROCESS_EVENT_CALLS        20

//#define DEBUG_EVENT_TIMING

//#define TIMED_EVENT_PROCESSING
#define MAX_PLEVENT_TIME_MILLISECONDS  500


- (void)eventTimer:(NSTimer *)theTimer
{
#ifdef DEBUG_EVENT_TIMING
  AbsoluteTime startTime = ::UpTime();
#endif
  
  if (mMainThreadEventQueue)
  {
#ifdef TIMED_EVENT_PROCESSING
    // the new way; process events until some time has elapsed, or there are
    // no events left. UpTime() is a very low-overhead way to measure time.
    AbsoluteTime bailTime = ::AddDurationToAbsolute(MAX_PLEVENT_TIME_MILLISECONDS * durationMillisecond, ::UpTime());
    while (1)
    {
      PRBool pendingEvents = PR_FALSE;
      mMainThreadEventQueue->PendingEvents(&pendingEvents);
      if (!pendingEvents)
        break;
      mMainThreadEventQueue->ProcessPendingEvents();
      
      AbsoluteTime now = ::UpTime();
      if (UnsignedWideToUInt64(now) > UnsignedWideToUInt64(bailTime))
        break;
    }
#else
    // the old way; process events 20 times. Can suck CPU, and make the app
    // unresponsive
    for (PRInt32 i = 0; i < MAX_PROCESS_EVENT_CALLS; i ++)
    {
      PRBool pendingEvents = PR_FALSE;
      mMainThreadEventQueue->PendingEvents(&pendingEvents);
      if (!pendingEvents)
        break;
      mMainThreadEventQueue->ProcessPendingEvents();
    } 
#endif
  }

#ifdef DEBUG_EVENT_TIMING
  Nanoseconds duration = ::AbsoluteDeltaToNanoseconds(::UpTime(), startTime);
  UInt32 milliseconds = UnsignedWideToUInt64(duration) / 1000000;
    
  static UInt32 sMaxDuration = 0;
  
  if (milliseconds > sMaxDuration)
    sMaxDuration = milliseconds;
  
  printf("Event handling took %u ms (max %u)\n", milliseconds, sMaxDuration);
#endif
  
}


@end


#pragma mark -

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
  if (!gEventQueueHandler)
  {
    // autorelease this so that if Init is never called, it is not
    // leaked. Init retains it.
    gEventQueueHandler = [[[EventQueueHandler alloc] init] autorelease];
  }
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{ 
  /* Decrement our refcount on gEventQueueHandler; a prelude toward
     stopping event handling. This is not something you want to do unless you've
     bloody well started event handling and incremented the refcount. That's
     done in the Init method, not the constructor, and that's what mInited is about.
  */
  if (mInited && gEventQueueHandler)
    [gEventQueueHandler release];
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsresult
nsToolkit::InitEventQueue(PRThread * aThread)
{
  if (!gEventQueueHandler)
    return NS_ERROR_FAILURE;
  
  [gEventQueueHandler retain];

  return NS_OK;
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkitBase* NS_CreateToolkitInstance()
{
  return new nsToolkit();
}
