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
 * The Original Code is Mozilla Communicator client code. This file was split
 * from xpfe/appshell/src/nsAppShellService.cpp
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAppStartup.h"

#include "nsIAppShellService.h"
#include "nsICharsetConverterManager.h"
#include "nsICloseAllWindows.h"
#include "nsIDOMWindowInternal.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestor.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIPlatformCharset.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIProfileChangeStatus.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsITimelineService.h"
#include "nsIUnicodeDecoder.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebShellWindow.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsIXULWindow.h"

#include "prprf.h"
#include "nsCRT.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsWidgetsCID.h"
#include "nsAppShellCID.h"
#include "nsXPFEComponentsCID.h"

NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

// Static Function Prototypes
static nsresult ConvertToUnicode(nsCString& aCharset, const char* inString, nsAString& outString); 

//
// nsAppStartup
//

nsAppStartup::nsAppStartup() :
  mConsiderQuitStopper(0),
  mShuttingDown(PR_FALSE),
  mAttemptingQuit(PR_FALSE)
{ }


nsresult
nsAppStartup::Init()
{
  nsresult rv;

  // Create widget application shell
  mAppShell = do_CreateInstance(kAppShellCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICmdLineService> cmdLineService
    (do_GetService(NS_COMMANDLINESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 argc = 0;
  char** argv = nsnull;
  cmdLineService->GetArgc(&argc);
  cmdLineService->GetArgv(&argv);

  rv = mAppShell->Create(&argc, argv);
  NS_ENSURE_SUCCESS(rv, rv);

  // listen to EventQueues' comings and goings. do this after the appshell
  // has been created, but after the event queue has been created. that
  // latter bit is unfortunate, but we deal with it.
  nsCOMPtr<nsIObserverService> os
    (do_GetService("@mozilla.org/observer-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  os->AddObserver(this, "nsIEventQueueActivated", PR_TRUE);
  os->AddObserver(this, "nsIEventQueueDestroyed", PR_TRUE);
  os->AddObserver(this, "profile-change-teardown", PR_TRUE);
  os->AddObserver(this, "profile-initial-state", PR_TRUE);
  os->AddObserver(this, "xul-window-registered", PR_TRUE);
  os->AddObserver(this, "xul-window-destroyed", PR_TRUE);

  return NS_OK;
}


//
// nsAppStartup->nsISupports
//

NS_IMPL_THREADSAFE_ISUPPORTS5(nsAppStartup,
                              nsIAppStartup,
                              nsIWindowCreator,
                              nsIWindowCreator2,
                              nsIObserver,
                              nsISupportsWeakReference)


//
// nsAppStartup->nsIAppStartup
//

NS_IMETHODIMP
nsAppStartup::CreateHiddenWindow()
{
  nsCOMPtr<nsIAppShellService> appShellService
    (do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShellService, NS_ERROR_FAILURE);

  return appShellService->CreateHiddenWindow(mAppShell);
}


NS_IMETHODIMP
nsAppStartup::Run(void)
{
  return mAppShell->Run();
}


NS_IMETHODIMP
nsAppStartup::Quit(PRUint32 aFerocity)
{
  // Quit the application. We will asynchronously call the appshell's
  // Exit() method via the ExitCallback() to allow one last pass
  // through any events in the queue. This guarantees a tidy cleanup.
  nsresult rv = NS_OK;
  PRBool postedExitEvent = PR_FALSE;

  if (mShuttingDown)
    return NS_OK;

  /* eForceQuit doesn't actually work; it can cause a subtle crash if
     there are windows open which have unload handlers which open
     new windows. Use eAttemptQuit for now. */
  if (aFerocity == eForceQuit) {
    NS_WARNING("attempted to force quit");
    // it will be treated the same as eAttemptQuit, below
  }

  mShuttingDown = PR_TRUE;

  nsCOMPtr<nsIWindowMediator> mediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));

  if (aFerocity == eConsiderQuit && mConsiderQuitStopper == 0) {
    // attempt quit if the last window has been unregistered/closed

    PRBool windowsRemain = PR_TRUE;

    if (mediator) {
      nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
      mediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
      if (windowEnumerator)
        windowEnumerator->HasMoreElements(&windowsRemain);
    }
    if (!windowsRemain) {
      aFerocity = eAttemptQuit;
    }
  }

  /* Currently aFerocity can never have the value of eForceQuit here.
     That's temporary (in an unscheduled kind of way) and logically
     this code is part of the eForceQuit case, so I'm checking against
     that value anyway. Reviewers made me add this comment. */
  if (aFerocity == eAttemptQuit || aFerocity == eForceQuit) {

    AttemptingQuit(PR_TRUE);

    /* Enumerate through each open window and close it. It's important to do
       this before we forcequit because this can control whether we really quit
       at all. e.g. if one of these windows has an unload handler that
       opens a new window. Ugh. I know. */
    if (mediator) {
      nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

      mediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));

      if (windowEnumerator) {

        while (1) {
          PRBool more;
          if (NS_FAILED(rv = windowEnumerator->HasMoreElements(&more)) || !more)
            break;

          nsCOMPtr<nsISupports> isupports;
          rv = windowEnumerator->GetNext(getter_AddRefs(isupports));
          if (NS_FAILED(rv))
            break;

          nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(isupports);
          NS_ASSERTION(window, "not an nsIDOMWindowInternal");
          if (!window)
            continue;

          window->Close();
        }
      }

      if (aFerocity == eAttemptQuit) {

        aFerocity = eForceQuit; // assume success

        /* Were we able to immediately close all windows? if not, eAttemptQuit
           failed. This could happen for a variety of reasons; in fact it's
           very likely. Perhaps we're being called from JS and the window->Close
           method hasn't had a chance to wrap itself up yet. So give up.
           We'll return (with eConsiderQuit) as the remaining windows are
           closed. */
        mediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
        if (windowEnumerator) {
          PRBool more;
          while (windowEnumerator->HasMoreElements(&more), more) {
            /* we can't quit immediately. we'll try again as the last window
               finally closes. */
            aFerocity = eAttemptQuit;
            nsCOMPtr<nsISupports> window;
            windowEnumerator->GetNext(getter_AddRefs(window));
            nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(window));
            if (domWindow) {
              PRBool closed = PR_FALSE;
              domWindow->GetClosed(&closed);
              if (!closed) {
                rv = NS_ERROR_FAILURE;
                break;
              }
            }
          }
        }
      }
    }
  }

  if (aFerocity == eForceQuit) {
    // do it!

    // No chance of the shutdown being cancelled from here on; tell people
    // we're shutting down for sure while all services are still available.
    nsCOMPtr<nsIObserverService> obsService
      (do_GetService("@mozilla.org/observer-service;1"));
    if (obsService)
      obsService->NotifyObservers(nsnull, "quit-application", nsnull);

    nsCOMPtr<nsIAppShellService> appShellService
      (do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    NS_ASSERTION(appShellService, "We're gonna leak something.");
    if (appShellService)
      appShellService->DestroyHiddenWindow();

    // no matter what, make sure we send the exit event.  If
    // worst comes to worst, we'll do a leaky shutdown but we WILL
    // shut down. Well, assuming that all *this* stuff works ;-).
    nsCOMPtr<nsIEventQueueService> svc = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {

      nsCOMPtr<nsIEventQueue> queue;
      rv = svc->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
      if (NS_SUCCEEDED(rv)) {

        PLEvent* event = new PLEvent;
        if (event) {
          NS_ADDREF_THIS();
          PL_InitEvent(event,
                       this,
                       HandleExitEvent,
                       DestroyExitEvent);

          rv = queue->PostEvent(event);
          if (NS_SUCCEEDED(rv)) {
            postedExitEvent = PR_TRUE;
          }
          else {
            PL_DestroyEvent(event);
          }
        }
        else {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }

  // turn off the reentrancy check flag, but not if we have
  // more asynchronous work to do still.
  if (!postedExitEvent)
    mShuttingDown = PR_FALSE;
  return rv;
}


/* We know we're trying to quit the app, but may not be able to do so
   immediately. Enter a state where we're more ready to quit.
   (Does useful work only on the Mac.) */
void
nsAppStartup::AttemptingQuit(PRBool aAttempt)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  if (aAttempt) {
    // now even the Mac wants to quit when the last window is closed
    if (!mAttemptingQuit)
      ExitLastWindowClosingSurvivalArea();
    mAttemptingQuit = PR_TRUE;
  } else {
    // changed our mind. back to normal.
    if (mAttemptingQuit)
      EnterLastWindowClosingSurvivalArea();
    mAttemptingQuit = PR_FALSE;
  }
#else
  mAttemptingQuit = aAttempt;
#endif
}

NS_IMETHODIMP
nsAppStartup::EnterLastWindowClosingSurvivalArea(void)
{
  ++mConsiderQuitStopper;
  return NS_OK;
}


NS_IMETHODIMP
nsAppStartup::ExitLastWindowClosingSurvivalArea(void)
{
  NS_ASSERTION(mConsiderQuitStopper > 0, "consider quit stopper out of bounds");
  --mConsiderQuitStopper;
  return NS_OK;
}


NS_IMETHODIMP
nsAppStartup::CreateStartupState(PRInt32 aWindowWidth, PRInt32 aWindowHeight,
                                 PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv;

#ifndef MOZ_THUNDERBIRD // xxxbsmedberg: more ifdef badness!
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefService)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPrefBranch> startupBranch;
  prefService->GetBranch(PREF_STARTUP_PREFIX, getter_AddRefs(startupBranch));
  if (!startupBranch)
    return NS_ERROR_FAILURE;
  
  PRUint32 childCount;
  char **childArray = nsnull;
  rv = startupBranch->GetChildList("", &childCount, &childArray);
  if (NS_FAILED(rv))
    return rv;
    
  for (PRUint32 i = 0; i < childCount; i++) {
    PRBool prefValue;
    startupBranch->GetBoolPref(childArray[i], &prefValue);
    if (prefValue) {
      PRBool windowOpened;
      rv = LaunchTask(childArray[i], aWindowHeight, aWindowWidth, &windowOpened);
      if (NS_SUCCEEDED(rv) && windowOpened)
        *_retval = PR_TRUE;
    }
  }
  
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(childCount, childArray);
#else
  PRBool windowOpened;
  rv = LaunchTask("mail", aWindowHeight, aWindowWidth, &windowOpened);
  if (NS_SUCCEEDED(rv) && windowOpened)
    *_retval = PR_TRUE;
  else
    *_retval = PR_FALSE;
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsAppStartup::Ensure1Window(nsICmdLineService *aCmdLineService)
{
  nsresult rv;

  nsCOMPtr<nsIWindowMediator> windowMediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator))))
  {
    PRBool more;
    windowEnumerator->HasMoreElements(&more);
    if (!more)
    {
      // No window exists so lets create a browser one
      PRInt32 height = nsIAppShellService::SIZE_TO_CONTENT;
      PRInt32 width  = nsIAppShellService::SIZE_TO_CONTENT;
				
      // Get the value of -width option
      nsXPIDLCString tempString;
      rv = aCmdLineService->GetCmdLineValue("-width", getter_Copies(tempString));
      if (NS_SUCCEEDED(rv) && !tempString.IsEmpty())
        PR_sscanf(tempString.get(), "%d", &width);


      // Get the value of -height option
      rv = aCmdLineService->GetCmdLineValue("-height", getter_Copies(tempString));
      if (NS_SUCCEEDED(rv) && !tempString.IsEmpty())
        PR_sscanf(tempString.get(), "%d", &height);

#ifdef MOZ_THUNDERBIRD // XXXbsmedberg more badness, needs NVU thought
      PRBool windowOpened = PR_FALSE;
      
      rv = LaunchTask(NULL, height, width, &windowOpened); 
      
      if (NS_FAILED(rv) || !windowOpened)
        rv = LaunchTask("mail", height, width, &windowOpened);
#else
      rv = OpenBrowserWindow(height, width);
#endif
    }
  }
  return rv;
}


void* PR_CALLBACK
nsAppStartup::HandleExitEvent(PLEvent* aEvent)
{
  nsAppStartup *service =
    NS_REINTERPRET_CAST(nsAppStartup*, aEvent->owner);

  // Tell the appshell to exit
  service->mAppShell->Exit();

  // We're done "shutting down".
  service->mShuttingDown = PR_FALSE;

  return nsnull;
}

void PR_CALLBACK
nsAppStartup::DestroyExitEvent(PLEvent* aEvent)
{
  nsAppStartup *service =
    NS_REINTERPRET_CAST(nsAppStartup*, aEvent->owner);
  NS_RELEASE(service);
  delete aEvent;
}


nsresult
nsAppStartup::LaunchTask(const char *aParam, PRInt32 height, PRInt32 width, PRBool *windowOpened)
{
  nsresult rv = NS_OK;

  nsCOMPtr <nsICmdLineService> cmdLine =
    do_GetService(NS_COMMANDLINESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr <nsICmdLineHandler> handler;
  rv = cmdLine->GetHandlerForParam(aParam, getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString chromeUrlForTask;
  rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));
  if (NS_FAILED(rv)) return rv;

  PRBool handlesArgs = PR_FALSE;
  rv = handler->GetHandlesArgs(&handlesArgs);
  if (handlesArgs) {
#ifndef MOZ_THUNDERBIRD //XXXbsmedberg bad toolkit->app dependency!
    nsXPIDLString defaultArgs;
    rv = handler->GetDefaultArgs(getter_Copies(defaultArgs));
    if (NS_FAILED(rv)) return rv;
    rv = OpenWindow(chromeUrlForTask, defaultArgs,
                    nsIAppShellService::SIZE_TO_CONTENT,
                    nsIAppShellService::SIZE_TO_CONTENT);
#else
    // XXX horibble thunderbird hack. Don't pass in the default args if the
    // cmd line service says we have real arguments! Use those instead.
    nsXPIDLCString args;
    nsXPIDLCString cmdLineArgument; // -mail, -compose, etc. 
    rv = handler->GetCommandLineArgument(getter_Copies(cmdLineArgument));
    if (NS_SUCCEEDED(rv)) {
      rv = cmdLine->GetCmdLineValue(cmdLineArgument, getter_Copies(args));
      if (NS_SUCCEEDED(rv) && args.get() && strcmp(args.get(), "1")) {
        nsAutoString cmdArgs; cmdArgs.AssignWithConversion(args);
        rv = OpenWindow(chromeUrlForTask, cmdArgs, height, width);
      }
      else
        rv = NS_ERROR_FAILURE;
    }
    
    // any failure case, do what we used to do:
    if (NS_FAILED(rv)) {
      nsXPIDLString defaultArgs;
      rv = handler->GetDefaultArgs(getter_Copies(defaultArgs));
      if (NS_FAILED(rv)) return rv;
      rv = OpenWindow(chromeUrlForTask, defaultArgs,
                      nsIAppShellService::SIZE_TO_CONTENT, nsIAppShellService::SIZE_TO_CONTENT);
    }
#endif
  }
  else {
    rv = OpenWindow(chromeUrlForTask, nsString(), width, height);
  }
  
  // If we get here without an error, then a window was opened OK.
  if (NS_SUCCEEDED(rv)) {
    *windowOpened = PR_TRUE;
  }

  return rv;
}

nsresult
nsAppStartup::OpenWindow(const nsAFlatCString& aChromeURL,
                         const nsAFlatString& aAppArgs,
                         PRInt32 aWidth, PRInt32 aHeight)
{
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsString> sarg(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!wwatch || !sarg)
    return NS_ERROR_FAILURE;

  sarg->SetData(aAppArgs);

  nsCAutoString features("chrome,dialog=no,all");
  if (aHeight != nsIAppShellService::SIZE_TO_CONTENT) {
    features.Append(",height=");
    features.AppendInt(aHeight);
  }
  if (aWidth != nsIAppShellService::SIZE_TO_CONTENT) {
    features.Append(",width=");
    features.AppendInt(aWidth);
  }

  nsCOMPtr<nsIDOMWindow> newWindow;
  return wwatch->OpenWindow(0, aChromeURL.get(), "_blank",
                            features.get(), sarg,
                            getter_AddRefs(newWindow));
}


nsresult
nsAppStartup::OpenBrowserWindow(PRInt32 height, PRInt32 width)
{
    nsresult rv;
    nsCOMPtr<nsICmdLineHandler> handler(do_GetService("@mozilla.org/commandlinehandler/general-startup;1?type=browser", &rv));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString chromeUrlForTask;
    rv = handler->GetChromeUrlForTask(getter_Copies(chromeUrlForTask));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr <nsICmdLineService> cmdLine = do_GetService(NS_COMMANDLINESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString urlToLoad;
    rv = cmdLine->GetURLToLoad(getter_Copies(urlToLoad));
    if (NS_FAILED(rv)) return rv;

    if (!urlToLoad.IsEmpty()) {

#ifdef DEBUG_CMD_LINE
      printf("url to load: %s\n", urlToLoad.get());
#endif /* DEBUG_CMD_LINE */

      nsAutoString url; 
      if (nsCRT::IsAscii(urlToLoad))  {
        url.AssignWithConversion(urlToLoad);
      }
      else {
        // get a platform charset
        nsCAutoString charSet;
        nsCOMPtr <nsIPlatformCharset> platformCharset(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
        if (NS_FAILED(rv)) {
          NS_ASSERTION(0, "Failed to get a platform charset");
          return rv;
        }

        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, charSet);
        if (NS_FAILED(rv)) {
          NS_ASSERTION(0, "Failed to get a charset");
          return rv;
        }

        // convert the cmdLine URL to Unicode
        rv = ConvertToUnicode(charSet, urlToLoad, url);
        if (NS_FAILED(rv)) {
          NS_ASSERTION(0, "Failed to convert commandline url to unicode");
          return rv;
        }
      }
      rv = OpenWindow(chromeUrlForTask, url, width, height);

    } else {

      nsXPIDLString defaultArgs;
      rv = handler->GetDefaultArgs(getter_Copies(defaultArgs));
      if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_CMD_LINE
      printf("default args: %s\n", NS_ConvertUCS2toUTF8(defaultArgs).get());
#endif /* DEBUG_CMD_LINE */

      rv = OpenWindow(chromeUrlForTask, defaultArgs, width, height);
    }

    return rv;
}


//
// nsAppStartup->nsIWindowCreator
//

NS_IMETHODIMP
nsAppStartup::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                 PRUint32 aChromeFlags,
                                 nsIWebBrowserChrome **_retval)
{
  PRBool cancel;
  return CreateChromeWindow2(aParent, aChromeFlags, 0, 0, &cancel, _retval);
}


//
// nsAppStartup->nsIWindowCreator2
//

NS_IMETHODIMP
nsAppStartup::CreateChromeWindow2(nsIWebBrowserChrome *aParent,
                                  PRUint32 aChromeFlags,
                                  PRUint32 aContextFlags,
                                  nsIURI *aURI,
                                  PRBool *aCancel,
                                  nsIWebBrowserChrome **_retval)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  NS_ENSURE_ARG_POINTER(_retval);
  *aCancel = PR_FALSE;
  *_retval = 0;

  nsCOMPtr<nsIXULWindow> newWindow;

  if (aParent) {
    nsCOMPtr<nsIXULWindow> xulParent(do_GetInterface(aParent));
    NS_ASSERTION(xulParent, "window created using non-XUL parent. that's unexpected, but may work.");

    if (xulParent)
      xulParent->CreateNewWindow(aChromeFlags, mAppShell, getter_AddRefs(newWindow));
    // And if it fails, don't try again without a parent. It could fail
    // intentionally (bug 115969).
  } else { // try using basic methods:
    /* You really shouldn't be making dependent windows without a parent.
      But unparented modal (and therefore dependent) windows happen
      in our codebase, so we allow it after some bellyaching: */
    if (aChromeFlags & nsIWebBrowserChrome::CHROME_DEPENDENT)
      NS_WARNING("dependent window created without a parent");

    nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    if (!appShell)
      return NS_ERROR_FAILURE;
    
    appShell->CreateTopLevelWindow(0, 0, PR_FALSE, PR_FALSE,
      aChromeFlags, nsIAppShellService::SIZE_TO_CONTENT,
      nsIAppShellService::SIZE_TO_CONTENT, mAppShell, getter_AddRefs(newWindow));
  }

  // if anybody gave us anything to work with, use it
  if (newWindow) {
    newWindow->SetContextFlags(aContextFlags);
    nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(newWindow));
    if (thing)
      CallGetInterface(thing.get(), _retval);
  }

  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}


//
// nsAppStartup->nsIObserver
//

NS_IMETHODIMP
nsAppStartup::Observe(nsISupports *aSubject,
                      const char *aTopic, const PRUnichar *aData)
{
  NS_ASSERTION(mAppShell, "appshell service notified before appshell built");
  if (!strcmp(aTopic, "nsIEventQueueActivated")) {
    nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
    if (eq) {
      PRBool isNative = PR_TRUE;
      // we only add native event queues to the appshell
      eq->IsQueueNative(&isNative);
      if (isNative)
        mAppShell->ListenToEventQueue(eq, PR_TRUE);
    }
  } else if (!strcmp(aTopic, "nsIEventQueueDestroyed")) {
    nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
    if (eq) {
      PRBool isNative = PR_TRUE;
      // we only remove native event queues from the appshell
      eq->IsQueueNative(&isNative);
      if (isNative)
        mAppShell->ListenToEventQueue(eq, PR_FALSE);
    }
  } else if (!strcmp(aTopic, "profile-change-teardown")) {
    nsresult rv;
    EnterLastWindowClosingSurvivalArea();
    // NOTE: No early error exits because we need to execute the
    // balancing ExitLastWindowClosingSurvivalArea().
    nsCOMPtr<nsICloseAllWindows> closer =
            do_CreateInstance("@mozilla.org/appshell/closeallwindows;1", &rv);
    NS_ASSERTION(closer, "Failed to create nsICloseAllWindows impl.");
    PRBool proceedWithSwitch = PR_FALSE;
    if (closer)
      rv = closer->CloseAll(PR_TRUE, &proceedWithSwitch);

    if (NS_FAILED(rv) || !proceedWithSwitch) {
      nsCOMPtr<nsIProfileChangeStatus> changeStatus(do_QueryInterface(aSubject));
      if (changeStatus)
        changeStatus->VetoChange();
    }
    ExitLastWindowClosingSurvivalArea();
  } else if (!strcmp(aTopic, "profile-initial-state")) {
    if (nsDependentString(aData).Equals(NS_LITERAL_STRING("switch"))) {
      // Now, establish the startup state according to the new prefs.
      PRBool openedWindow;
      CreateStartupState(nsIAppShellService::SIZE_TO_CONTENT,
                         nsIAppShellService::SIZE_TO_CONTENT, &openedWindow);
      if (!openedWindow)
        OpenBrowserWindow(nsIAppShellService::SIZE_TO_CONTENT,
                          nsIAppShellService::SIZE_TO_CONTENT);
    }
  } else if (!strcmp(aTopic, "xul-window-registered")) {
    AttemptingQuit(PR_FALSE);
  } else if (!strcmp(aTopic, "xul-window-destroyed")) {
    Quit(eConsiderQuit);
  } else {
    NS_ERROR("Unexpected observer topic.");
  }

  return NS_OK;
}

static nsresult
ConvertToUnicode(nsCString& aCharset, const char* inString, nsAString& outString)
{
  nsresult rv;

  // convert result to unicode
  nsCOMPtr<nsICharsetConverterManager> ccm(do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID , &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr <nsIUnicodeDecoder> decoder; 
  rv = ccm->GetUnicodeDecoderRaw(aCharset.get(), getter_AddRefs(decoder));
  if (NS_FAILED(rv))
    return rv;

  PRInt32 uniLength = 0;
  PRInt32 srcLength = strlen(inString);
  rv = decoder->GetMaxLength(inString, srcLength, &uniLength);
  if (NS_FAILED(rv))
    return rv;

  PRUnichar *unichars = new PRUnichar [uniLength];
  if (nsnull != unichars) {
    // convert to unicode
    rv = decoder->Convert(inString, &srcLength, unichars, &uniLength);
    if (NS_SUCCEEDED(rv)) {
      // Pass back the unicode string
      outString.Assign(unichars, uniLength);
    }
    delete [] unichars;
  }
  else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}
