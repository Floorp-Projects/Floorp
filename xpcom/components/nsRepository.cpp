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

#ifndef XP_MAC
	// including this on mac causes odd link errors in static initialization stuff that we
	// (pinkerton & scc) don't yet understand. If you want to turn this on for mac, talk
	// to one of us.
	#include <iostream.h>
#endif

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

#if 0
#ifdef XP_MAC
#ifdef MOZ_NGLAYOUT
#define IMPL_MAC_REPOSITORY
#include "nsMacRepository.h"
#endif
#endif
#endif

#include "nsIServiceManager.h"

nsHashtable *nsRepository::factories = NULL;
PRMonitor *nsRepository::monitor = NULL;

static PRLogModuleInfo *logmodule = NULL;

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
  ~FactoryEntry(void) {
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
      }
    }
    NR_RegClose(hreg);
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

nsresult nsRepository::loadFactory(FactoryEntry *aEntry,
                                   nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aFactory = NULL;

  if (aEntry->instance == NULL) {
    PR_LOG(logmodule, PR_LOG_ALWAYS, 
           ("nsRepository: + Loading \"%s\".", aEntry->library));
    aEntry->instance = PR_LoadLibrary(aEntry->library);
  }
  if (aEntry->instance != NULL) {
#ifdef MOZ_TRACE_XPCOM_REFCNT
    // Inform refcnt tracer of new library so that calls through the
    // new library can be traced.
    nsTraceRefcnt::LoadLibrarySymbols(aEntry->library, aEntry->instance);
#endif
    nsFactoryProc proc = (nsFactoryProc) PR_FindSymbol(aEntry->instance,
                                                       "NSGetFactory");
    if (proc != NULL) {
      nsIServiceManager* serviceMgr = NULL;
      nsresult res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
      if (res == NS_OK) {
        res = proc(aEntry->cid, serviceMgr, aFactory);
      }
      return res;
    }
    PR_LOG(logmodule, PR_LOG_ERROR, 
           ("nsRepository: NSGetFactory entrypoint not found."));
  }

  PR_LOG(logmodule, PR_LOG_ERROR,
         ("nsRepository: Library load unsuccessful."));
  return NS_ERROR_FACTORY_NOT_LOADED;
}

nsresult nsRepository::FindFactory(const nsCID &aClass,
                                   nsIFactory **aFactory) 
{
  checkInitialized();
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository:   Finding Factory.");
    PR_LogPrint("nsRepository:   + %s", 
                buf);
    delete [] buf;
  }

  if (aFactory == NULL) {
    PR_LOG(logmodule, PR_LOG_ERROR, ("nsRepository:   !! NULL pointer."));
    return NS_ERROR_NULL_POINTER;
  }

  PR_EnterMonitor(monitor);

  nsIDKey key(aClass);
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

  PR_LOG(logmodule, PR_LOG_WARNING,
         ("nsRepository:   ! Find Factory %s",
          res == NS_OK ? "succeeded" : "failed"));

  return res;
}

nsresult nsRepository::checkInitialized(void) 
{
  nsresult res = NS_OK;
  if (factories == NULL) {
    res = Initialize();
  }
  return res;
}

nsresult nsRepository::Initialize(void) 
{
  if (factories == NULL) {
    factories = new nsHashtable();
  }
  if (monitor == NULL) {
    monitor = PR_NewMonitor();
  }
  if (logmodule == NULL) {
    logmodule = PR_NewLogModule("nsRepository");
  }

  PR_LOG(logmodule, PR_LOG_ALWAYS, 
         ("nsRepository: Initialized."));
#ifdef USE_NSREG
  NR_StartupRegistry();
#endif
  return NS_OK;
}

nsresult nsRepository::CreateInstance(const nsCID &aClass, 
                                      nsISupports *aDelegate,
                                      const nsIID &aIID,
                                      void **aResult)
{
  checkInitialized();
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository: Creating Instance.");
    PR_LogPrint("nsRepository: + %s", 
                buf);
    delete [] buf;
  }
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

nsresult nsRepository::CreateInstance2(const nsCID &aClass, 
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void *aSignature,
                                       void **aResult)
{
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository: Creating Instance.");
    PR_LogPrint("nsRepository: + %s.", 
                buf);
    PR_LogPrint("nsRepository: + Signature = %p.",
                aSignature);
    delete [] buf;
  }
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

nsresult nsRepository::RegisterFactory(const nsCID &aClass,
                                       nsIFactory *aFactory, 
                                       PRBool aReplace)
{
  checkInitialized();
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository: Registering Factory.");
    PR_LogPrint("nsRepository: + %s", 
                buf);
    PR_LogPrint("nsRepository: + Replace = %d.",
                (int) aReplace);
    delete [] buf;
  }

  nsIFactory *old = NULL;
  FindFactory(aClass, &old);
  
  if (old != NULL) {
    old->Release();
    if (!aReplace) {
      PR_LOG(logmodule, PR_LOG_WARNING,
             ("nsRepository: ! Factory already registered."));
      return NS_ERROR_FACTORY_EXISTS;
    }
  }

  PR_EnterMonitor(monitor);

  nsIDKey key(aClass);
  factories->Put(&key, new FactoryEntry(aClass, aFactory, NULL));

  PR_ExitMonitor(monitor);

  PR_LOG(logmodule, PR_LOG_WARNING,
         ("nsRepository: ! Factory register succeeded."));

  return NS_OK;
}

nsresult nsRepository::RegisterFactory(const nsCID &aClass,
                                       const char *aLibrary,
                                       PRBool aReplace,
                                       PRBool aPersist)
{
  checkInitialized();
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository: Registering Factory.");
    PR_LogPrint("nsRepository: + %s in \"%s\".",
                buf, aLibrary);
    PR_LogPrint("nsRepository: + Replace = %d, Persist = %d.",
                (int) aReplace, (int) aPersist);
    delete [] buf;
  }
  nsIFactory *old = NULL;
  FindFactory(aClass, &old);
  
  if (old != NULL) {
    old->Release();
    if (!aReplace) {
      PR_LOG(logmodule, PR_LOG_WARNING,
             ("nsRepository: ! Factory already registered."));
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
    nsIDKey key(aClass);
    factories->Put(&key, new FactoryEntry(aClass, NULL, aLibrary));
  }

  PR_ExitMonitor(monitor);

  PR_LOG(logmodule, PR_LOG_WARNING,
         ("nsRepository: ! Factory register succeeded."));

  return NS_OK;
}

nsresult nsRepository::UnregisterFactory(const nsCID &aClass,
                                         nsIFactory *aFactory)
{
  checkInitialized();
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository: Unregistering Factory.");
    PR_LogPrint("nsRepository: + %s.", buf);
    delete [] buf;
  }

  nsIFactory *old = NULL;
  FindFactory(aClass, &old);

  nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
  if (old != NULL) {
    if (old == aFactory) {
      PR_EnterMonitor(monitor);

      nsIDKey key(aClass);
      FactoryEntry *entry = (FactoryEntry *) factories->Remove(&key);
      delete entry;

      PR_ExitMonitor(monitor);
  
      res = NS_OK;
    }
    old->Release();
  }

  PR_LOG(logmodule, PR_LOG_WARNING,
         ("nsRepository: ! Factory unregister %s.", 
          res == NS_OK ? "succeeded" : "failed"));

  return res;
}

nsresult nsRepository::UnregisterFactory(const nsCID &aClass,
                                         const char *aLibrary)
{
  checkInitialized();
  if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) {
    char *buf = aClass.ToString();
    PR_LogPrint("nsRepository: Unregistering Factory.");
    PR_LogPrint("nsRepository: + %s in \"%s\".", buf, 
                aLibrary);
    delete [] buf;
  }

  nsIDKey key(aClass);
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

  PR_LOG(logmodule, PR_LOG_WARNING,
         ("nsRepository: ! Factory unregister %s.", 
          res == NS_OK ? "succeeded" : "failed"));

  return res;
}

static PRBool freeLibraryEnum(nsHashKey *aKey, void *aData, void* closure) 
{
  FactoryEntry *entry = (FactoryEntry *) aData;

  if (entry->instance != NULL) {
    nsCanUnloadProc proc = (nsCanUnloadProc) PR_FindSymbol(entry->instance,
                                                           "NSCanUnload");
    if (proc != NULL) {
      PRBool res = proc();
      if (res) {
        PR_LOG(logmodule, PR_LOG_ALWAYS, 
               ("nsRepository: + Unloading \"%s\".", entry->library));
        PR_UnloadLibrary(entry->instance);
        entry->instance = NULL;
      }
    }    
  }

  return PR_TRUE;
}

nsresult nsRepository::FreeLibraries(void) 
{
  PR_EnterMonitor(monitor);

  PR_LOG(logmodule, PR_LOG_ALWAYS, 
         ("nsRepository: Freeing Libraries."));
  factories->Enumerate(freeLibraryEnum);

  PR_ExitMonitor(monitor);

  return NS_OK;
}

