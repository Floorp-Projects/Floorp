/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#include "nsIComponentManager.h"

nsresult
nsComponentManager::Initialize(void)
{
    return NS_OK;
}

nsresult
nsComponentManager::FindFactory(const nsCID &aClass,
                                nsIFactory **aFactory)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->FindFactory(aClass, aFactory);
}

nsresult
nsComponentManager::GetClassObject(const nsCID &aClass, const nsIID &aIID,
                                   void **aResult)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->GetClassObject(aClass, aIID, aResult);
}

nsresult
nsComponentManager::ContractIDToClassID(const char *aContractID,
                                  nsCID *aClass)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->ContractIDToClassID(aContractID, aClass);
}
  
nsresult
nsComponentManager::CLSIDToContractID(nsCID *aClass,
                                  char* *aClassName,
                                  char* *aContractID)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CLSIDToContractID(*aClass, aClassName, aContractID);
}
  
nsresult
nsComponentManager::CreateInstance(const nsCID &aClass, 
                                   nsISupports *aDelegate,
                                   const nsIID &aIID,
                                   void **aResult)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CreateInstance(aClass, aDelegate, aIID, aResult);
}

nsresult
nsComponentManager::CreateInstance(const char *aContractID,
                                   nsISupports *aDelegate,
                                   const nsIID &aIID,
                                   void **aResult)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
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
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
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
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
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
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
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
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponentLib(aClass, aClassName, aContractID,
                                     adllName, aReplace, aPersist);
}

nsresult
nsComponentManager::UnregisterFactory(const nsCID &aClass,
                                      nsIFactory *aFactory)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->UnregisterFactory(aClass, aFactory);
}

nsresult
nsComponentManager::UnregisterComponent(const nsCID &aClass,
                                        const char *aLibrary)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->UnregisterComponent(aClass, aLibrary);
}

nsresult
nsComponentManager::UnregisterComponentSpec(const nsCID &aClass,
                                            nsIFile *aLibrarySpec)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->UnregisterComponentSpec(aClass, aLibrarySpec);
}

nsresult
nsComponentManager::FreeLibraries(void)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->FreeLibraries();
}

nsresult
nsComponentManager::AutoRegister(PRInt32 when, nsIFile *directory)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoRegister(when, directory);
}

nsresult
nsComponentManager::AutoRegisterComponent(PRInt32 when,
                                          nsIFile *fullname)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoRegisterComponent(when, fullname);
}

nsresult
nsComponentManager::AutoUnregisterComponent(PRInt32 when,
                                          nsIFile *fullname)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoUnregisterComponent(when, fullname);
}

nsresult 
nsComponentManager::IsRegistered(const nsCID &aClass,
                                 PRBool *aRegistered)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->IsRegistered(aClass, aRegistered);
}

nsresult 
nsComponentManager::EnumerateCLSIDs(nsIEnumerator** aEmumerator)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->EnumerateCLSIDs(aEmumerator);
}

nsresult 
nsComponentManager::EnumerateContractIDs(nsIEnumerator** aEmumerator)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->EnumerateContractIDs(aEmumerator);
}



