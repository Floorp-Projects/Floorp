/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
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
nsComponentManager::ProgIDToCLSID(const char *aProgID,
                                  nsCID *aClass)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->ProgIDToCLSID(aProgID, aClass);
}
  
nsresult
nsComponentManager::CLSIDToProgID(nsCID *aClass,
                                  char* *aClassName,
                                  char* *aProgID)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CLSIDToProgID(*aClass, aClassName, aProgID);
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
nsComponentManager::CreateInstance(const char *aProgID,
                                   nsISupports *aDelegate,
                                   const nsIID &aIID,
                                   void **aResult)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->CreateInstanceByProgID(aProgID, aDelegate, aIID, aResult);
}

nsresult
nsComponentManager::RegisterFactory(const nsCID &aClass,
                                    const char *aClassName,
                                    const char *aProgID,
                                    nsIFactory *aFactory,
                                    PRBool aReplace)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterFactory(aClass, aClassName, aProgID,
                               aFactory, aReplace);
}

nsresult
nsComponentManager::RegisterComponent(const nsCID &aClass,
                                      const char *aClassName,
                                      const char *aProgID,
                                      const char *aLibraryPersistentDescriptor,
                                      PRBool aReplace,
                                      PRBool aPersist)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponent(aClass, aClassName, aProgID,
                                 aLibraryPersistentDescriptor, aReplace, aPersist);
}

nsresult
nsComponentManager::RegisterComponentSpec(const nsCID &aClass,
                                      const char *aClassName,
                                      const char *aProgID,
                                      nsIFileSpec *aLibrary,
                                      PRBool aReplace,
                                      PRBool aPersist)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponentSpec(aClass, aClassName, aProgID,
                                     aLibrary, aReplace, aPersist);
}

nsresult
nsComponentManager::RegisterComponentLib(const nsCID &aClass,
                                         const char *aClassName,
                                         const char *aProgID,
                                         const char *adllName,
                                         PRBool aReplace,
                                         PRBool aPersist)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->RegisterComponentLib(aClass, aClassName, aProgID,
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
                                            nsIFileSpec *aLibrarySpec)
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
nsComponentManager::AutoRegister(PRInt32 when, nsIFileSpec *directory)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoRegister(when, directory);
}

nsresult
nsComponentManager::AutoRegisterComponent(PRInt32 when,
                                          nsIFileSpec *fullname)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->AutoRegisterComponent(when, fullname);
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
nsComponentManager::EnumerateProgIDs(nsIEnumerator** aEmumerator)
{
    nsIComponentManager* cm;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if (NS_FAILED(rv)) return rv;
    return cm->EnumerateProgIDs(aEmumerator);
}



