/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"

#import <CoreServices/CoreServices.h>
#import <Cocoa/Cocoa.h>

#include "nsCOMPtr.h"
#include "nsCocoaFeatures.h"
#include "nsNativeAppSupportBase.h"

#include "nsIAppShellService.h"
#include "nsIAppStartup.h"
#include "nsIBaseWindow.h"
#include "nsCommandLine.h"
#include "mozIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsIServiceManager.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"

// This must be included last:
#include "nsObjCExceptions.h"

nsresult GetNativeWindowPointerFromDOMWindow(mozIDOMWindowProxy* a_window,
                                             NSWindow** a_nativeWindow) {
  *a_nativeWindow = nil;
  if (!a_window) return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIWebNavigation> mruWebNav(do_GetInterface(a_window));
  if (mruWebNav) {
    nsCOMPtr<nsIDocShellTreeItem> mruTreeItem(do_QueryInterface(mruWebNav));
    nsCOMPtr<nsIDocShellTreeOwner> mruTreeOwner = nullptr;
    mruTreeItem->GetTreeOwner(getter_AddRefs(mruTreeOwner));
    if (mruTreeOwner) {
      nsCOMPtr<nsIBaseWindow> mruBaseWindow(do_QueryInterface(mruTreeOwner));
      if (mruBaseWindow) {
        nsCOMPtr<nsIWidget> mruWidget = nullptr;
        mruBaseWindow->GetMainWidget(getter_AddRefs(mruWidget));
        if (mruWidget) {
          *a_nativeWindow = (NSWindow*)mruWidget->GetNativeData(NS_NATIVE_WINDOW);
        }
      }
    }
  }

  return NS_OK;
}

// Essentially this notification handler implements the
// "Mozilla remote" functionality on Mac, handing command line arguments (passed
// to a newly launched process) to another copy of the current process that was
// already running (which had registered the handler for this notification). All
// other new copies just broadcast this notification and quit (unless -no-remote
// was specified in either of these processes), making the original process handle
// the arguments passed to this handler.
void remoteClientNotificationCallback(CFNotificationCenterRef aCenter, void* aObserver,
                                      CFStringRef aName, const void* aObject,
                                      CFDictionaryRef aUserInfo) {
  // Autorelease pool to prevent memory leaks, in case there is no outer pool.
  mozilla::MacAutoreleasePool pool;
  NSDictionary* userInfoDict = (__bridge NSDictionary*)aUserInfo;
  if (userInfoDict && [userInfoDict objectForKey:@"commandLineArgs"] &&
      [userInfoDict objectForKey:@"senderPath"]) {
    NSString* senderPath = [userInfoDict objectForKey:@"senderPath"];
    if (![senderPath isEqual:[[NSBundle mainBundle] bundlePath]]) {
      // The caller is not the process at the same path as we are at. Skipping.
      return;
    }

    NSArray* args = [userInfoDict objectForKey:@"commandLineArgs"];
    nsCOMPtr<nsICommandLineRunner> cmdLine(new nsCommandLine());

    // Converting Objective-C array into a C array,
    // which nsICommandLineRunner understands.
    int argc = [args count];
    const char** argv = new const char*[argc];
    for (int i = 0; i < argc; i++) {
      const char* arg = [[args objectAtIndex:i] UTF8String];
      argv[i] = arg;
    }

    // We're not currently passing the working dir as third argument because it
    // does not appear to be required.
    nsresult rv = cmdLine->Init(argc, argv, nullptr, nsICommandLine::STATE_REMOTE_AUTO);

    // Cleaning up C array.
    delete[] argv;

    if (NS_FAILED(rv)) {
      NS_ERROR("Error initializing command line.");
      return;
    }

    // Processing the command line, passed from a remote instance
    // in the current instance.
    cmdLine->Run();

    // And bring the app's window to front.
    [[NSRunningApplication currentApplication]
        activateWithOptions:NSApplicationActivateIgnoringOtherApps];
  }
}

class nsNativeAppSupportCocoa : public nsNativeAppSupportBase {
 public:
  nsNativeAppSupportCocoa() : mCanShowUI(false) {}

  NS_IMETHOD Start(bool* aRetVal) override;
  NS_IMETHOD ReOpen() override;
  NS_IMETHOD Enable() override;

 private:
  bool mCanShowUI;
};

NS_IMETHODIMP
nsNativeAppSupportCocoa::Enable() {
  mCanShowUI = true;
  return NS_OK;
}

NS_IMETHODIMP nsNativeAppSupportCocoa::Start(bool* _retval) {
  int major, minor, bugfix;
  nsCocoaFeatures::GetSystemVersion(major, minor, bugfix);

  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Check that the OS version is supported, if not return false,
  // which will make the browser quit.  In principle we could display an
  // alert here.  But the alert's message and buttons would require custom
  // localization.  So (for now at least) we just log an English message
  // to the console before quitting.
  if (major < 10 || minor < 6) {
    NSLog(@"Minimum OS version requirement not met!");
    return NS_OK;
  }

  *_retval = true;

  // Here are the "special" CLI arguments that we can expect to be passed that
  // should alter the default "hand args list to remote process and quit" algorithm:
  // -headless : was already handled on macOS (allowing running multiple instances
  // of the app), meaning this patch shouldn't break it.
  // -no-remote : should always proceed, creating a second instance (which will
  // fail on macOS, showing a MessageBox "Only one instance can be run at a time",
  // unless a different profile dir path is specified).
  // The rest of the arguments should be either passed on to
  // the original running process (exiting the current process), or be processed by
  // the current process (if -no-remote is specified).

  mozilla::MacAutoreleasePool pool;

  NSArray* arguments = [[NSProcessInfo processInfo] arguments];
  BOOL shallProceedLikeNoRemote = NO;
  for (NSString* arg in arguments) {
    if ([arg isEqualToString:@"-no-remote"] || [arg isEqualToString:@"--no-remote"] ||
        [arg isEqualToString:@"-headless"] || [arg isEqualToString:@"--headless"] ||
        [arg isEqualToString:@"-createProfile"] || [arg isEqualToString:@"--createProfile"]) {
      shallProceedLikeNoRemote = YES;
      break;
    }
  }

  BOOL mozillaRestarting = NO;
  if ([[[[NSProcessInfo processInfo] environment] objectForKey:@"MOZ_APP_RESTART"]
          isEqualToString:@"1"]) {
    // Update process completed or restarting the app for another reason.
    // Triggered by an old instance that just quit.
    mozillaRestarting = YES;
  }

  // Apart from -no-remote, the user can specify an env variable
  // MOZ_NO_REMOTE=1, which makes it behave the same way.
  // Also, to make sure the tests do not break,
  // if env var MOZ_TEST_PROCESS_UPDATES is present, it means the test is running.
  // We should proceed as if -no-remote had been specified.
  if (shallProceedLikeNoRemote == NO) {
    NSDictionary* environmentVariables = [[NSProcessInfo processInfo] environment];
    for (NSString* key in [environmentVariables allKeys]) {
      if ([key isEqualToString:@"MOZ_NO_REMOTE"] &&
          [environmentVariables[key] isEqualToString:@"1"]) {
        shallProceedLikeNoRemote = YES;
        break;
      }
    }
  }

  // Now that we have handled no-remote-like arguments, at this point:
  // 1) Either only the first instance of the process has been launched in any way
  //    (.app double click, "open", "open -n", invoking executable in Terminal, etc.
  // 2) Or the process has been launched with a "macos single instance" mechanism
  //    override (using "open -n" OR directly by invoking the executable in Terminal
  //    instead of clicking the .app bundle's icon, etc.).

  // So, let's check if this is the first instance ever of the process for the
  // current user.
  NSString* notificationName = [[[NSBundle mainBundle] bundleIdentifier]
      stringByAppendingString:@".distributedNotification.commandLineArgs"];

  BOOL runningInstanceFound = NO;
  if (!shallProceedLikeNoRemote) {
    // We check for other running instances only if -no-remote was not specified.
    // The check is needed so the marAppApplyUpdateSuccess.js test doesn't fail on next call.
    NSArray* appsWithMatchingId = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]];
    NSString* currentAppBundlePath = [[NSBundle mainBundle] bundlePath];
    NSRunningApplication* currentApp = [NSRunningApplication currentApplication];
    for (NSRunningApplication* app in appsWithMatchingId) {
      if ([currentAppBundlePath isEqual:[[app bundleURL] path]] && ![currentApp isEqual:app]) {
        runningInstanceFound = YES;
        break;
      }
    }
  }

  if (!shallProceedLikeNoRemote && !mozillaRestarting && runningInstanceFound) {
    // There is another instance of this app already running!
    NSArray* arguments = [[NSProcessInfo processInfo] arguments];
    NSString* senderPath = [[NSBundle mainBundle] bundlePath];
    CFDictionaryRef userInfoDict =
        (__bridge CFDictionaryRef) @{@"commandLineArgs" : arguments, @"senderPath" : senderPath};

    // This code is shared between Firefox, Thunderbird and other Mozilla products.
    // So we need a notification name that is unique to the product, so we
    // do not send a notification to Firefox from Thunderbird and so on. I am using
    // bundle Id (assuming all Mozilla products come wrapped in .app bundles) -
    // it should be unique
    // (e.g., org.mozilla.firefox.distributedNotification.commandLineArgs for Firefox).
    // We also need to make sure the notifications are "local" to the current user,
    // so we do not pass it on to perhaps another running Thunderbird by another
    // logged in user. Distributed notifications is the best candidate
    // (while darwin notifications ignore the user context).
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),
                                         (__bridge CFStringRef)notificationName, NULL, userInfoDict,
                                         true);

    // Do not continue start up sequence for this process - just self-terminate,
    // we already passed the arguments on to the original instance of the process.
    *_retval = false;
  } else {
    // This is the first instance ever (or launched as -no-remote)!
    // Let's register a notification listener here,
    // In case future instances would want to notify us about command line arguments
    // passed to them. Note, that if mozilla process is restarting, we still need to
    // register for notifications.
    CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), NULL,
                                    remoteClientNotificationCallback,
                                    (__bridge CFStringRef)notificationName, NULL,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);

    // Continue the start up sequence of this process.
    *_retval = true;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsNativeAppSupportCocoa::ReOpen() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mCanShowUI) return NS_ERROR_FAILURE;

  bool haveNonMiniaturized = false;
  bool haveOpenWindows = false;
  bool done = false;

  nsCOMPtr<nsIWindowMediator> wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm) {
    return NS_ERROR_FAILURE;
  } else {
    nsCOMPtr<nsISimpleEnumerator> windowList;
    wm->GetXULWindowEnumerator(nullptr, getter_AddRefs(windowList));
    bool more;
    windowList->HasMoreElements(&more);
    while (more) {
      nsCOMPtr<nsISupports> nextWindow = nullptr;
      windowList->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(nextWindow));
      if (!baseWindow) {
        windowList->HasMoreElements(&more);
        continue;
      } else {
        haveOpenWindows = true;
      }

      nsCOMPtr<nsIWidget> widget = nullptr;
      baseWindow->GetMainWidget(getter_AddRefs(widget));
      if (!widget) {
        windowList->HasMoreElements(&more);
        continue;
      }
      NSWindow* cocoaWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
      if (![cocoaWindow isMiniaturized]) {
        haveNonMiniaturized = true;
        break;  // have un-minimized windows, nothing to do
      }
      windowList->HasMoreElements(&more);
    }  // end while

    if (!haveNonMiniaturized) {
      // Deminiaturize the most recenty used window
      nsCOMPtr<mozIDOMWindowProxy> mru;
      wm->GetMostRecentWindow(nullptr, getter_AddRefs(mru));

      if (mru) {
        NSWindow* cocoaMru = nil;
        GetNativeWindowPointerFromDOMWindow(mru, &cocoaMru);
        if (cocoaMru) {
          [cocoaMru deminiaturize:nil];
          done = true;
        }
      }
    }  // end if have non miniaturized

    if (!haveOpenWindows && !done) {
      char* argv[] = {nullptr};

      // use an empty command line to make the right kind(s) of window open
      nsCOMPtr<nsICommandLineRunner> cmdLine(new nsCommandLine());

      nsresult rv;
      rv = cmdLine->Init(0, argv, nullptr, nsICommandLine::STATE_REMOTE_EXPLICIT);
      NS_ENSURE_SUCCESS(rv, rv);

      return cmdLine->Run();
    }

  }  // got window mediator
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#pragma mark -

// Create and return an instance of class nsNativeAppSupportCocoa.
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport** aResult) {
  *aResult = new nsNativeAppSupportCocoa;
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
