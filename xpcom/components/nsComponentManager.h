/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsComponentManager_h__
#define nsComponentManager_h__

#include "nsXPCOM.h"

#include "xpcom-private.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManagerObsolete.h"
#include "nsCategoryManager.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsIModule.h"
#include "nsIModuleLoader.h"
#include "nsStaticComponentLoader.h"
#include "nsNativeComponentLoader.h"
#include "nsIFactory.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "pldhash.h"
#include "prtime.h"
#include "prmon.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsIFile.h"
#include "plarena.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

struct nsFactoryEntry;
class nsIServiceManager;
struct PRThread;

#define NS_COMPONENTMANAGER_CID                      \
{ /* 91775d60-d5dc-11d2-92fb-00e09805570f */         \
    0x91775d60,                                      \
    0xd5dc,                                          \
    0x11d2,                                          \
    {0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

/* keys for registry use */
extern const char xpcomKeyName[];
extern const char xpcomComponentsKeyName[];
extern const char lastModValueName[];
extern const char fileSizeValueName[];
extern const char nativeComponentType[];
extern const char staticComponentType[];

typedef int LoaderType;

// Predefined loader types.
#define NS_LOADER_TYPE_NATIVE  -1
#define NS_LOADER_TYPE_STATIC  -2
#define NS_LOADER_TYPE_INVALID -3

#ifdef DEBUG
#define XPCOM_CHECK_PENDING_CIDS
#endif
////////////////////////////////////////////////////////////////////////////////

// Array of Loaders and their type strings
struct nsLoaderdata {
    nsCOMPtr<nsIModuleLoader> loader;
    nsCString                 type;
};

// Deferred modules are kept as a nsTArray of this type. This is not a nested
// class so that nsStaticComponentLoader.h can forward-declare it.
struct DeferredModule
{
    DeferredModule() :
        type(nsnull), modTime(0) { }

    const char             *type;
    nsCOMPtr<nsILocalFile>  file;
    nsCString               location;
    nsCOMPtr<nsIModule>     module;
    PRInt64                 modTime;
};

class nsComponentManagerImpl
    : public nsIComponentManager,
      public nsIServiceManager,
      public nsIComponentRegistrar,
      public nsSupportsWeakReference,
      public nsIInterfaceRequestor,
      public nsIServiceManagerObsolete,
      public nsIComponentManagerObsolete
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    // Since the nsIComponentManagerObsolete and nsIComponentManager share some of the 
    // same interface function names, we have to manually define the functions here.
    // The only function that is in nsIComponentManagerObsolete and is in nsIComponentManager
    // is GetClassObjectContractID.  
    //
    // nsIComponentManager function not in nsIComponentManagerObsolete:
    NS_IMETHOD GetClassObjectByContractID(const char *aContractID,
                                          const nsIID &aIID,
                                          void **_retval);


    NS_DECL_NSICOMPONENTMANAGEROBSOLETE

    // Since the nsIComponentManagerObsolete and nsIComponentRegistrar share some of the
    // same interface function names, we have to manually define the functions here.
    // the only function that is shared is UnregisterFactory
    NS_IMETHOD AutoRegister(nsIFile *aSpec); 
    NS_IMETHOD AutoUnregister(nsIFile *aSpec); 
    NS_IMETHOD RegisterFactory(const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFactory *aFactory); 
    //  NS_IMETHOD UnregisterFactory(const nsCID & aClass, nsIFactory *aFactory); 
    NS_IMETHOD RegisterFactoryLocation(const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFile *aFile, const char *loaderStr, const char *aType); 
    NS_IMETHOD UnregisterFactoryLocation(const nsCID & aClass, nsIFile *aFile); 
    NS_IMETHOD IsCIDRegistered(const nsCID & aClass, PRBool *_retval); 
    NS_IMETHOD IsContractIDRegistered(const char *aClass, PRBool *_retval); 
    NS_IMETHOD EnumerateCIDs(nsISimpleEnumerator **_retval); 
    NS_IMETHOD EnumerateContractIDs(nsISimpleEnumerator **_retval); 
    NS_IMETHOD CIDToContractID(const nsCID & aClass, char **_retval); 
    NS_IMETHOD ContractIDToCID(const char *aContractID, nsCID * *_retval); 
    
    nsresult RegistryLocationForFile(nsIFile* aFile,
                                     nsCString& aResult);
    nsresult FileForRegistryLocation(const nsCString &aLocation,
                                     nsILocalFile **aSpec);

    NS_DECL_NSISERVICEMANAGER
    NS_DECL_NSISERVICEMANAGEROBSOLETE

    // nsComponentManagerImpl methods:
    nsComponentManagerImpl();

    static nsComponentManagerImpl* gComponentManager;
    nsresult Init(nsStaticModuleInfo const *aStaticModules,
                  PRUint32 aStaticModuleCount);
    // NOTE: XPCOM initialization must call either ReadPersistentRegistry or
    // AutoRegister to fully initialize the nsComponentManagerImpl.

    nsresult WritePersistentRegistry();
    nsresult ReadPersistentRegistry();

    nsresult Shutdown(void);

    nsresult FreeServices();

    nsresult
    NS_GetService(const char *aContractID, const nsIID& aIID, PRBool aDontCreate, nsISupports** result);

    nsresult RegisterComponentCommon(const nsCID &aClass,
                                     const char *aClassName,
                                     const char *aContractID,
                                     PRUint32 aContractIDLen,
                                     const char *aRegistryName,
                                     PRUint32 aRegistryNameLen,
                                     PRBool aReplace, PRBool aPersist,
                                     const char *aType);
    // Convert a loader type string into an index into the loader data
    // array. Empty loader types are converted to NATIVE. Returns
    // NS_LOADER_TYPE_INVALID if loader type cannot be determined.
    LoaderType GetLoaderType(const char *typeStr);
    LoaderType AddLoaderType(const char *typeStr);
    const char* StringForLoaderType(LoaderType aType) {
        return mLoaderData[aType].type.get();
    }
    nsIModuleLoader* LoaderForType(LoaderType aType);
    int GetLoaderCount() { return mLoaderData.Length(); }
    void GetAllLoaders();

    nsresult FindFactory(const char *contractID, PRUint32 aContractIDLen, nsIFactory **aFactory) ;
    nsresult LoadFactory(nsFactoryEntry *aEntry, nsIFactory **aFactory);

    nsFactoryEntry *GetFactoryEntry(const char *aContractID,
                                    PRUint32 aContractIDLen);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass);

    nsresult SyncComponentsInDir(PRInt32 when, nsIFile *dirSpec);
    nsresult HashContractID(const char *acontractID, PRUint32 aContractIDLen,
                            nsFactoryEntry *fe_ptr);

    void DeleteContractIDEntriesByCID(const nsCID* aClass, nsIFactory* factory);
    nsresult AutoRegisterImpl(nsIFile*                  inDirSpec,
                              nsCOMArray<nsILocalFile> &aLeftovers,
                              nsTArray<DeferredModule> &aDeferred);
    nsresult AutoRegisterDirectory(nsIFile*                  aComponentFile,
                                   nsCOMArray<nsILocalFile> &aLeftovers,
                                   nsTArray<DeferredModule> &aDeferred);
    nsresult AutoRegisterComponent(nsILocalFile*             aComponentFile,
                                   nsTArray<DeferredModule> &aDeferred,
                                   LoaderType                minLoader = NS_LOADER_TYPE_NATIVE);
    void LoadLeftoverComponents(nsCOMArray<nsILocalFile> &aLeftovers,
                                nsTArray<DeferredModule> &aDeferred,
                                LoaderType                minLoader);

    void LoadDeferredModules(nsTArray<DeferredModule> &aDeferred);

    PLDHashTable        mFactories;
    PLDHashTable        mContractIDs;
    PRMonitor*          mMon;

    nsNativeModuleLoader mNativeModuleLoader;
    nsStaticModuleLoader mStaticModuleLoader;
    nsCOMPtr<nsIFile>   mComponentsDir;
    PRInt32             mComponentsOffset;

    nsCOMPtr<nsIFile>   mGREComponentsDir;
    PRInt32             mGREComponentsOffset;

    nsCOMPtr<nsIFile>   mRegistryFile;

    // Shutdown
    #define NS_SHUTDOWN_NEVERHAPPENED 0
    #define NS_SHUTDOWN_INPROGRESS 1
    #define NS_SHUTDOWN_COMPLETE 2
    PRUint32 mShuttingDown;

    nsTArray<nsLoaderdata> mLoaderData;

    nsDataHashtable<nsHashableHashKey, PRInt64> mAutoRegEntries;

    PRBool              mRegistryDirty;
    nsCOMPtr<nsCategoryManager>  mCategoryManager;

    PLArenaPool   mArena;

    struct PendingServiceInfo {
      const nsCID* cid;
      PRThread* thread;
    };

    inline PendingServiceInfo* AddPendingService(const nsCID& aServiceCID,
                                                 PRThread* aThread);
    inline void RemovePendingService(const nsCID& aServiceCID);
    inline PRThread* GetPendingServiceThread(const nsCID& aServiceCID) const;

    nsTArray<PendingServiceInfo> mPendingServices;

private:
    ~nsComponentManagerImpl();
};


#define NS_MAX_FILENAME_LEN	1024

#define NS_ERROR_IS_DIR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XPCOM, 24)

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

struct nsFactoryEntry {
    nsFactoryEntry(const nsCID    &aClass,
                   LoaderType      aLoaderType,
                   const char     *aLocationKey,
                   nsFactoryEntry *aParent = nsnull);

    nsFactoryEntry(const nsCID    &aClass,
                   nsIFactory     *aFactory,
                   nsFactoryEntry *aParent = nsnull) :
        mCid(aClass),
        mLoaderType(NS_LOADER_TYPE_INVALID),
        mFactory(aFactory),
        mParent(aParent) {
        mLocationKey = nsnull;
    }

    ~nsFactoryEntry();

    void ReInit(LoaderType  aLoaderType,
                const char *aLocationKey);

    nsresult GetFactory(nsIFactory **aFactory);

    nsCID                  mCid;
    LoaderType             mLoaderType;
    const char            *mLocationKey;
    nsCOMPtr<nsIFactory>   mFactory;
    nsCOMPtr<nsISupports>  mServiceObject;
    nsFactoryEntry        *mParent;
};

////////////////////////////////////////////////////////////////////////////////

struct nsFactoryTableEntry : public PLDHashEntryHdr {
    nsFactoryEntry *mFactoryEntry;    
};

struct nsContractIDTableEntry : public PLDHashEntryHdr {
    char           *mContractID;
    PRUint32        mContractIDLen;
    nsFactoryEntry *mFactoryEntry;    
};

#endif // nsComponentManager_h__

