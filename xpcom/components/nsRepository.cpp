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
	// including this on mac causes odd link errors in static initialization
	// stuff that we (pinkerton & scc) don't yet understand. If you want to
	// turn this on for mac, talk to one of us.
	#include <iostream.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif
#include "plstr.h"
#include "prlink.h"
#include "prsystem.h"
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

#include "xcDllStore.h"
#include "nsIServiceManager.h"

nsHashtable *nsRepository::factories = NULL;
PRMonitor *nsRepository::monitor = NULL;
nsDllStore *nsRepository::dllStore = NULL;

static PRLogModuleInfo *logmodule = NULL;

static NS_DEFINE_IID(kFactory2IID, NS_IFACTORY2_IID);

class FactoryEntry {
public:
  nsCID cid;
  nsIFactory *factory;
  char *library;
  
  // DO NOT DELETE THIS. Many FactoryEntry(s) could be sharing the same Dll.
  // This gets deleted from the dllStore going away.
  nsDll *dll;	

  FactoryEntry(const nsCID &aClass, nsIFactory *aFactory,
	  const char *aLibrary)
	  : cid(aClass), factory(aFactory), library(NULL), dll(NULL)
  {
	  nsDllStore *dllCollection = nsRepository::dllStore;

	  if (aLibrary == NULL)
	  {
		  return;
	  }

      library = PL_strdup(aLibrary);
	  if (library == NULL)
	  {
		  // No memory
		  return;
	  }

	  // If dll not already in dllCollection, add it.
	  // PR_EnterMonitor(nsRepository::monitor);
	  dll = dllCollection->Get(library);
	  // PR_ExitMonitor(nsRepository::monitor);

	  if (dll == NULL)
	  {
		  // Add a new Dll into the nsDllStore
		  dll = new nsDll(library);
		  if (dll->GetStatus() != DLL_OK)
		  {
			  // Cant create a nsDll. Backoff.
			  delete dll;
			  dll = NULL;
			  PL_strfree(library);
			  library = NULL;
		  }
		  else
		  {
			  // PR_EnterMonitor(nsRepository::monitor);
			  dllCollection->Put(library, dll);
			  // PR_ExitMonitor(nsRepository::monitor);
		  }
	  }
  }

  ~FactoryEntry(void)
  {
	  if (library != NULL) {
		  free(library);
	  }
	  if (factory != NULL) {
		  factory->Release();
	  }
	  // DO NOT DELETE nsDll *dll;
  }
};

class IDKey: public nsHashKey {
private:
	nsID id;
	
public:
	IDKey(const nsID &aID) { id = aID; }
	
	PRUint32 HashValue(void) const { return id.m0; }
	
	PRBool Equals(const nsHashKey *aKey) const
	{
		return (id.Equals(((const IDKey *) aKey)->id));
	}
	
	nsHashKey *Clone(void) const { return new IDKey(id); }
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
	if (aFactory == NULL)
	{
		return NS_ERROR_NULL_POINTER;
	}
	*aFactory = NULL;
	
	if (aEntry->dll->IsLoaded() == PR_FALSE)
	{
		// Load the dll
		PR_LOG(logmodule, PR_LOG_ALWAYS, 
			("nsRepository: + Loading \"%s\".", aEntry->library));
		if (aEntry->dll->Load() == PR_FALSE)
		{
			PR_LOG(logmodule, PR_LOG_ERROR,
				("nsRepository: Library load unsuccessful."));
			return (NS_ERROR_FAILURE);
		}
	}

#ifdef MOZ_TRACE_XPCOM_REFCNT
	// Inform refcnt tracer of new library so that calls through the
	// new library can be traced.
	nsTraceRefcnt::LoadLibrarySymbols(aEntry->library, aEntry->dll->GetInstance());
#endif
	nsFactoryProc proc = (nsFactoryProc) aEntry->dll->FindSymbol("NSGetFactory");
	if (proc != NULL)
	{
		nsIServiceManager* serviceMgr = NULL;
		nsresult res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
		if (res == NS_OK)
		{
			res = proc(aEntry->cid, serviceMgr, aFactory);
		}
		return res;
	}
	PR_LOG(logmodule, PR_LOG_ERROR, 
		("nsRepository: NSGetFactory entrypoint not found."));
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
  if (dllStore == NULL) {
    dllStore = new nsDllStore();
  }

  PR_LOG(logmodule, PR_LOG_ALWAYS, 
         ("nsRepository: Initialized."));
#ifdef USE_NSREG
  NR_StartupRegistry();
#endif

// Initiate autoreg
  AutoRegister(NS_Startup, NULL);

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

  if (entry->dll->IsLoaded() == PR_TRUE) {
    nsCanUnloadProc proc = (nsCanUnloadProc) entry->dll->FindSymbol("NSCanUnload");
    if (proc != NULL) {
      PRBool res = proc();
      if (res) {
        PR_LOG(logmodule, PR_LOG_ALWAYS, 
               ("nsRepository: + Unloading \"%s\".", entry->library));
        entry->dll->Unload();
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


/*
 * AutoRegister(RegistrationInstant, const char *pathlist)
 *
 * Given a ; separated list of paths, this will ensure proper registration
 * of all components. A default pathlist is maintained in the registry at
 *		\\HKEYROOT_COMMON\\Classes\\PathList
 * In addition to looking at the pathlist, the default pathlist is looked at.
 *
 * This will take care not loading already registered dlls, finding and
 * registering new dlls, re-registration of modified dlls
 *
 */

nsresult nsRepository::AutoRegister(NSRegistrationInstant when,
									const char* pathlist)
{
	if (pathlist != NULL)
	{
		SyncComponentsInPathList(pathlist);
	}
	
	//XXX get default pathlist from registry
	//XXX Temporary hack. Registering components from current directory
	const char *defaultPathList = ".";
	SyncComponentsInPathList(defaultPathList);
	return (NS_OK);
}


nsresult nsRepository::AddToDefaultPathList(const char *pathlist)
{
	//XXX add pathlist to the defaultpathlist in the registrys
	return (NS_ERROR_FAILURE);
}

nsresult nsRepository::SyncComponentsInPathList(const char *pathlist)
{
	char *paths = PL_strdup(pathlist);

	if (paths == NULL || *paths == '\0')
		return(NS_ERROR_FAILURE);

	char *pathsMem = paths;
	while (paths != NULL)
	{
		char *nextpath = PL_strchr(paths, NS_PATH_SEPARATOR);
		if (nextpath != NULL) *nextpath = '\0';
		SyncComponentsInDir(paths);
		paths = nextpath;
	}
	PL_strfree(pathsMem);
	return (NS_OK);
}

nsresult nsRepository::SyncComponentsInDir(const char *dir)
{
	PRDir *prdir = PR_OpenDir(dir);
	if (prdir == NULL)
		return (NS_ERROR_FAILURE);
	
	// Create a buffer that has dir/ in it so we can append
	// the filename each time in the loop
	char fullname[NS_MAX_FILENAME_LEN];
	PL_strncpyz(fullname, dir, sizeof(fullname));
	unsigned int n = strlen(fullname);
	if (n+1 < sizeof(fullname))
	{
		fullname[n] = PR_GetDirectorySeparator();
		n++;
	}
	char *filepart = fullname + n;

	PRDirEntry *dirent = NULL;
	while ((dirent = PR_ReadDir(prdir, PR_SKIP_BOTH)) != NULL)
	{
		PL_strncpyz(filepart, dirent->name, sizeof(fullname)-n);
		nsresult ret = SyncComponentsInFile(fullname);
		if (NS_FAILED(ret) &&
			NS_ERROR_GET_CODE(ret) == NS_XPCOM_ERRORCODE_IS_DIR)
		{
			SyncComponentsInDir(fullname);
		}
	} // foreach file
	PR_CloseDir(prdir);
	return (NS_OK);
}

nsresult nsRepository::SyncComponentsInFile(const char *fullname)
{
	const char *ValidDllExtensions[] = {
		".dll",	/* Windows */
			".dso",	/* Unix */
			".so",	/* Unix */
			"_dll",	/* Mac ? */
			".dlm",	/* new for all platforms */
			NULL
	};


	PRFileInfo64 statbuf;
	if (PR_GetFileInfo64(fullname, &statbuf) != PR_SUCCESS)
	{
		// Skip files that cannot be stat
		return (NS_ERROR_FAILURE);
	}
	if (statbuf.type == PR_FILE_DIRECTORY)
	{
		// Cant register a directory
		return (NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XPCOM,
			NS_XPCOM_ERRORCODE_IS_DIR));
	}
	else if (statbuf.type != PR_FILE_FILE)
	{
		// Skip non-files
		return (NS_ERROR_FAILURE);
	}
	
	// deal with only files that have the right extension
	PRBool validExtension = PR_FALSE;
	int flen = PL_strlen(fullname);
	for (int i=0; ValidDllExtensions[i] != NULL; i++)
	{
		int extlen = PL_strlen(ValidDllExtensions[i]);

		// Does fullname end with this extension
		if (flen >= extlen &&
			!PL_strcasecmp(&(fullname[flen - extlen]), ValidDllExtensions[i])
			)
		{
			validExtension = PR_TRUE;
			break;
		}
	}

	if (validExtension == PR_FALSE)
	{
		// Skip invalid extensions
		return (NS_ERROR_FAILURE);
	}
	
	// Check if dll is one that we have already seen
	nsDll *dll = dllStore->Get(fullname);
	if (dll == NULL)
	{
		// XXX Create nsDll for this from registry and
		// XXX add it to our dll cache dllStore.
	}
	
	if (dll != NULL)
	{
		// Make sure the dll is OK
		if (dll->GetStatus() != NS_OK)
		{
			PR_LOG(logmodule, PR_LOG_ALWAYS, 
				("nsRepository: + nsDll not NS_OK \"%s\". Skipping...",
				dll->GetFullPath()));
			return (NS_ERROR_FAILURE);
		}
		
		// We already have seen this dll. Check if this dll changed
		if (LL_EQ(dll->GetLastModifiedTime(), statbuf.modifyTime) &&
			LL_EQ(dll->GetSize(), statbuf.size))
		{
			// Dll hasn't changed. Skip.
			PR_LOG(logmodule, PR_LOG_ALWAYS, 
				("nsRepository: + nsDll not changed and already "
				"registered \"%s\". Skipping...",
				dll->GetFullPath()));
				return (NS_OK);
		}
		
		// Aagh! the dll has changed since the last time we saw it.
		// re-register dll
		if (dll->IsLoaded())
		{
			// We are screwed. We loaded the old version of the dll
			// and now we find that the on-disk copy if newer.
			// The only thing to do would be to ask the dll if it can
			// unload itself. It can do that if it hasn't created objects
			// yet.
			nsCanUnloadProc proc = (nsCanUnloadProc)
				dll->FindSymbol("NSCanUnload");
			if (proc != NULL)
			{
				PRBool res = proc(/*PR_TRUE*/);
				if (res)
				{
					PR_LOG(logmodule, PR_LOG_ALWAYS, 
						("nsRepository: + Unloading \"%s\".",
						dll->GetFullPath()));
					dll->Unload();
				}
				else
				{
					// THIS IS THE WORST SITUATION TO BE IN.
					// Dll doesn't want to be unloaded. Cannot re-register
					// this dll.
					PR_LOG(logmodule, PR_LOG_ALWAYS,
						("nsRepository: *** Dll already loaded. "
						"Cannot unload either. Hence cannot re-register "
						"\"%s\". Skipping...", dll->GetFullPath()));
					return (NS_ERROR_FAILURE);
				}
			}
			
		} // dll isloaded
		
		// Sanity.
		if (dll->IsLoaded())
		{
			// We went through all the above to make sure the dll
			// is unloaded. And here we are with the dll still
			// loaded. Whoever taught dp programming...
			PR_LOG(logmodule, PR_LOG_ALWAYS,
				("nsRepository: Dll still loaded. Cannot re-register "
				"\"%s\". Skipping...", dll->GetFullPath()));
			return (NS_ERROR_FAILURE);
		}
	} // dll != NULL
	else
	{
		// Create and add the dll to the dllStore
		// It is ok to do this even if the creation of nsDll
		// didnt succeed. That way we wont do this again
		// when we encounter the same dll.
		dll = new nsDll(fullname);
		dllStore->Put(fullname, dll);
	} // dll == NULL
	
	// Either we are seeing the dll for the first time or the dll has
	// changed since we last saw it and it is unloaded successfully.
	//
	// Now we can try register the dll for sure. 
	nsresult res = SelfRegisterDll(dll);
	nsresult ret = NS_OK;
	if (NS_FAILED(res))
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS,
			("nsRepository: Autoregistration FAILED for "
			"\"%s\". Skipping...", dll->GetFullPath()));
		// XXX Mark dll as not xpcom dll along with modified time
		// XXX and size in the registry so that in the next
		// XXX session, we wont do all this again until
		// XXX the dll changes.
		ret = NS_ERROR_FAILURE;
	}
	else
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS,
			("nsRepository: Autoregistration Passed for "
			"\"%s\". Skipping...", dll->GetFullPath()));
		// XXX Mark dll along with modified time and size in the
		// XXX registry.
	}
	return (ret);
}


/*
 * SelfRegisterDll
 *
 * Given a dll abstraction, this will load, selfregister the dll and
 * unload the dll.
 *
 */
nsresult nsRepository::SelfRegisterDll(nsDll *dll)
{
	// Precondition: dll is not loaded already
	PR_ASSERT(dll->IsLoaded() == PR_FALSE);

	if (dll->Load() == PR_FALSE)
	{
		// Cannot load. Probably not a dll.
		return(NS_ERROR_FAILURE);
	}

	nsRegisterProc regproc = (nsRegisterProc)dll->FindSymbol("NSRegisterSelf");
	nsresult res;

	if (regproc == NULL)
	{
		// Smart registration
		NSQuickRegisterData qr = (NSQuickRegisterData)dll->FindSymbol(
			NS_QUICKREGISTER_DATA_SYMBOL);
		if (qr == NULL)
		{
			return(NS_ERROR_NO_INTERFACE);
		}
		// XXX register the quick registration data on behalf of the dll
	}
	else
	{
		// Call the NSRegisterSelfProc to enable dll registration
		nsIServiceManager* serviceMgr = NULL;
		nsresult res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
		if (res == NS_OK)
		{
			res = regproc(/* serviceMgr, */ dll->GetFullPath());
		}
		dll->Unload();
	}
	return (res);
}


nsresult nsRepository::SelfUnregisterDll(nsDll *dll)
{
	// Precondition: dll is not loaded
	PR_ASSERT(dll->IsLoaded() == PR_FALSE);

	if (dll->Load() == PR_FALSE)
	{
		// Cannot load. Probably not a dll.
		return(NS_ERROR_FAILURE);
	}

	nsUnregisterProc unregproc =
		(nsUnregisterProc) dll->FindSymbol("NSUnregisterSelf");
	nsresult res = NS_OK;

	if (unregproc == NULL)
	{
		// Smart unregistration
		NSQuickRegisterData qr = (NSQuickRegisterData)
			dll->FindSymbol(NS_QUICKREGISTER_DATA_SYMBOL);
		if (qr == NULL)
		{
			return(NS_ERROR_NO_INTERFACE);
		}
		// XXX unregister the dll based on the quick registration data
	}
	else
	{
		// Call the NSUnregisterSelfProc to enable dll de-registration
		nsIServiceManager* serviceMgr = NULL;
		res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
		if (res == NS_OK)
		{
			res = unregproc(/* serviceMgr, */dll->GetFullPath());
		}
		dll->Unload();
	}
	return (res);
}
