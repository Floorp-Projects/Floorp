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
#include "nsICommandLineRunner.h"
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

nsresult
GetNativeWindowPointerFromDOMWindow(mozIDOMWindowProxy *a_window, NSWindow **a_nativeWindow)
{
  *a_nativeWindow = nil;
  if (!a_window)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIWebNavigation> mruWebNav(do_GetInterface(a_window));
  if (mruWebNav) {
    nsCOMPtr<nsIDocShellTreeItem> mruTreeItem(do_QueryInterface(mruWebNav));
    nsCOMPtr<nsIDocShellTreeOwner> mruTreeOwner = nullptr;
    mruTreeItem->GetTreeOwner(getter_AddRefs(mruTreeOwner));
    if(mruTreeOwner) {
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

class nsNativeAppSupportCocoa : public nsNativeAppSupportBase
{
public:
  nsNativeAppSupportCocoa() :
    mCanShowUI(false) { }

  NS_IMETHOD Start(bool* aRetVal);
  NS_IMETHOD ReOpen();
  NS_IMETHOD Enable();

private:
  bool mCanShowUI;
};

NS_IMETHODIMP
nsNativeAppSupportCocoa::Enable()
{
  mCanShowUI = true;
  return NS_OK;
}

NS_IMETHODIMP nsNativeAppSupportCocoa::Start(bool *_retval)
{
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
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsNativeAppSupportCocoa::ReOpen()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mCanShowUI)
    return NS_ERROR_FAILURE;

  bool haveNonMiniaturized = false;
  bool haveOpenWindows = false;
  bool done = false;
  
  nsCOMPtr<nsIWindowMediator> 
    wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm) {
    return NS_ERROR_FAILURE;
  } 
  else {
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
      }
      else {
        haveOpenWindows = true;
      }

      nsCOMPtr<nsIWidget> widget = nullptr;
      baseWindow->GetMainWidget(getter_AddRefs(widget));
      if (!widget) {
        windowList->HasMoreElements(&more);
        continue;
      }
      NSWindow *cocoaWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
      if (![cocoaWindow isMiniaturized]) {
        haveNonMiniaturized = true;
        break;  //have un-minimized windows, nothing to do
      }
      windowList->HasMoreElements(&more);
    } // end while

    if (!haveNonMiniaturized) {
      // Deminiaturize the most recenty used window
      nsCOMPtr<mozIDOMWindowProxy> mru;
      wm->GetMostRecentWindow(nullptr, getter_AddRefs(mru));

      if (mru) {
        NSWindow *cocoaMru = nil;
        GetNativeWindowPointerFromDOMWindow(mru, &cocoaMru);
        if (cocoaMru) {
          [cocoaMru deminiaturize:nil];
          done = true;
        }
      }
    } // end if have non miniaturized

    if (!haveOpenWindows && !done) {
      char* argv[] = { nullptr };

      // use an empty command line to make the right kind(s) of window open
      nsCOMPtr<nsICommandLineRunner> cmdLine
        (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
      NS_ENSURE_TRUE(cmdLine, NS_ERROR_FAILURE);

      nsresult rv;
      rv = cmdLine->Init(0, argv, nullptr,
                         nsICommandLine::STATE_REMOTE_EXPLICIT);
      NS_ENSURE_SUCCESS(rv, rv);

      return cmdLine->Run();
    }
    
  } // got window mediator
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#pragma mark -

// Create and return an instance of class nsNativeAppSupportCocoa.
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport**aResult)
{
  *aResult = new nsNativeAppSupportCocoa;
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
