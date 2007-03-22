/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Christopher Seawood <cls@seawood.org> 
 *     Doug Turner <dougt@netscape.com>
 *     Chris Waterson <waterson@netscape.com>
 *     Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIGenericFactory.h"
#include "nsXPCOM.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsMemory.h"
#include "nsAutoPtr.h"

#include "module_list.h"

#define NSGETMODULE(_name) _name##_NSGetModule

#define MODULE(_name) \
NSGETMODULE_ENTRY_POINT(_name) (nsIComponentManager*, nsIFile*, nsIModule**);

MODULES

#undef MODULE

#define MODULE(_name) NSGETMODULE(_name),

static const nsGetModuleProc kGetModules[] = {
    MODULES
};

#undef MODULE

class MetaModule : public nsIModule
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMODULE

    MetaModule() { }
    nsresult Init(nsIComponentManager*, nsIFile*);

private:
    ~MetaModule() { }

    nsCOMArray<nsIModule> mModules;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(MetaModule, nsIModule)

nsresult
MetaModule::Init(nsIComponentManager* aCompMgr, nsIFile* aLocation)
{
    if (!mModules.SetCapacity(NS_ARRAY_LENGTH(kGetModules)))
        return NS_ERROR_OUT_OF_MEMORY;

    // eat all errors
    nsGetModuleProc const *end = kGetModules + NS_ARRAY_LENGTH(kGetModules);

    nsCOMPtr<nsIModule> module;
    for (nsGetModuleProc const *cur = kGetModules; cur < end; ++cur) {
        nsresult rv = (*cur)(aCompMgr, aLocation, getter_AddRefs(module));
        if (NS_SUCCEEDED(rv))
            mModules.AppendObject(module);
    }

    return NS_OK;
}

NS_IMETHODIMP
MetaModule::GetClassObject(nsIComponentManager* aCompMgr, REFNSCID aCID,
                           REFNSIID aIID, void **aResult)
{
    for (PRInt32 i = mModules.Count() - 1; i >= 0; --i) {
        nsresult rv = mModules[i]->GetClassObject(aCompMgr, aCID,
                                                  aIID, aResult);
        if (NS_SUCCEEDED(rv))
            return rv;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MetaModule::RegisterSelf(nsIComponentManager* aCompMgr, nsIFile* aLocation,
                         const char *aStr, const char *aType)
{
    for (PRInt32 i = mModules.Count() - 1; i >= 0; --i) {
        mModules[i]->RegisterSelf(aCompMgr, aLocation, aStr, aType);
    }
    return NS_OK;
}

NS_IMETHODIMP
MetaModule::UnregisterSelf(nsIComponentManager* aCompMgr, nsIFile* aLocation,
                           const char *aStr)
{
    for (PRInt32 i = mModules.Count() - 1; i >= 0; --i) {
        mModules[i]->UnregisterSelf(aCompMgr, aLocation, aStr);
    }
    return NS_OK;
}

NS_IMETHODIMP
MetaModule::CanUnload(nsIComponentManager* aCompMgr, PRBool *aResult)
{
    for (PRInt32 i = mModules.Count() - 1; i >= 0; --i) {
        nsresult rv = mModules[i]->CanUnload(aCompMgr, aResult);
        if (NS_FAILED(rv))
            return rv;

        // Any submodule may veto unloading
        if (!*aResult)
            return NS_OK;
    }

    return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSGetModule(nsIComponentManager *servMgr,      
            nsIFile *location,                 
            nsIModule **result)                
{                                                                            
    nsRefPtr<MetaModule> mmodule = new MetaModule();
    nsresult rv = mmodule->Init(servMgr, location);
    if (NS_FAILED(rv))
        return rv;

    NS_ADDREF(*result = mmodule);
    return NS_OK;
}
