
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


#include "nsAppCoresCIDs.h"
#include "nsAppCores.h"
#include "nsAppCoresManager.h"
#include "nsAppCoresNameSet.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsRepository.h"

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

#include "nsIDOMAppCores.h"
#include "nsIDOMMailCore.h"

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

static NS_DEFINE_IID(kIMailCoreIID,           NS_IDOMMAILCORE_IID);
static NS_DEFINE_IID(kIAppCoresIID,           NS_IDOMAPPCORES_IID); 
static NS_DEFINE_IID(kAppCoresCID,            NS_AppCores_CID);
static NS_DEFINE_IID(kMailCoreCID,            NS_MailCore_CID);
static NS_DEFINE_IID(kAppCoresFactoryCID,     NS_AppCoresFactory_CID);
static NS_DEFINE_IID(kMailCoreFactoryCID,     NS_MailCoreFactory_CID);


static SDL_TaskList     *gTasks         = nsnull;
static SDL_TaskList     *gNextReadyTask = nsnull;


/////////////////////////////////////////////////////////////////////////
// nsAppCoresManager
/////////////////////////////////////////////////////////////////////////

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
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    
    if ( aIID.Equals(kIAppCoresIID) )
    {
        nsIDOMAppCores* tmp = this;
        *aInstancePtr = (void*)tmp;
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kIScriptObjectOwnerIID))
    {   
        nsIScriptObjectOwner* tmp = this;
        *aInstancePtr = (void*)tmp;
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
         
        nsIDOMAppCores* tmp1 = this;
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
    
    if (nsnull == mScriptObject) 
    {
        nsIScriptGlobalObject *global = aContext->GetGlobalObject();

        res = NS_NewScriptAppCores(aContext, (nsISupports *)(nsIDOMAppCores*)this, global, (void**)&mScriptObject);
        
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
    if (NS_OK == result) 
    {
        nsAppCoresNameSet* nameSet = new nsAppCoresNameSet();
        registry->AddExternalNameSet(nameSet);
        /* FIX - do we need to release this service?  When we do, it get deleted,and our name is lost. */
    }

    return result;
}

//--------------------------------------------------------
NS_IMETHODIMP
nsAppCoresManager::Shutdown()
{
    return NS_OK;
}

//--------------------------------------------------------
NS_IMETHODIMP    
nsAppCoresManager::Add(nsIDOMBaseAppCore* aTask)
{
   
    if (aTask == NULL)
        return NS_ERROR_FAILURE;

    /* Check to see if we already have this task in our list */
    SDL_TaskList *node = gTasks;
    nsString      nodeIDString;
    nsString      addIDString;

    aTask->GetId(addIDString);

    while (node != NULL)
    {
        node->task->GetId(nodeIDString);
         
        if (nodeIDString == addIDString)
        {
            /*we already have this ID in our list, ignore */
            return NS_OK;    
        }

        node = node->next;
    }

    /* add the task to our list */
    SDL_TaskList* taskNode = (SDL_TaskList*)PR_MALLOC(sizeof(SDL_TaskList));
    
    aTask->AddRef();

    taskNode->next = gTasks;
    taskNode->task = aTask;
    gTasks = taskNode;

    /* Lets set the next task to run to this one */
    gNextReadyTask = taskNode;
    
    return NS_OK;
}

//--------------------------------------------------------
NS_IMETHODIMP    
nsAppCoresManager::Remove(nsIDOMBaseAppCore* aTask)
{
    if (aTask == NULL)
        return NS_ERROR_FAILURE;

    /* Remove from our list */
    
    SDL_TaskList *node = gTasks;
    SDL_TaskList *lastnode = gTasks;
    nsString      nodeIDString;
    nsString      doomedIDString;

    aTask->GetId(doomedIDString);

    while (node != NULL)
    {
        node->task->GetId(nodeIDString);
        
        if (nodeIDString == doomedIDString)
        {
            /* we want to delete this node */
            
            if (node == gTasks)
            {
                gTasks = node->next;
            }
            else
            {
                lastnode->next = node->next;
            }

            node->task->Release();
            PR_DELETE(node);
            break;
        }

        lastnode = node;
        node = node->next;
    }
    return NS_OK;
}



NS_IMETHODIMP    
nsAppCoresManager::Find(const nsString& aId, nsIDOMBaseAppCore** aReturn)
{
    *aReturn=nsnull;

    SDL_TaskList *node = gTasks;
    nsString      nodeIDString;

    while (node != NULL)
    {
        node->task->GetId(nodeIDString);
        
        if (nodeIDString == aId)
        {
            *aReturn = node->task;
            node->task->AddRef();
            break;
        }
        node = node->next;
    }

    return NS_OK;
}






