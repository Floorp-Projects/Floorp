/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsStaticComponent.h"
#include "nsIComponentLoader.h"

struct StaticModuleInfo : public nsStaticModuleInfo {
    nsCOMPtr<nsIModule> module;
};

class nsStaticComponentLoader : public nsIComponentLoader
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTLOADER

    nsStaticComponentLoader() : 
        mInfo(0), mCount(0), mAutoRegistered(PR_FALSE) {
		NS_INIT_REFCNT(); 
	}

    virtual ~nsStaticComponentLoader() {
        if (mInfo)
            delete [] mInfo;
    }

protected:
    nsresult GetModuleInfo();
    nsresult GetInfoFor(const char *aLocation, StaticModuleInfo **retval);

    StaticModuleInfo              *mInfo;
    PRUint32                      mCount;
    PRBool                        mAutoRegistered;
    nsCOMPtr<nsIComponentManager> mComponentMgr;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStaticComponentLoader, nsIComponentLoader);

NS_COM NSGetStaticModuleInfoFunc NSGetStaticModuleInfo;

nsresult
nsStaticComponentLoader::GetModuleInfo()
{
    if (!mInfo) {
        if (! NSGetStaticModuleInfo)
        {
            // apparently we're a static build with no static modules
            // to register. Suspicious, but might be as intended in certain
            // shared uses (such as by the stand-alone install engine)
            NS_WARNING("NSGetStaticModuleInfo not initialized -- is this right?");
            return NS_OK;
        }

        nsStaticModuleInfo *info;
        nsresult rv;
        if (NS_FAILED(rv = (*NSGetStaticModuleInfo)(&info, &mCount)))
            return rv;
        mInfo = new StaticModuleInfo[mCount];
        if (!mInfo)
            return NS_ERROR_OUT_OF_MEMORY;
        for (PRUint32 i = 0; i < mCount; i++) {
            mInfo[i].name = info[i].name;
            mInfo[i].getModule = info[i].getModule;
        }
    }
    return NS_OK;
}

nsresult
nsStaticComponentLoader::GetInfoFor(const char *aLocation,
                                    StaticModuleInfo **retval)
{
    nsresult rv;
    if (NS_FAILED(rv = GetModuleInfo()))
        return rv;

    for (PRUint32 i = 0; i < mCount; i++) {
        if (!strcmp(mInfo[i].name, aLocation)) {
            if (!mInfo[i].module) {
                rv = mInfo[i].getModule(mComponentMgr, nsnull,
                                        getter_AddRefs(mInfo[i].module));
#ifdef DEBUG
                fprintf(stderr, "nSCL: GetInfoFor(\"%s\"): %lx\n",
                        aLocation, rv);
#endif
                if (NS_FAILED(rv))
                    return rv;
            }
            *retval = &mInfo[i];
            return NS_OK;
        }
    }

    return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP
nsStaticComponentLoader::Init(nsIComponentManager *mgr, nsISupports *aReg)
{
    mComponentMgr = mgr;
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::AutoRegisterComponents(PRInt32 when, nsIFile *dir)
{
    if (mAutoRegistered)
        return NS_OK;

    nsresult rv;
    if (NS_FAILED(rv = GetModuleInfo()))
        return rv;

    for (PRUint32 i = 0; i < mCount; i++) {
        if (!mInfo[i].module) {
            mInfo[i].getModule(mComponentMgr, dir,
                               getter_AddRefs(mInfo[i].module));
        }
        if (mInfo[i].module) {
            rv = mInfo[i].module->RegisterSelf(mComponentMgr, dir,
                                               mInfo[i].name,
                                               staticComponentType);
#ifdef DEBUG
            fprintf(stderr, "nSCL: autoreg of \"%s\": %lx\n",
                    mInfo[i].name, rv);
#endif
            // XXX handle deferred registration
        }
    }

    mAutoRegistered = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::AutoUnregisterComponent(PRInt32 when, 
                                                 nsIFile *component,
                                                 PRBool *retval)
{
    *retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::AutoRegisterComponent(PRInt32 when, nsIFile *component,
                                               PRBool *retval)
{
    *retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::RegisterDeferredComponents(PRInt32 when,
                                                    PRBool *retval)
{
    *retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::OnRegister(const nsCID &aCID, const char *aType,
                                    const char *aClassName,
                                    const char *aContractID,
                                    const char *aLocation,
                                    PRBool aReplace, PRBool aPersist)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::UnloadAll(PRInt32 aWhen)
{
    if (mInfo) {
        for (PRUint32 i = 0; i < mCount; i++)
            mInfo[i].module = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::GetFactory(const nsCID &aCID, const char *aLocation,
                                    const char *aType, nsIFactory **_retval)
{
    StaticModuleInfo *info;
    nsresult rv;

    if (NS_FAILED(rv = GetInfoFor(aLocation, &info)))
        return rv;

    return info->module->GetClassObject(mComponentMgr, aCID,
                                        NS_GET_IID(nsIFactory),
                                        (void **)_retval);
}

nsresult
NS_NewStaticComponentLoader(nsIComponentLoader **retval)
{
    *retval = NS_STATIC_CAST(nsIComponentLoader *, 
                             new nsStaticComponentLoader);
    return *retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
