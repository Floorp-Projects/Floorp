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

#include "nsIComponentLoader.h"
#include "xpcom-private.h"
#include "nsNativeComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManagerObsolete.h"
#include "nsIComponentLoaderManager.h"
#include "nsCategoryManager.h"
#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "pldhash.h"
#include "prtime.h"
#include "prmon.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "plarena.h"

class nsFactoryEntry;
class nsDll;
class nsIServiceManager;


// Predefined loader types. Do not change the numbers.
// NATIVE should be 0 as it is being used as the first array index.
#define NS_COMPONENT_TYPE_NATIVE 0
#define NS_COMPONENT_TYPE_FACTORY_ONLY -1
// this define means that the factory entry only has a ContractID 
// to service mapping and has no cid mapping.
#define NS_COMPONENT_TYPE_SERVICE_ONLY -2


#ifdef DEBUG
#define XPCOM_CHECK_PENDING_CIDS
#endif
////////////////////////////////////////////////////////////////////////////////

// Array of Loaders and their type strings
struct nsLoaderdata {
    nsIComponentLoader *loader;
    const char *type;
};

class nsComponentManagerImpl
    : public nsIComponentManager,
      public nsIServiceManager,
      public nsIComponentRegistrar,
      public nsSupportsWeakReference,
      public nsIInterfaceRequestor,
      public nsIComponentLoaderManager,
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
    
    NS_DECL_NSISERVICEMANAGER
    NS_DECL_NSISERVICEMANAGEROBSOLETE
    NS_DECL_NSICOMPONENTLOADERMANAGER

    // nsComponentManagerImpl methods:
    nsComponentManagerImpl();

    static nsComponentManagerImpl* gComponentManager;
    nsresult Init(void);

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
    nsresult GetLoaderForType(int aType, 
                              nsIComponentLoader **aLoader);
    nsresult FindFactory(const char *contractID, PRUint32 aContractIDLen, nsIFactory **aFactory) ;
    nsresult LoadFactory(nsFactoryEntry *aEntry, nsIFactory **aFactory);

    nsFactoryEntry *GetFactoryEntry(const char *aContractID,
                                    PRUint32 aContractIDLen);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass);

    nsresult SyncComponentsInDir(PRInt32 when, nsIFile *dirSpec);
    nsresult SelfRegisterDll(nsDll *dll);
    nsresult SelfUnregisterDll(nsDll *dll);
    nsresult HashContractID(const char *acontractID, PRUint32 aContractIDLen,
                            nsFactoryEntry *fe_ptr);

    void DeleteContractIDEntriesByCID(const nsCID* aClass, const char*registryName);
    void DeleteContractIDEntriesByCID(const nsCID* aClass, nsIFactory* factory);

    nsresult UnloadLibraries(nsIServiceManager *servmgr, PRInt32 when);
    
    // Convert a loader type string into an index into the loader data
    // array. Empty loader types are converted to NATIVE. Returns -1 if
    // loader type cannot be determined.
    int GetLoaderType(const char *typeStr);

    // Add a loader type if not already known. Out the typeIndex
    // if the loader type is either added or already there;
    // returns NS_OK or an error on failure.
    nsresult AddLoaderType(const char *typeStr, int *typeIndex);

    int GetLoaderCount() { return mNLoaderData + 1; }

    // registers only the files in spec's location by loaders other than the
    // native loader.  This is an optimization method only.
    nsresult AutoRegisterNonNativeComponents(nsIFile* spec);

    nsresult AutoRegisterImpl(PRInt32 when, nsIFile *inDirSpec, PRBool fileIsCompDir=PR_TRUE);
    nsresult RemoveEntries(nsIFile* file);

    PLDHashTable        mFactories;
    PLDHashTable        mContractIDs;
    PRMonitor*          mMon;

    nsNativeComponentLoader *mNativeComponentLoader;
#ifdef ENABLE_STATIC_COMPONENT_LOADER
    nsIComponentLoader  *mStaticComponentLoader;
#endif
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

    nsLoaderdata *mLoaderData;
    int mNLoaderData;
    int mMaxNLoaderData;

    PRBool              mRegistryDirty;
    nsHashtable         mAutoRegEntries;
    nsCOMPtr<nsCategoryManager>  mCategoryManager;

    PLArenaPool   mArena;

#ifdef XPCOM_CHECK_PENDING_CIDS
    nsresult AddPendingCID(const nsCID &aClass);
    void RemovePendingCID(const nsCID &aClass);

    nsVoidArray         mPendingCIDs;
#endif

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

class nsFactoryEntry {
public:
    nsFactoryEntry(const nsCID &aClass,
                   const char *location, PRUint32 locationlen, int aType, class nsFactoryEntry* parent = nsnull);
    nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory, class nsFactoryEntry* parent = nsnull);
    ~nsFactoryEntry();

    nsresult ReInit(const nsCID &aClass, const char *location, int aType);

    nsresult GetFactory(nsIFactory **aFactory, 
                        nsComponentManagerImpl * mgr) {
        if (mFactory) {
            *aFactory = mFactory.get();
            NS_ADDREF(*aFactory);
            return NS_OK;
        }

        if (mTypeIndex < 0)
            return NS_ERROR_FAILURE;

        nsresult rv;
        nsCOMPtr<nsIComponentLoader> loader;
        rv = mgr->GetLoaderForType(mTypeIndex, getter_AddRefs(loader));
        if(NS_FAILED(rv))
            return rv;

        rv = loader->GetFactory(mCid, mLocation, mgr->mLoaderData[mTypeIndex].type, aFactory);
        if (NS_SUCCEEDED(rv))
            mFactory = do_QueryInterface(*aFactory);
        return rv;
    }

    nsCID mCid;
    nsCOMPtr<nsIFactory> mFactory;
    // This is an index into the mLoaderData array that holds the type string and the loader 
    int mTypeIndex;
    nsCOMPtr<nsISupports> mServiceObject;
    char* mLocation;
    class nsFactoryEntry* mParent;
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


class AutoRegEntry
{
public:
    AutoRegEntry(const nsACString& name, PRInt64* modDate);
    ~AutoRegEntry();

    const nsDependentCString GetName()
      { return nsDependentCString(mName, mNameLen); }
    PRInt64 GetDate() {return mModDate;}
    void    SetDate(PRInt64 *date) { mModDate = *date;}
    PRBool  Modified(PRInt64 *date);

    // this is the optional field line in the compreg.dat.
    // it must not contain any comma's and it must be null terminated.
    char*   GetOptionalData() {return mData;};
    void    SetOptionalData(const char* data);

private:
    char*   mName;
    PRUint32 mNameLen;
    char*   mData;
    PRInt64 mModDate;
};
#endif // nsComponentManager_h__

