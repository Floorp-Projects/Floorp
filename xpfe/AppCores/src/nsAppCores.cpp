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
#include "nsAppCoresManagerFactory.h"
#include "nsDOMPropsCoreFactory.h"
#include "nsMailCoreFactory.h"
#include "nsPrefsCoreFactory.h"
#include "nsRDFCoreFactory.h"
#include "nsToolbarCoreFactory.h"
#include "nsBrowserAppCoreFactory.h"
#include "nsEditorAppCoreFactory.h"
#include "nsToolkitCoreFactory.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "pratom.h"
#include <stdio.h>
#include "nsIServiceManager.h"

static PRInt32 gLockCnt = 0;
static PRInt32 gInstanceCnt = 0;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kIFactoryIID,        NS_IFACTORY_IID);
static NS_DEFINE_IID(kDOMPropsCoreCID,    NS_DOMPROPSCORE_CID);
static NS_DEFINE_IID(kMailCoreCID,        NS_MAILCORE_CID);
static NS_DEFINE_IID(kPrefsCoreCID,       NS_PREFSCORE_CID);
static NS_DEFINE_IID(kRDFCoreCID,         NS_RDFCORE_CID);
static NS_DEFINE_IID(kToolbarCoreCID,     NS_TOOLBARCORE_CID);
static NS_DEFINE_IID(kToolkitCoreCID,     NS_TOOLKITCORE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,  NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kEditorAppCoreCID,   NS_EDITORAPPCORE_CID);
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
    nsComponentManager::RegisterComponent(kDOMPropsCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kMailCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kPrefsCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kRDFCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kToolbarCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kToolkitCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kBrowserAppCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsComponentManager::RegisterComponent(kEditorAppCoreCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* serviceMgr, const char *path)
{
    printf("*** AppCores object is being unregistered\n");
    
    nsComponentManager::UnregisterFactory(kAppCoresManagerCID, path);
    nsComponentManager::UnregisterFactory(kDOMPropsCoreCID, path);
    nsComponentManager::UnregisterFactory(kMailCoreCID, path);
    nsComponentManager::UnregisterFactory(kPrefsCoreCID, path);
    nsComponentManager::UnregisterFactory(kRDFCoreCID, path);
    nsComponentManager::UnregisterFactory(kToolbarCoreCID, path);
    nsComponentManager::UnregisterFactory(kToolkitCoreCID, path);
    nsComponentManager::UnregisterFactory(kBrowserAppCoreCID, path);
    nsComponentManager::UnregisterFactory(kEditorAppCoreCID, path);
    
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
    else if ( aClass.Equals(kDOMPropsCoreCID) )
    {
        inst = new nsDOMPropsCoreFactory();      
    }
    else if ( aClass.Equals(kMailCoreCID) )
    {
        inst = new nsMailCoreFactory();      
    }
    else if ( aClass.Equals(kPrefsCoreCID) )
    {
        inst = new nsPrefsCoreFactory();      
    }
    else if ( aClass.Equals(kRDFCoreCID) )
    {
        inst = new nsRDFCoreFactory();      
    }
    else if ( aClass.Equals(kToolbarCoreCID) )
    {
        inst = new nsToolbarCoreFactory();      
    }
    else if ( aClass.Equals(kToolkitCoreCID) )
    {
        inst = new nsToolkitCoreFactory();
    }
    else if ( aClass.Equals(kBrowserAppCoreCID) )
    {
        inst = new nsBrowserAppCoreFactory();      
    }
    else if ( aClass.Equals(kEditorAppCoreCID) )
    {
        inst = new nsEditorAppCoreFactory();      
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
