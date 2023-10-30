/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NSApplication delegate for Mac OS X Cocoa API.

// As of 10.4 Tiger, the system can send six kinds of Apple Events to an
// application; a well-behaved XUL app should have some kind of handling for all
// of them.
//
// See
// http://developer.apple.com/documentation/Cocoa/Conceptual/ScriptableCocoaApplications/SApps_handle_AEs/chapter_11_section_3.html
// for details.

#include <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#include "NativeMenuMac.h"
#import <Carbon/Carbon.h>

#include "CustomCocoaEvents.h"
#include "nsCOMPtr.h"
#include "nsINativeAppSupport.h"
#include "nsAppRunner.h"
#include "nsAppShell.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIAppStartup.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsObjCExceptions.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCommandLine.h"
#include "nsIMacDockSupport.h"
#include "nsIStandaloneNativeMenu.h"
#include "nsILocalFileMac.h"
#include "nsString.h"
#include "nsCommandLineServiceMac.h"
#include "nsCommandLine.h"
#include "nsStandaloneNativeMenu.h"
#include "nsCocoaUtils.h"

class AutoAutoreleasePool {
 public:
  AutoAutoreleasePool() { mLocalPool = [[NSAutoreleasePool alloc] init]; }
  ~AutoAutoreleasePool() { [mLocalPool release]; }

 private:
  NSAutoreleasePool* mLocalPool;
};

@interface MacApplicationDelegate : NSObject <NSApplicationDelegate> {
}

@end

enum class LaunchStatus {
  Initial,
  DelegateIsSetup,
  ProcessingURLs,
  ProcessedURLs
};

static LaunchStatus sLaunchStatus = LaunchStatus::Initial;

// Methods that can be called from non-Objective-C code.

// This is needed, on relaunch, to force the OS to use the "Cocoa Dock API"
// instead of the "Carbon Dock API".  For more info see bmo bug 377166.
void EnsureUseCocoaDockAPI() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [GeckoNSApplication sharedApplication];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void DisableAppNap() {
  // Prevent the parent process from entering App Nap. macOS does not put our
  // child processes into App Nap and, as a result, when the parent is in
  // App Nap, child processes continue to run normally generating IPC messages
  // for the parent which can end up being queued. This can cause the browser
  // to be unresponsive for a period of time after the App Nap until the parent
  // process "catches up." NSAppSleepDisabled has to be set early during
  // startup before the OS reads the value for the process.
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    @"NSAppSleepDisabled" : @YES,
  }];
}

void SetupMacApplicationDelegate(bool* gRestartedByOS) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // this is called during startup, outside an event loop, and therefore
  // needs an autorelease pool to avoid cocoa object leakage (bug 559075)
  AutoAutoreleasePool pool;

  // Ensure that ProcessPendingGetURLAppleEvents() doesn't regress bug 377166.
  [GeckoNSApplication sharedApplication];

  // This call makes it so that application:openFile: doesn't get bogus calls
  // from Cocoa doing its own parsing of the argument string. And yes, we need
  // to use a string with a boolean value in it. That's just how it works.
  [[NSUserDefaults standardUserDefaults]
      setObject:@"NO"
         forKey:@"NSTreatUnknownArgumentsAsOpen"];

  // Create the delegate. This should be around for the lifetime of the app.
  id<NSApplicationDelegate> delegate = [[MacApplicationDelegate alloc] init];
  [[GeckoNSApplication sharedApplication] setDelegate:delegate];

  *gRestartedByOS = !!nsCocoaUtils::ShouldRestoreStateDueToLaunchAtLogin();

  MOZ_ASSERT(
      sLaunchStatus == LaunchStatus::Initial,
      "Launch status should be in intial state when setting up delegate");
  sLaunchStatus = LaunchStatus::DelegateIsSetup;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Indirectly make the OS process any pending GetURL Apple events.  This is
// done via _DPSNextEvent() (an undocumented AppKit function called from
// [NSApplication nextEventMatchingMask:untilDate:inMode:dequeue:]).  Apple
// events are only processed if 'dequeue' is 'YES' -- so we need to call
// [NSApplication sendEvent:] on any event that gets returned.  'event' will
// never itself be an Apple event, and it may be 'nil' even when Apple events
// are processed.
void ProcessPendingGetURLAppleEvents() {
  if (sLaunchStatus != LaunchStatus::DelegateIsSetup) {
    // Delegate has not been set up or NSApp has been launched already.
    return;
  }

  sLaunchStatus = LaunchStatus::ProcessingURLs;
  [NSApp run];
  sLaunchStatus = LaunchStatus::ProcessedURLs;
}

@implementation MacApplicationDelegate

- (id)init {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((self = [super init])) {
    if (![NSApp windowsMenu]) {
      // If the application has a windows menu, it will keep it up to date and
      // prepend the window list to the Dock menu automatically.
      NSMenu* windowsMenu = [[NSMenu alloc] initWithTitle:@"Window"];
      [NSApp setWindowsMenu:windowsMenu];
      [windowsMenu release];
    }
  }
  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

// The method that NSApplication calls upon a request to reopen, such as when
// the Dock icon is clicked and no windows are open. A "visible" window may be
// miniaturized, so we can't skip nsCocoaNativeReOpen() if 'flag' is 'true'.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApp
                    hasVisibleWindows:(BOOL)flag {
  nsCOMPtr<nsINativeAppSupport> nas = NS_GetNativeAppSupport();
  NS_ENSURE_TRUE(nas, NO);

  // Go to the common Carbon/Cocoa reopen method.
  nsresult rv = nas->ReOpen();
  NS_ENSURE_SUCCESS(rv, NO);

  // NO says we don't want NSApplication to do anything else for us.
  return NO;
}

// The method that NSApplication calls when documents are requested to be
// printed from the Finder (under the "File" menu). It will be called once for
// each selected document.
- (BOOL)application:(NSApplication*)theApplication
          printFile:(NSString*)filename {
  return NO;
}

// The method that NSApplication calls for using secure state restoration.
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}

// Create the menu that shows up in the Dock.
- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Create the NSMenu that will contain the dock menu items.
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
  [menu setAutoenablesItems:NO];

  // Add application-specific dock menu items. On error, do not insert the
  // dock menu items.
  nsresult rv;
  nsCOMPtr<nsIMacDockSupport> dockSupport =
      do_GetService("@mozilla.org/widget/macdocksupport;1", &rv);
  if (NS_FAILED(rv) || !dockSupport) return menu;

  nsCOMPtr<nsIStandaloneNativeMenu> dockMenuInterface;
  rv = dockSupport->GetDockMenu(getter_AddRefs(dockMenuInterface));
  if (NS_FAILED(rv) || !dockMenuInterface) return menu;

  RefPtr<mozilla::widget::NativeMenuMac> dockMenu =
      static_cast<nsStandaloneNativeMenu*>(dockMenuInterface.get())
          ->GetNativeMenu();

  // Give the menu the opportunity to update itself before display.
  dockMenu->MenuWillOpen();

  // Obtain a copy of the native menu.
  NSMenu* nativeDockMenu = dockMenu->NativeNSMenu();
  if (!nativeDockMenu) {
    return menu;
  }

  // Loop through the application-specific dock menu and insert its
  // contents into the dock menu that we are building for Cocoa.
  int numDockMenuItems = [nativeDockMenu numberOfItems];
  if (numDockMenuItems > 0) {
    if ([menu numberOfItems] > 0) [menu addItem:[NSMenuItem separatorItem]];

    for (int i = 0; i < numDockMenuItems; i++) {
      NSMenuItem* itemCopy = [[nativeDockMenu itemAtIndex:i] copy];
      [menu addItem:itemCopy];
      [itemCopy release];
    }
  }

  return menu;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)applicationWillFinishLaunching:(NSNotification*)notification {
  // We provide our own full screen menu item, so we don't want the OS providing
  // one as well.
  [[NSUserDefaults standardUserDefaults]
      setBool:NO
       forKey:@"NSFullScreenMenuItemEverywhere"];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  if (sLaunchStatus == LaunchStatus::ProcessingURLs) {
    // We are in an inner `run` loop that we are spinning in order to get
    // URLs that were requested while launching. `application:openURLs:` will
    // have been called by this point and we will have finished reconstructing
    // the command line. We now stop the app loop for the rest of startup to be
    // processed and will call `run` again when the main event loop should
    // start.
    [NSApp stop:self];

    // Send a bogus event so that the internal "app stopped" flag is processed.
    // Since we aren't calling this from a responder, we need to post an event
    // to have the loop iterate and respond to the stopped flag.
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
}

// If we don't handle applicationShouldTerminate:, a call to [NSApp terminate:]
// (from the browser or from the OS) can result in an unclean shutdown.
- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication*)sender {
  nsCOMPtr<nsIObserverService> obsServ =
      do_GetService("@mozilla.org/observer-service;1");
  if (!obsServ) return NSTerminateNow;

  nsCOMPtr<nsISupportsPRBool> cancelQuit =
      do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
  if (!cancelQuit) return NSTerminateNow;

  cancelQuit->SetData(false);
  obsServ->NotifyObservers(cancelQuit, "quit-application-requested", nullptr);

  bool abortQuit;
  cancelQuit->GetData(&abortQuit);
  if (abortQuit) return NSTerminateCancel;

  nsCOMPtr<nsIAppStartup> appService =
      do_GetService("@mozilla.org/toolkit/app-startup;1");
  if (appService) {
    bool userAllowedQuit = true;
    appService->Quit(nsIAppStartup::eForceQuit, 0, &userAllowedQuit);
    if (!userAllowedQuit) {
      return NSTerminateCancel;
    }
  }

  return NSTerminateNow;
}

- (void)application:(NSApplication*)application
           openURLs:(NSArray<NSURL*>*)urls {
  [self openURLs:urls];
}

- (BOOL)application:(NSApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  return [userActivityType isEqualToString:NSUserActivityTypeBrowsingWeb];
}

- (BOOL)application:(NSApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:
          (void (^)(NSArray<id<NSUserActivityRestoring>>*))restorationHandler {
  if (![userActivity.activityType
          isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    return NO;
  }

  return [self openURLs:@[ userActivity.webpageURL ]];
}

- (void)application:(NSApplication*)application
    didFailToContinueUserActivityWithType:(NSString*)userActivityType
                                    error:(NSError*)error {
  NSLog(@"Failed to continue user activity %@: %@", userActivityType, error);
}

- (BOOL)openURLs:(NSArray<NSURL*>*)urls {
  nsTArray<const char*> args([urls count] * 2 + 2);
  // Placeholder for unused program name.
  args.AppendElement(nullptr);

  for (NSURL* url in urls) {
    if (!url || !url.scheme ||
        [url.scheme caseInsensitiveCompare:@"chrome"] == NSOrderedSame) {
      continue;
    }

    const char* const urlString = [[url absoluteString] UTF8String];
    // Add the URL to any command line we're currently setting up.
    if (CommandLineServiceMac::AddURLToCurrentCommandLine(urlString)) {
      continue;
    }

    args.AppendElement("-url");
    args.AppendElement(urlString);
  }

  if (args.Length() <= 1) {
    // No URLs were added to the command line.
    return NO;
  }

  nsCOMPtr<nsICommandLineRunner> cmdLine(new nsCommandLine());
  nsCOMPtr<nsIFile> workingDir;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR,
                                       getter_AddRefs(workingDir));
  if (NS_FAILED(rv)) {
    // Couldn't find a working dir. Uh oh. Good job cmdline::Init can cope.
    workingDir = nullptr;
  }

  rv = cmdLine->Init(args.Length(), args.Elements(), workingDir,
                     nsICommandLine::STATE_REMOTE_EXPLICIT);
  if (NS_FAILED(rv)) {
    return NO;
  }
  rv = cmdLine->Run();
  if (NS_FAILED(rv)) {
    return NO;
  }

  return YES;
}

@end
