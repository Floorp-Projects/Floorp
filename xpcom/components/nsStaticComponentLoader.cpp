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
#include "pldhash.h"

struct StaticModuleInfo : public PLDHashEntryHdr {
    nsStaticModuleInfo  info;
    nsCOMPtr<nsIModule> module;
};

class nsStaticComponentLoader : public nsIComponentLoader
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTLOADER

    nsStaticComponentLoader() : 
        mAutoRegistered(PR_FALSE), mLoadedInfo(PR_FALSE) {
	}

    virtual ~nsStaticComponentLoader() {
        if (mInfoHash.ops)
            PL_DHashTableFinish(&mInfoHash);
    }

protected:
    nsresult GetModuleInfo();
    nsresult GetInfoFor(const char *aLocation, StaticModuleInfo **retval);

    PRBool                        mAutoRegistered;
    PRBool                        mLoadedInfo;
    nsCOMPtr<nsIComponentManager> mComponentMgr;
    PLDHashTable                  mInfoHash;
    static PLDHashTableOps        sInfoHashOps;
};

PR_STATIC_CALLBACK(const void *)
info_GetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    StaticModuleInfo *info = NS_STATIC_CAST(StaticModuleInfo *, entry);
    return info->info.name;
}

PR_STATIC_CALLBACK(PRBool)
info_MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,
                const void *key)
{
    const StaticModuleInfo *info = NS_STATIC_CAST(const StaticModuleInfo *,
                                                  entry);
    const char *name = NS_STATIC_CAST(const char *, key);
    return !strcmp(info->info.name, name);
}

PR_STATIC_CALLBACK(void)
info_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    StaticModuleInfo *info = NS_STATIC_CAST(StaticModuleInfo *, entry);
    info->module = 0;
    info->~StaticModuleInfo();
}

PR_STATIC_CALLBACK(void)
info_InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry, const void *key)
{
    // Construct so that our nsCOMPtr is zeroed, etc.
    (void)new (NS_STATIC_CAST(void *, entry)) StaticModuleInfo();
}

/* static */ PLDHashTableOps nsStaticComponentLoader::sInfoHashOps = {
    PL_DHashAllocTable,    PL_DHashFreeTable,
    info_GetKey,           PL_DHashStringKey, info_MatchEntry,
    PL_DHashMoveEntryStub, info_ClearEntry,
    PL_DHashFinalizeStub,  info_InitEntry
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStaticComponentLoader, nsIComponentLoader);

NS_COM NSGetStaticModuleInfoFunc NSGetStaticModuleInfo;

nsresult
nsStaticComponentLoader::GetModuleInfo()
{
    if (mLoadedInfo)
        return NS_OK;

    if (!mInfoHash.ops) {       // creation failed in init, why are we here?
        NS_WARNING("operating on uninitialized static component loader");
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (! NSGetStaticModuleInfo) {
        // apparently we're a static build with no static modules
        // to register. Suspicious, but might be as intended in certain
        // shared uses (such as by the stand-alone install engine)
        NS_WARNING("NSGetStaticModuleInfo not initialized -- is this right?");
        return NS_OK;
    }

    nsStaticModuleInfo *infoList;
    PRUint32 count;
    nsresult rv;
    if (NS_FAILED(rv = (*NSGetStaticModuleInfo)(&infoList, &count)))
        return rv;
    for (PRUint32 i = 0; i < count; i++) {
        StaticModuleInfo *info =
            NS_STATIC_CAST(StaticModuleInfo *,
                           PL_DHashTableOperate(&mInfoHash, infoList[i].name,
                                                PL_DHASH_ADD));
        if (!info)
            return NS_ERROR_OUT_OF_MEMORY;
        info->info = infoList[i];
    }

    mLoadedInfo = PR_TRUE;
    return NS_OK;
}

nsresult
nsStaticComponentLoader::GetInfoFor(const char *aLocation,
                                    StaticModuleInfo **retval)
{
    nsresult rv;
    if (NS_FAILED(rv = GetModuleInfo()))
        return rv;

    StaticModuleInfo *info = 
        NS_STATIC_CAST(StaticModuleInfo *,
                       PL_DHashTableOperate(&mInfoHash, aLocation,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(info))
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    if (!info->module) {
        rv = info->info.getModule(mComponentMgr, nsnull,
                             getter_AddRefs(info->module));
#ifdef DEBUG
        fprintf(stderr, "nSCL: GetInfoFor(\"%s\"): %lx\n", aLocation, rv);
#endif
        if (NS_FAILED(rv))
            return rv;
    }

    *retval = info;
    return NS_OK;
}

NS_IMETHODIMP
nsStaticComponentLoader::Init(nsIComponentManager *mgr, nsISupports *aReg)
{
    mComponentMgr = mgr;
    if (!PL_DHashTableInit(&mInfoHash, &sInfoHashOps, nsnull,
                           sizeof(StaticModuleInfo), 1024)) {
        mInfoHash.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

struct RegisterSelfData
{
    nsIComponentManager *mgr;
    nsIFile             *dir;
};

PR_STATIC_CALLBACK(PLDHashOperator)
info_RegisterSelf(PLDHashTable *table, PLDHashEntryHdr *hdr,
                  PRUint32 number, void *arg)
{
    RegisterSelfData *data = NS_STATIC_CAST(RegisterSelfData *, arg);
    StaticModuleInfo *info = NS_STATIC_CAST(StaticModuleInfo *, hdr);
    
    nsresult rv;
    if (!info->module) {
        rv = info->info.getModule(data->mgr, nsnull,
                                  getter_AddRefs(info->module));
#ifdef DEBUG
        fprintf(stderr, "nSCL: getModule(\"%s\"): %lx\n", info->info.name, rv);
#endif
        if (NS_FAILED(rv))
            return PL_DHASH_NEXT; // oh well.
    }

    rv = info->module->RegisterSelf(data->mgr, data->dir,
                                    info->info.name, staticComponentType);
#ifdef DEBUG
    fprintf(stderr, "nSCL: autoreg of \"%s\": %lx\n", info->info.name, rv);
#endif

    // XXX handle deferred registration

    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsStaticComponentLoader::AutoRegisterComponents(PRInt32 when, nsIFile *dir)
{
    if (mAutoRegistered)
        return NS_OK;

    nsresult rv;
    if (NS_FAILED(rv = GetModuleInfo()))
        return rv;

    RegisterSelfData data = { mComponentMgr, dir };

    PL_DHashTableEnumerate(&mInfoHash, info_RegisterSelf, &data);

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
    NS_IF_ADDREF(*retval = NS_STATIC_CAST(nsIComponentLoader *, 
                                          new nsStaticComponentLoader));
    return *retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
