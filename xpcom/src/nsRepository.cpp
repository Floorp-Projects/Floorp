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

#include "plstr.h"
#include "prlink.h"
#include "prsystem.h"
#include "prprf.h"
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

/**
 * dllStore
 *
 * The dllStore maintains a collection of dlls that are hashed by the dll
 * name. The repository uses the DllStore only to store these dlls:
 *
 * 1. dlls that store components in them.
 *		The inmemory nsDll that is stored here always reflects the values of
 *		{lastModTime, fileSize} that are stored in the registry.
 *		It is the job of the autoregistration mechanism to do the right
 *		thing if the dll on disk is newer.
 * 2. dlls that dont store components in them.
 *		This is for the purpose of the autoregistration mechanism only.
 *		These are marked in the registry too so that unless they change
 *		they wont have to loaded every session.
 *
 */
nsDllStore *nsRepository::dllStore = NULL;

static PRLogModuleInfo *logmodule = NULL;

#if 0
// Factory2 commented out until proven required
static NS_DEFINE_IID(kFactory2IID, NS_IFACTORY2_IID);
#endif /* 0 */

/***************************************************************************/

/**
 * Class: FactoryEntry()
 *
 * There are two types of FactoryEntries.
 *
 * 1. {CID, dll} mapping.
 *		Factory is a consequence of the dll. These can be either session
 *		specific or persistent based on whether we write this
 *		to the registry or not.
 *
 * 2. {CID, factory} mapping
 *		These are strictly session specific and in memory only.
 */

class FactoryEntry {
public:
	nsCID cid;
	nsIFactory *factory;

	FactoryEntry(const nsCID &aClass, const char *aLibrary,
		PRTime lastModTime, PRUint64 fileSize);
	FactoryEntry(const nsCID &aClass, nsIFactory *aFactory);
	~FactoryEntry();
	// DO NOT DELETE THIS. Many FactoryEntry(s) could be sharing the same Dll.
	// This gets deleted from the dllStore going away.
	nsDll *dll;	
	
};

FactoryEntry::FactoryEntry(const nsCID &aClass, const char *aLibrary,
						   PRTime lastModTime, PRUint64 fileSize)
						   : cid(aClass), factory(NULL), dll(NULL)
{
	nsDllStore *dllCollection = nsRepository::dllStore;
	
	if (aLibrary == NULL)
	{
		return;
	}
	
	// If dll not already in dllCollection, add it.
	// PR_EnterMonitor(nsRepository::monitor);
	dll = dllCollection->Get(aLibrary);
	// PR_ExitMonitor(nsRepository::monitor);
	
	if (dll == NULL)
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS,
			("nsRepository: New dll \"%s\".", aLibrary));

		// Add a new Dll into the nsDllStore
		dll = new nsDll(aLibrary, lastModTime, fileSize);
		if (dll->GetStatus() != DLL_OK)
		{
			// Cant create a nsDll. Backoff.
			PR_LOG(logmodule, PR_LOG_ALWAYS,
				("nsRepository: New dll status error. \"%s\".", aLibrary));
			delete dll;
			dll = NULL;
		}
		else
		{
			PR_LOG(logmodule, PR_LOG_ALWAYS,
				("nsRepository: Adding New dll \"%s\" to dllStore.",
				aLibrary));

			// PR_EnterMonitor(nsRepository::monitor);
			dllCollection->Put(aLibrary, dll);
			// PR_ExitMonitor(nsRepository::monitor);
		}
	}
	else
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS,
			("nsRepository: Found in dllStore \"%s\".", aLibrary));
		// XXX We found the dll in the dllCollection.
		// XXX Consistency check: dll needs to have the same
		// XXX lastModTime and fileSize. If not that would mean
		// XXX that the dll wasn't registered properly.
	}
}


FactoryEntry::FactoryEntry(const nsCID &aClass, nsIFactory *aFactory)
						   : cid(aClass), factory(aFactory), dll(NULL)
{
}


FactoryEntry::~FactoryEntry(void)
{
	if (factory != NULL)
	{
		factory->Release();
	}
	// DO NOT DELETE nsDll *dll;
}

/***************************************************************************/

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

/***************************************************************************/

#ifdef USE_NSREG
#define USE_REGISTRY

static nsDll *platformCreateDll(const char *fullname)
{
	HREG hreg;
	REGERR err = NR_RegOpen(NULL, &hreg);
	
	if (err != REGERR_OK)
	{
		return (NULL);
	}
	
	RKEY xpcomKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Software/Netscape/XPCOM", &xpcomKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NULL);
	}
	
	RKEY key;
	err = NR_RegGetKey(hreg, xpcomKey, (char *)fullname, &key);
	if (err != REGERR_OK)
	{
		return (NULL);
	}

	PRTime lastModTime = LL_ZERO;
	PRUint64 fileSize = LL_ZERO;
	uint32 n = sizeof(lastModTime);
	NR_RegGetEntry(hreg, key, "LastModTimeStamp", &lastModTime, &n);
	n = sizeof(fileSize);
	NR_RegGetEntry(hreg, key, "FileSize", &fileSize, &n);

	nsDll *dll = new nsDll(fullname, lastModTime, fileSize);

	return (dll);
}

/**
 * platformMarkNoComponents(nsDll)
 *
 * Stores the dll name, last modified time, size and 0 for number of
 * components in dll in the registry at location
 *		ROOTKEY_COMMON/Software/Netscape/XPCOM/dllname
 */
static nsresult platformMarkNoComponents(nsDll *dll)
{
	HREG hreg;
	REGERR err = NR_RegOpen(NULL, &hreg);
	
	if (err != REGERR_OK)
	{
		return (NS_ERROR_FAILURE);
	}
	
	// XXX Gross. LongLongs dont have a serialization format. This makes
	// XXX the registry non-xp. Someone beat on the nspr people to get
	// XXX a longlong serialization function please!
	RKEY xpcomKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Software/Netscape/XPCOM", &xpcomKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NS_ERROR_FAILURE);
	}
	
	RKEY key;
	err = NR_RegAddKey(hreg, xpcomKey, (char *)dll->GetFullPath(), &key);
	
	if (err != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NS_ERROR_FAILURE);
	}
	
	PRTime lastModTime = dll->GetLastModifiedTime();
	PRUint64 fileSize = dll->GetSize();
	NR_RegSetEntry(hreg, key, "LastModTimeStamp", REGTYPE_ENTRY_BYTES,
		&lastModTime, sizeof(lastModTime));
	NR_RegSetEntry(hreg, key, "FileSize",  REGTYPE_ENTRY_BYTES,
		&fileSize, sizeof(fileSize));
	
	char *ncomponentsString = "0";
	
	NR_RegSetEntryString(hreg, key, "ComponentsCount",
		ncomponentsString);
	
	NR_RegClose(hreg);
	return (NS_OK);
}

static nsresult platformRegister(NSQuickRegisterData regd, nsDll *dll)
{
	HREG hreg;
	// Preconditions
	PR_ASSERT(regd != NULL);
	PR_ASSERT(regd->CIDString != NULL);
	PR_ASSERT(dll != NULL);

	REGERR err = NR_RegOpen(NULL, &hreg);

	if (err != REGERR_OK)
	{
		return (NS_ERROR_FAILURE);
	}

	RKEY classesKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Classes", &classesKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NS_ERROR_FAILURE);
	}

	RKEY key;
	NR_RegAddKey(hreg, classesKey, "CLSID", &key);
	NR_RegAddKey(hreg, key, (char *)regd->CIDString, &key);

	NR_RegSetEntryString(hreg, key, "ClassName", (char *)regd->className);
	if (regd->progID)
		NR_RegSetEntryString(hreg, key, "ProgID", (char *)(regd->progID));
	char *libName = (char *)dll->GetFullPath();
	NR_RegSetEntry(hreg, key, "InprocServer",  REGTYPE_ENTRY_FILE, libName,
		strlen(libName) + 1);

	if (regd->progID)
	{
		NR_RegAddKey(hreg, classesKey, (char *)regd->progID, &key);
		NR_RegSetEntryString(hreg, key, "CLSID", (char *)regd->CIDString);
	}

	// XXX Gross. LongLongs dont have a serialization format. This makes
	// XXX the registry non-xp. Someone beat on the nspr people to get
	// XXX a longlong serialization function please!
	RKEY xpcomKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Software/Netscape/XPCOM", &xpcomKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		// This aint a fatal error. It causes autoregistration to fail
		// and hence the dll would be registered again the next time
		// we startup. Let us run with it.
		return (NS_OK);
	}

	NR_RegAddKey(hreg, xpcomKey, (char *)dll->GetFullPath(), &key);

	PRTime lastModTime = dll->GetLastModifiedTime();
	PRUint64 fileSize = dll->GetSize();

	NR_RegSetEntry(hreg, key, "LastModTimeStamp", REGTYPE_ENTRY_BYTES,
		&lastModTime, sizeof(lastModTime));
	NR_RegSetEntry(hreg, key, "FileSize",  REGTYPE_ENTRY_BYTES,
		&fileSize, sizeof(fileSize));

	unsigned int nComponents = 0;
	char buf[MAXREGNAMELEN];
	uint32 len = sizeof(buf);

	if (NR_RegGetEntryString(hreg, key, "ComponentsCount", buf, len) == REGERR_OK)
	{
		nComponents = atoi(buf);
	}
	nComponents++;
	PR_snprintf(buf, sizeof(buf), "%d", nComponents);
	NR_RegSetEntryString(hreg, key, "ComponentsCount", buf);
	
	NR_RegClose(hreg);
	return err;
}

static nsresult platformUnregister(NSQuickRegisterData regd, const char *aLibrary)
{  
	HREG hreg;
	REGERR err = NR_RegOpen(NULL, &hreg);

	if (err != REGERR_OK)
	{
		return (NS_ERROR_FAILURE);
	}

	RKEY classesKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Classes", &classesKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NS_ERROR_FAILURE);
	}
	
	RKEY key;
	NR_RegAddKey(hreg, classesKey, "CLSID", &key);
	NR_RegDeleteKey(hreg, key, (char *)regd->CIDString);
	
	if (regd->progID)
		NR_RegDeleteKey(hreg, classesKey, (char *)regd->progID);

	RKEY xpcomKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Software/Netscape/XPCOM", &xpcomKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		// This aint a fatal error. It causes autoregistration to fail
		// and hence the dll would be registered again the next time
		// we startup. Let us run with it.
		return (NS_OK);
	}

	NR_RegGetKey(hreg, xpcomKey, (char *)aLibrary, &key);

	// We need to reduce the ComponentCount by 1.
	// If the ComponentCount hits 0, delete the entire key.
	int nComponents = 0;
	char buf[MAXREGNAMELEN];
	uint32 len = sizeof(buf);

	if (NR_RegGetEntryString(hreg, key, "ComponentsCount", buf, len) == REGERR_OK)
	{
		nComponents = atoi(buf);
		nComponents--;
	}
	if (nComponents <= 0)
	{
		NR_RegDeleteKey(hreg, key, (char *)aLibrary);
	}
	else
	{
		PR_snprintf(buf, sizeof(buf), "%d", nComponents);
		NR_RegSetEntryString(hreg, key, "ComponentsCount", buf);		
	}

	NR_RegClose(hreg);
	return (NS_OK);
}

static FactoryEntry *platformFind(const nsCID &aCID)
{
	HREG hreg;
	REGERR err = NR_RegOpen(NULL, &hreg);

	if (err != REGERR_OK)
	{
		return (NULL);
	}

	RKEY classesKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Classes", &classesKey) != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NULL);
	}

	RKEY key;
	NR_RegAddKey(hreg, classesKey, "CLSID", &key);
	
	FactoryEntry *res = NULL;
	
	RKEY cidKey;
	char *cidString = aCID.ToString();
	err = NR_RegGetKey(hreg, key, cidString, &cidKey);
	delete [] cidString;

	if (err != REGERR_OK)
	{
		NR_RegClose(hreg);
		return (NULL);
	}

	// Get the library name, modifiedtime and size
	PRTime lastModTime = LL_ZERO;
	PRUint64 fileSize = LL_ZERO;

	char buf[MAXREGNAMELEN];
	uint32 len = sizeof(buf);
	err = NR_RegGetEntry(hreg, cidKey, "InprocServer", buf, &len);
	if (err != REGERR_OK)
	{
		// Registry inconsistent. No File name for CLSID.
		NR_RegClose(hreg);
		return (NULL);
	}

	char *library = buf;

	// XXX Gross. LongLongs dont have a serialization format. This makes
	// XXX the registry non-xp. Someone beat on the nspr people to get
	// XXX a longlong serialization function please!
	RKEY xpcomKey;
	if (NR_RegAddKey(hreg, ROOTKEY_COMMON, "Software/Netscape/XPCOM", &xpcomKey) == REGERR_OK)
	{
		if (NR_RegGetKey(hreg, xpcomKey, library, &key) == REGERR_OK)
		{
			uint32 n = sizeof(lastModTime);
			NR_RegGetEntry(hreg, key, "LastModTimeStamp", &lastModTime, &n);
			PR_ASSERT(n == sizeof(lastModTime));
			n = sizeof(fileSize);
			NR_RegGetEntry(hreg, key, "FileSize", &fileSize, &n);
			PR_ASSERT(n == sizeof(fileSize));
		}
	}

	res = new FactoryEntry(aCID, library, lastModTime, fileSize);

	NR_RegClose(hreg);

	return (res);
}

#endif // USE_NSREG

/***************************************************************************/

nsresult nsRepository::loadFactory(FactoryEntry *aEntry,
								   nsIFactory **aFactory)
{
	if (aFactory == NULL)
	{
		return NS_ERROR_NULL_POINTER;
	}
	*aFactory = NULL;

	// loadFactory() cannot be called for entries that are CID<->factory
	// mapping entries for the session.
	PR_ASSERT(aEntry->dll != NULL);
	
	if (aEntry->dll->IsLoaded() == PR_FALSE)
	{
		// Load the dll
		PR_LOG(logmodule, PR_LOG_ALWAYS, 
			("nsRepository: + Loading \"%s\".", aEntry->dll->GetFullPath()));
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
	nsTraceRefcnt::LoadLibrarySymbols(aEntry->dll->GetFullPath(), aEntry->dll->GetInstance());
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
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS))
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: FindFactory(%s)", buf);
		delete [] buf;
	}
	
	PR_ASSERT(aFactory != NULL);
	
	PR_EnterMonitor(monitor);
	
	nsIDKey key(aClass);
	FactoryEntry *entry = (FactoryEntry*) factories->Get(&key);
	
	nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
	
#ifdef USE_REGISTRY
	if (entry == NULL)
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS, ("\t\tnot found in factory cache. Looking in registry"));
		entry = platformFind(aClass);
		// If we got one, cache it in our hashtable
		if (entry != NULL)
		{
			PR_LOG(logmodule, PR_LOG_ALWAYS, ("\t\tfound in registry."));
			factories->Put(&key, entry);
		}
	}
	else
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS, ("\t\tfound in factory cache."));
	}
#endif
	
	PR_ExitMonitor(monitor);
	
	if (entry != NULL)
	{
		if ((entry)->factory == NULL)
		{
			res = loadFactory(entry, aFactory);
		}
		else
		{
			*aFactory = entry->factory;
			(*aFactory)->AddRef();
			res = NS_OK;
		}
	}
	
	PR_LOG(logmodule, PR_LOG_WARNING, ("nsRepository: FindFactory() %s",
		res == NS_OK ? "succeeded" : "FAILED"));
	
	return res;
}

nsresult nsRepository::checkInitialized(void) 
{
	nsresult res = NS_OK;
	if (factories == NULL)
	{
		res = Initialize();
	}
	return res;
}

nsresult nsRepository::Initialize(void) 
{
	if (factories == NULL)
	{
		factories = new nsHashtable();
	}
	if (monitor == NULL)
	{
		monitor = PR_NewMonitor();
	}
	if (logmodule == NULL)
	{
		logmodule = PR_NewLogModule("nsRepository");
	}
	if (dllStore == NULL)
	{
		dllStore = new nsDllStore();
	}
	
	PR_LOG(logmodule, PR_LOG_ALWAYS, ("nsRepository: Initialized."));
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
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS))
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: CreateInstance(%s)", buf);
		delete [] buf;
	}
	if (aResult == NULL)
	{
		return NS_ERROR_NULL_POINTER;
	}
	*aResult = NULL;
	
	nsIFactory *factory = NULL;
	nsresult res = FindFactory(aClass, &factory);
	if (NS_SUCCEEDED(res))
	{
		res = factory->CreateInstance(aDelegate, aIID, aResult);
		factory->Release();
		PR_LOG(logmodule, PR_LOG_ALWAYS, ("\t\tCreateInstance() succeeded."));		
		return res;
	}

	PR_LOG(logmodule, PR_LOG_ALWAYS, ("\t\tCreateInstance() FAILED."));
	return NS_ERROR_FACTORY_NOT_REGISTERED;
}

#if 0
/*
nsresult nsRepository::CreateInstance2(const nsCID &aClass, 
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void *aSignature,
                                       void **aResult)
{
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS))
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: Creating Instance.");
		PR_LogPrint("nsRepository: + %s.", 
			buf);
		PR_LogPrint("nsRepository: + Signature = %p.",
			aSignature);
		delete [] buf;
	}
	if (aResult == NULL)
	{
		return NS_ERROR_NULL_POINTER;
	}
	*aResult = NULL;
	
	nsIFactory *factory = NULL;
	
	nsresult res = FindFactory(aClass, &factory);
	
	if (NS_SUCCEEDED(res))
	{
		nsIFactory2 *factory2 = NULL;
		res = NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT;
		
		factory->QueryInterface(kFactory2IID, (void **) &factory2);
		
		if (factory2 != NULL)
		{
			res = factory2->CreateInstance2(aDelegate, aIID, aSignature, aResult);
			factory2->Release();
		}
		
		factory->Release();
		return res;
	}
	
	return NS_ERROR_FACTORY_NOT_REGISTERED;
}
*/
#endif /* 0 */

nsresult nsRepository::RegisterFactory(const nsCID &aClass,
                                       nsIFactory *aFactory, 
                                       PRBool aReplace)
{
	checkInitialized();
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS))
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: RegisterFactory(%s, factory), replace = %d.", buf, (int)aReplace);
		delete [] buf;
	}

	nsIFactory *old = NULL;
	FindFactory(aClass, &old);

	if (old != NULL)
	{
		old->Release();
		if (!aReplace)
		{
			PR_LOG(logmodule, PR_LOG_WARNING, ("\t\tFactory already registered."));
			return NS_ERROR_FACTORY_EXISTS;
		}
		else
		{
			PR_LOG(logmodule, PR_LOG_WARNING, ("\t\tdeleting old Factory Entry."));
		}
	}
	
	PR_EnterMonitor(monitor);

	nsIDKey key(aClass);
	factories->Put(&key, new FactoryEntry(aClass, aFactory));
	
	PR_ExitMonitor(monitor);
	
	PR_LOG(logmodule, PR_LOG_WARNING,
		("\t\tFactory register succeeded."));
	
	return NS_OK;
}

nsresult nsRepository::RegisterFactory(const nsCID &aClass,
                                       const char *aLibrary,
                                       PRBool aReplace,
                                       PRBool aPersist)
{
	checkInitialized();
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS))
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: RegisterFactory(%s, %s), replace = %d, persist = %d.", buf, aLibrary, (int)aReplace, (int)aPersist);
		delete [] buf;
	}

	nsIFactory *old = NULL;
	FindFactory(aClass, &old);
	
	if (old != NULL)
	{
		old->Release();
		if (!aReplace)
		{
			PR_LOG(logmodule, PR_LOG_WARNING,("\t\tFactory already registered."));
			return NS_ERROR_FACTORY_EXISTS;
		}
		else
		{
			PR_LOG(logmodule, PR_LOG_WARNING,("\t\tdeleting registered Factory."));
		}
	}
	
	PR_EnterMonitor(monitor);
	
#ifdef USE_REGISTRY
	if (aPersist == PR_TRUE)
	{
		// Add it to the registry
		nsDll *dll = new nsDll(aLibrary);
		// XXX temp hack until we get the dll to give us the entire
		// XXX NSQuickRegisterClassData
		NSQuickRegisterClassData cregd = {0};
		cregd.CIDString = aClass.ToString();
		platformRegister(&cregd, dll);
		delete [] (char *)cregd.CIDString;
		delete dll;
	} 
	else
#endif
	{
		nsDll *dll = new nsDll(aLibrary);
		nsIDKey key(aClass);
		factories->Put(&key, new FactoryEntry(aClass, aLibrary,
			dll->GetLastModifiedTime(), dll->GetSize()));
		delete dll;
	}
	
	PR_ExitMonitor(monitor);
	
	PR_LOG(logmodule, PR_LOG_WARNING,
		("\t\tFactory register succeeded."));
	
	return NS_OK;
}

nsresult nsRepository::UnregisterFactory(const nsCID &aClass,
                                         nsIFactory *aFactory)
{
	checkInitialized();
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS)) 
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: Unregistering Factory.");
		PR_LogPrint("nsRepository: + %s.", buf);
		delete [] buf;
	}

	
	nsIDKey key(aClass);
	nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
	FactoryEntry *old = (FactoryEntry *) factories->Get(&key);
	if (old != NULL)
	{
		if (old->factory == aFactory)
		{
			PR_EnterMonitor(monitor);
			old = (FactoryEntry *) factories->Remove(&key);
			PR_ExitMonitor(monitor);
			delete old;
			res = NS_OK;
		}

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
	if (PR_LOG_TEST(logmodule, PR_LOG_ALWAYS))
	{
		char *buf = aClass.ToString();
		PR_LogPrint("nsRepository: Unregistering Factory.");
		PR_LogPrint("nsRepository: + %s in \"%s\".", buf, aLibrary);
		delete [] buf;
	}
	
	nsIDKey key(aClass);
	FactoryEntry *old = (FactoryEntry *) factories->Get(&key);
	
	nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
	
	PR_EnterMonitor(monitor);
	
	if (old != NULL && old->dll != NULL)
	{
		if (old->dll->GetFullPath() != NULL &&
#ifdef XP_UNIX
			PL_strcasecmp(old->dll->GetFullPath(), aLibrary)
#else
			PL_strcmp(old->dll->GetFullPath(), aLibrary)
#endif
			)
		{
			FactoryEntry *entry = (FactoryEntry *) factories->Remove(&key);
			delete entry;
			res = NS_OK;
		}
#ifdef USE_REGISTRY
		// XXX temp hack until we get the dll to give us the entire
		// XXX NSQuickRegisterClassData
		NSQuickRegisterClassData cregd = {0};
		cregd.CIDString = aClass.ToString();
		res = platformUnregister(&cregd, aLibrary);
		delete [] (char *)cregd.CIDString;
#endif
	}
	
	PR_ExitMonitor(monitor);
	
	PR_LOG(logmodule, PR_LOG_WARNING,
		("nsRepository: ! Factory unregister %s.", 
		res == NS_OK ? "succeeded" : "failed"));
	
	return res;
}

static PRBool freeLibraryEnum(nsHashKey *aKey, void *aData, void* closure) 
{
	FactoryEntry *entry = (FactoryEntry *) aData;
	
	if (entry->dll->IsLoaded() == PR_TRUE)
	{
		nsCanUnloadProc proc = (nsCanUnloadProc) entry->dll->FindSymbol("NSCanUnload");
		if (proc != NULL)
		{
			PRBool res = proc();
			if (res)
			{
				PR_LOG(logmodule, PR_LOG_ALWAYS, 
					("nsRepository: + Unloading \"%s\".", entry->dll->GetFullPath()));
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


/**
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
		".sl",	/* Unix: HP */
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
#ifdef USE_REGISTRY
		dll = platformCreateDll(fullname);
#endif /* USE_REGISTRY */
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
				("nsRepository: + nsDll not changed \"%s\". Skipping...",
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
			else
			{
				// dll doesn't have a CanUnload proc. Guess it is
				// ok to unload it.
				dll->Unload();
				PR_LOG(logmodule, PR_LOG_ALWAYS, 
					("nsRepository: + Unloading \"%s\". (no CanUnloadProc).",
					dll->GetFullPath()));
				
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
		// Mark dll as not xpcom dll along with modified time and size in
		// the registry so that we wont need to load the dll again every
		// session until the dll changes.
#ifdef USE_REGISTRY
		platformMarkNoComponents(dll);
#endif /* USE_REGISTRY */
		ret = NS_ERROR_FAILURE;
	}
	else
	{
		PR_LOG(logmodule, PR_LOG_ALWAYS,
			("nsRepository: Autoregistration Passed for "
			"\"%s\". Skipping...", dll->GetFullPath()));
		// Marking dll along with modified time and size in the
		// registry happens at platformregister(). No need to do it
		// here again.
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
	}
	dll->Unload();
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
	}
	dll->Unload();
	return (res);
}
