/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsJPGModule.h"
#include "nsJPGDecoder.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kJPGDecoderCID, NS_JPGDECODER_CID);
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

static nsJPGModule *gModule = NULL;

//////////////////////////////////////////////////////////////////////
// Module entry point

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsJPGModule *module;
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsJPGModule: Module already created.");

    module = new nsJPGModule;
    if (module == NULL) return NS_ERROR_OUT_OF_MEMORY;

    rv = module->QueryInterface(nsIModule::GetIID(), (void **)return_cobj);

    if (NS_FAILED(rv))
    {
        delete module;
        module = NULL;
    }

    // WARNING: Weak Reference
    gModule = module;

    return rv;
}

//////////////////////////////////////////////////////////////////////
// JPG Decoder Factory using nsIGenericFactory

static NS_IMETHODIMP
nsJPGDecoderCreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    JPGDecoder *dec = NULL;
    *aResult = NULL;
    il_container* ic = NULL;

    if (aOuter && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;  

    ic = new il_container();
    if (!ic)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    dec = new JPGDecoder(ic);
    if (!dec)
    {
        delete ic;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult res = dec->QueryInterface(aIID, aResult);
    if (NS_FAILED(res))
    {
        *aResult = NULL;
        delete dec;
    }
    delete ic; /* is a place holder */
    return res;
}


//////////////////////////////////////////////////////////////////////
// JPG Decoder Module Implementation

NS_IMPL_ISUPPORTS(nsJPGModule, nsIModule::GetIID())

nsJPGModule::nsJPGModule(void)
  : mObjCount(-1), mClassObject(NULL)
{
    NS_INIT_ISUPPORTS();
}

nsJPGModule::~nsJPGModule(void)
{
    NS_ASSERTION(mObjCount <= 0, "Module released while having outstanding objects.");
    if (mClassObject)
    {
        int refcnt;
        NS_RELEASE2(mClassObject, refcnt);
        NS_ASSERTION(refcnt == 0, "nsJPGModule::~nsJPGModule() ClassObject refcount nonzero");
    }
}

//
// The class object for us is just the factory and nothing more.
//
NS_IMETHODIMP
nsJPGModule::GetClassObject(nsIComponentManager *aCompMgr, const nsCID & aClass,
                            const nsIID &aIID, void **r_classObj)
{
    nsresult rv;

    if( !aClass.Equals(kJPGDecoderCID))
		return NS_ERROR_FACTORY_NOT_REGISTERED;

    // If this aint the first time, return the cached class object
    if (mClassObject == NULL)
    {
        nsCOMPtr<nsIGenericFactory> fact;
        rv = NS_NewGenericFactory(getter_AddRefs(fact), nsJPGDecoderCreateInstance);
        if (NS_FAILED(rv)) return rv;
    
        // Store the class object in our global
        rv = fact->QueryInterface(NS_GET_IID(nsISupports), (void **)&mClassObject);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mClassObject->QueryInterface(aIID, r_classObj);
    return rv;
}

NS_IMETHODIMP
nsJPGModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec *location,
                          const char *registryLocation,
                          const char *componentType)
{
    nsresult rv;
    rv = aCompMgr->RegisterComponentSpec(kJPGDecoderCID, 
                                         "Netscape JPGDec", 
                                         "component://netscape/image/decoder&type=image/jpeg",
                                         location, PR_TRUE, PR_TRUE);
    return rv;
}

NS_IMETHODIMP
nsJPGModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                            nsIFileSpec *location,
                            const char *registryLocation)
{
    nsresult rv;
    rv = aCompMgr->UnregisterComponentSpec(kJPGDecoderCID, location);
    return rv;
}

NS_IMETHODIMP
nsJPGModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
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
