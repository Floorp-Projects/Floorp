/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steven Michaud <smichaud@pobox.com>
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

#include "nsString.h"

#import <CoreServices/CoreServices.h>
#import <Cocoa/Cocoa.h>

#include "nsCOMPtr.h"
#include "nsObjCExceptions.h"
#include "nsNativeAppSupportBase.h"

#include "nsIAppShellService.h"
#include "nsIAppStartup.h"
#include "nsIBaseWindow.h"
#include "nsICommandLineRunner.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsIServiceManager.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"

nsresult
GetNativeWindowPointerFromDOMWindow(nsIDOMWindow *a_window, NSWindow **a_nativeWindow)
{
  *a_nativeWindow = nil;
  if (!a_window)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIWebNavigation> mruWebNav(do_GetInterface(a_window));
  if (mruWebNav) {
    nsCOMPtr<nsIDocShellTreeItem> mruTreeItem(do_QueryInterface(mruWebNav));
    nsCOMPtr<nsIDocShellTreeOwner> mruTreeOwner = nsnull;
    mruTreeItem->GetTreeOwner(getter_AddRefs(mruTreeOwner));
    if(mruTreeOwner) {
      nsCOMPtr<nsIBaseWindow> mruBaseWindow(do_QueryInterface(mruTreeOwner));
      if (mruBaseWindow) {
        nsCOMPtr<nsIWidget> mruWidget = nsnull;
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
    mCanShowUI(PR_FALSE) { }

  NS_IMETHOD Start(bool* aRetVal);
  NS_IMETHOD ReOpen();
  NS_IMETHOD Enable();

private:
  bool mCanShowUI;

};

NS_IMETHODIMP
nsNativeAppSupportCocoa::Enable()
{
  mCanShowUI = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsNativeAppSupportCocoa::Start(bool *_retval)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  SInt32 response = 0;
  OSErr err = ::Gestalt (gestaltSystemVersion, &response);
  response &= 0xFFFF; // The system version is in the low order word

  // Check that the OS version is supported, if not return PR_FALSE,
  // which will make the browser quit.  In principle we could display an
  // alert here.  But the alert's message and buttons would require custom
  // localization.  So (for now at least) we just log an English message
  // to the console before quitting.
#ifdef __LP64__
  SInt32 minimumOS = 0x00001060;
#else
  SInt32 minimumOS = 0x00001050;
#endif
  if ((err != noErr) || response < minimumOS) {
    NSLog(@"Minimum OS version requirement not met!");
    return PR_FALSE;
  }

  *_retval = PR_TRUE;
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
    wm->GetXULWindowEnumerator(nsnull, getter_AddRefs(windowList));
    bool more;
    windowList->HasMoreElements(&more);
    while (more) {
      nsCOMPtr<nsISupports> nextWindow = nsnull;
      windowList->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(nextWindow));
      if (!baseWindow) {
        windowList->HasMoreElements(&more);
        continue;
      }
      else {
        haveOpenWindows = PR_TRUE;
      }

      nsCOMPtr<nsIWidget> widget = nsnull;
      baseWindow->GetMainWidget(getter_AddRefs(widget));
      if (!widget) {
        windowList->HasMoreElements(&more);
        continue;
      }
      NSWindow *cocoaWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
      if (![cocoaWindow isMiniaturized]) {
        haveNonMiniaturized = PR_TRUE;
        break;  //have un-minimized windows, nothing to do
      } 
      windowList->HasMoreElements(&more);
    } // end while
        
    if (!haveNonMiniaturized) {
      // Deminiaturize the most recenty used window
      nsCOMPtr<nsIDOMWindow> mru;
      wm->GetMostRecentWindow(nsnull, getter_AddRefs(mru));
            
      if (mru) {        
        NSWindow *cocoaMru = nil;
        GetNativeWindowPointerFromDOMWindow(mru, &cocoaMru);
        if (cocoaMru) {
          [cocoaMru deminiaturize:nil];
          done = PR_TRUE;
        }
      }
      
    } // end if have non miniaturized
    
    if (!haveOpenWindows && !done) {
      char* argv[] = { nsnull };
    
      // use an empty command line to make the right kind(s) of window open
      nsCOMPtr<nsICommandLineRunner> cmdLine
        (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
      NS_ENSURE_TRUE(cmdLine, NS_ERROR_FAILURE);

      nsresult rv;
      rv = cmdLine->Init(0, argv, nsnull,
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
