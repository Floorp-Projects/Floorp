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
#include <dlfcn.h>

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
#include "nsCocoaUtils.h"
#include "nsChildView.h"
#include "nsToolkit.h"

#include "npapi.h"

// defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;
extern PRUint32          gLastModifierState;

// defined in nsCocoaWindow.mm
extern PRInt32             gXULModalLevel;

static PRBool gAppShellMethodsSwizzled = PR_FALSE;
// List of current Cocoa app-modal windows (nested if more than one).
nsCocoaAppModalWindowList *gCocoaAppModalWindowList = NULL;

// Push a Cocoa app-modal window onto the top of our list.
nsresult nsCocoaAppModalWindowList::PushCocoa(NSWindow *aWindow, NSModalSession aSession)
{
  NS_ENSURE_STATE(aWindow && aSession);
  mList.AppendElement(nsCocoaAppModalWindowListItem(aWindow, aSession));
  return NS_OK;
}

// Pop the topmost Cocoa app-modal window off our list.  aWindow and aSession
// are just used to check that it's what we expect it to be.
nsresult nsCocoaAppModalWindowList::PopCocoa(NSWindow *aWindow, NSModalSession aSession)
{
  NS_ENSURE_STATE(aWindow && aSession);

  for (int i = mList.Length(); i > 0; --i) {
    nsCocoaAppModalWindowListItem &item = mList.ElementAt(i - 1);
    if (item.mSession) {
      NS_ASSERTION((item.mWindow == aWindow) && (item.mSession == aSession),
                   "PopCocoa() called without matching call to PushCocoa()!");
      mList.RemoveElementAt(i - 1);
      return NS_OK;
    }
  }

  NS_ERROR("PopCocoa() called without matching call to PushCocoa()!");
  return NS_ERROR_FAILURE;
}

// Push a Gecko-modal window onto the top of our list.
nsresult nsCocoaAppModalWindowList::PushGecko(NSWindow *aWindow, nsCocoaWindow *aWidget)
{
  NS_ENSURE_STATE(aWindow && aWidget);
  mList.AppendElement(nsCocoaAppModalWindowListItem(aWindow, aWidget));
  return NS_OK;
}

// Pop the topmost Gecko-modal window off our list.  aWindow and aWidget are
// just used to check that it's what we expect it to be.
nsresult nsCocoaAppModalWindowList::PopGecko(NSWindow *aWindow, nsCocoaWindow *aWidget)
{
  NS_ENSURE_STATE(aWindow && aWidget);

  for (int i = mList.Length(); i > 0; --i) {
    nsCocoaAppModalWindowListItem &item = mList.ElementAt(i - 1);
    if (item.mWidget) {
      NS_ASSERTION((item.mWindow == aWindow) && (item.mWidget == aWidget),
                   "PopGecko() called without matching call to PushGecko()!");
      mList.RemoveElementAt(i - 1);
      return NS_OK;
    }
  }

  NS_ERROR("PopGecko() called without matching call to PushGecko()!");
  return NS_ERROR_FAILURE;
}

// The "current session" is normally the "session" corresponding to the
// top-most Cocoa app-modal window (both on the screen and in our list).
// But because Cocoa app-modal dialog can be "interrupted" by a Gecko-modal
// dialog, the top-most Cocoa app-modal dialog may already have finished
// (and no longer be visible).  In this case we need to check the list for
// the "next" visible Cocoa app-modal window (and return its "session"), or
// (if no Cocoa app-modal window is visible) return nil.  This way we ensure
// (as we need to) that all nested Cocoa app-modal sessions are dealt with
// before we get to any Gecko-modal session(s).  See nsAppShell::
// ProcessNextNativeEvent() below.
NSModalSession nsCocoaAppModalWindowList::CurrentSession()
{
  if (![NSApp _isRunningAppModal])
    return nil;

  NSModalSession currentSession = nil;

  for (int i = mList.Length(); i > 0; --i) {
    nsCocoaAppModalWindowListItem &item = mList.ElementAt(i - 1);
    if (item.mSession && [item.mWindow isVisible]) {
      currentSession = item.mSession;
      break;
    }
  }

  return currentSession;
}

// Has a Gecko modal dialog popped up over a Cocoa app-modal dialog?
PRBool nsCocoaAppModalWindowList::GeckoModalAboveCocoaModal()
{
  if (mList.IsEmpty())
    return PR_FALSE;

  nsCocoaAppModalWindowListItem &topItem = mList.ElementAt(mList.Length() - 1);

  return (topItem.mWidget != nsnull);
}

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
, mSkippedNativeCallback(PR_FALSE)
, mHadMoreEventsCount(0)
, mRecursionDepth(0)
, mNativeEventCallbackDepth(0)
, mNativeEventScheduledDepth(0)
{
  // A Cocoa event loop is running here if (and only if) we've been embedded
  // by a Cocoa app (like Camino).
  mRunningCocoaEmbedded = [NSApp isRunning] ? PR_TRUE : PR_FALSE;
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

#ifndef NP_NO_CARBON
  NS_InstallPluginKeyEventsHandler();
#endif

  gCocoaAppModalWindowList = new nsCocoaAppModalWindowList;
  if (!gAppShellMethodsSwizzled) {
    nsToolkit::SwizzleMethods([NSApplication class], @selector(beginModalSessionForWindow:),
                              @selector(nsAppShell_NSApplication_beginModalSessionForWindow:));
    nsToolkit::SwizzleMethods([NSApplication class], @selector(endModalSession:),
                              @selector(nsAppShell_NSApplication_endModalSession:));
    // We should only replace the original terminate: method if we're not
    // running in a Cocoa embedder (like Camino).  See bug 604901.
    if (!mRunningCocoaEmbedded) {
      nsToolkit::SwizzleMethods([NSApplication class], @selector(terminate:),
                                @selector(nsAppShell_NSApplication_terminate:));
    }
    if (!nsToolkit::OnSnowLeopardOrLater()) {
      dlopen("/System/Library/Frameworks/Carbon.framework/Frameworks/Print.framework/Versions/Current/Plugins/PrintCocoaUI.bundle/Contents/MacOS/PrintCocoaUI",
             RTLD_LAZY);
      Class PDEPluginCallbackClass = ::NSClassFromString(@"PDEPluginCallback");
      nsresult rv1 = nsToolkit::SwizzleMethods(PDEPluginCallbackClass, @selector(initWithPrintWindowController:),
                                               @selector(nsAppShell_PDEPluginCallback_initWithPrintWindowController:));
      if (NS_SUCCEEDED(rv1)) {
        nsToolkit::SwizzleMethods(PDEPluginCallbackClass, @selector(dealloc),
                                  @selector(nsAppShell_PDEPluginCallback_dealloc));
      }
    }
    gAppShellMethodsSwizzled = PR_TRUE;
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

  // Normally every call to ScheduleNativeEventCallback() results in
  // exactly one call to ProcessGeckoEvents().  So each Release() here
  // normally balances exactly one AddRef() in ScheduleNativeEventCallback().
  // But if Exit() is called just after ScheduleNativeEventCallback(), the
  // corresponding call to ProcessGeckoEvents() will never happen.  We check
  // for this possibility in two different places -- here and in Exit()
  // itself.  If we find here that Exit() has been called (that mTerminated
  // is PR_TRUE), it's because we've been called recursively, that Exit() was
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
    PRInt32 releaseCount = 0;
    if (self->mNativeEventScheduledDepth > self->mNativeEventCallbackDepth) {
      releaseCount = PR_AtomicSet(&self->mNativeEventScheduledDepth,
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
    if (PR_AtomicDecrement(&self->mNativeEventScheduledDepth) < 0) {
      PR_AtomicSet(&self->mNativeEventScheduledDepth, 0);
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

  mTerminated = PR_TRUE;
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
  PR_AtomicIncrement(&mNativeEventScheduledDepth);

  // This will invoke ProcessGeckoEvents on the main thread.
  ::CFRunLoopSourceSignal(mCFRunLoopSource);
  ::CFRunLoopWakeUp(mCFRunLoop);

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PRBool eventProcessed = PR_FALSE;
  NSString* currentMode = nil;

  if (mTerminated)
    return PR_FALSE;

  // We don't want any native events to be processed here (via Gecko) while
  // Cocoa is displaying an app-modal dialog (as opposed to a window-modal
  // "sheet" or a Gecko-modal dialog).  Otherwise Cocoa event-processing loops
  // may be interrupted, and inappropriate events may get through to the
  // browser window(s) underneath.  This resolves bmo bugs 419668 and 420967.
  //
  // But we need more complex handling (we need to make an exception) if a
  // Gecko modal dialog is running above the Cocoa app-modal dialog -- for
  // which see below.
  if ([NSApp _isRunningAppModal] &&
      (!gCocoaAppModalWindowList || !gCocoaAppModalWindowList->GeckoModalAboveCocoaModal()))
    return PR_FALSE;

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
      if ((nextEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
                                          untilDate:waitUntil
                                             inMode:currentMode
                                            dequeue:YES])) {
        // If we're in a Cocoa app-modal session that's been interrupted by a
        // Gecko-modal dialog, send the event to the Cocoa app-modal dialog's
        // session.  This ensures that the app-modal session won't be starved
        // of events, and fixes bugs 463473 and 442442.  (The case of an
        // ordinary Cocoa app-modal dialog has been dealt with above.)
        //
        // Otherwise (if we're in an ordinary Gecko-modal dialog, or if we're
        // otherwise not in a Gecko main event loop), process the event as
        // expected.
        NSModalSession currentAppModalSession = nil;
        if (gCocoaAppModalWindowList)
          currentAppModalSession = gCocoaAppModalWindowList->CurrentSession();
        if (currentAppModalSession) {
          [NSApp _modalSession:currentAppModalSession sendEvent:nextEvent];
        } else {
          [NSApp sendEvent:nextEvent];
        }
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

  NS_OBJC_END_TRY_ABORT_BLOCK;

  if (!moreEvents) {
    nsChildView::UpdateCurrentInputEventCount();
  }

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
  NS_OBJC_TRY_ABORT([NSApp run]);

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

  mTerminated = PR_TRUE;

  delete gCocoaAppModalWindowList;
  gCocoaAppModalWindowList = NULL;

#ifndef NP_NO_CARBON
  NS_RemovePluginKeyEventsHandler();
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
    [NSApp stop:nsnull];
  [NSApp stop:nsnull];

  // A call to Exit() just after a call to ScheduleNativeEventCallback()
  // prevents the (normally) matching call to ProcessGeckoEvents() from
  // happening.  If we've been called from ProcessGeckoEvents() (as usually
  // happens), we take care of it there.  But if we have an unbalanced call
  // to ScheduleNativeEventCallback() and ProcessGeckoEvents() isn't on the
  // stack, we need to take care of the problem here.
  if (!mNativeEventCallbackDepth && mNativeEventScheduledDepth) {
    PRInt32 releaseCount = PR_AtomicSet(&mNativeEventScheduledDepth, 0);
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
nsAppShell::OnProcessNextEvent(nsIThreadInternal *aThread, PRBool aMayWait,
                               PRUint32 aRecursionDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mRecursionDepth = aRecursionDepth;

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
                                  PRUint32 aRecursionDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mRecursionDepth = aRecursionDepth;

  CFIndex count = ::CFArrayGetCount(mAutoreleasePools);

  NS_ASSERTION(mAutoreleasePools && count,
               "Processed an event, but there's no autorelease pool?");

  const NSAutoreleasePool* pool = static_cast<const NSAutoreleasePool*>
    (::CFArrayGetValueAtIndex(mAutoreleasePools, count - 1));
  ::CFArrayRemoveValueAtIndex(mAutoreleasePools, count - 1);
  [pool release];

  return nsBaseAppShell::AfterProcessNextEvent(aThread, aRecursionDepth);

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
// Make sure gLastModifierState is updated when we become active (since we
// won't have received [ChildView flagsChanged:] messages while inactive).
- (void)applicationDidBecomeActive:(NSNotification*)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // [NSEvent modifierFlags] is valid on every kind of event, so we don't need
  // to worry about getting an NSInternalInconsistencyException here.
  NSEvent* currentEvent = [NSApp currentEvent];
  if (currentEvent) {
    gLastModifierState = [currentEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
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
    if (gRollupListener && gRollupWidget)
      gRollupListener->Rollup(nsnull, nsnull);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

// We hook beginModalSessionForWindow: and endModalSession: in order to
// maintain a list of Cocoa app-modal windows (and the "sessions" to which
// they correspond).  We need this in order to deal with the consequences
// of a Cocoa app-modal dialog being "interrupted" by a Gecko-modal dialog.
// See nsCocoaAppModalWindowList::CurrentSession() and
// nsAppShell::ProcessNextNativeEvent() above.
//
// We hook terminate: in order to make OS-initiated termination work nicely
// with Gecko's shutdown sequence.  (Two ways to trigger OS-initiated
// termination:  1) Quit from the Dock menu; 2) Log out from (or shut down)
// your computer while the browser is active.)
@interface NSApplication (MethodSwizzling)
- (NSModalSession)nsAppShell_NSApplication_beginModalSessionForWindow:(NSWindow *)aWindow;
- (void)nsAppShell_NSApplication_endModalSession:(NSModalSession)aSession;
- (void)nsAppShell_NSApplication_terminate:(id)sender;
@end

@implementation NSApplication (MethodSwizzling)

// Called if and only if a Cocoa app-modal session is beginning.  Always call
// gCocoaAppModalWindowList->PushCocoa() here (if gCocoaAppModalWindowList is
// non-nil).
- (NSModalSession)nsAppShell_NSApplication_beginModalSessionForWindow:(NSWindow *)aWindow
{
  NSModalSession session =
    [self nsAppShell_NSApplication_beginModalSessionForWindow:aWindow];
  if (gCocoaAppModalWindowList)
    gCocoaAppModalWindowList->PushCocoa(aWindow, session);
  return session;
}

// Called to end any Cocoa modal session (app-modal or otherwise).  Only call
// gCocoaAppModalWindowList->PopCocoa() when an app-modal session is ending
// (and when gCocoaAppModalWindowList is non-nil).
- (void)nsAppShell_NSApplication_endModalSession:(NSModalSession)aSession
{
  BOOL wasRunningAppModal = [NSApp _isRunningAppModal];
  NSWindow *prevAppModalWindow = [NSApp modalWindow];
  [self nsAppShell_NSApplication_endModalSession:aSession];
  if (gCocoaAppModalWindowList &&
      wasRunningAppModal && (prevAppModalWindow != [NSApp modalWindow]))
    gCocoaAppModalWindowList->PopCocoa(prevAppModalWindow, aSession);
}

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

@interface NSObject (PDEPluginCallbackMethodSwizzling)
- (id)nsAppShell_PDEPluginCallback_initWithPrintWindowController:(id)controller;
- (void)nsAppShell_PDEPluginCallback_dealloc;
@end

@implementation NSObject (PDEPluginCallbackMethodSwizzling)

// On Leopard, the PDEPluginCallback class in Apple's PrintCocoaUI module
// fails to retain and release its PMPrintWindowController object.  This
// causes the PMPrintWindowController to sometimes be deleted prematurely,
// leading to crashes on attempts to access it.  One example is bug 396680,
// caused by attempting to call a deleted PMPrintWindowController object's
// printSettings method.  We work around the problem by hooking the
// appropriate methods and retaining and releasing the object ourselves.
// PrintCocoaUI.bundle is a "plugin" of the Carbon framework's Print
// framework.

- (id)nsAppShell_PDEPluginCallback_initWithPrintWindowController:(id)controller
{
  return [self nsAppShell_PDEPluginCallback_initWithPrintWindowController:[controller retain]];
}

- (void)nsAppShell_PDEPluginCallback_dealloc
{
  // Since the PDEPluginCallback class is undocumented (and the OS header
  // files have no definition for it), we need to use low-level methods to
  // access its _printWindowController variable.  (object_getInstanceVariable()
  // is also available in Objective-C 2.0, so this code is 64-bit safe.)
  id _printWindowController = nil;
  object_getInstanceVariable(self, "_printWindowController",
                             (void **) &_printWindowController);
  [_printWindowController release];
  [self nsAppShell_PDEPluginCallback_dealloc];
}

@end
