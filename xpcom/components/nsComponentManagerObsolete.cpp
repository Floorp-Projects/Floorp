
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
 * The Original Code is XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

////////////////////////////////////////////////////////////////////////////////
// Global Static Component Manager Methods
// (for when you need to link with xpcom)

#include "nsXPCOM.h"
#include "nsIComponentManagerObsolete.h"
#include "nsComponentManagerObsolete.h"


nsresult
nsComponentManager::Initialize(void)
{
    NS_NOTREACHED("nsComponentManager::Initialize is deprecated");
    return NS_OK;
}

nsresult
nsComponentManager::FindFactory(const nsCID &aClass,
                                nsIFactory **aFactory)
{
    NS_NOTREACHED("nsComponentManager::FindFactory is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->FindFactory(aClass, aFactory);
}

nsresult
nsComponentManager::GetClassObject(const nsCID &aClass, const nsIID &aIID,
                                   void **aResult)
{
    // XXX still in use by nsComponentManagerUtils.h

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->GetClassObject(aClass, aIID, aResult);
}

nsresult
nsComponentManager::GetClassObjectByContractID(const char *aContractID,
                                               const nsIID &aIID,
                                               void **aResult)
{
    // XXX still in use by nsComponentManagerUtils.h

    nsCOMPtr<nsIComponentManager> cm;
    nsresult rv = NS_GetComponentManager(getter_AddRefs(cm));
    if (NS_FAILED(rv)) return rv;
    return cm->GetClassObjectByContractID(aContractID, aIID, aResult);
}

nsresult
nsComponentManager::ContractIDToClassID(const char *aContractID,
                                  nsCID *aClass)
{
    NS_NOTREACHED("nsComponentManager::ContractIDToClassID is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->ContractIDToClassID(aContractID, aClass);
}
  
nsresult
nsComponentManager::CLSIDToContractID(nsCID *aClass,
                                  char* *aClassName,
                                  char* *aContractID)
{
    NS_NOTREACHED("nsComponentManager::CLSIDToContractID is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CLSIDToContractID(*aClass, aClassName, aContractID);
}
  
nsresult
nsComponentManager::CreateInstance(const nsCID &aClass, 
                                   nsISupports *aDelegate,
                                   const nsIID &aIID,
                                   void **aResult)
{
    // XXX still in use by nsComponentManagerUtils.h

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CreateInstance(aClass, aDelegate, aIID, aResult);
}

nsresult
nsComponentManager::CreateInstance(const char *aContractID,
                                   nsISupports *aDelegate,
                                   const nsIID &aIID,
                                   void **aResult)
{
    // XXX still in use by nsComponentManagerUtils.h

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CreateInstanceByContractID(aContractID, aDelegate, aIID, aResult);
}

nsresult
nsComponentManager::RegisterFactory(const nsCID &aClass,
                                    const char *aClassName,
                                    const char *aContractID,
                                    nsIFactory *aFactory,
                                    PRBool aReplace)
{
    NS_NOTREACHED("nsComponentManager::RegisterFactory is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterFactory(aClass, aClassName, aContractID,
                               aFactory, aReplace);
}

nsresult
nsComponentManager::RegisterComponent(const nsCID &aClass,
                                      const char *aClassName,
                                      const char *aContractID,
                                      const char *aLibraryPersistentDescriptor,
                                      PRBool aReplace,
                                      PRBool aPersist)
{
    NS_NOTREACHED("nsComponentManager::RegisterComponent is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponent(aClass, aClassName, aContractID,
                                 aLibraryPersistentDescriptor, aReplace, aPersist);
}

nsresult
nsComponentManager::RegisterComponentSpec(const nsCID &aClass,
                                      const char *aClassName,
                                      const char *aContractID,
                                      nsIFile *aLibrary,
                                      PRBool aReplace,
                                      PRBool aPersist)
{
    NS_NOTREACHED("nsComponentManager::RegisterComponentSpec is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponentSpec(aClass, aClassName, aContractID,
                                     aLibrary, aReplace, aPersist);
}

nsresult
nsComponentManager::RegisterComponentLib(const nsCID &aClass,
                                         const char *aClassName,
                                         const char *aContractID,
                                         const char *adllName,
                                         PRBool aReplace,
                                         PRBool aPersist)
{
    NS_NOTREACHED("nsComponentManager::RegisterComponentLib is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponentLib(aClass, aClassName, aContractID,
                                     adllName, aReplace, aPersist);
}

nsresult
nsComponentManager::UnregisterFactory(const nsCID &aClass,
                                      nsIFactory *aFactory)
{
    NS_NOTREACHED("nsComponentManager::UnregisterFactory is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->UnregisterFactory(aClass, aFactory);
}

nsresult
nsComponentManager::UnregisterComponent(const nsCID &aClass,
                                        const char *aLibrary)
{
    NS_NOTREACHED("nsComponentManager::UnregisterComponent is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->UnregisterComponent(aClass, aLibrary);
}

nsresult
nsComponentManager::UnregisterComponentSpec(const nsCID &aClass,
                                            nsIFile *aLibrarySpec)
{
    NS_NOTREACHED("nsComponentManager::UnregisterComponentSpec is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->UnregisterComponentSpec(aClass, aLibrarySpec);
}

nsresult
nsComponentManager::FreeLibraries(void)
{
    NS_NOTREACHED("nsComponentManager::FreeLibraries is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->FreeLibraries();
}

nsresult
nsComponentManager::AutoRegister(PRInt32 when, nsIFile *directory)
{
    NS_NOTREACHED("nsComponentManager::AutoRegister is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoRegister(when, directory);
}

nsresult
nsComponentManager::AutoRegisterComponent(PRInt32 when,
                                          nsIFile *fullname)
{
    NS_NOTREACHED("nsComponentManager::AutoRegisterComponent is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoRegisterComponent(when, fullname);
}

nsresult
nsComponentManager::AutoUnregisterComponent(PRInt32 when,
                                          nsIFile *fullname)
{
    NS_NOTREACHED("nsComponentManager::AutoUnregisterComponent is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoUnregisterComponent(when, fullname);
}

nsresult 
nsComponentManager::IsRegistered(const nsCID &aClass,
                                 PRBool *aRegistered)
{
    NS_NOTREACHED("nsComponentManager::IsRegistered is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->IsRegistered(aClass, aRegistered);
}

nsresult 
nsComponentManager::EnumerateCLSIDs(nsIEnumerator** aEnumerator)
{
    NS_NOTREACHED("nsComponentManager::EnumerateCLSIDs is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->EnumerateCLSIDs(aEnumerator);
}

nsresult 
nsComponentManager::EnumerateContractIDs(nsIEnumerator** aEnumerator)
{
    NS_NOTREACHED("nsComponentManager::EnumerateContractIDs is deprecated");

    nsIComponentManagerObsolete* cm;
    nsresult rv = NS_GetGlobalComponentManager((nsIComponentManager**)&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->EnumerateContractIDs(aEnumerator);
}
