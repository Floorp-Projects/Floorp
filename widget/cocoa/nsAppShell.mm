/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Runs the main native Cocoa run loop, interrupting it as needed to process
 * Gecko events.
 */

#import <Cocoa/Cocoa.h>

#include "CustomCocoaEvents.h"
#include "mozilla/WidgetTraceEvent.h"
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
#include "nsObjCExceptions.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsChildView.h"
#include "nsToolkit.h"
#include "TextInputHandler.h"
#include "mozilla/HangMonitor.h"
#include "GeckoProfiler.h"
#include "pratom.h"

#include <IOKit/pwr_mgt/IOPMLib.h>
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"

using namespace mozilla::widget;

// A wake lock listener that disables screen saver when requested by
// Gecko. For example when we're playing video in a foreground tab we
// don't want the screen saver to turn on.

class MacWakeLockListener MOZ_FINAL : public nsIDOMMozWakeLockListener {
public:
  NS_DECL_ISUPPORTS;

private:
  IOPMAssertionID mAssertionID = kIOPMNullAssertionID;

  NS_IMETHOD Callback(const nsAString& aTopic, const nsAString& aState) {
    bool isLocked = mLockedTopics.Contains(aTopic);
    bool shouldLock = aState.EqualsLiteral("locked-foreground");
    if (isLocked == shouldLock) {
      return NS_OK;
    }
    if (shouldLock) {
      if (!mLockedTopics.Count()) {
        // This is the first topic to request the screen saver be disabled.
        // Prevent screen saver.
        CFStringRef cf_topic =
          ::CFStringCreateWithCharacters(kCFAllocatorDefault,
                                         reinterpret_cast<const UniChar*>
                                           (aTopic.Data()),
                                         aTopic.Length());
        IOReturn success =
          ::IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                                        kIOPMAssertionLevelOn,
                                        cf_topic,
                                        &mAssertionID);
        CFRelease(cf_topic);
        if (success != kIOReturnSuccess) {
          NS_WARNING("fail to disable screensaver");
        }
      }
      mLockedTopics.PutEntry(aTopic);
    } else {
      mLockedTopics.RemoveEntry(aTopic);
      if (!mLockedTopics.Count()) {
        // No other outstanding topics have requested screen saver be disabled.
        // Re-enable screen saver.
        if (mAssertionID != kIOPMNullAssertionID) {
          IOReturn result = ::IOPMAssertionRelease(mAssertionID);
          if (result != kIOReturnSuccess) {
            NS_WARNING("fail to release screensaver");
          }
        }
      }
    }
    return NS_OK;
  }
  // Keep track of all the topics that have requested a wake lock. When the
  // number of topics in the hashtable reaches zero, we can uninhibit the
  // screensaver again.
  nsTHashtable<nsStringHashKey> mLockedTopics;
};

// defined in nsCocoaWindow.mm
extern int32_t             gXULModalLevel;

static bool gAppShellMethodsSwizzled = false;

@implementation GeckoNSApplication

- (void)sendEvent:(NSEvent *)anEvent
{
  mozilla::HangMonitor::NotifyActivity();
  if ([anEvent type] == NSApplicationDefined &&
      [anEvent subtype] == kEventSubtypeTrace) {
    mozilla::SignalTracerThread();
    return;
  }
  [super sendEvent:anEvent];
}

- (NSEvent*)nextEventMatchingMask:(NSUInteger)mask
                        untilDate:(NSDate*)expiration
                           inMode:(NSString*)mode
                          dequeue:(BOOL)flag
{
  if (expiration) {
    mozilla::HangMonitor::Suspend();
  }
  return [super nextEventMatchingMask:mask
          untilDate:expiration inMode:mode dequeue:flag];
}

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
    mSkippedNativeCallback = false;
    ScheduleNativeEventCallback();
  }
  return retval;
}

nsAppShell::nsAppShell()
: mAutoreleasePools(nullptr)
, mDelegate(nullptr)
, mCFRunLoop(NULL)
, mCFRunLoopSource(NULL)
, mRunningEventLoop(false)
, mStarted(false)
, mTerminated(false)
, mSkippedNativeCallback(false)
, mNativeEventCallbackDepth(0)
, mNativeEventScheduledDepth(0)
{
  // A Cocoa event loop is running here if (and only if) we've been embedded
  // by a Cocoa app (like Camino).
  mRunningCocoaEmbedded = [NSApp isRunning] ? true : false;
}

nsAppShell::~nsAppShell()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK
}

NS_IMPL_ISUPPORTS(MacWakeLockListener, nsIDOMMozWakeLockListener)
mozilla::StaticRefPtr<MacWakeLockListener> sWakeLockListener;

static void
AddScreenWakeLockListener()
{
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService = do_GetService(
                                                          POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sWakeLockListener = new MacWakeLockListener();
    sPowerManagerService->AddWakeLockListener(sWakeLockListener);
  } else {
    NS_WARNING("Failed to retrieve PowerManagerService, wakelocks will be broken!");
  }
}

static void
RemoveScreenWakeLockListener()
{
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService = do_GetService(
                                                          POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);
    sPowerManagerService = nullptr;
    sWakeLockListener = nullptr;
  }
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // No event loop is running yet (unless Camino is running, or another
  // embedding app that uses NSApplicationMain()).
  NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

  // mAutoreleasePools is used as a stack of NSAutoreleasePool objects created
  // by |this|.  CFArray is used instead of NSArray because NSArray wants to
  // retain each object you add to it, and you can't retain an
  // NSAutoreleasePool.
  mAutoreleasePools = ::CFArrayCreateMutable(nullptr, 0, nullptr);
  NS_ENSURE_STATE(mAutoreleasePools);

  // Get the path of the nib file, which lives in the GRE location
  nsCOMPtr<nsIFile> nibFile;
  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(nibFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nibFile->AppendNative(NS_LITERAL_CSTRING("res"));
  nibFile->AppendNative(NS_LITERAL_CSTRING("MainMenu.nib"));

  nsAutoCString nibPath;
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
           [NSDictionary dictionaryWithObject:[GeckoNSApplication sharedApplication]
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

#ifndef __LP64__
  TextInputHandler::InstallPluginKeyEventsHandler();
#endif

  if (!gAppShellMethodsSwizzled) {
    // We should only replace the original terminate: method if we're not
    // running in a Cocoa embedder (like Camino).  See bug 604901.
    if (!mRunningCocoaEmbedded) {
      nsToolkit::SwizzleMethods([NSApplication class], @selector(terminate:),
                                @selector(nsAppShell_NSApplication_terminate:));
    }
    gAppShellMethodsSwizzled = true;
  }

  [localPool release];

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// ProcessGeckoEvents
//
// The "perform" target of mCFRunLoop, called when mCFRunLoopSource is
// signalled from ScheduleNativeEventCallback.
//
// Arrange for Gecko events to be processed on demand (in response to a call
// to ScheduleNativeEventCallback(), if processing of Gecko events via "native
// methods" hasn't been suspended).  This happens in NativeEventCallback().
//
// protected static
void
nsAppShell::ProcessGeckoEvents(void* aInfo)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  PROFILER_LABEL("Events", "ProcessGeckoEvents",
    js::ProfileEntry::Category::EVENTS);

  nsAppShell* self = static_cast<nsAppShell*> (aInfo);

  if (self->mRunningEventLoop) {
    self->mRunningEventLoop = false;

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
                                         subtype:kEventSubtypeNone
                                           data1:0
                                           data2:0]
             atStart:NO];
  }

  if (self->mSuspendNativeCount <= 0) {
    ++self->mNativeEventCallbackDepth;
    self->NativeEventCallback();
    --self->mNativeEventCallbackDepth;
  } else {
    self->mSkippedNativeCallback = true;
  }

  // Still needed to avoid crashes on quit in most Mochitests.
  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSMakePoint(0,0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:NULL
                                       subtype:kEventSubtypeNone
                                         data1:0
                                         data2:0]
           atStart:NO];

  // Normally every call to ScheduleNativeEventCallback() results in
  // exactly one call to ProcessGeckoEvents().  So each Release() here
  // normally balances exactly one AddRef() in ScheduleNativeEventCallback().
  // But if Exit() is called just after ScheduleNativeEventCallback(), the
  // corresponding call to ProcessGeckoEvents() will never happen.  We check
  // for this possibility in two different places -- here and in Exit()
  // itself.  If we find here that Exit() has been called (that mTerminated
  // is true), it's because we've been called recursively, that Exit() was
  // called from self->NativeEventCallback() above, and that we're unwinding
  // the recursion.  In this case we'll never be called again, and we balance
  // here any extra calls to ScheduleNativeEventCallback().
  //
  // When ProcessGeckoEvents() is called recursively, it's because of a
  // call to ScheduleNativeEventCallback() from NativeEventCallback().  We
  // balance the "extra" AddRefs here (rather than always in Exit()) in order
  // to ensure that 'self' stays alive until the end of this method.  We also
  // make sure not to finish the balancing until all the recursion has been
  // unwound.
  if (self->mTerminated) {
    int32_t releaseCount = 0;
    if (self->mNativeEventScheduledDepth > self->mNativeEventCallbackDepth) {
      releaseCount = PR_ATOMIC_SET(&self->mNativeEventScheduledDepth,
                                   self->mNativeEventCallbackDepth);
    }
    while (releaseCount-- > self->mNativeEventCallbackDepth)
      self->Release();
  } else {
    // As best we can tell, every call to ProcessGeckoEvents() is triggered
    // by a call to ScheduleNativeEventCallback().  But we've seen a few
    // (non-reproducible) cases of double-frees that *might* have been caused
    // by spontaneous calls (from the OS) to ProcessGeckoEvents().  So we
    // deal with that possibility here.
    if (PR_ATOMIC_DECREMENT(&self->mNativeEventScheduledDepth) < 0) {
      PR_ATOMIC_SET(&self->mNativeEventScheduledDepth, 0);
      NS_WARNING("Spontaneous call to ProcessGeckoEvents()!");
    } else {
      self->Release();
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  if (mTerminated)
    return;

  // Make sure that the nsAppExitEvent posted by nsAppStartup::Quit() (called
  // from [MacApplicationDelegate applicationShouldTerminate:]) gets run.
  NS_ProcessPendingEvents(NS_GetCurrentThread());

  mTerminated = true;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mTerminated)
    return;

  // Each AddRef() here is normally balanced by exactly one Release() in
  // ProcessGeckoEvents().  But there are exceptions, for which see
  // ProcessGeckoEvents() and Exit().
  NS_ADDREF_THIS();
  PR_ATOMIC_INCREMENT(&mNativeEventScheduledDepth);

  // This will invoke ProcessGeckoEvents on the main thread.
  ::CFRunLoopSourceSignal(mCFRunLoopSource);
  ::CFRunLoopWakeUp(mCFRunLoop);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Undocumented Cocoa Event Manager function, present in the same form since
// at least OS X 10.6.
extern "C" EventAttributes GetEventAttributes(EventRef inEvent);

// ProcessNextNativeEvent
//
// If aMayWait is false, process a single native event.  If it is true, run
// the native run loop until stopped by ProcessGeckoEvents.
//
// Returns true if more events are waiting in the native event queue.
//
// protected virtual
bool
nsAppShell::ProcessNextNativeEvent(bool aMayWait)
{
  bool moreEvents = false;

  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  bool eventProcessed = false;
  NSString* currentMode = nil;

  if (mTerminated)
    return false;

  bool wasRunningEventLoop = mRunningEventLoop;
  mRunningEventLoop = aMayWait;
  NSDate* waitUntil = nil;
  if (aMayWait)
    waitUntil = [NSDate distantFuture];

  NSRunLoop* currentRunLoop = [NSRunLoop currentRunLoop];

  EventQueueRef currentEventQueue = GetCurrentEventQueue();
  EventTargetRef eventDispatcherTarget = GetEventDispatcherTarget();

  if (aMayWait) {
    mozilla::HangMonitor::Suspend();
  }

  // Only call -[NSApp sendEvent:] (and indirectly send user-input events to
  // Gecko) if aMayWait is true.  Tbis ensures most calls to -[NSApp
  // sendEvent:] happen under nsAppShell::Run(), at the lowest level of
  // recursion -- thereby making it less likely Gecko will process user-input
  // events in the wrong order or skip some of them.  It also avoids eating
  // too much CPU in nsBaseAppShell::OnProcessNextEvent() (which calls
  // us) -- thereby avoiding the starvation of nsIRunnable events in
  // nsThread::ProcessNextEvent().  For more information see bug 996848.
  do {
    // No autorelease pool is provided here, because OnProcessNextEvent
    // and AfterProcessNextEvent are responsible for maintaining it.
    NS_ASSERTION(mAutoreleasePools && ::CFArrayGetCount(mAutoreleasePools),
                 "No autorelease pool for native event");

    if (aMayWait) {
      currentMode = [currentRunLoop currentMode];
      if (!currentMode)
        currentMode = NSDefaultRunLoopMode;
      NSEvent *nextEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
                                              untilDate:waitUntil
                                                 inMode:currentMode
                                                dequeue:YES];
      if (nextEvent) {
        mozilla::HangMonitor::NotifyActivity();
        [NSApp sendEvent:nextEvent];
        eventProcessed = true;
      }
    } else {
      // AcquireFirstMatchingEventInQueue() doesn't spin the (native) event
      // loop, though it does queue up any newly available events from the
      // window server.
      EventRef currentEvent = AcquireFirstMatchingEventInQueue(currentEventQueue, 0, NULL,
                                                               kEventQueueOptionsNone);
      if (!currentEvent) {
        continue;
      }
      EventAttributes attrs = GetEventAttributes(currentEvent);
      UInt32 eventKind = GetEventKind(currentEvent);
      UInt32 eventClass = GetEventClass(currentEvent);
      bool osCocoaEvent =
        ((eventClass == 'appl') ||
         ((eventClass == 'cgs ') && (eventKind != NSApplicationDefined)));
      // If attrs is kEventAttributeUserEvent or kEventAttributeMonitored
      // (i.e. a user input event), we shouldn't process it here while
      // aMayWait is false.  Likewise if currentEvent will eventually be
      // turned into an OS-defined Cocoa event.  Doing otherwise risks
      // doing too much work here, and preventing the event from being
      // properly processed as a Cocoa event.
      if ((attrs != kEventAttributeNone) || osCocoaEvent) {
        // Since we can't process the next event here (while aMayWait is false),
        // we want moreEvents to be false on return.
        eventProcessed = false;
        // This call to ReleaseEvent() matches a call to RetainEvent() in
        // AcquireFirstMatchingEventInQueue() above.
        ReleaseEvent(currentEvent);
        break;
      }
      // This call to RetainEvent() matches a call to ReleaseEvent() in
      // RemoveEventFromQueue() below.
      RetainEvent(currentEvent);
      RemoveEventFromQueue(currentEventQueue, currentEvent);
      SendEventToEventTarget(currentEvent, eventDispatcherTarget);
      // This call to ReleaseEvent() matches a call to RetainEvent() in
      // AcquireFirstMatchingEventInQueue() above.
      ReleaseEvent(currentEvent);
      eventProcessed = true;
    }
  } while (mRunningEventLoop);

  if (eventProcessed) {
    moreEvents =
      (AcquireFirstMatchingEventInQueue(currentEventQueue, 0, NULL,
                                        kEventQueueOptionsNone) != NULL);
  }

  mRunningEventLoop = wasRunningEventLoop;

  NS_OBJC_END_TRY_ABORT_BLOCK;

  if (!moreEvents) {
    nsChildView::UpdateCurrentInputEventCount();
  }

  return moreEvents;
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
  if (mStarted || mTerminated)
    return NS_OK;

  mStarted = true;

  AddScreenWakeLockListener();

  NS_OBJC_TRY_ABORT([NSApp run]);

  RemoveScreenWakeLockListener();

  return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Exit(void)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // This method is currently called more than once -- from (according to
  // mento) an nsAppExitEvent dispatched by nsAppStartup::Quit() and from an
  // XPCOM shutdown notification that nsBaseAppShell has registered to
  // receive.  So we need to ensure that multiple calls won't break anything.
  // But we should also complain about it (since it isn't quite kosher).
  if (mTerminated) {
    NS_WARNING("nsAppShell::Exit() called redundantly");
    return NS_OK;
  }

  mTerminated = true;

#ifndef __LP64__
  TextInputHandler::RemovePluginKeyEventsHandler();
#endif

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
    [NSApp stop:nullptr];
  [NSApp stop:nullptr];

  // A call to Exit() just after a call to ScheduleNativeEventCallback()
  // prevents the (normally) matching call to ProcessGeckoEvents() from
  // happening.  If we've been called from ProcessGeckoEvents() (as usually
  // happens), we take care of it there.  But if we have an unbalanced call
  // to ScheduleNativeEventCallback() and ProcessGeckoEvents() isn't on the
  // stack, we need to take care of the problem here.
  if (!mNativeEventCallbackDepth && mNativeEventScheduledDepth) {
    int32_t releaseCount = PR_ATOMIC_SET(&mNativeEventScheduledDepth, 0);
    while (releaseCount-- > 0)
      NS_RELEASE_THIS();
  }

  return nsBaseAppShell::Exit();

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
nsAppShell::OnProcessNextEvent(nsIThreadInternal *aThread, bool aMayWait,
                               uint32_t aRecursionDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mAutoreleasePools,
               "No stack on which to store autorelease pool");

  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  ::CFArrayAppendValue(mAutoreleasePools, pool);

  return nsBaseAppShell::OnProcessNextEvent(aThread, aMayWait, aRecursionDepth);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
                                  uint32_t aRecursionDepth,
                                  bool aEventWasProcessed)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  CFIndex count = ::CFArrayGetCount(mAutoreleasePools);

  NS_ASSERTION(mAutoreleasePools && count,
               "Processed an event, but there's no autorelease pool?");

  const NSAutoreleasePool* pool = static_cast<const NSAutoreleasePool*>
    (::CFArrayGetValueAtIndex(mAutoreleasePools, count - 1));
  ::CFArrayRemoveValueAtIndex(mAutoreleasePools, count - 1);
  [pool release];

  return nsBaseAppShell::AfterProcessNextEvent(aThread, aRecursionDepth,
                                               aEventWasProcessed);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


// AppShellDelegate implementation


@implementation AppShellDelegate
// initWithAppShell:
//
// Constructs the AppShellDelegate object
- (id)initWithAppShell:(nsAppShell*)aAppShell
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [self init])) {
    mAppShell = aAppShell;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationDidBecomeActive:)
                                                 name:NSApplicationDidBecomeActiveNotification
                                               object:NSApp];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                        selector:@selector(beginMenuTracking:)
                                                            name:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                                                          object:nil];
  }

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// applicationWillTerminate:
//
// Notify the nsAppShell that native event processing should be discontinued.
- (void)applicationWillTerminate:(NSNotification*)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mAppShell->WillTerminate();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// applicationDidBecomeActive
//
// Make sure TextInputHandler::sLastModifierState is updated when we become
// active (since we won't have received [ChildView flagsChanged:] messages
// while inactive).
- (void)applicationDidBecomeActive:(NSNotification*)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // [NSEvent modifierFlags] is valid on every kind of event, so we don't need
  // to worry about getting an NSInternalInconsistencyException here.
  NSEvent* currentEvent = [NSApp currentEvent];
  if (currentEvent) {
    TextInputHandler::sLastModifierState =
      [currentEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// beginMenuTracking
//
// Roll up our context menu (if any) when some other app (or the OS) opens
// any sort of menu.  But make sure we don't do this for notifications we
// send ourselves (whose 'sender' will be @"org.mozilla.gecko.PopupWindow").
- (void)beginMenuTracking:(NSNotification*)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSString *sender = [aNotification object];
  if (!sender || ![sender isEqualToString:@"org.mozilla.gecko.PopupWindow"]) {
    nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
    nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
    if (rollupWidget)
      rollupListener->Rollup(0, nullptr, nullptr);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

// We hook terminate: in order to make OS-initiated termination work nicely
// with Gecko's shutdown sequence.  (Two ways to trigger OS-initiated
// termination:  1) Quit from the Dock menu; 2) Log out from (or shut down)
// your computer while the browser is active.)
@interface NSApplication (MethodSwizzling)
- (void)nsAppShell_NSApplication_terminate:(id)sender;
@end

@implementation NSApplication (MethodSwizzling)

// Called by the OS after [MacApplicationDelegate applicationShouldTerminate:]
// has returned NSTerminateNow.  This method "subclasses" and replaces the
// OS's original implementation.  The only thing the orginal method does which
// we need is that it posts NSApplicationWillTerminateNotification.  Everything
// else is unneeded (because it's handled elsewhere), or actively interferes
// with Gecko's shutdown sequence.  For example the original terminate: method
// causes the app to exit() inside [NSApp run] (called from nsAppShell::Run()
// above), which means that nothing runs after the call to nsAppStartup::Run()
// in XRE_Main(), which in particular means that ScopedXPCOMStartup's destructor
// and NS_ShutdownXPCOM() never get called.
- (void)nsAppShell_NSApplication_terminate:(id)sender
{
  [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillTerminateNotification
                                                      object:NSApp];
}

@end
