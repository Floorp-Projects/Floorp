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

#ifndef nsComponentManager_h__
#define nsComponentManager_h__

#include "nsIComponentLoader.h"
#include "nsNativeComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsHashtable.h"
#include "prtime.h"
#include "prmon.h"
#include "nsCOMPtr.h"

class nsFactoryEntry;
class nsDll;
class nsIServiceManager;

// Registry Factory creation function defined in nsRegistry.cpp
// We hook into this function locally to create and register the registry
// Since noone outside xpcom needs to know about this and nsRegistry.cpp
// does not have a local include file, we are putting this definition
// here rather than in nsIRegistry.h
extern "C" NS_EXPORT nsresult NS_RegistryGetFactory(nsIFactory** aFactory);

extern const char xpcomBaseName[];
extern const char xpcomKeyName[];
extern const char lastModValueName[];
extern const char fileSizeValueName[];
extern const char XPCOM_LIB_PREFIX[];

////////////////////////////////////////////////////////////////////////////////

class nsComponentManagerImpl : public nsIComponentManager {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTMANAGER

    // nsComponentManagerImpl methods:
    nsComponentManagerImpl();
    virtual ~nsComponentManagerImpl();

    static nsComponentManagerImpl* gComponentManager;
    nsresult Init(void);
    nsresult PlatformPrePopulateRegistry();

protected:
    nsresult RegistryNameForLib(const char *aLibName, char **aRegistryName);
    nsresult RegisterComponentCommon(const nsCID &aClass,
                                     const char *aClassName,
                                     const char *aProgID, char *aRegistryName,
                                     PRBool aReplace, PRBool aPersist,
                                     const char *aType);
    nsresult AddComponentToRegistry(const nsCID &aCID, const char *aClassName,
                                    const char *aProgID,
                                    const char *aRegistryName,
                                    const char *aType);
    nsresult GetLoaderForType(const char *aType, 
                              nsIComponentLoader **aLoader);

    nsresult LoadFactory(nsFactoryEntry *aEntry, nsIFactory **aFactory);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass, PRBool checkRegistry);

    nsresult SyncComponentsInDir(PRInt32 when, nsIFileSpec *dirSpec);
    nsresult SelfRegisterDll(nsDll *dll);
    nsresult SelfUnregisterDll(nsDll *dll);
    nsresult HashProgID(const char *aprogID, const nsCID &aClass);
    nsresult UnloadLibraries(nsIServiceManager *servmgr);

    // The following functions are the only ones that operate on the persistent
    // registry
    nsresult PlatformInit(void);
    nsresult PlatformVersionCheck();
    nsresult PlatformRegister(const char *cidString, const char *className, const char *progID, nsDll *dll);
    nsresult PlatformUnregister(const char *cidString, const char *aLibrary);
    nsresult PlatformFind(const nsCID &aCID, nsFactoryEntry* *result);
    nsresult PlatformProgIDToCLSID(const char *aProgID, nsCID *aClass);
    nsresult PlatformCLSIDToProgID(const nsCID *aClass, char* *aClassName, char* *aProgID);

protected:
    nsObjectHashtable*  mFactories;
    nsObjectHashtable*  mProgIDs;
    nsSupportsHashtable*  mLoaders;
    PRMonitor*          mMon;
    nsIRegistry*        mRegistry;
    nsIRegistry::Key    mXPCOMKey;
    nsIRegistry::Key    mClassesKey;
    nsIRegistry::Key    mCLSIDKey;
    PRBool              mPrePopulationDone;
    nsIRegistry::Key    mLoadersKey;
    nsNativeComponentLoader *mNativeComponentLoader;
    nsSpecialSystemDirectory *mComponentsDir;
    PRUint32 mComponentsDirLen;
};

#define NS_MAX_FILENAME_LEN	1024

#define NS_ERROR_IS_DIR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XPCOM, 24)

#ifdef XP_UNIX
/* The default registry on the unix system is $HOME/.mozilla/registry per
 * vr_findGlobalRegName(). vr_findRegFile() will create the registry file
 * if it doesn't exist. But it wont create directories.
 *
 * Hence we need to create the directory if it doesn't exist already.
 *
 * Why create it here as opposed to the app ?
 * ------------------------------------------
 * The app cannot create the directory in main() as most of the registry
 * and initialization happens due to use of static variables.
 * And we dont want to be dependent on the order in which
 * these static stuff happen.
 *
 * Permission for the $HOME/.mozilla will be Read,Write,Execute
 * for user only. Nothing to group and others.
 */
#define NS_MOZILLA_DIR_NAME		".mozilla"
#define NS_MOZILLA_DIR_PERMISSION	00700
#endif /* XP_UNIX */

#ifdef XP_BEOS
#define NS_MOZILLA_DIR_NAME		"mozilla"
#define NS_MOZILLA_DIR_PERMISSION	00700
#endif /* XP_BEOS */

/**
 * When using the registry we put a version number in it.
 * If the version number that is in the registry doesn't match
 * the following, we ignore the registry. This lets news versions
 * of the software deal with old formats of registry and not
 *
 * alpha0.20 : First time we did versioning
 * alpha0.30 : Changing autoreg to begin registration from ./components on unix
 * alpha0.40 : repository -> component manager
 * alpha0.50 : using nsIRegistry
 * alpha0.60 : xpcom 2.0 landing
 * alpha0.70 : using nsIFileSpec. PRTime -> PRUint32
 * alpha0.90 : using nsIComponentLoader, abs:/rel:/lib:, shaver-cleanup
 */
#define NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING "alpha0.90"

////////////////////////////////////////////////////////////////////////////////
/**
 * Class: nsFactoryEntry()
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

class nsFactoryEntry {
public:
    nsFactoryEntry(const nsCID &aClass, char *location, char *aType,
                   nsIComponentLoader *aLoader);
    nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory);
    ~nsFactoryEntry();

    nsresult GetFactory(nsIFactory **aFactory) {
        if (factory) {
            *aFactory = factory.get();
            NS_ADDREF(*aFactory);
            return NS_OK;
        }
        nsresult rv = loader->GetFactory(cid, location, type, aFactory);
        if (NS_SUCCEEDED(rv))
            factory = do_QueryInterface(*aFactory);
        return rv;
    }

#if 0 /* unused? */
    nsresult Init(nsHashtable* dllHashtable, const nsCID &aClass, const char *aLibrary,
                  PRTime lastModTime, PRUint32 fileSize);
#endif

    nsCID cid;
    char *location;
    nsCOMPtr<nsIFactory> factory;
    char *type;
    nsCOMPtr<nsIComponentLoader> loader;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsComponentManager_h__
