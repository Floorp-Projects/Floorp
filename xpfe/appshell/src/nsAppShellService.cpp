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
#include "nsRepository.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsXPComFactory.h"    /* template implementation of a XPCOM factory */

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsWebShellWindow.h"

#include "nsWidgetsCID.h"
#include "nsIStreamObserver.h"

#ifdef MOZ_FULLCIRCLE
#include "fullsoft.h"
#endif

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellCID,          NS_APPSHELL_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

/* Define Interface IDs */

static NS_DEFINE_IID(kIFactoryIID,           NS_IFACTORY_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,   NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID,          NS_IAPPSHELL_IID);



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
  NS_IMETHOD CreateTopLevelWindow(nsIURL* aUrl, nsString& aControllerIID,
                                  nsIWidget*& aResult, nsIStreamObserver* anObserver);
  NS_IMETHOD CreateDialogWindow(nsIWidget * aParent,
                                nsIURL* aUrl, 
                                nsString& aControllerIID,
                                nsIWidget*& aResult, nsIStreamObserver* anObserver);
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
  nsIEventQueueService* mEventQService;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&mEventQService);
  if (NS_OK == rv) {
    // XXX: What if this fails?
    rv = mEventQService->CreateThreadEventQueue();
  }

  rv = NS_NewISupportsArray(&mWindowList);
  if (NS_FAILED(rv)) {
    goto done;
  }

  // Create widget application shell
  rv = nsRepository::CreateInstance(kAppShellCID, nsnull, kIAppShellIID,
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
  return NS_OK;
}

/*
 * Create a new top level window and display the given URL within it...
 *
 * XXX:
 * Currently, the IID of the Controller object for the URL is provided as an
 * argument.  In the future, this argument will be specified by the XUL document
 * itself.
 */
NS_IMETHODIMP
nsAppShellService::CreateTopLevelWindow(nsIURL* aUrl, nsString& aControllerIID,
                                        nsIWidget*& aResult, nsIStreamObserver* anObserver)
{
  nsresult rv;
  nsWebShellWindow* window;

  window = new nsWebShellWindow();
  if (nsnull == window) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else {
    rv = window->Initialize(mAppShell, aUrl, aControllerIID, anObserver);
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
  nsresult rv;

  rv = UnregisterTopLevelWindow(aWindow);
  if (0 == mWindowList->Count()) {
    mAppShell->Exit();
  }

  return rv;
}

/*
 * Create a new top level window and display the given URL within it...
 *
 * XXX:
 * Currently, the IID of the Controller object for the URL is provided as an
 * argument.  In the future, this argument will be specified by the XUL document
 * itself.
 */
NS_IMETHODIMP
nsAppShellService::CreateDialogWindow(nsIWidget * aParent,
                                      nsIURL* aUrl, nsString& aControllerIID,
                                      nsIWidget*& aResult, nsIStreamObserver* anObserver)
{
  nsresult rv;
  nsWebShellWindow* window;

  window = new nsWebShellWindow();
  if (nsnull == window) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else {
    rv = window->Initialize(nsnull, mAppShell, aUrl, aControllerIID, anObserver);
    if (NS_SUCCEEDED(rv)) {
      mWindowList->AppendElement((nsIWebShellContainer*)window);
      aResult = window->GetWidget();
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
//    nsCOMPtr<nsIWebShellContainer> wsc(window); DRaM
    nsIWebShellContainer* wsc;
    rv = window->QueryInterface(kIWebShellContainerIID, (void **) &wsc);
    if (NS_SUCCEEDED(rv))
      mWindowList->AppendElement(wsc);
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
//    nsCOMPtr<nsIWebShellContainer> wsc(window); DRaM
    nsIWebShellContainer* wsc;
    rv = window->QueryInterface(kIWebShellContainerIID, (void **) &wsc);
    if (NS_SUCCEEDED(rv))
      mWindowList->RemoveElement(wsc);
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

nsresult NS_NewAppShellServiceFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsFactory<nsAppShellService>();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
