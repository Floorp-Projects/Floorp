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

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsIDOMWindow.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShellWindow.h"
#include "nsWebShellWindow.h"

#include "nsIWindowMediator.h"

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

#ifdef MOZ_FULLCIRCLE
#include "fullsoft.h"
#endif

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


// copied from nsEventQueue.cpp
static char *gEQActivatedNotification = "nsIEventQueueActivated";
static char *gEQDestroyedNotification = "nsIEventQueueDestroyed";

nsAppShellService::nsAppShellService() : mWindowMediator( NULL ), mShuttingDown( PR_FALSE )
{
  NS_INIT_REFCNT();

  mAppShell     = nsnull;
  mWindowList   = nsnull;
  mCmdLineService = nsnull;
  mDeleteCalled		= PR_FALSE;
  mSplashScreen = nsnull;
}

nsAppShellService::~nsAppShellService()
{
  mDeleteCalled = PR_TRUE;
  NS_IF_RELEASE(mAppShell);
  NS_IF_RELEASE(mWindowList);
  NS_IF_RELEASE(mCmdLineService);
  NS_IF_RELEASE(mSplashScreen);
  if (mHiddenWindow)
    mHiddenWindow->Close(); // merely releasing the ref isn't enough!
  if (mWindowMediator)
    nsServiceManager::ReleaseService(kWindowMediatorCID, mWindowMediator);
  /* Note we don't unregister with the observer service
     (RegisterObserver(PR_FALSE)) because, being refcounted, we can't have
     reached our own destructor until after the ObserverService has shut down
     and released us. This means we leak until the end of the application, but
     so what; this is the appshell service. */
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ADDREF(nsAppShellService)
NS_IMPL_RELEASE(nsAppShellService)

NS_INTERFACE_MAP_BEGIN(nsAppShellService)
	NS_INTERFACE_MAP_ENTRY(nsIAppShellService)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAppShellService)
NS_INTERFACE_MAP_END


NS_IMETHODIMP
nsAppShellService::Initialize( nsICmdLineService *aCmdLineService,
                               nsISplashScreen *aSplashScreen )
{
  nsresult rv;
  
#ifdef MOZ_FULLCIRCLE
  FCInitialize();
#endif

  // Remember cmd line service.
  mCmdLineService = aCmdLineService;
  NS_IF_ADDREF( mCmdLineService );

  // Remember the splash screen.
  mSplashScreen = aSplashScreen;
  NS_IF_ADDREF( mSplashScreen );

  // Create the Event Queue for the UI thread...
  nsIEventQueueService* eventQService;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    NS_GET_IID(nsIEventQueueService),
                                    (nsISupports **)&eventQService);
  if (NS_OK == rv) {
    // XXX: What if this fails?
    rv = eventQService->CreateThreadEventQueue();
  }

  // Create the toplevel window list...
  rv = NS_NewISupportsArray(&mWindowList);
  if (NS_FAILED(rv)) {
    goto done;
  }

  nsIMetaCharsetService* metacharset;
  rv = nsServiceManager::GetService(kMetaCharsetCID,
                                    NS_GET_IID(nsIMetaCharsetService),
                                     (nsISupports **) &metacharset);
   if(NS_FAILED(rv)) {
      goto done;
   }
   rv = metacharset->Start();
   if(NS_FAILED(rv)) {
      goto done;
   }
   rv = nsServiceManager::ReleaseService(kMetaCharsetCID, metacharset);

  // Create widget application shell
  rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull, NS_GET_IID(nsIAppShell),
                                    (void**)&mAppShell);
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
  rv = nsServiceManager::GetService(kWindowMediatorCID, NS_GET_IID(nsIWindowMediator),
                                   (nsISupports**) &mWindowMediator);

//  CreateHiddenWindow();	// rjc: now require this to be explicitly called

done:
  return rv;
}



NS_IMETHODIMP
nsAppShellService::CreateHiddenWindow()
{
  nsresult rv;
  nsIURI* url = nsnull;

#if XP_MAC
  rv = NS_NewURI(&url, "chrome://global/content/hiddenWindow.xul");
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWebShellWindow> newWindow;
    rv = JustCreateTopWindow(nsnull, url, PR_FALSE, PR_FALSE,
                        0, nsnull, 0, 0,
                        getter_AddRefs(newWindow));
#else
  rv = NS_NewURI(&url, "about:blank");
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWebShellWindow> newWindow;
    	 rv = JustCreateTopWindow(nsnull, url, PR_FALSE, PR_FALSE,
                        NS_CHROME_ALL_CHROME, nsnull, 100, 100,
                        getter_AddRefs(newWindow));
#endif
    if (NS_SUCCEEDED(rv)) {
      mHiddenWindow = newWindow;
      // RegisterTopLevelWindow(newWindow); -- Mac only
    }
    NS_RELEASE(url);
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
    nsIRegistry *registry = 0;
    nsRegistryKey key;
    nsIEnumerator *components = 0;
    const char *failed = "GetService";
    if ( NS_SUCCEEDED( ( rv = nsServiceManager::GetService( NS_REGISTRY_PROGID,
                                                          NS_GET_IID(nsIRegistry),
                                                          (nsISupports**)&registry ) ) )
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
                                                           &components ) ) )
         &&
         ( failed = "First" )
         &&
         NS_SUCCEEDED( ( rv = components->First() ) ) ) {
        // Enumerate all subtrees
        while ( NS_SUCCEEDED( rv ) && (NS_OK != components->IsDone()) ) {
            nsISupports *base;
            
            rv = components->CurrentItem( &base );
            if ( NS_SUCCEEDED( rv ) ) {
                // Get specific interface.
                nsIRegistryNode *node;
                nsIID nodeIID = NS_IREGISTRYNODE_IID;
                rv = base->QueryInterface( nodeIID, (void**)&node );
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
                    // Release the node.
                    nsCRT::free(name);
                    NS_RELEASE( node );
                } else {
                    // Unable to convert item to registry node, ignore it.
                }

                // Release the current (generic) item.
                NS_RELEASE( base );
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

    // Clean up.
    if ( registry ) {
        // Release enumerator (if necessary).
        NS_IF_RELEASE( components );

        // Release nsIRegistry service.
        nsServiceManager::ReleaseService( NS_REGISTRY_PROGID, registry );
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
    NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
      rv = windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));

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

void*
nsAppShellService::HandleExitEvent(PLEvent* aEvent)
{
  ExitEvent* event = NS_REINTERPRET_CAST(ExitEvent*, aEvent);

  // Tell the appshell to exit
  event->mService->mAppShell->Exit();

  // We're done "shutting down".
  event->mService->mShuttingDown = PR_FALSE;

  return nsnull;
}

void
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
nsAppShellService::CreateTopLevelWindow(nsIWebShellWindow *aParent,
                                  nsIURI *aUrl, 
                                  PRBool aShowWindow, PRBool aLoadDefaultPage,
                                  PRUint32 aChromeMask,
                                  nsIXULWindowCallbacks *aCallbacks,
                                  PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                  nsIWebShellWindow **aResult)

{
  nsresult rv;

  rv = JustCreateTopWindow(aParent, aUrl, aShowWindow, aLoadDefaultPage,
                                 aChromeMask, aCallbacks,
                                 aInitialWidth, aInitialHeight,
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
nsAppShellService::JustCreateTopWindow(nsIWebShellWindow *aParent,
                                 nsIURI *aUrl, 
                                 PRBool aShowWindow, PRBool aLoadDefaultPage,
                                 PRUint32 aChromeMask,
                                 nsIXULWindowCallbacks *aCallbacks,
                                 PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                                 nsIWebShellWindow **aResult)
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

    widgetInitData.mWindowType = aChromeMask & NS_CHROME_OPEN_AS_DIALOG ?
                                 eWindowType_dialog : eWindowType_toplevel;

    // note default chrome overrides other OS chrome settings, but
    // not internal chrome
    if (aChromeMask & NS_CHROME_DEFAULT_CHROME)
      widgetInitData.mBorderStyle = eBorderStyle_default;
    else if ((aChromeMask & NS_CHROME_ALL_CHROME) == NS_CHROME_ALL_CHROME)
      widgetInitData.mBorderStyle = eBorderStyle_all;
    else {
      widgetInitData.mBorderStyle = eBorderStyle_none; // assumes none == 0x00
      if (aChromeMask & NS_CHROME_WINDOW_BORDERS_ON)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_border);
      if (aChromeMask & NS_CHROME_TITLEBAR_ON)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_title);
      if (aChromeMask & NS_CHROME_WINDOW_CLOSE_ON)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_close);
      if (aChromeMask & NS_CHROME_WINDOW_RESIZE_ON)
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_resizeh | eBorderStyle_minimize | eBorderStyle_maximize | eBorderStyle_menu);
    }

    if (aInitialWidth == NS_SIZETOCONTENT ||
        aInitialHeight == NS_SIZETOCONTENT) {
      aInitialWidth = 1;
      aInitialHeight = 1;
      intrinsicallySized = PR_TRUE;
      window->SetIntrinsicallySized(PR_TRUE);
    }

    rv = window->Initialize(aParent, mAppShell, aUrl,
                            aShowWindow, aLoadDefaultPage, aCallbacks,
                            aInitialWidth, aInitialHeight, widgetInitData);
      
    if (NS_SUCCEEDED(rv)) {

      // this does the AddRef of the return value
      rv = window->QueryInterface(NS_GET_IID(nsIWebShellWindow), (void **) aResult);
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
  }

  return rv;
}


NS_IMETHODIMP
nsAppShellService::CloseTopLevelWindow(nsIWebShellWindow* aWindow)
{
  return aWindow->Close();
}

NS_IMETHODIMP
nsAppShellService::GetHiddenWindow(nsIWebShellWindow **aWindow)
{
  nsIWebShellWindow *rv;

  NS_ASSERTION(aWindow, "null param to GetHiddenWindow");
  rv = mHiddenWindow;
  NS_IF_ADDREF(rv);
  *aWindow = rv;
  return rv ? NS_OK : NS_ERROR_FAILURE;
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
                // 1. Get webshell for hidden window.
                nsCOMPtr<nsIWebShell> webShell;
                rv = mHiddenWindow->GetWebShell( *getter_AddRefs( webShell ) );
                if (NS_FAILED(rv)) break;

                // 2. Convert that to an nsIDOMWindow.
                nsCOMPtr<nsIDOMWindow> hiddenDOMWindow;
                rv = mHiddenWindow->ConvertWebShellToDOMWindow( webShell,
                                                                getter_AddRefs( hiddenDOMWindow ) );
                if (NS_FAILED(rv)) break;

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

/* Create a Window, run it modally, and destroy it.  To make initial control
   settings or get information out of the dialog before dismissal, use
   event handlers.  This wrapper method is desirable because of the
   complications creeping in to the modal window story: there's a lot of setup.
   See the code..

   If a window is passed in via the first parameter, that window will be
   the one displayed modally.  If no window is passed in (if *aWindow is null)
   the window created will be returned in *aWindow.  Note that by the time
   this function exits, that window has been partially destroyed.  We return it
   anyway, in the hopes that it may be queried for results, somehow.
   This may be a mistake.  It is returned addrefed (by the QueryInterface
   to nsIWebShellWindow in CreateTopLevelWindow).
*/
NS_IMETHODIMP
nsAppShellService::RunModalDialog(
                      nsIWebShellWindow **aWindow,
                      nsIWebShellWindow *aParent,
                      nsIURI *aUrl, 
                      PRUint32 aChromeMask,
                      nsIXULWindowCallbacks *aCallbacks,
                      PRInt32 aInitialWidth, PRInt32 aInitialHeight)
{
  nsresult          rv;
  nsIWebShellWindow *theWindow;
  nsIEventQueue     *pushedQueue;

  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  pushedQueue = NULL;
  if (aWindow && *aWindow) {
    theWindow = *aWindow;
    NS_ADDREF(theWindow);
    rv = NS_OK;
  } else {
    eventQService->PushThreadEventQueue(&pushedQueue);
    rv = CreateTopLevelWindow(aParent, aUrl, PR_TRUE, PR_TRUE, aChromeMask,
            aCallbacks, aInitialWidth, aInitialHeight, &theWindow);
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWidget> parentWindowWidgetThing;
    nsresult gotParent;
    gotParent = aParent ? aParent->GetWidget(*getter_AddRefs(parentWindowWidgetThing)) :
                          NS_ERROR_FAILURE;
    // Windows OS wants the parent disabled for modality
    if (NS_SUCCEEDED(gotParent))
      parentWindowWidgetThing->Enable(PR_FALSE);
    theWindow->ShowModal();
    if (NS_SUCCEEDED(gotParent))
      parentWindowWidgetThing->Enable(PR_TRUE);

    // return the used window if possible, or otherwise get rid of it
    if (aWindow)
      if (*aWindow)
        NS_RELEASE(theWindow); // we borrowed it, now let it go
      else
        *aWindow = theWindow;  // and it's addrefed from Create...
    else
      NS_RELEASE(theWindow);   // can't return it; let it go
  }

  if (pushedQueue)
    eventQService->PopThreadEventQueue(pushedQueue);

  return rv;
}


/*
 * Register a new top level window (created elsewhere)
 */
NS_IMETHODIMP
nsAppShellService::RegisterTopLevelWindow(nsIWebShellWindow* aWindow)
{
  nsresult rv;

  nsIWebShellContainer* wsc;
  rv = aWindow->QueryInterface(NS_GET_IID(nsIWebShellContainer), (void **) &wsc);
  if (NS_SUCCEEDED(rv)) {
    mWindowList->AppendElement(wsc);
    NS_RELEASE(wsc);
    
    nsIWindowMediator* service;
		if (NS_SUCCEEDED(nsServiceManager::GetService(kWindowMediatorCID, NS_GET_IID(nsIWindowMediator), (nsISupports**) &service ) ) )
		{
			service->RegisterWindow( aWindow);
			nsServiceManager::ReleaseService(kWindowMediatorCID, service);
		}
  }
  return rv;
}


NS_IMETHODIMP
nsAppShellService::UnregisterTopLevelWindow(nsIWebShellWindow* aWindow)
{
  

	if (mDeleteCalled) {
		// return an error code in order to:
		// - avoid doing anything with other member variables while we are in the destructor
		// - notify the caller not to release the AppShellService after unregistering the window
		//   (we don't want to be deleted twice consecutively to mHiddenWindow->Close() in our destructor)
		return NS_ERROR_FAILURE;
	}
  
  nsIWindowMediator* service;
  if (NS_SUCCEEDED(nsServiceManager::GetService(kWindowMediatorCID, NS_GET_IID(nsIWindowMediator), (nsISupports**) &service ) ) )
  {
	service->UnregisterWindow( aWindow );
	nsServiceManager::ReleaseService(kWindowMediatorCID, service);
  }
	
  nsresult rv;

  nsIWebShellContainer* wsc;
  rv = aWindow->QueryInterface(NS_GET_IID(nsIWebShellContainer), (void **) &wsc);
  if (NS_SUCCEEDED(rv)) {
    mWindowList->RemoveElement(wsc);
    NS_RELEASE(wsc);
  }
  PRUint32 cnt;
  rv = mWindowList->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (0 == cnt)
  {
  #if XP_MAC
  	if (mHiddenWindow)
  	{
	  	// Given hidden window the focus so it puts up the menu
	 	nsIWidget* widget = NULL;
	 	mHiddenWindow->GetWidget( widget );
	 	if( widget )
	 	{
	 		widget->SetFocus();
	 	
	  		widget->Release();
	  	}
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
  if (topic.Equals(gEQActivatedNotification)) {
    nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
    if (eq)
      mAppShell->ListenToEventQueue(eq, PR_TRUE);
  } else if (topic.Equals(gEQDestroyedNotification)) {
    nsCOMPtr<nsIEventQueue> eq(do_QueryInterface(aSubject));
    if (eq)
      mAppShell->ListenToEventQueue(eq, PR_FALSE);
  }
  return NS_OK;
}

/* ask nsIObserverService to tell us about nsEventQueue notifications */
void nsAppShellService::RegisterObserver(PRBool aRegister)
{
  nsresult           rv;
  nsISupports        *glop;

  nsAutoString topicA(gEQActivatedNotification);
  nsAutoString topicB(gEQDestroyedNotification);

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
    // Hide the splash screen (and release it) if there is one.
    if ( mSplashScreen ) {
        mSplashScreen->Hide();
        NS_RELEASE( mSplashScreen );
    }
    return NS_OK;
}
