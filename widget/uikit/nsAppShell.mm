/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIApplication.h>
#import <UIKit/UIScreen.h>
#import <UIKit/UIWindow.h>
#import <UIKit/UIViewController.h>

#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsIRollupListener.h"
#include "nsIWidget.h"
#include "nsThreadUtils.h"
#include "nsMemoryPressure.h"
#include "nsServiceManagerUtils.h"

nsAppShell* nsAppShell::gAppShell = NULL;
UIWindow* nsAppShell::gWindow = nil;
NSMutableArray* nsAppShell::gTopLevelViews = [[NSMutableArray alloc] init];

#define ALOG(args...)    \
  fprintf(stderr, args); \
  fprintf(stderr, "\n")

// ViewController
@interface ViewController : UIViewController
@end

@implementation ViewController

- (void)loadView {
  ALOG("[ViewController loadView]");
  CGRect r = {{0, 0}, {100, 100}};
  self.view = [[UIView alloc] initWithFrame:r];
  [self.view setBackgroundColor:[UIColor lightGrayColor]];
  // add all of the top level views as children
  for (UIView* v in nsAppShell::gTopLevelViews) {
    ALOG("[ViewController.view addSubView:%p]", v);
    [self.view addSubview:v];
  }
  [nsAppShell::gTopLevelViews release];
  nsAppShell::gTopLevelViews = nil;
}
@end

// AppShellDelegate
//
// Acts as a delegate for the UIApplication

@interface AppShellDelegate : NSObject <UIApplicationDelegate> {
}
@property(strong, nonatomic) UIWindow* window;
@end

@implementation AppShellDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  ALOG("[AppShellDelegate application:didFinishLaunchingWithOptions:]");
  // We only create one window, since we can only display one window at
  // a time anyway. Also, iOS 4 fails to display UIWindows if you
  // create them before calling UIApplicationMain, so this makes more sense.
  nsAppShell::gWindow = [[[UIWindow alloc]
      initWithFrame:[[UIScreen mainScreen] applicationFrame]] retain];
  self.window = nsAppShell::gWindow;

  self.window.rootViewController = [[ViewController alloc] init];

  // just to make things more visible for now
  nsAppShell::gWindow.backgroundColor = [UIColor blueColor];
  [nsAppShell::gWindow makeKeyAndVisible];

  return YES;
}

- (void)applicationWillTerminate:(UIApplication*)application {
  ALOG("[AppShellDelegate applicationWillTerminate:]");
  nsAppShell::gAppShell->WillTerminate();
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  ALOG("[AppShellDelegate applicationDidBecomeActive:]");
}

- (void)applicationWillResignActive:(UIApplication*)application {
  ALOG("[AppShellDelegate applicationWillResignActive:]");
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
  ALOG("[AppShellDelegate applicationDidReceiveMemoryWarning:]");
  NS_NotifyOfMemoryPressure(MemoryPressureState::LowMemory);
}
@end

// nsAppShell implementation

NS_IMETHODIMP
nsAppShell::ResumeNative(void) { return nsBaseAppShell::ResumeNative(); }

nsAppShell::nsAppShell()
    : mAutoreleasePool(NULL),
      mDelegate(NULL),
      mCFRunLoop(NULL),
      mCFRunLoopSource(NULL),
      mTerminated(false),
      mNotifiedWillTerminate(false) {
  gAppShell = this;
}

nsAppShell::~nsAppShell() {
  if (mAutoreleasePool) {
    [mAutoreleasePool release];
    mAutoreleasePool = NULL;
  }

  if (mCFRunLoop) {
    if (mCFRunLoopSource) {
      ::CFRunLoopRemoveSource(mCFRunLoop, mCFRunLoopSource,
                              kCFRunLoopCommonModes);
      ::CFRelease(mCFRunLoopSource);
    }
    ::CFRelease(mCFRunLoop);
  }

  gAppShell = NULL;
}

// Init
//
// public
nsresult nsAppShell::Init() {
  mAutoreleasePool = [[NSAutoreleasePool alloc] init];

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

  return nsBaseAppShell::Init();
}

// ProcessGeckoEvents
//
// The "perform" target of mCFRunLoop, called when mCFRunLoopSource is
// signalled from ScheduleNativeEventCallback.
//
// protected static
void nsAppShell::ProcessGeckoEvents(void* aInfo) {
  nsAppShell* self = static_cast<nsAppShell*>(aInfo);
  self->NativeEventCallback();
  self->Release();
}

// WillTerminate
//
// public
void nsAppShell::WillTerminate() {
  mNotifiedWillTerminate = true;
  if (mTerminated) return;
  mTerminated = true;
  // We won't get another chance to process events
  NS_ProcessPendingEvents(NS_GetCurrentThread());

  // Unless we call nsBaseAppShell::Exit() here, it might not get called
  // at all.
  nsBaseAppShell::Exit();
}

// ScheduleNativeEventCallback
//
// protected virtual
void nsAppShell::ScheduleNativeEventCallback() {
  if (mTerminated) return;

  NS_ADDREF_THIS();

  // This will invoke ProcessGeckoEvents on the main thread.
  ::CFRunLoopSourceSignal(mCFRunLoopSource);
  ::CFRunLoopWakeUp(mCFRunLoop);
}

// ProcessNextNativeEvent
//
// protected virtual
bool nsAppShell::ProcessNextNativeEvent(bool aMayWait) {
  if (mTerminated) return false;

  NSString* currentMode = nil;
  NSDate* waitUntil = nil;
  if (aMayWait) waitUntil = [NSDate distantFuture];
  NSRunLoop* currentRunLoop = [NSRunLoop currentRunLoop];

  BOOL eventProcessed = NO;
  do {
    currentMode = [currentRunLoop currentMode];
    if (!currentMode) currentMode = NSDefaultRunLoopMode;

    if (aMayWait)
      eventProcessed = [currentRunLoop runMode:currentMode
                                    beforeDate:waitUntil];
    else
      [currentRunLoop acceptInputForMode:currentMode beforeDate:waitUntil];
  } while (eventProcessed && aMayWait);

  return false;
}

// Run
//
// public
NS_IMETHODIMP
nsAppShell::Run(void) {
  ALOG("nsAppShell::Run");
  char argv[1][4] = {"app"};
  UIApplicationMain(1, (char**)argv, nil, @"AppShellDelegate");
  // UIApplicationMain doesn't exit. :-(
  return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Exit(void) {
  if (mTerminated) return NS_OK;

  mTerminated = true;
  return nsBaseAppShell::Exit();
}
