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
 * The Original Code is a Cocoa widget run loop and event implementation.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com> (Original Author)
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
 * Runs the main native Cocoa run loop, interrupting it as needed to process
 * Gecko events.
 */

#import <Cocoa/Cocoa.h>

#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"

// AppShellDelegate
//
// Cocoa bridge class.  An object of this class is used as an NSPort
// delegate called on the main thread when Gecko wants to interrupt
// the native run loop.
//
@interface AppShellDelegate : NSObject
{
  @private
    nsAppShell* mAppShell;
    nsresult    mRunRV;
}

- (id)initWithAppShell:(nsAppShell*)aAppShell;
- (void)handlePortMessage:(NSPortMessage*)aPortMessage;
- (void)runAppShell;
- (nsresult)rvFromRun;
- (void)applicationWillTerminate:(NSNotification*)aNotification;
@end

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
: mAutoreleasePools(nsnull)
, mPort(nil)
, mDelegate(nil)
, mRunningEventLoop(PR_FALSE)
, mTerminated(PR_FALSE)
{
  // mMainPool sits low on the autorelease pool stack to serve as a catch-all
  // for autoreleased objects on this thread.  Because it won't be popped
  // until the appshell is destroyed, objects attached to this pool will
  // be leaked until app shutdown.  You probably don't want this!
  //
  // Objects autoreleased to this pool may result in warnings in the future.
  mMainPool = [[NSAutoreleasePool alloc] init];
}

nsAppShell::~nsAppShell()
{
  if (mAutoreleasePools) {
    NS_ASSERTION(::CFArrayGetCount(mAutoreleasePools) == 0,
                 "nsAppShell destroyed without popping all autorelease pools");
    ::CFRelease(mAutoreleasePools);
  }

  if (mPort) {
    [[NSRunLoop currentRunLoop] removePort:mPort forMode:NSDefaultRunLoopMode];
    [mPort release];
  }

  [mDelegate release];
  [mMainPool release];
}

// Init
//
// Loads the nib (see bug 316076c21) and sets up the NSPort used to
// interrupt the main Cocoa event loop.
//
// public
nsresult
nsAppShell::Init()
{
  // No event loop is running yet.  Avoid autoreleasing objects to
  // mMainPool.  The appshell retains objects it needs to be long-lived
  // and will release them as appropriate.
  NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

  // mAutoreleasePools is used as a stack of NSAutoreleasePool objects created
  // by |this|.  CFArray is used instead of NSArray because NSArray wants to
  // retain each object you add to it, and you can't retain an
  // NSAutoreleasePool.
  mAutoreleasePools = ::CFArrayCreateMutable(nsnull, 0, nsnull);
  NS_ENSURE_STATE(mAutoreleasePools);

  // Get the path of the nib file, which lives in the GRE location
  nsCOMPtr<nsIFile> nibFile;
  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(nibFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nibFile->AppendNative(NS_LITERAL_CSTRING("res"));
  nibFile->AppendNative(NS_LITERAL_CSTRING("MainMenu.nib"));

  nsCAutoString nibPath;
  rv = nibFile->GetNativePath(nibPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // This call initializes NSApplication.
  [NSBundle loadNibFile:
                     [NSString stringWithUTF8String:(const char*)nibPath.get()]
      externalNameTable:
           [NSDictionary dictionaryWithObject:[NSApplication sharedApplication]
                                       forKey:@"NSOwner"]
               withZone:NSDefaultMallocZone()];

  // A message will be sent through mPort to mDelegate on the main thread
  // to interrupt the run loop while it is running.
  mDelegate = [[AppShellDelegate alloc] initWithAppShell:this];
  NS_ENSURE_STATE(mDelegate);

  mPort = [[NSPort port] retain];
  NS_ENSURE_STATE(mPort);

  [mPort setDelegate:mDelegate];
  [[NSRunLoop currentRunLoop] addPort:mPort forMode:NSDefaultRunLoopMode];

  rv = nsBaseAppShell::Init();

  [localPool release];

  return rv;
}

// ProcessGeckoEvents
//
// Arrange for Gecko events to be processed.  They will either be processed
// after the main run loop returns (if we own the run loop) or on
// NativeEventCallback (if an embedder owns the loop).
//
// Called by -[AppShellDelegate handlePortMessage:] after mPort signals as a
// result of a ScheduleNativeEventCallback call.  This method is public only
// because it needs to be called by that Objective-C fragment, and C++ can't
// make |friend|s with Objective-C.
//
// public
void
nsAppShell::ProcessGeckoEvents()
{
  if (mRunningEventLoop) {
    mRunningEventLoop = PR_FALSE;

    // The run loop is sleeping.  [NSApp nextEventMatchingMask:...] won't
    // return until it's given a reason to wake up.  Awaken it by posting
    // a bogus event.  There's no need to make the event presentable.
    [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0,0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:-1
                                         context:NULL
                                         subtype:0
                                           data1:0
                                           data2:0]
             atStart:NO];
  }

  if (mSuspendNativeCount <= 0)
    NativeEventCallback();

  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSMakePoint(0,0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:-1
                                       context:NULL
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:NO];
}

// WillTerminate
//
// Called by the AppShellDelegate when an NSApplicationWillTerminate
// notification is posted.  After this method is called, native events should
// no longer be processed.
//
// public
void
nsAppShell::WillTerminate()
{
  mTerminated = PR_TRUE;
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
  NS_ADDREF(this);

  void* self = NS_STATIC_CAST(void*, this);
  NSData* data = [[NSData alloc] initWithBytes:&self length:sizeof(this)];
  NSArray* components = [[NSArray alloc] initWithObjects:&data count:1];

  // This will invoke [mDelegate handlePortMessage:message] on the main thread.

  NSPortMessage* message = [[NSPortMessage alloc] initWithSendPort:mPort
                                                       receivePort:nil
                                                        components:components];
  [message sendBeforeDate:[NSDate distantFuture]];

  [message release];
  [components release];
  [data release];
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

  if (mTerminated)
    return eventProcessed;

  PRBool wasRunningEventLoop = mRunningEventLoop;
  mRunningEventLoop = aMayWait;
  NSDate* waitUntil = nil;
  if (aMayWait)
    waitUntil = [NSDate distantFuture];

  do {
    // No autorelease pool is provided here, because OnProcessNextEvent
    // and AfterProcessNextEvent are responsible for maintaining it.
    NS_ASSERTION(mAutoreleasePools && ::CFArrayGetCount(mAutoreleasePools),
                 "No autorelease pool for native event");

    if (NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                            untilDate:waitUntil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES]) {
      [NSApp sendEvent:event];

      // Additional processing that [NSApp run] does after each event.
      NSEventType type = [event type];
      if (type != NSPeriodic && type != NSMouseMoved) {
        [[NSApp servicesMenu] update];
        [[NSApp windowsMenu] update];
        [[NSApp mainMenu] update];
      }

      [NSApp updateWindows];

      eventProcessed = PR_TRUE;
    }
  } while (mRunningEventLoop);

  mRunningEventLoop = wasRunningEventLoop;

  return eventProcessed;
}

// Run
//
// Overrides the base class' Run method to ensure that [NSApp run] has been
// called.  When [NSApp run] has not yet been called, this method calls it
// after arranging for a selector to be called from the run loop.  That
// selector is responsible for calling Run again.  At that point, because
// [NSApp run] has been called, the base class' method is called.
//
// The runAppShell selector will call [NSApp stop:] as soon as the real
// Run method finishes.  The real Run method's return value is saved so
// that it may properly be returned.
//
// public
NS_IMETHODIMP
nsAppShell::Run(void)
{
  if (![NSApp isRunning]) {
    [mDelegate performSelector:@selector(runAppShell)
                    withObject:nil
                    afterDelay:0];
    [NSApp run];
    return [mDelegate rvFromRun];
  }

  return nsBaseAppShell::Run();
}

// OnProcessNextEvent
//
// This nsIThreadObserver method is called prior to processing an event.
// Set up an autorelease pool that will service any autoreleased Cocoa
// objects during this event.  This includes native events processed by
// ProcessNextNativeEvent.  The autorelease pool will be popped by
// AfterProcessNextEvent, it is important for these two methods to be
// tightly coupled.
//
// public
NS_IMETHODIMP
nsAppShell::OnProcessNextEvent(nsIThreadInternal *aThread, PRBool aMayWait,
                               PRUint32 aRecursionDepth)
{
  NS_ASSERTION(mAutoreleasePools,
               "No stack on which to store autorelease pool");

  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  ::CFArrayAppendValue(mAutoreleasePools, pool);

  return nsBaseAppShell::OnProcessNextEvent(aThread, aMayWait, aRecursionDepth);
}

// AfterProcessNextEvent
//
// This nsIThreadObserver method is called after event processing is complete.
// The Cocoa implementation cleans up the autorelease pool create by the
// previous OnProcessNextEvent call.
//
// public
NS_IMETHODIMP
nsAppShell::AfterProcessNextEvent(nsIThreadInternal *aThread,
                                  PRUint32 aRecursionDepth)
{
  CFIndex count = ::CFArrayGetCount(mAutoreleasePools);

  NS_ASSERTION(mAutoreleasePools && count,
               "Processed an event, but there's no autorelease pool?");

  NSAutoreleasePool* pool = NS_STATIC_CAST(const NSAutoreleasePool*,
                               ::CFArrayGetValueAtIndex(mAutoreleasePools,
                                                        count - 1));
  ::CFArrayRemoveValueAtIndex(mAutoreleasePools, count - 1);
  [pool release];

  return nsBaseAppShell::AfterProcessNextEvent(aThread, aRecursionDepth);
}

// AppShellDelegate implementation

@implementation AppShellDelegate
// initWithAppShell:
//
// Constructs the AppShellDelegate object
- (id)initWithAppShell:(nsAppShell*)aAppShell
{
  if ((self = [self init])) {
    mAppShell = aAppShell;
    mRunRV = NS_ERROR_NOT_INITIALIZED;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];
  }

  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// handlePortMessage:
//
// The selector called on the delegate object when nsAppShell::mPort is sent an
// NSPortMessage by ScheduleNativeEventCallback.  Call into the nsAppShell
// object for access to mRunningEventLoop and NativeEventCallback.
//
- (void)handlePortMessage:(NSPortMessage*)aPortMessage
{
  NSData* data = [[aPortMessage components] objectAtIndex:0];
  nsAppShell* appShell = *NS_STATIC_CAST(nsAppShell* const*,[data bytes]);
  appShell->ProcessGeckoEvents();

  NS_RELEASE(appShell);
}

// runAppShell
//
// Runs the nsAppShell, and immediately stops the Cocoa run loop when
// nsAppShell::Run is done, saving its return value.
- (void)runAppShell
{
  mRunRV = mAppShell->Run();
  [NSApp stop:self];
  return;
}

// rvFromRun
//
// Returns the nsresult return value saved by runAppShell.
- (nsresult)rvFromRun
{
  return mRunRV;
}

// applicationWillTerminate:
//
// Notify the nsAppShell that native event processing should be discontinued.
- (void)applicationWillTerminate:(NSNotification*)aNotification
{
  mAppShell->WillTerminate();
}
@end
