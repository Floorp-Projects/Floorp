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

#include <stdio.h>

#include "nsAppCoresCIDs.h"
#include "nsAppCoresManagerFactory.h"
#include "nsProfileCoreFactory.h" 
#include "nsBrowserAppCoreFactory.h"
#include "nsToolkitCoreFactory.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "pratom.h"
#include "nsIServiceManager.h"

static PRInt32 gLockCnt = 0;
static PRInt32 gInstanceCnt = 0;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kIFactoryIID,        NS_IFACTORY_IID);
static NS_DEFINE_IID(kProfileCoreCID,     NS_PROFILECORE_CID); 
static NS_DEFINE_IID(kToolkitCoreCID,     NS_TOOLKITCORE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,  NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kAppCoresManagerCID, NS_APPCORESMANAGER_CID);


////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return PRBool (gInstanceCnt == 0 && gLockCnt == 0);
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* serviceMgr, const char *path)
{
    printf("*** AppCores object is being registered\n");
    nsComponentManager::RegisterComponent(kAppCoresManagerCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kProfileCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE); 
    nsComponentManager::RegisterComponent(kToolkitCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kBrowserAppCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);

    return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* serviceMgr, const char *path)
{
    printf("*** AppCores object is being unregistered\n");
    
    nsComponentManager::UnregisterComponent(kAppCoresManagerCID, path);
    nsComponentManager::UnregisterComponent(kProfileCoreCID, path); 
    nsComponentManager::UnregisterComponent(kToolkitCoreCID, path);
    nsComponentManager::UnregisterComponent(kBrowserAppCoreCID, path);
    
    return NS_OK;
}



extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{

    if (aFactory == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = NULL;
    nsISupports *inst;
    if ( aClass.Equals(kAppCoresManagerCID) )
    {
        inst = new nsAppCoresManagerFactory();        
    }
    else if ( aClass.Equals(kProfileCoreCID) ) 
    { 
        inst = new nsProfileCoreFactory(); 
    }
    else if ( aClass.Equals(kToolkitCoreCID) )
    {
        inst = new nsToolkitCoreFactory();
    }
    else if ( aClass.Equals(kBrowserAppCoreCID) )
    {
        inst = new nsBrowserAppCoreFactory();      
    }
    else
    {
        return NS_ERROR_ILLEGAL_VALUE;
    }


    if (inst == NULL)
    {   
        return NS_ERROR_OUT_OF_MEMORY;
    }


    nsresult res = inst->QueryInterface(kIFactoryIID, (void**) aFactory);

    if (res != NS_OK)
    {   
        delete inst;
    }

  return res;

}

extern "C" void
IncInstanceCount(){
    PR_AtomicIncrement(&gInstanceCnt);
}

extern "C" void
IncLockCount(){
    PR_AtomicIncrement(&gLockCnt);
}

extern "C" void
DecInstanceCount(){
    PR_AtomicDecrement(&gInstanceCnt);
}

extern "C" void
DecLockCount(){
    PR_AtomicDecrement(&gLockCnt);
}
