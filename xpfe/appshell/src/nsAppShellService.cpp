/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsWeakReference.h"
#include "nsXPComFactory.h"    /* template implementation of a XPCOM factory */
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWebShellWindow.h"
#include "nsWebShellWindow.h"

#include "nsIAppShellComponent.h"
#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "nsICmdLineService.h"
#include "nsCRT.h"
#include "nsITimelineService.h"
#ifdef NS_DEBUG
#include "prprf.h"    
#endif

#include "nsWidgetsCID.h"
#include "nsIRequestObserver.h"

/* For implementing GetHiddenWindowAndJSContext */
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

#include "nsAppShellService.h"
#include "nsIProfileInternal.h"

/* Define Class IDs */
static NS_DEFINE_CID(kAppShellCID,          NS_APPSHELL_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
static char *sWindowWatcherContractID = "@mozilla.org/embedcomp/window-watcher;1";

// copied from nsEventQueue.cpp
static char *gEQActivatedNotification = "nsIEventQueueActivated";
static char *gEQDestroyedNotification = "nsIEventQueueDestroyed";

NS_NAMED_LITERAL_STRING(gSkinSelectedTopic, "skin-selected");
NS_NAMED_LITERAL_STRING(gLocaleSelectedTopic, "locale-selected");
NS_NAMED_LITERAL_STRING(gInstallRestartTopic, "xpinstall-restart");

nsAppShellService::nsAppShellService() : 
  mAppShell( nsnull ),
  mCmdLineService( nsnull ),
  mWindowMediator( nsnull ), 
  mHiddenWindow( nsnull ),
  mDeleteCalled( PR_FALSE ),
  mSplashScreen( nsnull ),
  mNativeAppSupport( nsnull ),
  mShuttingDown( PR_FALSE ),
  mQuitOnLastWindowClosing( PR_TRUE )
{
  NS_INIT_REFCNT();
}

nsAppShellService::~nsAppShellService()
{
  mDeleteCalled = PR_TRUE;
  nsCOMPtr<nsIWebShellWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
  if(hiddenWin) {
    ClearXPConnectSafeContext();
    hiddenWin->Close();
  }
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
  mNativeAppSupport = do_QueryInterface(aNativeAppSupportOrSplashScreen);

  // Or, remember the splash screen (for backward compatibility).
  if (!mNativeAppSupport)
    mSplashScreen = do_QueryInterface(aNativeAppSupportOrSplashScreen);

  NS_TIMELINE_ENTER("nsComponentManager::CreateInstance.");
  // Create widget application shell
  rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull,
                                          NS_GET_IID(nsIAppShell),
                                          (void**)getter_AddRefs(mAppShell));
  NS_TIMELINE_LEAVE("nsComponentManager::CreateInstance");
  if (NS_FAILED(rv))
    goto done;

  rv = mAppShell->Create(0, nsnull);
  if (NS_FAILED(rv))
    goto done;

  // listen to EventQueues' comings and goings. do this after the appshell
  // has been created, but after the event queue has been created. that
  // latter bit is unfortunate, but we deal with it.
  RegisterObserver(PR_TRUE);
 
  // enable window mediation (and fail if we can't get the mediator)
  mWindowMediator = do_GetService(kWindowMediatorCID, &rv);
  mWindowWatcher = do_GetService(sWindowWatcherContractID);

done:
  return rv;
}

nsresult 
nsAppShellService::SetXPConnectSafeContext()
{
  nsresult rv;

  nsCOMPtr<nsIThreadJSContextStack> cxstack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMWindowInternal> junk;
  JSContext *cx;
  rv = GetHiddenWindowAndJSContext(getter_AddRefs(junk), &cx);
  if (NS_FAILED(rv))
    return rv;

  return cxstack->SetSafeJSContext(cx);
}  

nsresult nsAppShellService::ClearXPConnectSafeContext()
{
  nsresult rv;

  nsCOMPtr<nsIThreadJSContextStack> cxstack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMWindowInternal> junk;
  JSContext *cx;
  rv = GetHiddenWindowAndJSContext(getter_AddRefs(junk), &cx);
  if (NS_FAILED(rv))
    return rv;

  JSContext *safe_cx;
  rv = cxstack->GetSafeJSContext(&safe_cx);
  if (NS_FAILED(rv))
    return rv;

  if (cx == safe_cx)
    rv = cxstack->SetSafeJSContext(nsnull);

  return rv;
}

NS_IMETHODIMP
nsAppShellService::DoProfileStartup(nsICmdLineService *aCmdLineService, PRBool canInteract)
{
    nsresult rv;

    nsCOMPtr<nsIProfileInternal> profileMgr(do_GetService(NS_PROFILE_CONTRACTID, &rv));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get profile manager");
    if (NS_FAILED(rv)) return rv;

    PRBool saveQuitOnLastWindowClosing = mQuitOnLastWindowClosing;
    mQuitOnLastWindowClosing = PR_FALSE;
        
    // If we being launched in turbo mode, profile mgr cannot show UI
    rv = profileMgr->StartupWithArgs(aCmdLineService, canInteract);
    if (!canInteract && rv == NS_ERROR_PROFILE_REQUIRES_INTERACTION) {
        NS_WARNING("nsIProfileInternal::StartupWithArgs returned NS_ERROR_PROFILE_REQUIRES_INTERACTION");       
        rv = NS_OK;
    }
    
    mQuitOnLastWindowClosing = saveQuitOnLastWindowClosing;

    return rv;
}

NS_IMETHODIMP
nsAppShellService::CreateHiddenWindow()
{
  nsresult rv;
  PRInt32 initialHeight = 100, initialWidth = 100;
#if XP_MAC
  const char* hiddenWindowURL = "chrome://global/content/hiddenWindow.xul";
  PRUint32    chromeMask = 0;
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
                        PR_TRUE, getter_AddRefs(newWindow));
    if (NS_SUCCEEDED(rv)) {
      mHiddenWindow = newWindow;

#if XP_MAC
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
    nsCOMPtr<nsIRegistry> registry = 
             do_GetService(NS_REGISTRY_CONTRACTID, &rv);
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

  if (mShuttingDown)
    return NS_OK;

  mShuttingDown = PR_TRUE;

  // Shutdown native app support; doing this first will prevent new
  // requests to open additional windows coming in.
  if (mNativeAppSupport) {
    mNativeAppSupport->Quit();
    mNativeAppSupport = 0;
  }

  // Enumerate through each open window and close it
  if (mWindowMediator) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
    mWindowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));

    if (windowEnumerator) {
      PRBool more;

      while (1) {
        rv = windowEnumerator->HasMoreElements(&more);
        if (NS_FAILED(rv) || !more)
          break;

        nsCOMPtr<nsISupports> isupports;
        rv = windowEnumerator->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
          break;

        nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(isupports);
        NS_ASSERTION(window != nsnull, "not an nsIDOMWindowInternal");
        if (! window)
          continue;

        window->Close();
      }
    }
  }

  {
    nsCOMPtr<nsIWebShellWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
    if (hiddenWin) {
      ClearXPConnectSafeContext();
      hiddenWin->Close();
    }
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
  if (!event)
    return NS_ERROR_OUT_OF_MEMORY;

  PL_InitEvent(NS_REINTERPRET_CAST(PLEvent*, event),
                nsnull,
                HandleExitEvent,
                DestroyExitEvent);

  event->mService = this;
  NS_ADDREF(event->mService);

  rv = queue->EnterMonitor();
  if (NS_SUCCEEDED(rv))
    rv = queue->PostEvent(NS_REINTERPRET_CAST(PLEvent*, event));
  queue->ExitMonitor();

  if (NS_FAILED(rv)) {
    NS_RELEASE(event->mService);
    delete event;
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
                                 PR_FALSE, aResult);

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
                                 PRBool aIsHiddenWindow, nsIXULWindow **aResult)
{
  nsresult rv;
  nsWebShellWindow* window;
  PRBool intrinsicallySized;
  PRUint32 zlevel;

  *aResult = nsnull;
  intrinsicallySized = PR_FALSE;
  window = new nsWebShellWindow();
  // Bump count to one so it doesn't die on us while doing init.
  nsCOMPtr<nsIXULWindow> tempRef(window); 
  if (!window)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else {
    nsWidgetInitData widgetInitData;

#if TARGET_CARBON
    if (aIsHiddenWindow)
      widgetInitData.mWindowType = eWindowType_invisible;
    else
#endif
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
      // Min button only?
      if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_MIN) {
        widgetInitData.mBorderStyle = NS_STATIC_CAST(enum nsBorderStyle, widgetInitData.mBorderStyle | eBorderStyle_minimize );
      }  
    }

    zlevel = nsIXULWindow::normalZ;
    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RAISED)
      zlevel = nsIXULWindow::raisedZ;
    else if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_LOWERED)
      zlevel = nsIXULWindow::loweredZ;
#ifdef XP_MAC
    /* Platforms on which modal windows are always application-modal, not
       window-modal (that's just the Mac, right?) want modal windows to
       be stacked on top of everyone else. */
    if ((aChromeMask & nsIWebBrowserChrome::CHROME_MODAL) && aParent)
      zlevel = nsIXULWindow::highestZ;
#else
    /* Platforms with native support for dependent windows (that's everyone
       but the Mac, right?) know how to stack dependent windows. On these
       platforms, give the dependent window the same level as its parent,
       so we won't try to override the normal platform behaviour. */
    if ((aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT) && aParent)
      aParent->GetZlevel(&zlevel);
#endif

    if (aInitialWidth == nsIAppShellService::SIZE_TO_CONTENT ||
        aInitialHeight == nsIAppShellService::SIZE_TO_CONTENT) {
      aInitialWidth = 1;
      aInitialHeight = 1;
      intrinsicallySized = PR_TRUE;
      window->SetIntrinsicallySized(PR_TRUE);
    }

    rv = window->Initialize(aParent, mAppShell, aUrl,
                            aShowWindow, aLoadDefaultPage, zlevel,
                            aInitialWidth, aInitialHeight, aIsHiddenWindow, widgetInitData);
      
    if (NS_SUCCEEDED(rv)) {

      // this does the AddRef of the return value
      rv = CallQueryInterface(NS_STATIC_CAST(nsIWebShellWindow*, window), aResult);
      if (aParent)
        aParent->AddChildWindow(*aResult);
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
nsAppShellService::GetHiddenDOMWindow(nsIDOMWindowInternal **aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMWindowInternal> hiddenDOMWindow(do_GetInterface(docShell, &rv));
  if (NS_FAILED(rv)) return rv;

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
  // tell the window mediator about the new window
  if (mWindowMediator)
    mWindowMediator->RegisterWindow(aWindow);

  // tell the window watcher about the new window
  if (mWindowWatcher) {
    nsCOMPtr<nsIDocShell> docShell;
    aWindow->GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docShell));
      if (domWindow)
        mWindowWatcher->AddWindow(domWindow, 0);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsAppShellService::UnregisterTopLevelWindow(nsIXULWindow* aWindow)
{
  PRBool windowsRemain = PR_TRUE;

  if (mDeleteCalled) {
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
  if (mWindowMediator) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
    mWindowMediator->UnregisterWindow(aWindow);

    mWindowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
    if (windowEnumerator)
      windowEnumerator->HasMoreElements(&windowsRemain);
  }
	
  // tell the window watcher
  if (mWindowWatcher) {
    nsCOMPtr<nsIDocShell> docShell;
    aWindow->GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docShell));
      if (domWindow)
        mWindowWatcher->RemoveWindow(domWindow);
    }
  }

  // now quit if the last window has been unregistered (unless we shouldn't)

  if (!mQuitOnLastWindowClosing)
    return NS_OK;

  if (!windowsRemain) {
      
#if XP_MAC
    // if no hidden window is available (perhaps due to initial
    // Profile Manager window being cancelled), then just quit. We don't have
    // to worry about focussing the hidden window, because it will get activated
    // by the OS since it is visible (but waaaay offscreen).
    nsCOMPtr<nsIBaseWindow> hiddenWin(do_QueryInterface(mHiddenWindow));
    if (!hiddenWin)
      Quit();
#else
    // Check to see if we should quit in this case.
    if (mNativeAppSupport) {
      PRBool serverMode = PR_FALSE;
      mNativeAppSupport->GetIsServerMode(&serverMode);
      if (serverMode) {
        mNativeAppSupport->OnLastWindowClosing(aWindow);
        return NS_OK;
      }
    }

    Quit();
#endif 
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::GetQuitOnLastWindowClosing(PRBool *aQuitOnLastWindowClosing)
{
    NS_ENSURE_ARG_POINTER(aQuitOnLastWindowClosing);
    *aQuitOnLastWindowClosing = mQuitOnLastWindowClosing;
    return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::SetQuitOnLastWindowClosing(PRBool aQuitOnLastWindowClosing)
{
    mQuitOnLastWindowClosing = aQuitOnLastWindowClosing;
    return NS_OK;
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
  } else if (topic.Equals(gSkinSelectedTopic) ||
             topic.Equals(gLocaleSelectedTopic) ||
             topic.Equals(gInstallRestartTopic)) {
    if (mNativeAppSupport)
      mNativeAppSupport->SetIsServerMode(PR_FALSE);
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

  rv = nsServiceManager::GetService(NS_OBSERVERSERVICE_CONTRACTID,
                           NS_GET_IID(nsIObserverService), &glop);
  if (NS_SUCCEEDED(rv)) {
    nsIObserverService *os = NS_STATIC_CAST(nsIObserverService*,glop);
    if (aRegister) {
      os->AddObserver(weObserve, topicA.get());
      os->AddObserver(weObserve, topicB.get());
      os->AddObserver(weObserve, gSkinSelectedTopic.get());
      os->AddObserver(weObserve, gLocaleSelectedTopic.get());
      os->AddObserver(weObserve, gInstallRestartTopic.get());
    } else {
      os->RemoveObserver(weObserve, topicA.get());
      os->RemoveObserver(weObserve, topicB.get());
      os->RemoveObserver(weObserve, gSkinSelectedTopic.get());
      os->RemoveObserver(weObserve, gLocaleSelectedTopic.get());
      os->RemoveObserver(weObserve, gInstallRestartTopic.get());
    }
    nsServiceManager::ReleaseService(NS_OBSERVERSERVICE_CONTRACTID, glop);
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
    return *aResult ? NS_OK : NS_ERROR_NULL_POINTER;
}
