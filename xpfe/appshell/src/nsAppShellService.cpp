/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 */


#include "nsIAppShellService.h"
#include "nsISupportsArray.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsXPComFactory.h"    /* template implementation of a XPCOM factory */
#include "nsIXPConnect.h"

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsIDOMWindow.h"
#include "nsIWebShellWindow.h"
#include "nsWebShellWindow.h"

#include "nsIAppShellComponent.h"
#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "nsICmdLineService.h"
#include "nsCRT.h"
#ifdef NS_DEBUG
#include "prprf.h"    
#endif

#include "nsWidgetsCID.h"
#include "nsIStreamObserver.h"

#include "nsMetaCharsetCID.h"
#include "nsIMetaCharsetService.h"

/* For implementing GetHiddenWindowAndJSContext */
#include "nsIScriptGlobalObject.h"
#include "jsapi.h"

#include "nsAppShellService.h"

/* Define Class IDs */
static NS_DEFINE_CID(kAppShellCID,          NS_APPSHELL_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kMetaCharsetCID, NS_META_CHARSET_CID);
static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);

// copied from nsEventQueue.cpp
static char *gEQActivatedNotification = "nsIEventQueueActivated";
static char *gEQDestroyedNotification = "nsIEventQueueDestroyed";

nsAppShellService::nsAppShellService() : 
  mAppShell( nsnull ),
  mWindowList( nsnull ),
  mCmdLineService( nsnull ),
  mWindowMediator( nsnull ), 
  mHiddenWindow( nsnull ),
  mDeleteCalled( PR_FALSE ),
  mSplashScreen( nsnull ),
  mNativeAppSupport( nsnull ),
  mShuttingDown( PR_FALSE )
{
  NS_INIT_REFCNT();
}

nsAppShellService::~nsAppShellService()
{
  mDeleteCalled = PR_TRUE;
  nsCOMPtr<nsIWebShellWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
  if(hiddenWin)
    hiddenWin->Close();
  /* Note we don't unregister with the observer service
     (RegisterObserver(PR_FALSE)) because, being refcounted, we can't have
     reached our own destructor until after the ObserverService has shut down
     and released us. This means we leak until the end of the application, but
     so what; this is the appshell service. */
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_THREADSAFE_ADDREF(nsAppShellService)
NS_IMPL_THREADSAFE_RELEASE(nsAppShellService)

NS_INTERFACE_MAP_BEGIN(nsAppShellService)
	NS_INTERFACE_MAP_ENTRY(nsIAppShellService)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAppShellService)
NS_INTERFACE_MAP_END_THREADSAFE


NS_IMETHODIMP
nsAppShellService::Initialize( nsICmdLineService *aCmdLineService,
                               nsISupports *aNativeAppSupportOrSplashScreen )
{
  nsresult rv;
  
  // Remember cmd line service.
  mCmdLineService = aCmdLineService;

  // Remember where the native app support lives.
  mNativeAppSupport = do_QueryInterface( aNativeAppSupportOrSplashScreen );

  // Or, remember the splash screen (for backward compatibility).
  if ( !mNativeAppSupport ) {
      mSplashScreen = do_QueryInterface( aNativeAppSupportOrSplashScreen );
  }

  NS_WITH_SERVICE(nsIMetaCharsetService, metacharset, kMetaCharsetCID, &rv);
  if(NS_FAILED(rv)) {
    goto done;
  }

  // Create the toplevel window list...
  rv = NS_NewISupportsArray(getter_AddRefs(mWindowList));
  if (NS_FAILED(rv)) {
    goto done;
  }

  rv = metacharset->Start();
  if(NS_FAILED(rv)) {
    goto done;
  }

  // Create widget application shell
  rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull,
                                          NS_GET_IID(nsIAppShell),
                                          (void**)getter_AddRefs(mAppShell));
  if (NS_FAILED(rv)) {
    goto done;
  }

  rv = mAppShell->Create(0, nsnull);

  if (NS_FAILED(rv)) {
      goto done;
  }

  // listen to EventQueues' comings and goings. do this after the appshell
  // has been created, but after the event queue has been created. that
  // latter bit is unfortunate, but we deal with it.
  RegisterObserver(PR_TRUE);
 
// enable window mediation
  mWindowMediator = do_GetService(kWindowMediatorCID);

//  CreateHiddenWindow();	// rjc: now require this to be explicitly called

done:
  return rv;
}



NS_IMETHODIMP
nsAppShellService::CreateHiddenWindow()
{
  nsresult rv;
#if XP_MAC
  const char* hiddenWindowURL = "chrome://global/content/hiddenWindow.xul";
  PRUint32    chromeMask = 0;
  PRInt32     initialHeight = 0, initialWidth = 0;
#else
  const char* hiddenWindowURL = "about:blank";
  PRUint32    chromeMask =  nsIWebBrowserChrome::CHROME_ALL;
  PRInt32     initialHeight = 100, initialWidth = 100;
#endif

  nsCOMPtr<nsIURI> url;
  rv = NS_NewURI(getter_AddRefs(url), hiddenWindowURL);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIXULWindow> newWindow;
    rv = JustCreateTopWindow(nsnull, url, PR_FALSE, PR_FALSE,
                        chromeMask, initialWidth, initialHeight,
                        getter_AddRefs(newWindow));
    if (NS_SUCCEEDED(rv)) {
      mHiddenWindow = newWindow;
      
      // Set XPConnect's fallback JSContext (used for JS Components)
      // to the DOM JSContext for this thread, so that DOM-to-XPConnect
      // conversions get the JSContext private magic they need to
      // succeed.
      NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);
      if (NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIDOMWindow> junk;
      JSContext *cx;
      rv = GetHiddenWindowAndJSContext(getter_AddRefs(junk), &cx);
      if (NS_FAILED(rv)) return rv;
      rv = xpc->SetSafeJSContextForCurrentThread(cx);
      if (NS_FAILED(rv)) return rv;

      // RegisterTopLevelWindow(newWindow); -- Mac only
    }
  }
  NS_ASSERTION(NS_SUCCEEDED(rv), "HiddenWindow not created");
  return(rv);
}

 NS_IMETHODIMP  nsAppShellService::EnumerateAndInitializeComponents(void)
 {
 	 // Initialize each registered component.
 	 EnumerateComponents( &nsAppShellService::InitializeComponent );
 	 return NS_OK;
 }
// Apply function (Initialize/Shutdown) to each app shell component.
void
nsAppShellService::EnumerateComponents( EnumeratorMemberFunction function ) {
    nsresult rv;
    nsRegistryKey key;
    nsCOMPtr<nsIEnumerator> components;
    const char *failed = "GetService";
    NS_WITH_SERVICE(nsIRegistry, registry, NS_REGISTRY_PROGID, &rv);
    if ( NS_SUCCEEDED(rv) 
         &&
         ( failed = "Open" )
         &&
         NS_SUCCEEDED( ( rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry) ) )
         &&
         ( failed = "GetSubtree" )
         &&
         NS_SUCCEEDED( ( rv = registry->GetSubtree( nsIRegistry::Common,
                                                    NS_IAPPSHELLCOMPONENT_KEY,
                                                    &key ) ) )
         &&
         ( failed = "EnumerateSubtrees" )
         &&
         NS_SUCCEEDED( ( rv = registry->EnumerateSubtrees( key,
                                               getter_AddRefs(components )) ) )
         &&
         ( failed = "First" )
         &&
         NS_SUCCEEDED( ( rv = components->First() ) ) ) {
        // Enumerate all subtrees
        while ( NS_SUCCEEDED( rv ) && (NS_OK != components->IsDone()) ) {
            nsCOMPtr<nsISupports> base;
            
            rv = components->CurrentItem( getter_AddRefs(base) );
            if ( NS_SUCCEEDED( rv ) ) {
                // Get specific interface.
                nsCOMPtr<nsIRegistryNode> node;
                nsIID nodeIID = NS_IREGISTRYNODE_IID;
                rv = base->QueryInterface( nodeIID,
                                           (void**)getter_AddRefs(node) );
                // Test that result.
                if ( NS_SUCCEEDED( rv ) ) {
                    // Get node name.
                    char *name;
                    rv = node->GetNameUTF8( &name );
                    if ( NS_SUCCEEDED( rv ) ) {
                        // If this is a CID of a component; apply function to it.
                        nsCID cid;
                        if ( cid.Parse( name ) ) {
                            (this->*function)( cid );
                        } else {
                            // Not a valid CID, ignore it.
                        }
                    } else {
                        // Unable to get subkey name, ignore it.
                    }
                    nsCRT::free(name);
                } else {
                    // Unable to convert item to registry node, ignore it.
                }

            } else {
                // Unable to get current item, ignore it.
            }

            // Go on to next component, if this fails, we quit.
            rv = components->Next();
        }
    } else {
        // Unable to set up for subkey enumeration.
        #ifdef NS_DEBUG
            printf( "Unable to enumerator app shell components, %s rv=0x%08X\n",
                    failed, (int)rv );
        #endif
    }

    return;
}

void
nsAppShellService::InitializeComponent( const nsCID &aComponentCID ) {
    // Attempt to create instance of the component.
    nsIAppShellComponent *component;
    nsresult rv = nsComponentManager::CreateInstance( aComponentCID,
                                                      0,
                                                      NS_GET_IID(nsIAppShellComponent),
                                                      (void**)&component );
    if ( NS_SUCCEEDED( rv ) ) {
        // Then tell it to initialize (it may RegisterService itself).
        rv = component->Initialize( this, mCmdLineService );
        #ifdef NS_DEBUG
            char *name = aComponentCID.ToString();
            printf( "Initialized app shell component %s, rv=0x%08X\n",
                    name, (int)rv );
            Recycle(name);
        #endif
        // Release it (will live on if it registered itself as service).
        component->Release();
    } else {
        // Error creating component.
        #ifdef NS_DEBUG
            char *name = aComponentCID.ToString();
            printf( "Error creating app shell component %s, rv=0x%08X\n",
                    name, (int)rv );
            Recycle(name);
        #endif
    }

    return;
}

void
nsAppShellService::ShutdownComponent( const nsCID &aComponentCID ) {
    // Attempt to create instance of the component (must be a service).
    nsIAppShellComponent *component;
    nsresult rv = nsServiceManager::GetService( aComponentCID,
                                                NS_GET_IID(nsIAppShellComponent),
                                                (nsISupports**)&component );
    if ( NS_SUCCEEDED( rv ) ) {
        // Instance accessed, tell it to shutdown.
        rv = component->Shutdown();
#ifdef NS_DEBUG
            char *name = aComponentCID.ToString();
            printf( "Shut down app shell component %s, rv=0x%08X\n",
                    name, (int)rv );
            nsCRT::free(name);
#endif
        // Release the service.
        nsServiceManager::ReleaseService( aComponentCID, component );
    } else {
        // Error getting component service (perhaps due to that component not being
        // a service).
#ifdef NS_DEBUG
            char *name = aComponentCID.ToString();
            printf( "Unable to shut down app shell component %s, rv=0x%08X\n",
                    name, (int)rv );
            nsCRT::free(name);
#endif
    }

    return;
}

NS_IMETHODIMP
nsAppShellService::Run(void)
{
  return mAppShell->Run();
}


NS_IMETHODIMP
nsAppShellService::Quit()
{
  // Quit the application. We will asynchronously call the appshell's
  // Exit() method via the ExitCallback() to allow one last pass
  // through any events in the queue. This guarantees a tidy cleanup.
  nsresult rv = NS_OK;

  if (! mShuttingDown) {
    mShuttingDown = PR_TRUE;

    // Enumerate through each open window and close it
    if (mWindowMediator) {
      nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
      rv = mWindowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));

      if (NS_SUCCEEDED(rv)) {
        PRBool more;

        while (1) {
          rv = windowEnumerator->HasMoreElements(&more);
          if (NS_FAILED(rv) || !more)
            break;

          nsCOMPtr<nsISupports> isupports;
          rv = windowEnumerator->GetNext(getter_AddRefs(isupports));
          if (NS_FAILED(rv))
            break;

          nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(isupports);
          NS_ASSERTION(window != nsnull, "not an nsIDOMWindow");
          if (! window)
            continue;

          window->Close();
        }
      }
    }

    {
      nsCOMPtr<nsIWebShellWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
      if (hiddenWin)
        hiddenWin->Close();
      mHiddenWindow = nsnull;
    }
    
    // Note that we don't allow any premature returns from the above
    // loop: no matter what, make sure we send the exit event.  If
    // worst comes to worst, we'll do a leaky shutdown but we WILL
    // shut down. Well, assuming that all *this* stuff works ;-).
    nsCOMPtr<nsIEventQueueService> svc = do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> queue;
    rv = svc->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

    ExitEvent* event = new ExitEvent;
    if (! event)
      return NS_ERROR_OUT_OF_MEMORY;

    PL_InitEvent(NS_REINTERPRET_CAST(PLEvent*, event),
                 nsnull,
                 HandleExitEvent,
                 DestroyExitEvent);

    event->mService = this;
    NS_ADDREF(event->mService);

    rv = queue->EnterMonitor();
    if (NS_SUCCEEDED(rv)) {
      rv = queue->PostEvent(NS_REINTERPRET_CAST(PLEvent*, event));
    }
    (void) queue->ExitMonitor();

    if (NS_FAILED(rv)) {
      NS_RELEASE(event->mService);
      delete event;
    }
  }

  return rv;
}

void* PR_CALLBACK
nsAppShellService::HandleExitEvent(PLEvent* aEvent)
{
  ExitEvent* event = NS_REINTERPRET_CAST(ExitEvent*, aEvent);

  // Tell the appshell to exit
  event->mService->mAppShell->Exit();

  // We're done "shutting down".
  event->mService->mShuttingDown = PR_FALSE;

  return nsnull;
}

void PR_CALLBACK
nsAppShellService::DestroyExitEvent(PLEvent* aEvent)
{
  ExitEvent* event = NS_REINTERPRET_CAST(ExitEvent*, aEvent);
  NS_RELEASE(event->mService);
  delete event;
}

NS_IMETHODIMP
nsAppShellService::Shutdown(void)
{
  // Shutdown all components.
  EnumerateComponents(&nsAppShellService::ShutdownComponent);

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
                                  nsIXULWindow **aResult)

{
  nsresult rv;

  rv = JustCreateTopWindow(aParent, aUrl, aShowWindow, aLoadDefaultPage,
                                 aChromeMask, aInitialWidth, aInitialHeight,
                                 aResult);

  if (NS_SUCCEEDED(rv))
    // the addref resulting from this is the owning addref for this window
    RegisterTopLevelWindow(*aResult);

  return rv;
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
                                 nsIXULWindow **aResult)
{
  nsresult rv;
  nsWebShellWindow* window;
  PRBool intrinsicallySized;
  PRUint32 zlevel;
  PRBool contentScrollbars;

  *aResult = nsnull;
  intrinsicallySized = PR_FALSE;
  contentScrollbars = PR_FALSE;
  window = new nsWebShellWindow();
  // Bump count to one so it doesn't die on us while doing init.
  nsCOMPtr<nsIXULWindow> tempRef(window); 
  if (!window)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else {
    nsWidgetInitData widgetInitData;

    widgetInitData.mWindowType = aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG ?
                                 eWindowType_dialog : eWindowType_toplevel;

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
        /* Associate the resize flag with min/max buttons and system menu.
           but not for dialogs. This is logic better associated with the
           platform implementation, and partially covered by the
           eBorderStyle_default style. But since I know of no platform
           that wants min/max buttons on dialogs, it works here, too.
           If you have such a platform, this is where the fun starts: */
        if (!(aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG))
          widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_minimize | eBorderStyle_maximize | eBorderStyle_menu);
      }
    }

    if (aChromeMask & nsIWebBrowserChrome::CHROME_SCROLLBARS)
      contentScrollbars = PR_TRUE;

    zlevel = nsIXULWindow::normalZ;
    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RAISED)
      zlevel = nsIXULWindow::raisedZ;
    else if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_LOWERED)
      zlevel = nsIXULWindow::loweredZ;
#ifdef XP_MAC
    /* Platforms on which modal windows are always application-modal, not
       window-modal (that's just the Mac, right) want modal windows to
       be stacked on top of everyone else. */
    zlevel = nsIXULWindow::highestZ;
#else
    /* Platforms with native support for dependent windows (that's everyone
       but the Mac, right?) know how to stack dependent windows. On these
       platforms, give the dependent window the same level as its parent,
       so we won't try to override the normal platform behaviour. */
    if ((aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT) && aParent)
      aParent->GetZlevel(&zlevel);
#endif

    if (aInitialWidth == NS_SIZETOCONTENT ||
        aInitialHeight == NS_SIZETOCONTENT) {
      aInitialWidth = 1;
      aInitialHeight = 1;
      intrinsicallySized = PR_TRUE;
      window->SetIntrinsicallySized(PR_TRUE);
    }

    rv = window->Initialize(aParent, mAppShell, aUrl,
                            aShowWindow, aLoadDefaultPage,
                            contentScrollbars, zlevel,
                            aInitialWidth, aInitialHeight, widgetInitData);
      
    if (NS_SUCCEEDED(rv)) {

      // this does the AddRef of the return value
      rv = CallQueryInterface(NS_STATIC_CAST(nsIWebShellWindow*, window), aResult);
#if 0
      // If intrinsically sized, don't show until we have the size figured out
      // (6 Dec 99: this is causing new windows opened from anchor links to
      // be visible too early. All windows should (and appear to in testing)
      // become visible in nsWebShellWindow::OnEndDocumentLoad. Timidly
      // commenting out for now.)
      if (aShowWindow && !intrinsicallySized)
        window->Show(PR_TRUE);
#endif

    }

    if (aChromeMask & nsIWebBrowserChrome::CHROME_CENTER_SCREEN)
      window->Center(nsnull, PR_TRUE, PR_FALSE);
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
nsAppShellService::GetHiddenDOMWindow(nsIDOMWindow **aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMWindow> hiddenDOMWindow(do_GetInterface(docShell, &rv));
  if (NS_FAILED(rv)) return rv;

  *aWindow = hiddenDOMWindow;
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenWindowAndJSContext(nsIDOMWindow **aWindow,
                                               JSContext    **aJSContext)
{
    nsresult rv = NS_OK;
    if ( aWindow && aJSContext ) {
        *aWindow    = nsnull;
        *aJSContext = nsnull;

        if ( mHiddenWindow ) {
            // Convert hidden window to nsIDOMWindow and extract its JSContext.
            do {
                // 1. Get doc for hidden window.
                nsCOMPtr<nsIDocShell> docShell;
                rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
                if (NS_FAILED(rv)) break;

                // 2. Convert that to an nsIDOMWindow.
                nsCOMPtr<nsIDOMWindow> hiddenDOMWindow(do_GetInterface(docShell));
                if(!hiddenDOMWindow) break;

                // 3. Get script global object for the window.
                nsCOMPtr<nsIScriptGlobalObject> sgo;
                sgo = do_QueryInterface( hiddenDOMWindow );
                if (!sgo) { rv = NS_ERROR_FAILURE; break; }

                // 4. Get script context from that.
                nsCOMPtr<nsIScriptContext> scriptContext;
                sgo->GetContext( getter_AddRefs( scriptContext ) );
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
   mWindowList->AppendElement(aWindow);

   if(mWindowMediator)
      mWindowMediator->RegisterWindow(aWindow);

   return NS_OK;
}


NS_IMETHODIMP
nsAppShellService::UnregisterTopLevelWindow(nsIXULWindow* aWindow)
{
	if (mDeleteCalled) {
		// return an error code in order to:
		// - avoid doing anything with other member variables while we are in the destructor
		// - notify the caller not to release the AppShellService after unregistering the window
		//   (we don't want to be deleted twice consecutively to mHiddenWindow->Close() in our destructor)
		return NS_ERROR_FAILURE;
	}
  
  if(mWindowMediator)
     mWindowMediator->UnregisterWindow(aWindow);
	
  nsresult rv;

  mWindowList->RemoveElement(aWindow);

  PRUint32 cnt;
  rv = mWindowList->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (0 == cnt)
  {
  #if XP_MAC
   nsCOMPtr<nsIBaseWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
  	if (hiddenWin)
  	{
	  	// Given hidden window the focus so it puts up the menu
      nsCOMPtr<nsIWidget> widget;
	 	hiddenWin->GetMainWidget(getter_AddRefs(widget));
	 	if(widget)
	 		widget->SetFocus();
	}
	else
	{
		// if no hidden window is available (perhaps due to initial
		// Profile Manager window being cancelled), then just quit
		Quit();
	}
  #else
  	  Quit();
  #endif 
  }
  return rv;
}



//-------------------------------------------------------------------------
// nsIObserver interface and friends
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShellService::Observe(nsISupports *aSubject,
                                         const PRUnichar *aTopic,
                                         const PRUnichar *)
{
  nsAutoString topic(aTopic);

  NS_ASSERTION(mAppShell, "appshell service notified before appshell built");
  if (topic.EqualsWithConversion(gEQActivatedNotification)) {
    nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
    if (eq) {
      PRBool isNative = PR_TRUE;
      // we only add native event queues to the appshell
      eq->IsQueueNative(&isNative);
      if (isNative)
        mAppShell->ListenToEventQueue(eq, PR_TRUE);
    }
  } else if (topic.EqualsWithConversion(gEQDestroyedNotification)) {
    nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
    if (eq) {
      PRBool isNative = PR_TRUE;
      // we only remove native event queues from the appshell
      eq->IsQueueNative(&isNative);
      if (isNative)
        mAppShell->ListenToEventQueue(eq, PR_FALSE);
    }
  }
  return NS_OK;
}

/* ask nsIObserverService to tell us about nsEventQueue notifications */
void nsAppShellService::RegisterObserver(PRBool aRegister)
{
  nsresult           rv;
  nsISupports        *glop;

  nsAutoString topicA; topicA.AssignWithConversion(gEQActivatedNotification);
  nsAutoString topicB; topicB.AssignWithConversion(gEQDestroyedNotification);

  // here's a silly dance. seems better to do it than not, though...
  nsCOMPtr<nsIObserver> weObserve(do_QueryInterface(NS_STATIC_CAST(nsIObserver *, this)));

  NS_ASSERTION(weObserve, "who's been chopping bits off nsAppShellService?");

  rv = nsServiceManager::GetService(NS_OBSERVERSERVICE_PROGID,
                           NS_GET_IID(nsIObserverService), &glop);
  if (NS_SUCCEEDED(rv)) {
    nsIObserverService *os = NS_STATIC_CAST(nsIObserverService*,glop);
    if (aRegister) {
      os->AddObserver(weObserve, topicA.GetUnicode());
      os->AddObserver(weObserve, topicB.GetUnicode());
    } else {
      os->RemoveObserver(weObserve, topicA.GetUnicode());
      os->RemoveObserver(weObserve, topicB.GetUnicode());
    }
    nsServiceManager::ReleaseService(NS_OBSERVERSERVICE_PROGID, glop);
  }
}

NS_IMETHODIMP
nsAppShellService::HideSplashScreen() {
    // Hide the splash screen.
    if ( mNativeAppSupport ) {
        mNativeAppSupport->HideSplashScreen();
    } else if ( mSplashScreen ) {
        mSplashScreen->Hide();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::GetNativeAppSupport( nsINativeAppSupport **aResult ) {
    NS_ENSURE_ARG( aResult );
    *aResult = mNativeAppSupport;
    NS_IF_ADDREF( *aResult );
    return NS_OK;
}
