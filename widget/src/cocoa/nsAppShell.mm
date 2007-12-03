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
#include "nsIRollupListener.h"
#include "nsIWidget.h"
#include "nsThreadUtils.h"
#include "nsIWindowMediator.h"
#include "nsServiceManagerUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebBrowserChrome.h"

// defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

// defined in nsCocoaWindow.mm
extern PRInt32             gXULModalLevel;

@interface NSApplication (Undocumented)

// Present in all versions of OS X from (at least) 10.2.8 through 10.5.
- (BOOL)_isRunningModal;

@end

// AppShellDelegate
//
// Cocoa bridge class.  An object of this class is registered to receive
// notifications.
//
@interface AppShellDelegate : NSObject
{
  @private
    nsAppShell* mAppShell;
}

- (id)initWithAppShell:(nsAppShell*)aAppShell;
- (void)applicationWillTerminate:(NSNotification*)aNotification;
- (void)beginMenuTracking:(NSNotification*)aNotification;
@end

// nsAppShell implementation

NS_IMETHODIMP
nsAppShell::ResumeNative(void)
{
  nsresult retval = nsBaseAppShell::ResumeNative();
  if (NS_SUCCEEDED(retval) && (mSuspendNativeCount == 0) &&
      mSkippedNativeCallback)
  {
    mSkippedNativeCallback = PR_FALSE;
    ScheduleNativeEventCallback();
  }
  return retval;
}

nsAppShell::nsAppShell()
: mAutoreleasePools(nsnull)
, mDelegate(nsnull)
, mCFRunLoop(NULL)
, mCFRunLoopSource(NULL)
, mRunningEventLoop(PR_FALSE)
, mStarted(PR_FALSE)
, mTerminated(PR_FALSE)
, mNotifiedWillTerminate(PR_FALSE)
, mSkippedNativeCallback(PR_FALSE)
, mHadMoreEventsCount(0)
, mRecursionDepth(0)
, mNativeEventCallbackDepth(0)
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
  if (mCFRunLoop) {
    if (mCFRunLoopSource) {
      ::CFRunLoopRemoveSource(mCFRunLoop, mCFRunLoopSource,
                              kCFRunLoopCommonModes);
      ::CFRelease(mCFRunLoopSource);
    }
    ::CFRelease(mCFRunLoop);
  }

  if (mAutoreleasePools) {
    NS_ASSERTION(::CFArrayGetCount(mAutoreleasePools) == 0,
                 "nsAppShell destroyed without popping all autorelease pools");
    ::CFRelease(mAutoreleasePools);
  }

  [mDelegate release];
  // When you quit Camino with at least one window open, embedding is shut
  // down (by a call to NS_TermEmbedding()) and we (the current appshell)
  // are destroyed as the last browser window (class BrowserWindow) is closed.
  // (The call to NS_TermEmbedding() is ultimately made from
  // [BrowserWindowController windowWillClose:].)  This means that some code
  // runs _after_ the appshell is destroyed.  This code assumes that various
  // objects which have a retain count >= 1 will remain in existence, and that
  // an autorelease pool will still be available.  But because mMainPool sits
  // so low on the autorelease stack, if we release it here there's a good
  // chance that all the aforementioned objects (including the other
  // autorelease pools) will be released, and havoc will result.
  //
  // So if we've been terminated using [NSApplication terminate:] (which
  // Camino always uses), we don't release mMainPool here.  This won't cause
  // leaks, because after [NSApplication terminate:] sends an
  // NSApplicationWillTerminate notification it calls [NSApplication
  // _deallocHardCore:], which (after it uses [NSArray
  // makeObjectsPerformSelector:] to close all remaining windows) calls
  // [NSAutoreleasePool releaseAllPools] (to release all autorelease pools
  // on the current thread, which is the main thread).
  if (!mNotifiedWillTerminate)
    [mMainPool release];
}

// Init
//
// Loads the nib (see bug 316076c21) and sets up the CFRunLoopSource used to
// interrupt the main native run loop.
//
// public
nsresult
nsAppShell::Init()
{
  // No event loop is running yet (unless Camino is running, or another
  // embedding app that uses NSApplicationMain()).  Avoid autoreleasing
  // objects to mMainPool.  The appshell retains objects it needs to be
  // long-lived and will release them as appropriate.
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

  // This call initializes NSApplication unless:
  // 1) we're using xre -- NSApp's already been initialized by
  //    MacApplicationDelegate.mm's EnsureUseCocoaDockAPI().
  // 2) Camino is running (or another embedding app that uses
  //    NSApplicationMain()) -- NSApp's already been initialized and
  //    its main run loop is already running.
  [NSBundle loadNibFile:
                     [NSString stringWithUTF8String:(const char*)nibPath.get()]
      externalNameTable:
           [NSDictionary dictionaryWithObject:[NSApplication sharedApplication]
                                       forKey:@"NSOwner"]
               withZone:NSDefaultMallocZone()];

  mDelegate = [[AppShellDelegate alloc] initWithAppShell:this];
  NS_ENSURE_STATE(mDelegate);

  // Add a CFRunLoopSource to the main native run loop.  The source is
  // responsible for interrupting the run loop when Gecko events are ready.

  mCFRunLoop = [[NSRunLoop currentRunLoop] getCFRunLoop];
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

  rv = nsBaseAppShell::Init();

  [localPool release];

  return rv;
}

// ProcessGeckoEvents
//
// The "perform" target of mCFRunLoop, called when mCFRunLoopSource is
// signalled from ScheduleNativeEventCallback.
//
// Arrange for Gecko events to be processed on demand (in response to a call
// to ScheduleNativeEventCallback(), if processing of Gecko events via "native
// methods" hasn't been suspended).  This happens in NativeEventCallback() ...
// or rather it's supposed to:  nsBaseAppShell::NativeEventCallback() doesn't
// actually process any Gecko events if elsewhere we're also processing Gecko
// events in a tight loop (as happens in nsBaseAppShell::Run()) -- in that
// case ProcessGeckoEvents() is always called while ProcessNextNativeEvent()
// is running (called from nsBaseAppShell::OnProcessNextEvent()) and
// mProcessingNextNativeEvent is always true (which makes NativeEventCallback()
// take an early out).
//
// protected static
void
nsAppShell::ProcessGeckoEvents(void* aInfo)
{
  nsAppShell* self = static_cast<nsAppShell*> (aInfo);

  if (self->mRunningEventLoop) {
    self->mRunningEventLoop = PR_FALSE;

    // The run loop may be sleeping -- [NSRunLoop runMode:...]
    // won't return until it's given a reason to wake up.  Awaken it by
    // posting a bogus event.  There's no need to make the event
    // presentable.
    //
    // But _don't_ set windowNumber to '-1' -- that can lead to nasty
    // wierdness like bmo bug 397039 (a crash in [NSApp sendEvent:] on one of
    // these fake events, because the -1 has gotten changed into the number
    // of an actual NSWindow object, and that NSWindow object has just been
    // destroyed).  Setting windowNumber to '0' seems to work fine -- this
    // seems to prevent the OS from ever trying to associate our bogus event
    // with a particular NSWindow object.
    [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0,0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:NULL
                                         subtype:0
                                           data1:0
                                           data2:0]
             atStart:NO];
  }

  if (self->mSuspendNativeCount <= 0) {
    ++self->mNativeEventCallbackDepth;
    self->NativeEventCallback();
    --self->mNativeEventCallbackDepth;
  } else {
    self->mSkippedNativeCallback = PR_TRUE;
  }

  // Still needed to fix bug 343033 ("5-10 second delay or hang or crash
  // when quitting Cocoa Firefox").
  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSMakePoint(0,0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:NULL
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:NO];

  // Each Release() here is balanced by exactly one AddRef() in
  // ScheduleNativeEventCallback().
  NS_RELEASE(self);
}

// WillTerminate
//
// Called by the AppShellDelegate when an NSApplicationWillTerminate
// notification is posted.  After this method is called, native events should
// no longer be processed.  The NSApplicationWillTerminate notification is
// only posted when [NSApp terminate:] is called, which doesn't happen on a
// "normal" application quit.
//
// public
void
nsAppShell::WillTerminate()
{
  mNotifiedWillTerminate = PR_TRUE;
  if (mTerminated)
    return;
  mTerminated = PR_TRUE;

  // Calling [NSApp terminate:] causes (among other things) an
  // NSApplicationWillTerminate notification to be posted and the main run
  // loop to die before returning (in the call to [NSApp run]).  So this is
  // our last crack at processing any remaining Gecko events.
  NS_ProcessPendingEvents(NS_GetCurrentThread());

  // Unless we call nsBaseAppShell::Exit() here, it might not get called
  // at all.
  nsBaseAppShell::Exit();
}

// ScheduleNativeEventCallback
//
// Called (possibly on a non-main thread) when Gecko has an event that
// needs to be processed.  The Gecko event needs to be processed on the
// main thread, so the native run loop must be interrupted.
//
// In nsBaseAppShell.cpp, the mNativeEventPending variable is used to
// ensure that ScheduleNativeEventCallback() is called no more than once
// per call to NativeEventCallback().  ProcessGeckoEvents() can skip its
// call to NativeEventCallback() if processing of Gecko events by native
// means is suspended (using nsIAppShell::SuspendNative()), which will
// suspend calls from nsBaseAppShell::OnDispatchedEvent() to
// ScheduleNativeEventCallback().  But when Gecko event processing by
// native means is resumed (in ResumeNative()), an extra call is made to
// ScheduleNativeEventCallback() (from ResumeNative()).  This triggers
// another call to ProcessGeckoEvents(), which calls NativeEventCallback(),
// and nsBaseAppShell::OnDispatchedEvent() resumes calling
// ScheduleNativeEventCallback().
//
// protected virtual
void
nsAppShell::ScheduleNativeEventCallback()
{
  if (mTerminated)
    return;

  // Each AddRef() here is balanced by exactly one Release() in
  // ProcessGeckoEvents().
  NS_ADDREF_THIS();

  // This will invoke ProcessGeckoEvents on the main thread.
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
// But (now that we're using [NSRunLoop runMode:beforeDate:]) it's too
// expensive to call ProcessNextNativeEvent() many times in a row (in a
// tight loop), so we never return true more than kHadMoreEventsCountMax
// times in a row.  This doesn't seem to cause native event starvation.
//
// protected virtual
PRBool
nsAppShell::ProcessNextNativeEvent(PRBool aMayWait)
{
  PRBool moreEvents = PR_FALSE;
  PRBool eventProcessed = PR_FALSE;
  NSString* currentMode = nil;

  if (mTerminated)
    return moreEvents;

  PRBool wasRunningEventLoop = mRunningEventLoop;
  mRunningEventLoop = aMayWait;
  NSDate* waitUntil = nil;
  if (aMayWait)
    waitUntil = [NSDate distantFuture];

  NSRunLoop* currentRunLoop = [NSRunLoop currentRunLoop];

  do {
    // No autorelease pool is provided here, because OnProcessNextEvent
    // and AfterProcessNextEvent are responsible for maintaining it.
    NS_ASSERTION(mAutoreleasePools && ::CFArrayGetCount(mAutoreleasePools),
                 "No autorelease pool for native event");

    // If an event is waiting to be processed, run the main event loop
    // just long enough to process it.  For some reason, using [NSApp
    // nextEventMatchingMask:...] to dequeue the event and [NSApp sendEvent:]
    // to "send" it causes trouble, so we no longer do that.  (The trouble
    // was very strange, and only happened while processing Gecko events on
    // demand (via ProcessGeckoEvents()), as opposed to processing Gecko
    // events in a tight loop (via nsBaseAppShell::Run()):  Particularly in
    // Camino, mouse-down events sometimes got dropped (or mis-handled), so
    // that (for example) you sometimes needed to click more than once on a
    // button to make it work (the zoom button was particularly susceptible).
    // You also sometimes had to ctrl-click or right-click multiple times to
    // bring up a context menu.)

    // Now that we're using [NSRunLoop runMode:beforeDate:], it's too
    // expensive to call ProcessNextNativeEvent() many times in a row, so we
    // never return true more than kHadMoreEventsCountMax in a row.  I'm not
    // entirely sure why [NSRunLoop runMode:beforeDate:] is too expensive,
    // since it and its cousin [NSRunLoop acceptInputForMode:beforeDate:] are
    // designed to be called in a tight loop.  Possibly the problem is due to
    // combining [NSRunLoop runMode:beforeDate] with [NSApp
    // nextEventMatchingMask:...].

    // We special-case timer events (events of type NSPeriodic) to avoid
    // starving them.  Apple's documentation is very scanty, and it's now
    // more scanty than it used to be.  But it appears that [NSRunLoop
    // acceptInputForMode:beforeDate:] doesn't process timer events at all,
    // that it is called from [NSRunLoop runMode:beforeDate:], and that
    // [NSRunLoop runMode:beforeDate:], though it does process timer events,
    // doesn't return after doing so.  To get around this, when aWait is
    // PR_FALSE we check for timer events and process them using [NSApp
    // sendEvent:].  When aWait is PR_TRUE [NSRunLoop runMode:beforeDate:]
    // will only return on a "real" event.  But there's code in
    // ProcessGeckoEvents() that should (when need be) wake us up by sending
    // a "fake" "real" event.  (See Apple's current doc on [NSRunLoop
    // runMode:beforeDate:] and a quote from what appears to be an older
    // version of this doc at
    // http://lists.apple.com/archives/cocoa-dev/2001/May/msg00559.html.)

    // If the current mode is something else than NSDefaultRunLoopMode, look
    // for events in that mode.
    currentMode = [currentRunLoop currentMode];
    if (!currentMode)
      currentMode = NSDefaultRunLoopMode;

    NSEvent* nextEvent = nil;

    // If we're running modal (or not in a Gecko "main" event loop) we still
    // need to use nextEventMatchingMask and sendEvent -- otherwise (in
    // Minefield) the modal window (or non-main event loop) won't receive key
    // events or most mouse events.
    if ([NSApp _isRunningModal] || !InGeckoMainEventLoop()) {
      if (nextEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
                                         untilDate:waitUntil
                                            inMode:currentMode
                                           dequeue:YES]) {
        [NSApp sendEvent:nextEvent];
        eventProcessed = PR_TRUE;
      }
    } else {
      if (aMayWait ||
          (nextEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
                                          untilDate:nil
                                             inMode:currentMode
                                            dequeue:NO])) {
        if (nextEvent && ([nextEvent type] == NSPeriodic)) {
          nextEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
                                         untilDate:waitUntil
                                            inMode:currentMode
                                           dequeue:YES];
          [NSApp sendEvent:nextEvent];
        } else {
          [currentRunLoop runMode:currentMode beforeDate:waitUntil];
        }
        eventProcessed = PR_TRUE;
      }
    }
  } while (mRunningEventLoop);

  if (eventProcessed && (mHadMoreEventsCount < kHadMoreEventsCountMax)) {
    moreEvents = ([NSApp nextEventMatchingMask:NSAnyEventMask
                                     untilDate:nil
                                        inMode:currentMode
                                       dequeue:NO] != nil);
  }

  if (moreEvents) {
    // Once this reaches kHadMoreEventsCountMax, it will be reset to 0 the
    // next time through (whether or not we process any events then).
    ++mHadMoreEventsCount;
  } else {
    mHadMoreEventsCount = 0;
  }

  mRunningEventLoop = wasRunningEventLoop;

  return moreEvents;
}

// Returns PR_TRUE if Gecko events are currently being processed in its "main"
// event loop (or one of its "main" event loops).  Returns PR_FALSE if Gecko
// events are being processed in a "nested" event loop, or if we're not
// running in any sort of Gecko event loop.  How we process native events in
// ProcessNextNativeEvent() turns on our decision (and if we make the wrong
// choice, the result may be a hang).
//
// We define the "main" event loop(s) as the place (or places) where Gecko
// event processing "normally" takes place, and all other Gecko event loops
// as "nested".  The "nested" event loops are normally processed while a call
// from a "main" event loop is on the stack ... but not always.  For example,
// the Venkman JavaScript debugger runs a "nested" event loop (in jsdService::
// EnterNestedEventLoop()) whenever it breaks into the current script.  But
// if this happens as the result of the user pressing a key combination, there
// won't be any other Gecko event-processing call on the stack (e.g.
// NS_ProcessNextEvent() or NS_ProcessPendingEvents()).  (In the current
// nsAppShell implementation, what counts as the "main" event loop is what
// nsBaseAppShell::NativeEventCallback() does to process Gecko events.  We
// don't currently use nsBaseAppShell::Run().)
PRBool
nsAppShell::InGeckoMainEventLoop()
{
  if ((gXULModalLevel > 0) || (mRecursionDepth > 0))
    return PR_FALSE;
  if (mNativeEventCallbackDepth <= 0)
    return PR_FALSE;
  return PR_TRUE;
}

// Run
//
// Overrides the base class's Run() method to call [NSApp run] (which spins
// the native run loop until the application quits).  Since (unlike the base
// class's Run() method) we don't process any Gecko events here, they need
// to be processed elsewhere (in NativeEventCallback(), called from
// ProcessGeckoEvents()).
//
// Camino calls [NSApp run] on its own (via NSApplicationMain()), and so
// doesn't call nsAppShell::Run().
//
// public
NS_IMETHODIMP
nsAppShell::Run(void)
{
  NS_ASSERTION(!mStarted, "nsAppShell::Run() called multiple times");
  if (mStarted)
    return NS_OK;

  mStarted = PR_TRUE;
  [NSApp run];

  return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Exit(void)
{
  // This method is currently called more than once -- from (according to
  // mento) an nsAppExitEvent dispatched by nsAppStartup::Quit() and from an
  // XPCOM shutdown notification that nsBaseAppShell has registered to
  // receive.  So we need to ensure that multiple calls won't break anything.
  // But we should also complain about it (since it isn't quite kosher).
  if (mTerminated) {
    NS_WARNING("nsAppShell::Exit() called redundantly");
    return NS_OK;
  }

  mTerminated = PR_TRUE;

  // Quoting from Apple's doc on the [NSApplication stop:] method (from their
  // doc on the NSApplication class):  "If this method is invoked during a
  // modal event loop, it will break that loop but not the main event loop."
  // nsAppShell::Exit() shouldn't be called from a modal event loop.  So if
  // it is we complain about it (to users of debug builds) and call [NSApp
  // stop:] one extra time.  (I'm not sure if modal event loops can be nested
  // -- Apple's docs don't say one way or the other.  But the return value
  // of [NSApp _isRunningModal] doesn't change immediately after a call to
  // [NSApp stop:], so we have to assume that one extra call to [NSApp stop:]
  // will do the job.)
  BOOL cocoaModal = [NSApp _isRunningModal];
  NS_ASSERTION(!cocoaModal,
               "Don't call nsAppShell::Exit() from a modal event loop!");
  if (cocoaModal)
    [NSApp stop:nsnull];
  [NSApp stop:nsnull];

  return nsBaseAppShell::Exit();
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
  mRecursionDepth = aRecursionDepth;

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
  mRecursionDepth = aRecursionDepth;

  CFIndex count = ::CFArrayGetCount(mAutoreleasePools);

  NS_ASSERTION(mAutoreleasePools && count,
               "Processed an event, but there's no autorelease pool?");

  const NSAutoreleasePool* pool = static_cast<const NSAutoreleasePool*>
    (::CFArrayGetValueAtIndex(mAutoreleasePools, count - 1));
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

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                        selector:@selector(beginMenuTracking:)
                                                            name:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                                                          object:nil];
  }

  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// applicationWillTerminate:
//
// Notify the nsAppShell that native event processing should be discontinued.
- (void)applicationWillTerminate:(NSNotification*)aNotification
{
  mAppShell->WillTerminate();
}

// beginMenuTracking
//
// Roll up our context menu (if any) when some other app (or the OS) opens
// any sort of menu.  But make sure we don't do this for notifications we
// send ourselves (whose 'sender' will be @"org.mozilla.gecko.PopupWindow").
- (void)beginMenuTracking:(NSNotification*)aNotification
{
  NSString *sender = [aNotification object];
  if (!sender || ![sender isEqualToString:@"org.mozilla.gecko.PopupWindow"]) {
    if (gRollupListener && gRollupWidget)
      gRollupListener->Rollup(nsnull);
  }
}

@end

