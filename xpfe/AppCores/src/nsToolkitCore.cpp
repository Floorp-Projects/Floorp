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

#include "nsAppCoresManager.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIDOMBaseAppCore.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIWidget.h"
#include "nsToolkitCore.h"

class nsIScriptContext;

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIDOMBaseAppCoreIID, NS_IDOMBASEAPPCORE_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIToolkitCoreIID, NS_IDOMTOOLKITCORE_IID);


/////////////////////////////////////////////////////////////////////////
// nsToolkitCore
/////////////////////////////////////////////////////////////////////////

nsToolkitCore::nsToolkitCore() {

  printf("Created nsToolkitCore\n");

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsToolkitCore::~nsToolkitCore() {

  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsToolkitCore)
NS_IMPL_RELEASE(nsToolkitCore)


NS_IMETHODIMP 
nsToolkitCore::QueryInterface(REFNSIID aIID, void** aInstancePtr) {

  if (aInstancePtr == NULL)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtr = NULL;

  if (aIID.Equals(kIToolkitCoreIID)) {
    *aInstancePtr = (void*) ((nsIDOMToolkitCore*) this);
    AddRef();
    return NS_OK;
  }
 
  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsToolkitCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject) {
  nsresult rv = NS_OK;

  NS_PRECONDITION(aScriptObject != nsnull, "null arg");
  if (mScriptObject == nsnull) {
      nsISupports *core;
      rv = QueryInterface(kISupportsIID, (void **)&core);
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewScriptToolkitCore(aContext, 
                                     (nsISupports *) core,
                                     nsnull, 
                                     &mScriptObject);
        NS_RELEASE(core);
      }
  }

  *aScriptObject = mScriptObject;
  return rv;
}


NS_IMETHODIMP    
nsToolkitCore::Init(const nsString& aId) {

  nsresult rv;

  nsBaseAppCore::Init(aId);

  nsIDOMBaseAppCore *core;
  rv = QueryInterface(kIDOMBaseAppCoreIID, (void **)&core);
  if (NS_SUCCEEDED(rv)) {
    nsAppCoresManager* sdm = new nsAppCoresManager();
    if (sdm) {
      sdm->Add(core);
      delete sdm;
      return NS_OK;
    } else
      rv = NS_ERROR_OUT_OF_MEMORY;
    NS_RELEASE(core);
  }
  return rv;
}


NS_IMETHODIMP
nsToolkitCore::ShowWindow(const nsString& aUrl, nsIDOMWindow* aParent) {

  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;
  nsIURL             *url;
  nsIWidget          *window;

printf("in toolkitcore::ShowWindow\n");

  window = nsnull;

  rv = NS_NewURL(&url, aUrl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_SUCCEEDED(rv)) {
    // hardwired temporary hack.  See nsAppRunner.cpp at main()
    controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
    appShell->CreateTopLevelWindow(url, controllerCID, window, nsnull);
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }
  if (window != nsnull) {
    window->Show(PR_TRUE);
  }

  return rv;
}
