/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsStaticComponent.h"
#include "nsIComponentLoader.h"
#include "nsVoidArray.h"
#include "pldhash.h"
#include NEW_H
#include <stdio.h>

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

    static NS_HIDDEN_(PLDHashOperator) PR_CALLBACK
    info_RegisterSelf(PLDHashTable *table, PLDHashEntryHdr *hdr,
                      PRUint32 number, void *arg);

private:
    ~nsStaticComponentLoader() {
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
    nsVoidArray                   mDeferredComponents;
};

PR_STATIC_CALLBACK(void)
info_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    StaticModuleInfo *info = NS_STATIC_CAST(StaticModuleInfo *, entry);
    info->module = 0;
    info->~StaticModuleInfo();
}

PR_STATIC_CALLBACK(PRBool)
info_InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry, const void *key)
{
    // Construct so that our nsCOMPtr is zeroed, etc.
    new (NS_STATIC_CAST(void *, entry)) StaticModuleInfo();
    return PR_TRUE;
}

/* static */ PLDHashTableOps nsStaticComponentLoader::sInfoHashOps = {
    PL_DHashAllocTable,    PL_DHashFreeTable,
    PL_DHashGetKeyStub,    PL_DHashStringKey, PL_DHashMatchStringKey,
    PL_DHashMoveEntryStub, info_ClearEntry,
    PL_DHashFinalizeStub,  info_InitEntry
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStaticComponentLoader, nsIComponentLoader)

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
        // We're a static build with no static modules to
        // register. This can happen in shared uses (such as the GRE)
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

PLDHashOperator PR_CALLBACK
nsStaticComponentLoader::info_RegisterSelf(PLDHashTable *table,
                                           PLDHashEntryHdr *hdr,
                                           PRUint32 number, void *arg)
{
    nsStaticComponentLoader *loader = NS_STATIC_CAST(nsStaticComponentLoader *,
                                                     arg);
    nsIComponentManager *mgr = loader->mComponentMgr;
    StaticModuleInfo *info = NS_STATIC_CAST(StaticModuleInfo *, hdr);
    
    nsresult rv;
    if (!info->module) {
        rv = info->info.getModule(mgr, nsnull, getter_AddRefs(info->module));
#ifdef DEBUG
        fprintf(stderr, "nSCL: getModule(\"%s\"): %lx\n", info->info.name, rv);
#endif
        if (NS_FAILED(rv))
            return PL_DHASH_NEXT; // oh well.
    }

    rv = info->module->RegisterSelf(mgr, nsnull, info->info.name,
                                    staticComponentType);
#ifdef DEBUG
    fprintf(stderr, "nSCL: autoreg of \"%s\": %lx\n", info->info.name, rv);
#endif

    if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN)
        loader->mDeferredComponents.AppendElement(info);

    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsStaticComponentLoader::AutoRegisterComponents(PRInt32 when, nsIFile *dir)
{
    if (mAutoRegistered)
        return NS_OK;

    // if a directory has been explicitly specified, then return early.  we
    // don't load static components from disk ;)
    if (dir)
        return NS_OK;

    nsresult rv;
    if (NS_FAILED(rv = GetModuleInfo()))
        return rv;

    PL_DHashTableEnumerate(&mInfoHash, info_RegisterSelf, this);

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
                                                    PRBool *aRegistered)
{
    *aRegistered = PR_FALSE;
    if (!mDeferredComponents.Count())
        return NS_OK;

    for (int i = mDeferredComponents.Count() - 1; i >= 0; i--) {
        StaticModuleInfo *info = NS_STATIC_CAST(StaticModuleInfo *,
                                                mDeferredComponents[i]);
        nsresult rv = info->module->RegisterSelf(mComponentMgr, nsnull,
                                                 info->info.name,
                                                 staticComponentType);
        if (rv != NS_ERROR_FACTORY_REGISTER_AGAIN) {
            if (NS_SUCCEEDED(rv))
                *aRegistered = PR_TRUE;
            mDeferredComponents.RemoveElementAt(i);
        }
    }
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
