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

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsWebShellWindow.h"

#include "nsWidgetsCID.h"

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellCID,         NS_APPSHELL_CID);

/* Define Interface IDs */

static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID,        NS_IAPPSHELL_IID);



class nsAppShellService : public nsIAppShellService
{
public:
  nsAppShellService(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Initialize(void);
  NS_IMETHOD Run(void);
  NS_IMETHOD Shutdown(void);
  NS_IMETHOD CreateTopLevelWindow(nsIURL* aUrl, nsIWidget*& aResult);
  NS_IMETHOD CloseTopLevelWindow(nsIWidget* aWindow);


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
nsAppShellService::Shutdown(void)
{
  return NS_OK;
}

/*
 * Create a new top level window and display the given URL within it...
 */
NS_IMETHODIMP
nsAppShellService::CreateTopLevelWindow(nsIURL* aUrl, nsIWidget*& aResult)
{
  nsresult rv;
  nsWebShellWindow* window;

  window = new nsWebShellWindow();
  if (nsnull == window) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else {
    rv = window->Initialize(mAppShell, aUrl);
    if (NS_SUCCEEDED(rv)) {
      mWindowList->AppendElement(window);
      aResult = window->GetWidget();
    }
  }

  return rv;
}


NS_IMETHODIMP
nsAppShellService::CloseTopLevelWindow(nsIWidget* aWindow)
{
  nsresult rv = NS_OK;

  nsWebShellWindow* window;
  void* data;

  // Get the nsWebShellWindow from the nsIWidget...
  aWindow->GetClientData(data);
  if (nsnull != data) {
    window = (nsWebShellWindow*)data;
  }

  if (nsnull != data) {
    PRBool bFound;

    bFound = mWindowList->RemoveElement(window);

  }

  if (0 == mWindowList->Count()) {
    mAppShell->Exit();
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

// Factory code for creating nsAppShellService's

class nsAppShellServiceFactory : public nsIFactory
{
public:
  nsAppShellServiceFactory();
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

protected:
  virtual ~nsAppShellServiceFactory();
};


nsAppShellServiceFactory::nsAppShellServiceFactory()
{
  NS_INIT_REFCNT();
}

nsAppShellServiceFactory::~nsAppShellServiceFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsAppShellServiceFactory, kIFactoryIID);


nsresult
nsAppShellServiceFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsAppShellService* inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsAppShellService);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}

nsresult
nsAppShellServiceFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" NS_APPSHELL nsresult
NS_NewAppShellServiceFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsAppShellServiceFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
