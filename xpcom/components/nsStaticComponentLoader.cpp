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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsStaticComponentLoader.h"

#include "nsComponentManager.h"
#include "nsCOMPtr.h"
#include "pldhash.h"

#include "nsIModule.h"

#include NEW_H
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *sLog = PR_NewLogModule("StaticComponentLoader");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

extern const char staticComponentType[];

struct StaticModuleInfo : public PLDHashEntryHdr {
    nsStaticModuleInfo  info;
    nsCOMPtr<nsIModule> module;

    // We want to autoregister the components in the order they come to us
    // in the static component list, so we keep a linked list.
    StaticModuleInfo   *next;
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

/* static */ PLDHashTableOps nsStaticModuleLoader::sInfoHashOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashStringKey,
    PL_DHashMatchStringKey,
    PL_DHashMoveEntryStub,
    info_ClearEntry,
    PL_DHashFinalizeStub,
    info_InitEntry
};

nsresult
nsStaticModuleLoader::Init(nsStaticModuleInfo const *aStaticModules,
                           PRUint32 aModuleCount)
{
    if (!PL_DHashTableInit(&mInfoHash, &sInfoHashOps, nsnull,
                           sizeof(StaticModuleInfo), 1024)) {
        mInfoHash.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (! aStaticModules)
        return NS_OK;

    StaticModuleInfo *prev = nsnull;

    for (PRUint32 i = 0; i < aModuleCount; ++i) {
        StaticModuleInfo *info =
            NS_STATIC_CAST(StaticModuleInfo *,
                           PL_DHashTableOperate(&mInfoHash, aStaticModules[i].name,
                                                PL_DHASH_ADD));
        if (!info)
            return NS_ERROR_OUT_OF_MEMORY;

        info->info = aStaticModules[i];
        if (prev)
            prev->next = info;
        else
            mFirst = info;

        prev = info;
    }

    return NS_OK;
}

void
nsStaticModuleLoader::EnumerateModules(StaticLoaderCallback cb,
                                       nsTArray<DeferredModule> &deferred)
{
    for (StaticModuleInfo *c = mFirst; c; c = c->next) {
        if (!c->module) {
            nsresult rv = c->info.
                getModule(nsComponentManagerImpl::gComponentManager, nsnull,
                          getter_AddRefs(c->module));
            LOG(("nSCL: EnumerateModules(): %lx\n", rv));
            if (NS_FAILED(rv))
                continue;
        }
        cb(c->info.name, c->module, deferred);
    }
}

nsresult
nsStaticModuleLoader::GetModuleFor(const char *aLocation,
                                   nsIModule* *aResult)
{
    nsresult rv;
    StaticModuleInfo *info = 
        NS_STATIC_CAST(StaticModuleInfo *,
                       PL_DHashTableOperate(&mInfoHash, aLocation,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(info))
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    if (!info->module) {
        rv = info->info.
            getModule(nsComponentManagerImpl::gComponentManager, nsnull,
                      getter_AddRefs(info->module));
        LOG(("nSCL: GetModuleForFor(\"%s\"): %lx\n", aLocation, rv));
        if (NS_FAILED(rv))
            return rv;
    }

    NS_ADDREF(*aResult = info->module);
    return NS_OK;
}
