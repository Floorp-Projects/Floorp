/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <stdlib.h>
#include <iostream.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "plstr.h"
#include "prlink.h"
#include "nsRepository.h"
#ifdef USE_NSREG
#define XP_BEGIN_PROTOS extern "C" {
#define XP_END_PROTOS };
#include "NSReg.h"
#endif

nsHashtable *NSRepository::factories = NULL;
PRMonitor *NSRepository::monitor = NULL;

static NS_DEFINE_IID(kFactory2IID, NS_IFACTORY2_IID);

class FactoryEntry {
public:
  nsCID cid;
  nsIFactory *factory;
  char *library;
  PRLibrary *instance;

  FactoryEntry(const nsCID &aClass, nsIFactory *aFactory, 
               const char *aLibrary) {
    cid = aClass;
    factory = aFactory;
    if (aLibrary != NULL) {
      library = PL_strdup(aLibrary);
    } else {
      library = NULL;
    }
    instance = NULL;
  }
  ~FactoryEntry() {
    if (library != NULL) {
      free(library);
    }
    if (instance != NULL) {
      PR_UnloadLibrary(instance);
    }
    if (factory != NULL) {
      factory->Release();
    }
  }
};

class IDKey: public nsHashKey {
private:
  nsID id;
  
public:
  IDKey(const nsID &aID) {
    id = aID;
  }
  
  PRUint32 HashValue(void) const {
    return id.m0;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (id.Equals(((const IDKey *) aKey)->id));
  }

  nsHashKey *Clone(void) const {
    return new IDKey(id);
  }
};

#ifdef USE_NSREG
#define USE_REGISTRY

static nsresult platformRegister(const nsCID &aCID, const char *aLibrary)
{
  HREG hreg;
  REGERR err = NR_RegOpen(NULL, &hreg);
  if (err == REGERR_OK) {
    RKEY key;
    err = NR_RegAddKey(hreg, ROOTKEY_COMMON, 
                       "Classes", &key);
    if (err == REGERR_OK) {
      char *cidString = aCID.ToString();
      err = NR_RegSetEntryString(hreg, key, cidString, (char *) aLibrary);
      delete [] cidString;
    }
    NR_RegClose(hreg);
  }

  return err;
}

static nsresult platformUnregister(const nsCID &aCID, const char *aLibrary) 
{  
  HREG hreg;
  REGERR err = NR_RegOpen(NULL, &hreg);
  if (err == REGERR_OK) {
    RKEY key;
    err = NR_RegAddKey(hreg, ROOTKEY_COMMON, 
                       "Classes", &key);
    if (err == REGERR_OK) {
      char *cidString = aCID.ToString();
      err = NR_RegDeleteEntry(hreg, key, cidString);
      delete [] cidString;
    }
    NR_RegClose(hreg);
  }
  return err;
}

static FactoryEntry *platformFind(const nsCID &aCID)
{
  FactoryEntry *res = NULL;
  HREG hreg;
  REGERR err = NR_RegOpen(NULL, &hreg);
  if (err == REGERR_OK) {
    RKEY key;
    err = NR_RegGetKey(hreg, ROOTKEY_COMMON, 
                       "Classes", &key);
    if (err == REGERR_OK) {
      char *cidString = aCID.ToString();
      char library[256];
      uint32 len = 256;
      err = NR_RegGetEntryString(hreg, key, cidString, library, len);
                                 
      delete [] cidString;
      if (err == REGERR_OK) {
        res = new FactoryEntry(aCID, NULL, library);
        NR_RegClose(hreg);
      }
    }
  }
  return res;
}

#else // USE_NSREG

#ifdef _WIN32
#define USE_REGISTRY

static nsresult platformRegister(const nsCID &aCID, const char *aLibrary)
{
  HKEY key;
  LONG res = RegCreateKey(HKEY_CURRENT_USER, "SOFTWARE\\Netscape\\CID", &key);
  if (res == ERROR_SUCCESS) {
    char *cidString = aCID.ToString();
    RegSetValue(key, cidString, REG_SZ, aLibrary, PL_strlen(aLibrary));
    delete [] cidString;
    RegCloseKey(key);
  }
  return NS_OK;
}

static nsresult platformUnregister(const nsCID &aCID, const char *aLibrary)
{
  HKEY key;
  nsresult res = NS_OK;
  LONG err = RegCreateKey(HKEY_CURRENT_USER, "SOFTWARE\\Netscape\\CID", &key);
  if (err == ERROR_SUCCESS) {
    char *cidString = aCID.ToString();
    err = RegDeleteKey(key, cidString);
    if (err != ERROR_SUCCESS) {
      res = NS_ERROR_FACTORY_NOT_UNREGISTERED;
    }
    delete [] cidString;
    RegCloseKey(key);
  }
  return res;
}

static FactoryEntry *platformFind(const nsCID &aCID)
{
  HKEY key;
  LONG res = RegOpenKey(HKEY_CURRENT_USER, "SOFTWARE\\Netscape\\CID", &key);

  if (res == ERROR_SUCCESS) {
    char *cidString = aCID.ToString();
    char library[_MAX_PATH];
    LONG len = _MAX_PATH;
    res = RegQueryValue(key, cidString, library, &len);
    delete [] cidString;
    if (res == ERROR_SUCCESS) {
      FactoryEntry *entry = new FactoryEntry(aCID, NULL, library);
      return entry;
    }
  }
  return NULL;
}

#endif // _WIN32

#endif // USE_NSREG

static nsresult loadFactory(FactoryEntry *aEntry,
                            nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aFactory = NULL;

  if (aEntry->instance == NULL) {
    aEntry->instance = PR_LoadLibrary(aEntry->library);
  }
  if (aEntry->instance != NULL) {
    nsFactoryProc proc = (nsFactoryProc) PR_FindSymbol(aEntry->instance,
                                                       "NSGetFactory");
    if (proc != NULL) {
      nsresult res = proc(aEntry->cid, aFactory);
      return res;
    }
  }

  return NS_ERROR_FACTORY_NOT_LOADED;
}

nsresult NSRepository::FindFactory(const nsCID &aClass,
                                   nsIFactory **aFactory) 
{
  checkInitialized();

  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  PR_EnterMonitor(monitor);

  IDKey key(aClass);
  FactoryEntry *entry = (FactoryEntry*) factories->Get(&key);

  nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef USE_REGISTRY
  if (entry == NULL) {
    entry = platformFind(aClass);
    // If we got one, cache it in our hashtable
    if (entry != NULL) {
      factories->Put(&key, entry);
    }
  }
#endif

  PR_ExitMonitor(monitor);

  if (entry != NULL) {
    if ((entry)->factory == NULL) {
      res = loadFactory(entry, aFactory);
    } else {
      *aFactory = entry->factory;
      (*aFactory)->AddRef();
      res = NS_OK;
    }
  }

  return res;
}

nsresult NSRepository::checkInitialized() 
{
  nsresult res = NS_OK;
  if (factories == NULL) {
    res = Initialize();
  }
  return res;
}

nsresult NSRepository::Initialize() 
{
  if (factories == NULL) {
    factories = new nsHashtable();
  }
  if (monitor == NULL) {
    monitor = PR_NewMonitor();
  }

#ifdef USE_NSREG
  NR_StartupRegistry();
#endif
  return NS_OK;
}

nsresult NSRepository::CreateInstance(const nsCID &aClass, 
                                      nsISupports *aDelegate,
                                      const nsIID &aIID,
                                      void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;

  nsIFactory *factory = NULL;
  nsresult res = FindFactory(aClass, &factory);
  if (NS_SUCCEEDED(res)) {
    res = factory->CreateInstance(aDelegate, aIID, aResult);
    factory->Release();

    return res;
  }
  
  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

nsresult NSRepository::CreateInstance2(const nsCID &aClass, 
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void *aSignature,
                                       void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;

  nsIFactory *factory = NULL;

  nsresult res = FindFactory(aClass, &factory);

  if (NS_SUCCEEDED(res)) {
    nsIFactory2 *factory2 = NULL;
    res = NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT;
    
    factory->QueryInterface(kFactory2IID, (void **) &factory2);

    if (factory2 != NULL) {
      res = factory2->CreateInstance2(aDelegate, aIID, aSignature, aResult);
      factory2->Release();
    }

    factory->Release();
    return res;
  }
  
  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

nsresult NSRepository::RegisterFactory(const nsCID &aClass,
                                       nsIFactory *aFactory, 
                                       PRBool aReplace)
{
  checkInitialized();

  nsIFactory *old = NULL;
  FindFactory(aClass, &old);
  
  if (old != NULL) {
    old->Release();
    if (!aReplace) {
      return NS_ERROR_FACTORY_EXISTS;
    }
  }

  PR_EnterMonitor(monitor);

  IDKey key(aClass);
  factories->Put(&key, new FactoryEntry(aClass, aFactory, NULL));

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult NSRepository::RegisterFactory(const nsCID &aClass,
                                       const char *aLibrary,
                                       PRBool aReplace,
                                       PRBool aPersist)
{
  checkInitialized();

  nsIFactory *old = NULL;
  FindFactory(aClass, &old);
  
  if (old != NULL) {
    old->Release();
    if (!aReplace) {
      return NS_ERROR_FACTORY_EXISTS;
    }
  }

  PR_EnterMonitor(monitor);

#ifdef USE_REGISTRY
  if (aPersist == PR_TRUE) {
    platformRegister(aClass, aLibrary);
  } 
  else
#endif
  {
    IDKey key(aClass);
    factories->Put(&key, new FactoryEntry(aClass, NULL, aLibrary));
  }

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult NSRepository::UnregisterFactory(const nsCID &aClass,
                                         nsIFactory *aFactory)
{
  checkInitialized();

  nsIFactory *old = NULL;
  FindFactory(aClass, &old);

  nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
  if (old != NULL) {
    if (old == aFactory) {
      PR_EnterMonitor(monitor);

      IDKey key(aClass);
      FactoryEntry *entry = (FactoryEntry *) factories->Remove(&key);
      delete entry;

      PR_ExitMonitor(monitor);
  
      res = NS_OK;
    }
    old->Release();
  }

  return res;
}

nsresult NSRepository::UnregisterFactory(const nsCID &aClass,
                                         const char *aLibrary)
{
  checkInitialized();

  IDKey key(aClass);
  FactoryEntry *old = (FactoryEntry *) factories->Get(&key);

  nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;

  PR_EnterMonitor(monitor);

  if (old != NULL) {
#ifdef XP_UNIX
    if (old->library != NULL && PL_strcasecmp(old->library, aLibrary))
#else
    if (old->library != NULL && PL_strcmp(old->library, aLibrary))
#endif
    {
      FactoryEntry *entry = (FactoryEntry *) factories->Remove(&key);
      delete entry;
      res = NS_OK;
    }
  }
#ifdef USE_REGISTRY
  res = platformUnregister(aClass, aLibrary);
#endif

  PR_ExitMonitor(monitor);

  return res;
}

static PRBool freeLibraryEnum(nsHashKey *aKey, void *aData) 
{
  FactoryEntry *entry = (FactoryEntry *) aData;

  if (entry->instance != NULL) {
    nsCanUnloadProc proc = (nsCanUnloadProc) PR_FindSymbol(entry->instance,
                                                           "NSCanUnload");
    if (proc != NULL) {
      PRBool res = proc();
      if (res) {
        PR_UnloadLibrary(entry->instance);
        entry->instance = NULL;
      }
    }    
  }

  return PR_TRUE;
}

nsresult NSRepository::FreeLibraries() 
{
  PR_EnterMonitor(monitor);

  factories->Enumerate(freeLibraryEnum);

  PR_ExitMonitor(monitor);

  return NS_OK;
}

