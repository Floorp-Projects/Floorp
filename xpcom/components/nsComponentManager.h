/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsComponentManager_h__
#define nsComponentManager_h__

#include "nsIComponentLoader.h"
#include "nsNativeComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsRegistry.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsHashtable.h"
#include "prtime.h"
#include "prmon.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

class nsFactoryEntry;
class nsDll;
class nsIServiceManager;

// Registry Factory creation function defined in nsRegistry.cpp
// We hook into this function locally to create and register the registry
// Since noone outside xpcom needs to know about this and nsRegistry.cpp
// does not have a local include file, we are putting this definition
// here rather than in nsIRegistry.h
extern "C" NS_EXPORT nsresult NS_RegistryGetFactory(nsIFactory** aFactory);

// Predefined loader types. Do not change the numbers.
// NATIVE should be 0 as it is being used as the first array index.
#define NS_COMPONENT_TYPE_NATIVE 0
#define NS_COMPONENT_TYPE_FACTORY_ONLY -1
// this define means that the factory entry only has a ContractID 
// to service mapping and has no cid mapping.
#define NS_COMPONENT_TYPE_SERVICE_ONLY -2

extern const char XPCOM_LIB_PREFIX[];

////////////////////////////////////////////////////////////////////////////////

class nsComponentManagerImpl
    : public nsIComponentManager,
      public nsIServiceManager,
      public nsSupportsWeakReference,
      public nsIInterfaceRequestor {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTMANAGER
    NS_DECL_NSIINTERFACEREQUESTOR

    // Service Manager
    NS_IMETHOD
    RegisterService(const nsCID& aClass, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const nsCID& aClass);

    NS_IMETHOD
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    RegisterService(const char* aContractID, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const char* aContractID);

    NS_IMETHOD
    GetService(const char* aContractID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const char* aContractID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    // nsComponentManagerImpl methods:
    nsComponentManagerImpl();
    virtual ~nsComponentManagerImpl();

    static nsComponentManagerImpl* gComponentManager;
    nsresult Init(void);
    nsresult PlatformPrePopulateRegistry();
    nsresult Shutdown(void);

    friend class nsFactoryEntry;
    friend class nsServiceManager;
    friend nsresult
    NS_GetService(const char *aContractID, const nsIID& aIID, PRBool aDontCreate, nsISupports** result);
protected:
    nsresult FetchService(const char *aContractID, const nsIID& aIID, nsISupports** result);
    nsresult RegistryNameForLib(const char *aLibName, char **aRegistryName);
    nsresult RegisterComponentCommon(const nsCID &aClass,
                                     const char *aClassName,
                                     const char *aContractID,
                                     const char *aRegistryName,
                                     PRBool aReplace, PRBool aPersist,
                                     const char *aType);
    nsresult AddComponentToRegistry(const nsCID &aCID, const char *aClassName,
                                    const char *aContractID,
                                    const char *aRegistryName,
                                    const char *aType);
    nsresult GetLoaderForType(int aType, 
                              nsIComponentLoader **aLoader);
    nsresult FindFactory(const char *contractID, nsIFactory **aFactory) ;
    nsresult LoadFactory(nsFactoryEntry *aEntry, nsIFactory **aFactory);

    nsFactoryEntry *GetFactoryEntry(const char *aContractID, int checkRegistry = -1);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass, int checkRegistry = -1);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass, nsIDKey &cidKey, int checkRegistry = -1);

    nsresult SyncComponentsInDir(PRInt32 when, nsIFile *dirSpec);
    nsresult SelfRegisterDll(nsDll *dll);
    nsresult SelfUnregisterDll(nsDll *dll);
    nsresult HashContractID(const char *acontractID, nsFactoryEntry *fe_ptr);
    nsresult HashContractID(const char *acontractID, const nsCID &aClass, nsFactoryEntry **fe_ptr = NULL);
    nsresult HashContractID(const char *acontractID, const nsCID &aClass, nsIDKey &cidKey, nsFactoryEntry **fe_ptr = NULL);
    nsresult UnloadLibraries(nsIServiceManager *servmgr, PRInt32 when);
    
    nsresult FreeServices();
    // The following functions are the only ones that operate on the persistent
    // registry
    nsresult PlatformInit(void);
    nsresult PlatformVersionCheck(nsRegistryKey *aXPCOMRootKey);
    nsresult PlatformRegister(const char *cidString, const char *className, const char *contractID, nsDll *dll);
    nsresult PlatformUnregister(const char *cidString, const char *aLibrary);
    nsresult PlatformFind(const nsCID &aCID, nsFactoryEntry* *result);
    nsresult PlatformContractIDToCLSID(const char *aContractID, nsCID *aClass);
    nsresult PlatformCLSIDToContractID(const nsCID *aClass, char* *aClassName, char* *aContractID);

    // Convert a loader type string into an index into the loader data
    // array. Empty loader types are converted to NATIVE. Returns -1 if
    // loader type cannot be determined.
    int GetLoaderType(const char *typeStr);

    // Add a loader type if not already known. Return the typeIndex
    // if the loader type is either added or already there; -1 if
    // there was an error
    int AddLoaderType(const char *typeStr);

private:
    nsresult AutoRegisterImpl(PRInt32 when, nsIFile *inDirSpec);

protected:
    nsObjectHashtable*  mFactories;
    nsObjectHashtable*  mContractIDs;
    PRMonitor*          mMon;
    nsRegistry*         mRegistry;
    nsRegistryKey       mXPCOMKey;
    nsRegistryKey       mClassesKey;
    nsRegistryKey       mCLSIDKey;
    PRBool              mPrePopulationDone;
    nsRegistryKey       mLoadersKey;
    nsNativeComponentLoader *mNativeComponentLoader;
    nsIComponentLoader  *mStaticComponentLoader;
    nsCOMPtr<nsIFile>   mComponentsDir;
    PRInt32             mComponentsOffset;

    // Shutdown
    #define NS_SHUTDOWN_NEVERHAPPENED 0
    #define NS_SHUTDOWN_INPROGRESS 1
    #define NS_SHUTDOWN_COMPLETE 2
    PRUint32 mShuttingDown;

    // Array of Loaders and their type strings
    struct nsLoaderdata {
        nsIComponentLoader *loader;
        const char *type;
    };

    nsLoaderdata *mLoaderData;
    int mNLoaderData;
    int mMaxNLoaderData;
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
 * alpha0.92 : restructured registry keys
 * alpha0.93 : changed component names to native strings instead of UTF8
 */
#define NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING "alpha0.93"

class nsServiceEntry {
public:
    nsServiceEntry(nsISupports* service, nsFactoryEntry* factEntry);
    ~nsServiceEntry();

    nsresult AddListener(nsIShutdownListener* listener);
    nsresult RemoveListener(nsIShutdownListener* listener);
    nsresult NotifyListeners(void);

    nsISupports* mObject;
    nsVoidArray* mListeners;
    PRBool mShuttingDown;
    nsFactoryEntry* mFactoryEntry; // non-owning back pointer
};

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
    nsFactoryEntry(const nsCID &aClass, const char *location, int aType);
    nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory);
    ~nsFactoryEntry();

    nsresult ReInit(const nsCID &aClass, const char *location, int aType);
    nsresult ReInit(const nsCID &aClass, nsIFactory *aFactory);

    nsresult GetFactory(nsIFactory **aFactory, 
                        nsComponentManagerImpl * mgr) {
        if (factory) {
            *aFactory = factory.get();
            NS_ADDREF(*aFactory);
            return NS_OK;
        }

        if (typeIndex < 0)
            return NS_ERROR_FAILURE;

        nsresult rv;
        nsCOMPtr<nsIComponentLoader> loader;
        rv = mgr->GetLoaderForType(typeIndex, getter_AddRefs(loader));
        if(NS_FAILED(rv))
            return rv;

        rv = loader->GetFactory(cid, location, mgr->mLoaderData[typeIndex].type, aFactory);
        if (NS_SUCCEEDED(rv))
            factory = do_QueryInterface(*aFactory);
        return rv;
    }

    nsCID cid;
    nsCString location;
    nsCOMPtr<nsIFactory> factory;
    nsServiceEntry *mServiceEntry;
    // This is an index into the mLoaderData array that holds the type string and the loader
    int typeIndex;
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsComponentManager_h__
