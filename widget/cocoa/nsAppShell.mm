/* -*- tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Runs the main native Cocoa run loop, interrupting it as needed to process
 * Gecko events.
 */

#import <Cocoa/Cocoa.h>

#include <dlfcn.h>

#include "mozilla/AvailableMemoryWatcher.h"
#include "CustomCocoaEvents.h"
#include "mozilla/WidgetTraceEvent.h"
#include "nsAppShell.h"
#include "gfxPlatform.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsIRollupListener.h"
#include "nsIWidget.h"
#include "nsMemoryPressure.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsCocoaFeatures.h"
#include "nsChildView.h"
#include "nsToolkit.h"
#include "TextInputHandler.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "ScreenHelperCocoa.h"
#include "mozilla/Hal.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerThreadSleep.h"
#include "mozilla/widget/ScreenManager.h"
#include "HeadlessScreenHelper.h"
#include "MOZMenuOpeningCoordinator.h"
#include "pratom.h"
#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
#  include "nsSandboxViolationSink.h"
#endif

#include <IOKit/pwr_mgt/IOPMLib.h>
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"

#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_widget.h"

using namespace mozilla;
using namespace mozilla::widget;

#define WAKE_LOCK_LOG(...) \
  MOZ_LOG(gMacWakeLockLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule gMacWakeLockLog("MacWakeLock");

// A wake lock listener that disables screen saver when requested by
// Gecko. For example when we're playing video in a foreground tab we
// don't want the screen saver to turn on.

class MacWakeLockListener final : public nsIDOMMozWakeLockListener {
 public:
  NS_DECL_ISUPPORTS;

 private:
  ~MacWakeLockListener() {}

  IOPMAssertionID mAssertionNoDisplaySleepID = kIOPMNullAssertionID;
  IOPMAssertionID mAssertionNoIdleSleepID = kIOPMNullAssertionID;

  NS_IMETHOD Callback(const nsAString& aTopic,
                      const nsAString& aState) override {
    if (!aTopic.EqualsASCII("screen") && !aTopic.EqualsASCII("audio-playing") &&
        !aTopic.EqualsASCII("video-playing")) {
      return NS_OK;
    }

    // we should still hold the lock for background audio.
    if (aTopic.EqualsASCII("audio-playing") &&
        aState.EqualsASCII("locked-background")) {
      WAKE_LOCK_LOG("keep audio playing even in background");
      return NS_OK;
    }

    bool shouldKeepDisplayOn =
        aTopic.EqualsASCII("screen") || aTopic.EqualsASCII("video-playing");
    CFStringRef assertionType = shouldKeepDisplayOn
                                    ? kIOPMAssertionTypeNoDisplaySleep
                                    : kIOPMAssertionTypeNoIdleSleep;
    IOPMAssertionID& assertionId = shouldKeepDisplayOn
                                       ? mAssertionNoDisplaySleepID
                                       : mAssertionNoIdleSleepID;
    WAKE_LOCK_LOG("topic=%s, state=%s, shouldKeepDisplayOn=%d",
                  NS_ConvertUTF16toUTF8(aTopic).get(),
                  NS_ConvertUTF16toUTF8(aState).get(), shouldKeepDisplayOn);

    // Note the wake lock code ensures that we're not sent duplicate
    // "locked-foreground" notifications when multiple wake locks are held.
    if (aState.EqualsASCII("locked-foreground")) {
      if (assertionId != kIOPMNullAssertionID) {
        WAKE_LOCK_LOG("already has a lock");
        return NS_OK;
      }
      // Prevent screen saver.
      CFStringRef cf_topic = ::CFStringCreateWithCharacters(
          kCFAllocatorDefault, reinterpret_cast<const UniChar*>(aTopic.Data()),
          aTopic.Length());
      IOReturn success = ::IOPMAssertionCreateWithName(
          assertionType, kIOPMAssertionLevelOn, cf_topic, &assertionId);
      CFRelease(cf_topic);
      if (success != kIOReturnSuccess) {
        WAKE_LOCK_LOG("failed to disable screensaver");
      }
      WAKE_LOCK_LOG("create screensaver");
    } else {
      // Re-enable screen saver.
      if (assertionId != kIOPMNullAssertionID) {
        IOReturn result = ::IOPMAssertionRelease(assertionId);
        if (result != kIOReturnSuccess) {
          WAKE_LOCK_LOG("failed to release screensaver");
        }
        WAKE_LOCK_LOG("Release screensaver");
        assertionId = kIOPMNullAssertionID;
      }
    }
    return NS_OK;
  }
};  // MacWakeLockListener

// defined in nsCocoaWindow.mm
extern int32_t gXULModalLevel;

static bool gAppShellMethodsSwizzled = false;

void OnUncaughtException(NSException* aException) {
  nsObjCExceptionLog(aException);
  MOZ_CRASH(
      "Uncaught Objective C exception from NSSetUncaughtExceptionHandler");
}

@implementation GeckoNSApplication

// Load is called very early during startup, when the Objective C runtime loads
// this class.
+ (void)load {
  NSSetUncaughtExceptionHandler(OnUncaughtException);
}

// This method is called from NSDefaultTopLevelErrorHandler, which is invoked
// when an Objective C exception propagates up into the native event loop. It is
// possible that it is also called in other cases.
- (void)reportException:(NSException*)aException {
  if (ShouldIgnoreObjCException(aException)) {
    return;
  }

  nsObjCExceptionLog(aException);

#ifdef NIGHTLY_BUILD
  MOZ_CRASH("Uncaught Objective C exception from -[GeckoNSApplication "
            "reportException:]");
#endif
}

- (void)run {
  _didLaunch = YES;
  [super run];
}

- (void)sendEvent:(NSEvent*)anEvent {
  mozilla::BackgroundHangMonitor().NotifyActivity();

  if ([anEvent type] == NSEventTypeApplicationDefined &&
      [anEvent subtype] == kEventSubtypeTrace) {
    mozilla::SignalTracerThread();
    return;
  }
  [super sendEvent:anEvent];
}

- (NSEvent*)nextEventMatchingMask:(NSEventMask)mask
                        untilDate:(NSDate*)expiration
                           inMode:(NSString*)mode
                          dequeue:(BOOL)flag {
  MOZ_ASSERT([NSApp didLaunch]);
  if (expiration) {
    mozilla::BackgroundHangMonitor().NotifyWait();
  }
  NSEvent* nextEvent = [super nextEventMatchingMask:mask
                                          untilDate:expiration
                                             inMode:mode
                                            dequeue:flag];
  if (expiration) {
    mozilla::BackgroundHangMonitor().NotifyActivity();
  }
  return nextEvent;
}

@end

// AppShellDelegate
//
// Cocoa bridge class.  An object of this class is registered to receive
// notifications.
//
@interface AppShellDelegate : NSObject {
 @private
  nsAppShell* mAppShell;
}

- (id)initWithAppShell:(nsAppShell*)aAppShell;
- (void)applicationWillTerminate:(NSNotification*)aNotification;
@end

// nsAppShell implementation

NS_IMETHODIMP
nsAppShell::ResumeNative(void) {
  nsresult retval = nsBaseAppShell::ResumeNative();
  if (NS_SUCCEEDED(retval) && (mSuspendNativeCount == 0) &&
      mSkippedNativeCallback) {
    mSkippedNativeCallback = false;
    ScheduleNativeEventCallback();
  }
  return retval;
}

nsAppShell::nsAppShell()
    : mAutoreleasePools(nullptr),
      mDelegate(nullptr),
      mCFRunLoop(NULL),
      mCFRunLoopSource(NULL),
      mRunningEventLoop(false),
      mStarted(false),
      mTerminated(false),
      mSkippedNativeCallback(false),
      mNativeEventCallbackDepth(0),
      mNativeEventScheduledDepth(0) {
  // A Cocoa event loop is running here if (and only if) we've been embedded
  // by a Cocoa app.
  mRunningCocoaEmbedded = [NSApp isRunning] ? true : false;
}

nsAppShell::~nsAppShell() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  hal::Shutdown();

  if (mMemoryPressureSource) {
    dispatch_release(mMemoryPressureSource);
    mMemoryPressureSource = nullptr;
  }

  if (mCFRunLoop) {
    if (mCFRunLoopSource) {
      ::CFRunLoopRemoveSource(mCFRunLoop, mCFRunLoopSource,
                              kCFRunLoopCommonModes);
      ::CFRelease(mCFRunLoopSource);
    }
    if (mCFRunLoopObserver) {
      ::CFRunLoopRemoveObserver(mCFRunLoop, mCFRunLoopObserver,
                                kCFRunLoopCommonModes);
      ::CFRelease(mCFRunLoopObserver);
    }
    ::CFRelease(mCFRunLoop);
  }

  if (mAutoreleasePools) {
    NS_ASSERTION(::CFArrayGetCount(mAutoreleasePools) == 0,
                 "nsAppShell destroyed without popping all autorelease pools");
    ::CFRelease(mAutoreleasePools);
  }

  [mDelegate release];

  NS_OBJC_END_TRY_IGNORE_BLOCK
}

NS_IMPL_ISUPPORTS(MacWakeLockListener, nsIDOMMozWakeLockListener)
mozilla::StaticRefPtr<MacWakeLockListener> sWakeLockListener;

static void AddScreenWakeLockListener() {
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sWakeLockListener = new MacWakeLockListener();
    sPowerManagerService->AddWakeLockListener(sWakeLockListener);
  } else {
    NS_WARNING(
        "Failed to retrieve PowerManagerService, wakelocks will be broken!");
  }
}

static void RemoveScreenWakeLockListener() {
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);
    sPowerManagerService = nullptr;
    sWakeLockListener = nullptr;
  }
}

void RunLoopObserverCallback(CFRunLoopObserverRef aObserver,
                             CFRunLoopActivity aActivity, void* aInfo) {
  static_cast<nsAppShell*>(aInfo)->OnRunLoopActivityChanged(aActivity);
}

void nsAppShell::OnRunLoopActivityChanged(CFRunLoopActivity aActivity) {
  if (aActivity == kCFRunLoopBeforeWaiting) {
    mozilla::BackgroundHangMonitor().NotifyWait();
  }

  // When the event loop is in its waiting state, we would like the profiler to
  // know that the thread is idle. The usual way to notify the profiler of
  // idleness would be to place a profiler label frame with the IDLE category on
  // the stack, for the duration of the function that does the waiting. However,
  // since macOS uses an event loop model where "the event loop calls you", we
  // do not control the function that does the waiting; the waiting happens
  // inside CFRunLoop code. Instead, the run loop notifies us when it enters and
  // exits the waiting state, by calling this function. So we do not have a
  // function under our control that stays on the stack for the duration of the
  // wait. So, rather than putting an AutoProfilerLabel on the stack, we will
  // manually push and pop the label frame here. The location in the stack where
  // this label frame is inserted is somewhat arbitrary. In practice, the label
  // frame will be at the very tip of the stack, looking like it's "inside" the
  // mach_msg_trap wait function.
  if (aActivity == kCFRunLoopBeforeWaiting) {
    using ThreadRegistration = mozilla::profiler::ThreadRegistration;
    ThreadRegistration::WithOnThreadRef(
        [&](ThreadRegistration::OnThreadRef aOnThreadRef) {
          ProfilingStack& profilingStack =
              aOnThreadRef.UnlockedConstReaderAndAtomicRWRef()
                  .ProfilingStackRef();
          mProfilingStackWhileWaiting = &profilingStack;
          uint8_t variableOnStack = 0;
          profilingStack.pushLabelFrame("Native event loop idle", nullptr,
                                        &variableOnStack,
                                        JS::ProfilingCategoryPair::IDLE, 0);
          profiler_thread_sleep();
        });
  } else {
    if (mProfilingStackWhileWaiting) {
      mProfilingStackWhileWaiting->pop();
      mProfilingStackWhileWaiting = nullptr;
      profiler_thread_wake();
    }
  }
}

// Init
//
// Loads the nib (see bug 316076c21) and sets up the CFRunLoopSource used to
// interrupt the main native run loop.
//
// public
nsresult nsAppShell::Init() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // No event loop is running yet (unless an embedding app that uses
  // NSApplicationMain() is running).
  NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

  char* mozAppNoDock = PR_GetEnv("MOZ_APP_NO_DOCK");
  if (mozAppNoDock && strcmp(mozAppNoDock, "") != 0) {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
  }

  // mAutoreleasePools is used as a stack of NSAutoreleasePool objects created
  // by |this|.  CFArray is used instead of NSArray because NSArray wants to
  // retain each object you add to it, and you can't retain an
  // NSAutoreleasePool.
  mAutoreleasePools = ::CFArrayCreateMutable(nullptr, 0, nullptr);
  NS_ENSURE_STATE(mAutoreleasePools);

  bool isNSApplicationProcessType =
      (XRE_GetProcessType() != GeckoProcessType_RDD) &&
      (XRE_GetProcessType() != GeckoProcessType_Socket);

  if (isNSApplicationProcessType) {
    // This call initializes NSApplication unless:
    // 1) we're using xre -- NSApp's already been initialized by
    //    MacApplicationDelegate.mm's EnsureUseCocoaDockAPI().
    // 2) an embedding app that uses NSApplicationMain() is running -- NSApp's
    //    already been initialized and its main run loop is already running.
    [[NSBundle mainBundle] loadNibNamed:@"res/MainMenu"
                                  owner:[GeckoNSApplication sharedApplication]
                        topLevelObjects:nil];
  }

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

  // Add a CFRunLoopObserver so that the profiler can be notified when we enter
  // and exit the waiting state.
  CFRunLoopObserverContext observerContext;
  PodZero(&observerContext);
  observerContext.info = this;

  mCFRunLoopObserver = ::CFRunLoopObserverCreate(
      kCFAllocatorDefault,
      kCFRunLoopBeforeWaiting | kCFRunLoopAfterWaiting | kCFRunLoopExit, true,
      0, RunLoopObserverCallback, &observerContext);
  NS_ENSURE_STATE(mCFRunLoopObserver);

  ::CFRunLoopAddObserver(mCFRunLoop, mCFRunLoopObserver, kCFRunLoopCommonModes);

  hal::Init();

  if (XRE_IsParentProcess()) {
    ScreenManager& screenManager = ScreenManager::GetSingleton();

    if (gfxPlatform::IsHeadless()) {
      screenManager.SetHelper(mozilla::MakeUnique<HeadlessScreenHelper>());
    } else {
      screenManager.SetHelper(mozilla::MakeUnique<ScreenHelperCocoa>());
    }

    InitMemoryPressureObserver();
  }

  nsresult rv = nsBaseAppShell::Init();

  if (isNSApplicationProcessType && !gAppShellMethodsSwizzled) {
    // We should only replace the original terminate: method if we're not
    // running in a Cocoa embedder. See bug 604901.
    if (!mRunningCocoaEmbedded) {
      nsToolkit::SwizzleMethods([NSApplication class], @selector(terminate:),
                                @selector(nsAppShell_NSApplication_terminate:));
    }
    gAppShellMethodsSwizzled = true;
  }

#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
  if (Preferences::GetBool("security.sandbox.mac.track.violations", false)) {
    nsSandboxViolationSink::Start();
  }
#endif

  [localPool release];

  return rv;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
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
void nsAppShell::ProcessGeckoEvents(void* aInfo) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
  AUTO_PROFILER_LABEL("nsAppShell::ProcessGeckoEvents", OTHER);

  nsAppShell* self = static_cast<nsAppShell*>(aInfo);

  if (self->mRunningEventLoop) {
    self->mRunningEventLoop = false;

    // The run loop may be sleeping -- [NSRunLoop runMode:...]
    // won't return until it's given a reason to wake up.  Awaken it by
    // posting a bogus event.  There's no need to make the event
    // presentable.
    //
    // But _don't_ set windowNumber to '-1' -- that can lead to nasty
    // weirdness like bmo bug 397039 (a crash in [NSApp sendEvent:] on one of
    // these fake events, because the -1 has gotten changed into the number
    // of an actual NSWindow object, and that NSWindow object has just been
    // destroyed).  Setting windowNumber to '0' seems to work fine -- this
    // seems to prevent the OS from ever trying to associate our bogus event
    // with a particular NSWindow object.
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:NULL
                                         subtype:kEventSubtypeNone
                                           data1:0
                                           data2:0]
             atStart:NO];
    // Previously we used to send this second event regardless of
    // self->mRunningEventLoop. However, that was removed in bug 1690687 for
    // performance reasons. It is still needed for the mRunningEventLoop case
    // otherwise we'll get in a cycle of sending postEvent followed by the
    // DummyEvent inserted by nsBaseAppShell::OnProcessNextEvent. This second
    // event will cause the second call to AcquireFirstMatchingEventInQueue in
    // nsAppShell::ProcessNextNativeEvent to return true. Which makes
    // nsBaseAppShell::OnProcessNextEvent call
    // nsAppShell::ProcessNextNativeEvent again during which it will loop until
    // it sleeps because ProcessGeckoEvents() won't be called for the
    // DummyEvent.
    //
    // This is not a good approach and we should fix things up so that only
    // one postEvent is needed.
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
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

  if (self->mTerminated) {
    // Still needed to avoid crashes on quit in most Mochitests.
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:NULL
                                         subtype:kEventSubtypeNone
                                           data1:0
                                           data2:0]
             atStart:NO];
  }
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
    while (releaseCount-- > self->mNativeEventCallbackDepth) self->Release();
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

  NS_OBJC_END_TRY_IGNORE_BLOCK;
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
void nsAppShell::WillTerminate() {
  if (mTerminated) return;

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
void nsAppShell::ScheduleNativeEventCallback() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mTerminated) return;

  // Each AddRef() here is normally balanced by exactly one Release() in
  // ProcessGeckoEvents().  But there are exceptions, for which see
  // ProcessGeckoEvents() and Exit().
  NS_ADDREF_THIS();
  PR_ATOMIC_INCREMENT(&mNativeEventScheduledDepth);

  // This will invoke ProcessGeckoEvents on the main thread.
  ::CFRunLoopSourceSignal(mCFRunLoopSource);
  ::CFRunLoopWakeUp(mCFRunLoop);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
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
bool nsAppShell::ProcessNextNativeEvent(bool aMayWait) {
  bool moreEvents = false;

  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  bool eventProcessed = false;
  NSString* currentMode = nil;

  if (mTerminated) return false;

  // Do not call -[NSApplication nextEventMatchingMask:...] when we're trying to
  // close a native menu. Doing so could confuse the NSMenu's closing mechanism.
  // Instead, we try to unwind the stack as quickly as possible and return to
  // the parent event loop. At that point, native events will be processed.
  if (MOZMenuOpeningCoordinator.needToUnwindForMenuClosing) {
    return false;
  }

  bool wasRunningEventLoop = mRunningEventLoop;
  mRunningEventLoop = aMayWait;
  NSDate* waitUntil = nil;
  if (aMayWait) waitUntil = [NSDate distantFuture];

  NSRunLoop* currentRunLoop = [NSRunLoop currentRunLoop];

  EventQueueRef currentEventQueue = GetCurrentEventQueue();

  if (aMayWait) {
    mozilla::BackgroundHangMonitor().NotifyWait();
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

    if (aMayWait && [[GeckoNSApplication sharedApplication] didLaunch]) {
      currentMode = [currentRunLoop currentMode];
      if (!currentMode) currentMode = NSDefaultRunLoopMode;
      NSEvent* nextEvent = [NSApp nextEventMatchingMask:NSEventMaskAny
                                              untilDate:waitUntil
                                                 inMode:currentMode
                                                dequeue:YES];
      if (nextEvent) {
        mozilla::BackgroundHangMonitor().NotifyActivity();
        [NSApp sendEvent:nextEvent];
        eventProcessed = true;
      }
    } else {
      // In at least 10.15, AcquireFirstMatchingEventInQueue will move 1
      // CGEvent from the CGEvent queue into the Carbon event queue.
      // Unfortunately, once an event has been moved to the Carbon event queue
      // it's no longer a candidate for coalescing. This means that even if we
      // don't remove the event from the queue, just calling
      // AcquireFirstMatchingEventInQueue can cause behaviour change. Prior to
      // bug 1690687 landing, the event that we got from
      // AcquireFirstMatchingEventInQueue was often our own ApplicationDefined
      // event. However, once we stopped posting that event on every Gecko
      // event we're much more likely to get a CGEvent. When we have a high
      // amount of load on the main thread, we end up alternating between Gecko
      // events and native events.  Without CGEvent coalescing, the native
      // event events can accumulate in the Carbon event queue which will
      // manifest as laggy scrolling.
#if 1
      eventProcessed = false;
      break;
#else
      // AcquireFirstMatchingEventInQueue() doesn't spin the (native) event
      // loop, though it does queue up any newly available events from the
      // window server.
      EventRef currentEvent = AcquireFirstMatchingEventInQueue(
          currentEventQueue, 0, NULL, kEventQueueOptionsNone);
      if (!currentEvent) {
        continue;
      }
      EventAttributes attrs = GetEventAttributes(currentEvent);
      UInt32 eventKind = GetEventKind(currentEvent);
      UInt32 eventClass = GetEventClass(currentEvent);
      bool osCocoaEvent =
          ((eventClass == 'appl') || (eventClass == kEventClassAppleEvent) ||
           ((eventClass == 'cgs ') &&
            (eventKind != NSEventTypeApplicationDefined)));
      // If attrs is kEventAttributeUserEvent or kEventAttributeMonitored
      // (i.e. a user input event), we shouldn't process it here while
      // aMayWait is false.  Likewise if currentEvent will eventually be
      // turned into an OS-defined Cocoa event, or otherwise needs AppKit
      // processing.  Doing otherwise risks doing too much work here, and
      // preventing the event from being properly processed by the AppKit
      // framework.
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
      EventTargetRef eventDispatcherTarget = GetEventDispatcherTarget();
      SendEventToEventTarget(currentEvent, eventDispatcherTarget);
      // This call to ReleaseEvent() matches a call to RetainEvent() in
      // AcquireFirstMatchingEventInQueue() above.
      ReleaseEvent(currentEvent);
      eventProcessed = true;
#endif
    }
  } while (mRunningEventLoop);

  if (eventProcessed) {
    moreEvents =
        (AcquireFirstMatchingEventInQueue(currentEventQueue, 0, NULL,
                                          kEventQueueOptionsNone) != NULL);
  }

  mRunningEventLoop = wasRunningEventLoop;

  NS_OBJC_END_TRY_IGNORE_BLOCK;

  if (!moreEvents) {
    nsChildView::UpdateCurrentInputEventCount();
  }

  return moreEvents;
}

// Attempt to work around bug 1801419 by loading and initializing the
// SidecarCore private framework as the app shell starts up. This normally
// happens on demand, the first time any Cmd-key combination is pressed, and
// sometimes triggers crashes, caused by an Apple bug. We hope that doing it
// now, and somewhat more simply, will avoid the crashes. They happen
// (intermittently) when SidecarCore code tries to access C strings in special
// sections of its own __TEXT segment, and triggers fatal page faults (which
// is Apple's bug). Many of the C strings are part of the Objective-C class
// hierarchy (class names and so forth). We hope that adding them to this
// hierarchy will "pin" them in place -- so they'll rarely, if ever, be paged
// out again. Bug 1801419's crashes happen much more often on macOS 13
// (Ventura) than on other versions of macOS. So we only use this hack on
// macOS 13 and up.
static void PinSidecarCoreTextCStringSections() {
  if (!dlopen(
          "/System/Library/PrivateFrameworks/SidecarCore.framework/SidecarCore",
          RTLD_LAZY)) {
    return;
  }

  // Explicitly run the most basic part of the initialization code that
  // normally runs automatically on the first Cmd-key combination.
  Class displayManagerClass = NSClassFromString(@"SidecarDisplayManager");
  if ([displayManagerClass respondsToSelector:@selector(sharedManager)]) {
    id sharedManager =
        [displayManagerClass performSelector:@selector(sharedManager)];
    if ([sharedManager respondsToSelector:@selector(devices)]) {
      [sharedManager performSelector:@selector(devices)];
    }
  }
}

// Run
//
// Overrides the base class's Run() method to call [NSApp run] (which spins
// the native run loop until the application quits).  Since (unlike the base
// class's Run() method) we don't process any Gecko events here, they need
// to be processed elsewhere (in NativeEventCallback(), called from
// ProcessGeckoEvents()).
//
// Camino called [NSApp run] on its own (via NSApplicationMain()), and so
// didn't call nsAppShell::Run().
//
// public
NS_IMETHODIMP
nsAppShell::Run(void) {
  NS_ASSERTION(!mStarted, "nsAppShell::Run() called multiple times");
  if (mStarted || mTerminated) return NS_OK;

  mStarted = true;

  if (XRE_IsParentProcess()) {
    if (nsCocoaFeatures::OnVenturaOrLater()) {
      PinSidecarCoreTextCStringSections();
    }
    AddScreenWakeLockListener();
  }

  // We use the native Gecko event loop in content processes.
  nsresult rv = NS_OK;
  if (XRE_UseNativeEventProcessing()) {
    NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
    [NSApp run];
    NS_OBJC_END_TRY_IGNORE_BLOCK;
  } else {
    rv = nsBaseAppShell::Run();
  }

  if (XRE_IsParentProcess()) {
    RemoveScreenWakeLockListener();
  }

  return rv;
}

NS_IMETHODIMP
nsAppShell::Exit(void) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // This method is currently called more than once -- from (according to
  // mento) an nsAppExitEvent dispatched by nsAppStartup::Quit() and from an
  // XPCOM shutdown notification that nsBaseAppShell has registered to
  // receive.  So we need to ensure that multiple calls won't break anything.
  if (mTerminated) {
    return NS_OK;
  }

  mTerminated = true;

#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
  nsSandboxViolationSink::Stop();
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
  if (cocoaModal) [NSApp stop:nullptr];
  [NSApp stop:nullptr];

  // A call to Exit() just after a call to ScheduleNativeEventCallback()
  // prevents the (normally) matching call to ProcessGeckoEvents() from
  // happening.  If we've been called from ProcessGeckoEvents() (as usually
  // happens), we take care of it there.  But if we have an unbalanced call
  // to ScheduleNativeEventCallback() and ProcessGeckoEvents() isn't on the
  // stack, we need to take care of the problem here.
  if (!mNativeEventCallbackDepth && mNativeEventScheduledDepth) {
    int32_t releaseCount = PR_ATOMIC_SET(&mNativeEventScheduledDepth, 0);
    while (releaseCount-- > 0) NS_RELEASE_THIS();
  }

  return nsBaseAppShell::Exit();

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
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
nsAppShell::OnProcessNextEvent(nsIThreadInternal* aThread, bool aMayWait) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ASSERTION(mAutoreleasePools,
               "No stack on which to store autorelease pool");

  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  ::CFArrayAppendValue(mAutoreleasePools, pool);

  return nsBaseAppShell::OnProcessNextEvent(aThread, aMayWait);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// AfterProcessNextEvent
//
// This nsIThreadObserver method is called after event processing is complete.
// The Cocoa implementation cleans up the autorelease pool create by the
// previous OnProcessNextEvent call.
//
// public
NS_IMETHODIMP
nsAppShell::AfterProcessNextEvent(nsIThreadInternal* aThread,
                                  bool aEventWasProcessed) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  CFIndex count = ::CFArrayGetCount(mAutoreleasePools);

  NS_ASSERTION(mAutoreleasePools && count,
               "Processed an event, but there's no autorelease pool?");

  const NSAutoreleasePool* pool = static_cast<const NSAutoreleasePool*>(
      ::CFArrayGetValueAtIndex(mAutoreleasePools, count - 1));
  ::CFArrayRemoveValueAtIndex(mAutoreleasePools, count - 1);
  [pool release];

  return nsBaseAppShell::AfterProcessNextEvent(aThread, aEventWasProcessed);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsAppShell::InitMemoryPressureObserver() {
  // Testing shows that sometimes the memory pressure event is not fired for
  // over a minute after the memory pressure change is reflected in sysctl
  // values. Hence this may need to be augmented with polling of the memory
  // pressure sysctls for lower latency reactions to OS memory pressure. This
  // was also observed when using DISPATCH_QUEUE_PRIORITY_HIGH.
  mMemoryPressureSource = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0,
      DISPATCH_MEMORYPRESSURE_NORMAL | DISPATCH_MEMORYPRESSURE_WARN |
          DISPATCH_MEMORYPRESSURE_CRITICAL,
      dispatch_get_main_queue());

  dispatch_source_set_event_handler(mMemoryPressureSource, ^{
    dispatch_source_memorypressure_flags_t pressureLevel =
        dispatch_source_get_data(mMemoryPressureSource);
    nsAppShell::OnMemoryPressureChanged(pressureLevel);
  });

  dispatch_resume(mMemoryPressureSource);

  // Initialize the memory watcher.
  RefPtr<mozilla::nsAvailableMemoryWatcherBase> watcher(
      nsAvailableMemoryWatcherBase::GetSingleton());
}

void nsAppShell::OnMemoryPressureChanged(
    dispatch_source_memorypressure_flags_t aPressureLevel) {
  // The memory pressure dispatch source is created (above) with
  // dispatch_get_main_queue() which always fires on the main thread.
  MOZ_ASSERT(NS_IsMainThread());

  MacMemoryPressureLevel geckoPressureLevel;
  switch (aPressureLevel) {
    case DISPATCH_MEMORYPRESSURE_NORMAL:
      geckoPressureLevel = MacMemoryPressureLevel::Value::eNormal;
      break;
    case DISPATCH_MEMORYPRESSURE_WARN:
      geckoPressureLevel = MacMemoryPressureLevel::Value::eWarning;
      break;
    case DISPATCH_MEMORYPRESSURE_CRITICAL:
      geckoPressureLevel = MacMemoryPressureLevel::Value::eCritical;
      break;
    default:
      geckoPressureLevel = MacMemoryPressureLevel::Value::eUnexpected;
  }

  RefPtr<mozilla::nsAvailableMemoryWatcherBase> watcher(
      nsAvailableMemoryWatcherBase::GetSingleton());
  watcher->OnMemoryPressureChanged(geckoPressureLevel);
}

// AppShellDelegate implementation

@implementation AppShellDelegate
// initWithAppShell:
//
// Constructs the AppShellDelegate object
- (id)initWithAppShell:(nsAppShell*)aAppShell {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((self = [self init])) {
    mAppShell = aAppShell;

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillTerminate:)
               name:NSApplicationWillTerminateNotification
             object:NSApp];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidBecomeActive:)
               name:NSApplicationDidBecomeActiveNotification
             object:NSApp];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(timezoneChanged:)
               name:NSSystemTimeZoneDidChangeNotification
             object:nil];
  }

  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// applicationWillTerminate:
//
// Notify the nsAppShell that native event processing should be discontinued.
- (void)applicationWillTerminate:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  mAppShell->WillTerminate();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// applicationDidBecomeActive
//
// Make sure TextInputHandler::sLastModifierState is updated when we become
// active (since we won't have received [ChildView flagsChanged:] messages
// while inactive).
- (void)applicationDidBecomeActive:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // [NSEvent modifierFlags] is valid on every kind of event, so we don't need
  // to worry about getting an NSInternalInconsistencyException here.
  NSEvent* currentEvent = [NSApp currentEvent];
  if (currentEvent) {
    TextInputHandler::sLastModifierState =
        [currentEvent modifierFlags] &
        NSEventModifierFlagDeviceIndependentFlagsMask;
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(
        nullptr, NS_WIDGET_MAC_APP_ACTIVATE_OBSERVER_TOPIC, nullptr);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)timezoneChanged:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  nsBaseAppShell::OnSystemTimezoneChange();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
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
- (void)nsAppShell_NSApplication_terminate:(id)sender {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:NSApp];
}

@end
