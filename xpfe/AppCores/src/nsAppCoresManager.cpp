
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

#include "nsAppCoresCIDs.h"
#include "nsAppCores.h"
#include "nsAppCoresNameSet.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsIComponentManager.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"

#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "mkutils.h"
#include "prefapi.h"

#include "nsIURL.h"
#include "nsINetlibURL.h"
#include "nsINetService.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"

#include "nsIDOMAppCoresManager.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"

// Globals

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);

static NS_DEFINE_IID(kInetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kInetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kInetLibURLIID, NS_INETLIBURL_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

static NS_DEFINE_IID(kBrowserWindowCID, NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);

static NS_DEFINE_IID(kIAppCoresManagerIID,    NS_IDOMAPPCORESMANAGER_IID); 
static NS_DEFINE_IID(kAppCoresManagerCID,     NS_APPCORESMANAGER_CID);
static NS_DEFINE_IID(kAppCoresFactoryCID,     NS_APPCORESFACTORY_CID);


/////////////////////////////////////////////////////////////////////////
// nsAppCoresManager
/////////////////////////////////////////////////////////////////////////

nsVoidArray nsAppCoresManager::mList;

nsAppCoresManager::nsAppCoresManager()
{
  mScriptObject   = nsnull;
  
  IncInstanceCount();
  NS_INIT_REFCNT();
}


//--------------------------------------------------------
nsAppCoresManager::~nsAppCoresManager()
{
  DecInstanceCount(); 
}


NS_IMPL_ADDREF(nsAppCoresManager)
NS_IMPL_RELEASE(nsAppCoresManager)



//--------------------------------------------------------
NS_IMETHODIMP 
nsAppCoresManager::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  
  if ( aIID.Equals(kIAppCoresManagerIID) ) {
    nsIDOMAppCoresManager* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kIScriptObjectOwnerIID)) {   
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
    nsIDOMAppCoresManager* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    
    *aInstancePtr = (void*)tmp2;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}


//--------------------------------------------------------
NS_IMETHODIMP 
nsAppCoresManager::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  
  if (nsnull == mScriptObject)  {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptAppCoresManager(aContext, (nsISupports *)(nsIDOMAppCoresManager*)this, global, (void**)&mScriptObject);
    NS_IF_RELEASE(global);
  }


  *aScriptObject = mScriptObject;
  return res;
}

//--------------------------------------------------------
NS_IMETHODIMP 
nsAppCoresManager::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

//--------------------------------------------------------
NS_IMETHODIMP
nsAppCoresManager::Startup()
{
    /***************************************/
  /* Add us to the Javascript Name Space */
  /***************************************/

  nsIScriptNameSetRegistry *registry;
  nsresult result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                                 kIScriptNameSetRegistryIID,
                                                (nsISupports **)&registry);
  if (NS_OK == result) {
    nsAppCoresNameSet* nameSet = new nsAppCoresNameSet();
    registry->AddExternalNameSet(nameSet);
    /* FIX - do we need to release this service?  When we do, it get deleted,and our name is lost. */
  }

  return result;
}


static PRBool CleanUp(void* aElement, void *aData)
{
  nsIDOMBaseAppCore * appCore = (nsIDOMBaseAppCore *)aElement;
  NS_RELEASE(appCore);
  return PR_TRUE;
}

//--------------------------------------------------------
NS_IMETHODIMP
nsAppCoresManager::Shutdown()
{
  mList.EnumerateForwards(CleanUp, nsnull);
  return NS_OK;
}

//--------------------------------------------------------
NS_IMETHODIMP    
nsAppCoresManager::Add(nsIDOMBaseAppCore* aAppCore)
{
   
  if (aAppCore == NULL)
      return NS_ERROR_FAILURE;

  /* Check to see if we already have this task in our list */
  nsString      nodeIDString;
  nsString      addIDString;

  aAppCore->GetId(addIDString);

  PRInt32 i;
  for (i=0;i<mList.Count();i++) {
    ((nsIDOMBaseAppCore *)mList[i])->GetId(nodeIDString);
     
    if (nodeIDString == addIDString) {
      /*we already have this ID in our list, ignore */
      return NS_ERROR_FAILURE;    
    }
  }

  aAppCore->AddRef();
  mList.AppendElement(aAppCore);

  return NS_OK;
}

//--------------------------------------------------------
NS_IMETHODIMP    
nsAppCoresManager::Remove(nsIDOMBaseAppCore* aAppCore)
{
  return (mList.RemoveElement(aAppCore)?NS_OK : NS_ERROR_FAILURE);
}



NS_IMETHODIMP    
nsAppCoresManager::Find(const nsString& aId, nsIDOMBaseAppCore** aReturn)
{
  *aReturn=nsnull;

  nsString nodeIDString;

  PRInt32 i;
  for (i=0;i<mList.Count();i++) {
    nsIDOMBaseAppCore * appCore = (nsIDOMBaseAppCore *)mList.ElementAt(i);
    appCore->GetId(nodeIDString);
    if (nodeIDString == aId) {
      NS_ADDREF(appCore);
      *aReturn = appCore;
      return NS_OK;    
    }
  }

  return NS_OK;
}






