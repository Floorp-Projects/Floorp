/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http:/www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsGIFModule.h"
#include "nsGIFDecoder.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kGIFDecoderCID, NS_GIFDECODER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


//////////////////////////////////////////////////////////////////////
// Module Global
//
// This is used by the module to count object. Constructors and
// destructors of objects created by this module need to
// inc/dec the object count for the module. Unload decision will
// be taken based on this.
//
// Constructor:
//	gModule->IncrementObjCount();
//
// Descructor:
//	gModule->DecrementObjCount();
//
// WARNING: This is weak reference. XPCOM guarantees that this module
// 			object will be deleted just before the dll is unloaded. Hence,
//			holding a weak reference is ok.

static nsGIFModule *gModule = NULL;

//////////////////////////////////////////////////////////////////////
// Module entry point

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsGIFModule *gifModule;
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsGIFModule: Module already created.");

    gifModule = new nsGIFModule;
    if (gifModule == NULL) return NS_ERROR_OUT_OF_MEMORY;

    rv = gifModule->QueryInterface(nsIModule::GetIID(), (void **)return_cobj);

    if (NS_FAILED(rv))
    {
        delete gifModule;
        gifModule = NULL;
    }

    // WARNING: Weak Reference
    gModule = gifModule;

    return rv;
}

//////////////////////////////////////////////////////////////////////
// GIF Decoder Factory using nsIGenericFactory

static NS_IMETHODIMP
nsGIFDecoderCreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    GIFDecoder *gifdec = NULL;
    *aResult = NULL;
    il_container* ic = NULL;

    if (aOuter && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;  

    ic = new il_container();
    if (!ic)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    gifdec = new GIFDecoder(ic);
    if (!gifdec)
    {
        delete ic;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult res = gifdec->QueryInterface(aIID, aResult);
    if (NS_FAILED(res))
    {
        *aResult = NULL;
        delete gifdec;
    }
    delete ic; /* is a place holder */
    return res;
}


//////////////////////////////////////////////////////////////////////
// GIF Decoder Module Implementation

NS_IMPL_ISUPPORTS(nsGIFModule, nsIModule::GetIID())

nsGIFModule::nsGIFModule(void)
  : mObjCount(-1), mClassObject(NULL)
{
    NS_INIT_ISUPPORTS();
}

nsGIFModule::~nsGIFModule(void)
{
    NS_ASSERTION(mObjCount <= 0, "Module released while having outstanding objects.");
    if (mClassObject)
    {
        int refcnt;
        NS_RELEASE2(mClassObject, refcnt);
        NS_ASSERTION(refcnt == 0, "nsGIFModule::~nsGIFModule() ClassObject refcount nonzero");
    }
}

//
// The class object for us is just the factory and nothing more.
//
NS_IMETHODIMP
nsGIFModule::GetClassObject(nsIComponentManager *aCompMgr, const nsCID & aClass,
                            const nsIID &aIID, void **r_classObj)
{
    nsresult rv;

    if( !aClass.Equals(kGIFDecoderCID))
		return NS_ERROR_FACTORY_NOT_REGISTERED;

    // If this aint the first time, return the cached class object
    if (mClassObject == NULL)
    {
        nsCOMPtr<nsIGenericFactory> fact;
        rv = NS_NewGenericFactory(getter_AddRefs(fact), nsGIFDecoderCreateInstance);
        if (NS_FAILED(rv)) return rv;
    
        // Store the class object in our global
        rv = fact->QueryInterface(NS_GET_IID(nsISupports), (void **)&mClassObject);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mClassObject->QueryInterface(aIID, r_classObj);
    return rv;
}

NS_IMETHODIMP
nsGIFModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec *location,
                          const char *registryLocation)
{
    nsresult rv;
    rv = aCompMgr->RegisterComponentSpec(kGIFDecoderCID, 
                                         "Netscape GIFDec", 
                                         "component://netscape/image/decoder&type=image/gif",
                                         location, PR_TRUE, PR_TRUE);
    return rv;
}

NS_IMETHODIMP
nsGIFModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                            nsIFileSpec *location,
                            const char *registryLocation)
{
    nsresult rv;
    rv = aCompMgr->UnregisterComponentSpec(kGIFDecoderCID, location);
    return rv;
}

NS_IMETHODIMP
nsGIFModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (mObjCount < 0)
    {
        // Dll doesn't count objects.
        return NS_ERROR_FAILURE;
    }

    PRBool unloadable = PR_FALSE;
    if (mObjCount == 0)
    {
        // This dll can be unloaded now
        unloadable = PR_TRUE;
    }

    if (okToUnload) *okToUnload = unloadable;
    return NS_OK;
}

//////////////////////////////////////////////////////////////////////
