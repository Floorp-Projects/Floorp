/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


#include "nsIAppShellService.h"
#include "nsISupportsArray.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsXPComFactory.h"    /* template implementation of a XPCOM factory */

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsIWebShellWindow.h"
#include "nsWebShellWindow.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsAppShellNameSet.h"

#include "nsWidgetsCID.h"
#include "nsIStreamObserver.h"

#ifdef MOZ_FULLCIRCLE
#include "fullsoft.h"
#endif

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellCID,          NS_APPSHELL_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

/* Define Interface IDs */

static NS_DEFINE_IID(kIFactoryIID,           NS_IFACTORY_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,   NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID,          NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIWebShellWindowIID,    NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);



class nsAppShellService : public nsIAppShellService
{
public:
  nsAppShellService(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Initialize(void);
  NS_IMETHOD Run(void);
  NS_IMETHOD GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent);
  NS_IMETHOD DispatchNativeEvent(void * aEvent);
  NS_IMETHOD Shutdown(void);
  NS_IMETHOD CreateTopLevelWindow(nsIWidget * aParent,
                                  nsIURL* aUrl, 
                                  nsString& aControllerIID,
                                  nsIWidget*& aResult, nsIStreamObserver* anObserver,
                                  nsIXULWindowCallbacks *aCallbacks,
                                  PRInt32 aInitialWidth, PRInt32 aInitialHeight);
  NS_IMETHOD CreateDialogWindow(  nsIWidget * aParent,
                                  nsIURL* aUrl, 
                                  nsString& aControllerIID,
                                  nsIWidget*& aResult, nsIStreamObserver* anObserver,
                                  nsIXULWindowCallbacks *aCallbacks,
                                  PRInt32 aInitialWidth, PRInt32 aInitialHeight);
  NS_IMETHOD CloseTopLevelWindow(nsIWidget* aWindow);
  NS_IMETHOD RegisterTopLevelWindow(nsIWidget* aWindow);
  NS_IMETHOD UnregisterTopLevelWindow(nsIWidget* aWindow);


protected:
  virtual ~nsAppShellService();

  nsIAppShell* mAppShell;
  nsISupportsArray* mWindowList;
};


nsAppShellService::nsAppShellService()
{
  NS_INIT_REFCNT();

  mAppShell     = nsnull;
  mWindowList   = nsnull;
}

nsAppShellService::~nsAppShellService()
{
  NS_IF_RELEASE(mAppShell);
  NS_IF_RELEASE(mWindowList);
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS(nsAppShellService, kIAppShellServiceIID);


NS_IMETHODIMP
nsAppShellService::Initialize(void)
{
  nsresult rv;
  
#ifdef MOZ_FULLCIRCLE
  FCInitialize();
#endif
  // Create the Event Queue for the UI thread...
  nsIEventQueueService* eventQService;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&eventQService);
  if (NS_OK == rv) {
    // XXX: What if this fails?
    rv = eventQService->CreateThreadEventQueue();
  }

#if 0 // XXX: Remove this when mac and unix build the AppShellNameset...
  // Register the nsAppShellNameSet with the global nameset registry...
  nsIScriptNameSetRegistry *registry;
  rv = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                    kIScriptNameSetRegistryIID,
                                    (nsISupports **)&registry);
  if (NS_FAILED(rv)) {
    goto done;
  }
  
  nsAppShellNameSet* nameSet;
  nameSet = new nsAppShellNameSet();
  registry->AddExternalNameSet(nameSet);
  /* FIX - do we need to release this service?  When we do, it get deleted,and our name is lost. */
#endif

  // Create the toplevel window list...
  rv = NS_NewISupportsArray(&mWindowList);
  if (NS_FAILED(rv)) {
    goto done;
  }

  // Create widget application shell
  rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull, kIAppShellIID,
                                    (void**)&mAppShell);
  if (NS_FAILED(rv)) {
    goto done;
  }

  rv = mAppShell->Create(0, nsnull);

done:
  return rv;
}


NS_IMETHODIMP
nsAppShellService::Run(void)
{
  return mAppShell->Run();
}

NS_IMETHODIMP
nsAppShellService::GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent)
{
  return mAppShell->GetNativeEvent(aEvent, aWidget, aIsInWindow, aIsMouseEvent);
}

NS_IMETHODIMP
nsAppShellService::DispatchNativeEvent(void * aEvent)
{
  return mAppShell->DispatchNativeEvent(aEvent);
}

NS_IMETHODIMP
nsAppShellService::Shutdown(void)
{

#if 1
  mAppShell->Exit();
#else
  while (mWindowList->Count() > 0) {
    nsISupports * winSupports = mWindowList->ElementAt(0);
    nsCOMPtr<nsIWidget> window(do_QueryInterface(winSupports));
    if (window) {
      mWindowList->RemoveElementAt(0);
      CloseTopLevelWindow(window);
    } else {
      nsCOMPtr<nsIWebShellWindow> webShellWin(do_QueryInterface(winSupports));
      if (webShellWin) {
        nsIWidget * win;
        webShellWin->GetWidget(win);
        CloseTopLevelWindow(win);
      }
      //nsCOMPtr<nsIWebShellContainer> wsc(do_QueryInterface(winSupports));
      //if (wsc) {
      //
      //}
      break;
    }
  }
#endif
  return NS_OK;
}

/*
 * Create a new top level window and display the given URL within it...
 *
 * @param aParent - parent for the window to be created (generally null;
 *                  included for compatibility with dialogs).
 *                  (currently unused).
 * @param aURL - location of XUL window contents description
 * @param aControllerIID - currently provided as an argument. in the future,
 *                         this will be specified by the XUL document itself.
 *                         (currently unused, but please specify the same
 *                         hardwired IID as others are using).
 * @param anObserver - a stream observer to give to the new window
 * @param aConstructionCallbacks - methods which will be called during
 *                                 window construction. can be null.
 * @param aInitialWidth - width of window, in pixels (currently unused)
 * @param aInitialHeight - height of window, in pixels (currently unused)
 */
NS_IMETHODIMP
nsAppShellService::CreateTopLevelWindow(nsIWidget *aParent,
                                        nsIURL* aUrl, nsString& aControllerIID,
                                        nsIWidget*& aResult, nsIStreamObserver* anObserver,
                                        nsIXULWindowCallbacks *aCallbacks,
                                        PRInt32 aInitialWidth, PRInt32 aInitialHeight)
{
  nsresult rv;
  nsWebShellWindow* window;

  window = new nsWebShellWindow();
  if (nsnull == window) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else {
    // temporarily disabling parentage because non-Windows platforms
    // seem to be interpreting it in unexpected ways.
    rv = window->Initialize((nsIWidget *) nsnull, mAppShell, aUrl, aControllerIID,
                            anObserver, aCallbacks,
                            aInitialWidth, aInitialHeight);
    if (NS_SUCCEEDED(rv)) {
      aResult = window->GetWidget();
      RegisterTopLevelWindow(aResult);
      window->Show(PR_TRUE);
    }
  }

  return rv;
}


NS_IMETHODIMP
nsAppShellService::CloseTopLevelWindow(nsIWidget* aWindow)
{
  nsresult closerv, unregrv;
  void     *data;

  aWindow->GetClientData(data);
  if (data == nsnull)
    closerv = NS_ERROR_NULL_POINTER;
  else {
    nsWebShellWindow* window = (nsWebShellWindow *) data;
    closerv = window->Close();
  }

  unregrv = UnregisterTopLevelWindow(aWindow);
  if (0 == mWindowList->Count())
    mAppShell->Exit();

  return closerv == NS_OK ? unregrv : closerv;
}

/*
 * Like CreateTopLevelWindow, but with dialog window borders.  This
 * method is necessary because of the current misfortune that the window
 * is created before its XUL description has been parsed, so the description
 * can't affect attributes like window type.
 */
NS_IMETHODIMP
nsAppShellService::CreateDialogWindow(nsIWidget * aParent,
                                      nsIURL* aUrl, nsString& aControllerIID,
                                      nsIWidget*& aResult, nsIStreamObserver* anObserver,
                                      nsIXULWindowCallbacks *aCallbacks,
                                      PRInt32 aInitialWidth, PRInt32 aInitialHeight)
{
  nsresult rv;
  nsWebShellWindow* window;

  window = new nsWebShellWindow();
  if (nsnull == window) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else {
    // temporarily disabling parentage because non-Windows platforms
    // seem to be interpreting it in unexpected ways.
    rv = window->Initialize((nsIWidget *) nsnull, mAppShell, aUrl, aControllerIID,
                            anObserver, aCallbacks,
                            aInitialWidth, aInitialHeight);
    if (NS_SUCCEEDED(rv)) {
      aResult = window->GetWidget();
      RegisterTopLevelWindow(aResult);
      window->Show(PR_TRUE);
    }
  }

  return rv;
}


/*
 * Register a new top level window (created elsewhere)
 */
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
NS_IMETHODIMP
nsAppShellService::RegisterTopLevelWindow(nsIWidget* aWindow)
{
  nsresult rv;
  void* data;

  aWindow->GetClientData(data);
  if (data == nsnull)
    rv = NS_ERROR_NULL_POINTER;
  else {
    nsWebShellWindow* window = (nsWebShellWindow *) data;
    nsIWebShellContainer* wsc;
    rv = window->QueryInterface(kIWebShellContainerIID, (void **) &wsc);
    if (NS_SUCCEEDED(rv)) {
      mWindowList->AppendElement(wsc);
      NS_RELEASE(wsc);
    }
  }
  return rv;
}


NS_IMETHODIMP
nsAppShellService::UnregisterTopLevelWindow(nsIWidget* aWindow)
{
  nsresult rv;
  void* data;

  aWindow->GetClientData(data);
  if (data == nsnull)
    rv = NS_ERROR_NULL_POINTER;
  else {
    nsWebShellWindow* window = (nsWebShellWindow *) data;
    nsIWebShellContainer* wsc;
    rv = window->QueryInterface(kIWebShellContainerIID, (void **) &wsc);
    if (NS_SUCCEEDED(rv)) {
      mWindowList->RemoveElement(wsc);
      NS_RELEASE(wsc);
    }
  }
  return rv;
}


NS_EXPORT nsresult NS_NewAppShellService(nsIAppShellService** aResult)
{
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = new nsAppShellService();
  if (nsnull == *aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------

// Entry point to create nsAppShellService factory instances...
NS_DEF_FACTORY(AppShellService,nsAppShellService)

nsresult NS_NewAppShellServiceFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsAppShellServiceFactory;
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
