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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
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


#include "nsIAppShellService.h"
#include "nsISupportsArray.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWebShellWindow.h"
#include "nsWebShellWindow.h"

#include "nsIEnumerator.h"
#include "nsCRT.h"
#include "nsITimelineService.h"
#include "prprf.h"    
#include "plevent.h"

#include "nsWidgetsCID.h"
#include "nsIRequestObserver.h"

/* For implementing GetHiddenWindowAndJSContext */
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

#include "nsAppShellService.h"
#include "nsISupportsPrimitives.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"

class nsIAppShell;

nsAppShellService::nsAppShellService() : 
  mXPCOMShuttingDown(PR_FALSE),
  mModalWindowCount(0)
{
  nsCOMPtr<nsIObserverService> obs
    (do_GetService("@mozilla.org/observer-service;1"));

  if (obs)
    obs->AddObserver(this, "xpcom-shutdown", PR_FALSE);
}

nsAppShellService::~nsAppShellService()
{
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS2(nsAppShellService,
                   nsIAppShellService,
                   nsIObserver);

nsresult 
nsAppShellService::SetXPConnectSafeContext()
{
  nsresult rv;

  nsCOMPtr<nsIThreadJSContextStack> cxstack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindowInternal> junk;
  JSContext *cx;
  rv = GetHiddenWindowAndJSContext(getter_AddRefs(junk), &cx);
  NS_ENSURE_SUCCESS(rv, rv);

  return cxstack->SetSafeJSContext(cx);
}  

nsresult nsAppShellService::ClearXPConnectSafeContext()
{
  nsresult rv;

  nsCOMPtr<nsIThreadJSContextStack> cxstack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("XPConnect ContextStack gone before XPCOM shutdown?");
    return rv;
  }

  nsCOMPtr<nsIDOMWindowInternal> junk;
  JSContext *cx;
  rv = GetHiddenWindowAndJSContext(getter_AddRefs(junk), &cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext *safe_cx;
  rv = cxstack->GetSafeJSContext(&safe_cx);
  NS_ENSURE_SUCCESS(rv, rv);

  if (cx == safe_cx)
    rv = cxstack->SetSafeJSContext(nsnull);

  return rv;
}

NS_IMETHODIMP
nsAppShellService::CreateHiddenWindow(nsIAppShell* aAppShell)
{
  nsresult rv;
  PRInt32 initialHeight = 100, initialWidth = 100;
    
#if defined(XP_MAC) || defined(XP_MACOSX)
  const char* defaultHiddenWindowURL = "chrome://global/content/hiddenWindow.xul";
  PRUint32    chromeMask = 0;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
  nsXPIDLCString prefVal;
  rv = prefBranch->GetCharPref("browser.hiddenWindowChromeURL", getter_Copies(prefVal));
  const char* hiddenWindowURL = prefVal.get() ? prefVal.get() : defaultHiddenWindowURL;
#else
  const char* hiddenWindowURL = "about:blank";
  PRUint32    chromeMask =  nsIWebBrowserChrome::CHROME_ALL;
#endif

  nsCOMPtr<nsIURI> url;
  rv = NS_NewURI(getter_AddRefs(url), hiddenWindowURL);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIXULWindow> newWindow;
    rv = JustCreateTopWindow(nsnull, url, PR_FALSE, PR_FALSE,
                        chromeMask, initialWidth, initialHeight,
                        PR_TRUE, aAppShell, getter_AddRefs(newWindow));
    if (NS_SUCCEEDED(rv)) {
      mHiddenWindow = newWindow;

#if defined(XP_MAC) || defined(XP_MACOSX)
      // hide the hidden window by launching it into outer space. This
      // way, we can keep it visible and let the OS send it activates
      // to keep menus happy. This will cause it to show up in window
      // lists under osx, but I think that's ok.
      nsCOMPtr<nsIBaseWindow> base ( do_QueryInterface(newWindow) );
      if ( base ) {
        base->SetPosition ( -32000, -32000 );
        base->SetVisibility ( PR_TRUE );
      }
#endif
      
      // Set XPConnect's fallback JSContext (used for JS Components)
      // to the DOM JSContext for this thread, so that DOM-to-XPConnect
      // conversions get the JSContext private magic they need to
      // succeed.
      SetXPConnectSafeContext();

      // RegisterTopLevelWindow(newWindow); -- Mac only
    }
  }
  NS_ASSERTION(NS_SUCCEEDED(rv), "HiddenWindow not created");
  return(rv);
}

NS_IMETHODIMP
nsAppShellService::DestroyHiddenWindow()
{
  if (mHiddenWindow) {
    nsCOMPtr<nsIWebShellWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
    NS_ASSERTION(hiddenWin, "Hidden window is not nsIWebShellWindow!");
    if (hiddenWin) {
      ClearXPConnectSafeContext();
      hiddenWin->Close();
    }
    mHiddenWindow = nsnull;
  }

  return NS_OK;
}

/*
 * Create a new top level window and display the given URL within it...
 */
NS_IMETHODIMP
nsAppShellService::CreateTopLevelWindow(nsIXULWindow *aParent,
                                  nsIURI *aUrl, 
                                  PRBool aShowWindow, PRBool aLoadDefaultPage,
                                  PRUint32 aChromeMask,
                                  PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                  nsIAppShell* aAppShell,
                                  nsIXULWindow **aResult)

{
  nsresult rv;

  rv = JustCreateTopWindow(aParent, aUrl, aShowWindow, aLoadDefaultPage,
                                 aChromeMask, aInitialWidth, aInitialHeight,
                                 PR_FALSE, aAppShell, aResult);

  if (NS_SUCCEEDED(rv)) {
    // the addref resulting from this is the owning addref for this window
    RegisterTopLevelWindow(*aResult);
    (*aResult)->SetZLevel(CalculateWindowZLevel(aParent, aChromeMask));
  }

  return rv;
}

PRUint32
nsAppShellService::CalculateWindowZLevel(nsIXULWindow *aParent,
                                         PRUint32      aChromeMask)
{
  PRUint32 zLevel;

  zLevel = nsIXULWindow::normalZ;
  if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RAISED)
    zLevel = nsIXULWindow::raisedZ;
  else if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_LOWERED)
    zLevel = nsIXULWindow::loweredZ;

#if defined(XP_MAC) || defined(XP_MACOSX)
  /* Platforms on which modal windows are always application-modal, not
     window-modal (that's just the Mac, right?) want modal windows to
     be stacked on top of everyone else.

     On Mac OS X, bind modality to parent window instead of app (ala Mac OS 9)
  */
  PRUint32 modalDepMask = nsIWebBrowserChrome::CHROME_MODAL |
                          nsIWebBrowserChrome::CHROME_DEPENDENT;
  if (aParent && (aChromeMask & modalDepMask)) {
    aParent->GetZLevel(&zLevel);
  }
#else
  /* Platforms with native support for dependent windows (that's everyone
      but pre-Mac OS X, right?) know how to stack dependent windows. On these
      platforms, give the dependent window the same level as its parent,
      so we won't try to override the normal platform behaviour. */
  if ((aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT) && aParent)
    aParent->GetZLevel(&zLevel);
#endif

  return zLevel;
}

/*
 * Just do the window-making part of CreateTopLevelWindow
 */
NS_IMETHODIMP
nsAppShellService::JustCreateTopWindow(nsIXULWindow *aParent,
                                 nsIURI *aUrl, 
                                 PRBool aShowWindow, PRBool aLoadDefaultPage,
                                 PRUint32 aChromeMask,
                                 PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                 PRBool aIsHiddenWindow, nsIAppShell* aAppShell,
                                 nsIXULWindow **aResult)
{
  nsresult rv;
  nsWebShellWindow* window;
  PRBool intrinsicallySized;

  *aResult = nsnull;
  intrinsicallySized = PR_FALSE;
  window = new nsWebShellWindow();
  // Bump count to one so it doesn't die on us while doing init.
  nsCOMPtr<nsIXULWindow> tempRef(window); 
  if (!window)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else {
    nsWidgetInitData widgetInitData;

    if (aIsHiddenWindow)
      widgetInitData.mWindowType = eWindowType_invisible;
    else
      widgetInitData.mWindowType = aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG ?
                                    eWindowType_dialog : eWindowType_toplevel;

    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_POPUP)
      widgetInitData.mWindowType = eWindowType_popup;

    widgetInitData.mContentType = eContentTypeUI;                
    // note default chrome overrides other OS chrome settings, but
    // not internal chrome
    if (aChromeMask & nsIWebBrowserChrome::CHROME_DEFAULT)
      widgetInitData.mBorderStyle = eBorderStyle_default;
    else if ((aChromeMask & nsIWebBrowserChrome::CHROME_ALL) == nsIWebBrowserChrome::CHROME_ALL)
      widgetInitData.mBorderStyle = eBorderStyle_all;
    else {
      widgetInitData.mBorderStyle = eBorderStyle_none; // assumes none == 0x00
      if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_BORDERS)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_border);
      if (aChromeMask & nsIWebBrowserChrome::CHROME_TITLEBAR)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_title);
      if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_CLOSE)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_close);
      if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) {
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_resizeh);
        // only resizable windows get the maximize button (but not dialogs)
        if (!(aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG))
          widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_maximize);
      }
      // all windows (except dialogs) get minimize buttons and the system menu
      if (!(aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG))
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_minimize | eBorderStyle_menu);
      // but anyone can explicitly ask for a minimize button
      if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_MIN) {
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_minimize );
      }  
    }

#if TARGET_CARBON
    // Mac OS X sheet support
    PRUint32 sheetMask = nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                         nsIWebBrowserChrome::CHROME_MODAL;
    if (aParent && ((aChromeMask & sheetMask) == sheetMask))
    {
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_sheet );
    }
#endif

    if (aInitialWidth == nsIAppShellService::SIZE_TO_CONTENT ||
        aInitialHeight == nsIAppShellService::SIZE_TO_CONTENT) {
      aInitialWidth = 1;
      aInitialHeight = 1;
      intrinsicallySized = PR_TRUE;
      window->SetIntrinsicallySized(PR_TRUE);
    }

    rv = window->Initialize(aParent, aAppShell, aUrl,
                            aShowWindow, aLoadDefaultPage,
                            aInitialWidth, aInitialHeight, aIsHiddenWindow, widgetInitData);
      
    if (NS_SUCCEEDED(rv)) {

      // this does the AddRef of the return value
      rv = CallQueryInterface(NS_STATIC_CAST(nsIWebShellWindow*, window), aResult);
      if (aParent)
        aParent->AddChildWindow(*aResult);
    }

    if (aChromeMask & nsIWebBrowserChrome::CHROME_CENTER_SCREEN)
      window->Center(aParent, aParent ? PR_FALSE : PR_TRUE, PR_FALSE);
  }

  return rv;
}


NS_IMETHODIMP
nsAppShellService::CloseTopLevelWindow(nsIXULWindow* aWindow)
{
   nsCOMPtr<nsIWebShellWindow> webShellWin(do_QueryInterface(aWindow));
   NS_ENSURE_TRUE(webShellWin, NS_ERROR_FAILURE);
   return webShellWin->Close();
}

NS_IMETHODIMP
nsAppShellService::GetHiddenWindow(nsIXULWindow **aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);

  *aWindow = mHiddenWindow;
  NS_IF_ADDREF(*aWindow);
  return *aWindow ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenDOMWindow(nsIDOMWindowInternal **aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  NS_ENSURE_TRUE(mHiddenWindow, NS_ERROR_FAILURE);

  rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMWindowInternal> hiddenDOMWindow(do_GetInterface(docShell, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aWindow = hiddenDOMWindow;
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenWindowAndJSContext(nsIDOMWindowInternal **aWindow,
                                               JSContext    **aJSContext)
{
    nsresult rv = NS_OK;
    if ( aWindow && aJSContext ) {
        *aWindow    = nsnull;
        *aJSContext = nsnull;

        if ( mHiddenWindow ) {
            // Convert hidden window to nsIDOMWindowInternal and extract its JSContext.
            do {
                // 1. Get doc for hidden window.
                nsCOMPtr<nsIDocShell> docShell;
                rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
                if (NS_FAILED(rv)) break;

                // 2. Convert that to an nsIDOMWindowInternal.
                nsCOMPtr<nsIDOMWindowInternal> hiddenDOMWindow(do_GetInterface(docShell));
                if(!hiddenDOMWindow) break;

                // 3. Get script global object for the window.
                nsCOMPtr<nsIScriptGlobalObject> sgo;
                sgo = do_QueryInterface( hiddenDOMWindow );
                if (!sgo) { rv = NS_ERROR_FAILURE; break; }

                // 4. Get script context from that.
                nsIScriptContext *scriptContext = sgo->GetContext();
                if (!scriptContext) { rv = NS_ERROR_FAILURE; break; }

                // 5. Get JSContext from the script context.
                JSContext *jsContext = (JSContext*)scriptContext->GetNativeContext();
                if (!jsContext) { rv = NS_ERROR_FAILURE; break; }

                // Now, give results to caller.
                *aWindow    = hiddenDOMWindow.get();
                NS_IF_ADDREF( *aWindow );
                *aJSContext = jsContext;
            } while (0);
        } else {
            rv = NS_ERROR_FAILURE;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*
 * Register a new top level window (created elsewhere)
 */
NS_IMETHODIMP
nsAppShellService::RegisterTopLevelWindow(nsIXULWindow* aWindow)
{
  // tell the window mediator about the new window
  nsCOMPtr<nsIWindowMediator> mediator
    ( do_GetService(NS_WINDOWMEDIATOR_CONTRACTID) );
  NS_ASSERTION(mediator, "Couldn't get window mediator.");

  if (mediator)
    mediator->RegisterWindow(aWindow);

  // tell the window watcher about the new window
  nsCOMPtr<nsPIWindowWatcher> wwatcher ( do_GetService(NS_WINDOWWATCHER_CONTRACTID) );
  NS_ASSERTION(wwatcher, "No windowwatcher?");
  if (wwatcher) {
    nsCOMPtr<nsIDocShell> docShell;
    aWindow->GetDocShell(getter_AddRefs(docShell));
    NS_ASSERTION(docShell, "Window has no docshell");
    if (docShell) {
      nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docShell));
      NS_ASSERTION(domWindow, "Couldn't get DOM window.");
      if (domWindow)
        wwatcher->AddWindow(domWindow, 0);
    }
  }

  // an ongoing attempt to quit is stopped by a newly opened window
  nsCOMPtr<nsIObserverService> obssvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obssvc, "Couldn't get observer service.");

  if (obssvc)
    obssvc->NotifyObservers(aWindow, "xul-window-registered", nsnull);

  return NS_OK;
}


NS_IMETHODIMP
nsAppShellService::UnregisterTopLevelWindow(nsIXULWindow* aWindow)
{
  if (mXPCOMShuttingDown) {
    /* return an error code in order to:
       - avoid doing anything with other member variables while we are in
         the destructor
       - notify the caller not to release the AppShellService after
         unregistering the window
         (we don't want to be deleted twice consecutively to
         mHiddenWindow->Close() in our destructor)
    */
    return NS_ERROR_FAILURE;
  }
  
  NS_ENSURE_ARG_POINTER(aWindow);

  // tell the window mediator
  nsCOMPtr<nsIWindowMediator> mediator
    ( do_GetService(NS_WINDOWMEDIATOR_CONTRACTID) );
  NS_ASSERTION(mediator, "Couldn't get window mediator. Doing xpcom shutdown?");

  if (mediator)
    mediator->UnregisterWindow(aWindow);
	
  // tell the window watcher
  nsCOMPtr<nsPIWindowWatcher> wwatcher ( do_GetService(NS_WINDOWWATCHER_CONTRACTID) );
  NS_ASSERTION(wwatcher, "Couldn't get windowwatcher, doing xpcom shutdown?");
  if (wwatcher) {
    nsCOMPtr<nsIDocShell> docShell;
    aWindow->GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docShell));
      if (domWindow)
        wwatcher->RemoveWindow(domWindow);
    }
  }

  return NS_OK;
}

/* Old function, needs to be removed... it was only used on Mac OS9 */
NS_IMETHODIMP
nsAppShellService::TopLevelWindowIsModal(nsIXULWindow *aWindow, PRBool aModal)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::Observe(nsISupports* aSubject, const char *aTopic,
                           const PRUnichar *aData)
{
  NS_ASSERTION(!strcmp(aTopic, "xpcom-shutdown"), "Unexpected observer topic!");

  mXPCOMShuttingDown = PR_TRUE;
  nsCOMPtr<nsIWebShellWindow> hiddenWin (do_QueryInterface(mHiddenWindow));
  if (hiddenWin) {
    ClearXPConnectSafeContext();
    hiddenWin->Close();
  }

  return NS_OK;
}
