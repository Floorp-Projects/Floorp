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

#include "nsXPCOM.h"

#include "nsIComponentLoader.h"
#include "nsNativeComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManagerObsolete.h"
#include "nsIComponentLoaderManager.h"
#include "nsICategoryManager.h"
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

extern const char XPCOM_LIB_PREFIX[];

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
    // the only fuction that is shared is UnregisterFactory
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
    virtual ~nsComponentManagerImpl();

    static nsComponentManagerImpl* gComponentManager;
    nsresult Init(void);

    nsresult WritePersistentRegistry();
    nsresult ReadPersistentRegistry();
private:
    nsresult WriteCategoryManagerToRegistry(PRFileDesc* fd);
public:

    nsresult Shutdown(void);

    nsresult FreeServices();

    friend class nsFactoryEntry;
    friend class nsServiceManager;

    friend nsresult
    NS_GetService(const char *aContractID, const nsIID& aIID, PRBool aDontCreate, nsISupports** result);

protected:
    nsresult RegistryNameForLib(const char *aLibName, char **aRegistryName);
    nsresult RegisterComponentCommon(const nsCID &aClass,
                                     const char *aClassName,
                                     const char *aContractID,
                                     const char *aRegistryName,
                                     PRBool aReplace, PRBool aPersist,
                                     const char *aType);
    nsresult GetLoaderForType(int aType, 
                              nsIComponentLoader **aLoader);
    nsresult FindFactory(const char *contractID, nsIFactory **aFactory) ;
    nsresult LoadFactory(nsFactoryEntry *aEntry, nsIFactory **aFactory);

    nsFactoryEntry *GetFactoryEntry(const char *aContractID);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass);
    nsFactoryEntry *GetFactoryEntry(const nsCID &aClass, nsIDKey &cidKey);

    nsresult SyncComponentsInDir(PRInt32 when, nsIFile *dirSpec);
    nsresult SelfRegisterDll(nsDll *dll);
    nsresult SelfUnregisterDll(nsDll *dll);
    nsresult HashContractID(const char *acontractID, nsFactoryEntry *fe_ptr);
    nsresult HashContractID(const char *acontractID, const nsCID &aClass, nsFactoryEntry **fe_ptr = NULL);
    nsresult HashContractID(const char *acontractID, const nsCID &aClass, nsIDKey &cidKey, nsFactoryEntry **fe_ptr = NULL);

    void DeleteContractIDEntriesByCID(const nsCID* aClass, const char*registryName);
    void DeleteContractIDEntriesByCID(const nsCID* aClass, nsIFactory* factory);

    nsresult UnloadLibraries(nsIServiceManager *servmgr, PRInt32 when);
    
    // Convert a loader type string into an index into the loader data
    // array. Empty loader types are converted to NATIVE. Returns -1 if
    // loader type cannot be determined.
    int GetLoaderType(const char *typeStr);

    // Add a loader type if not already known. Return the typeIndex
    // if the loader type is either added or already there; -1 if
    // there was an error
    int AddLoaderType(const char *typeStr);

public:
    int GetLoaderCount() { return mNLoaderData + 1; }

    // registers only the files in spec's location by loaders other than the
    // native loader.  This is an optimization method only.
    nsresult AutoRegisterNonNativeComponents(nsIFile* spec);


private:
    nsresult AutoRegisterImpl(PRInt32 when, nsIFile *inDirSpec, PRBool fileIsCompDir=PR_TRUE);

protected:
    PLDHashTable        mFactories;
    PLDHashTable        mContractIDs;
    PRMonitor*          mMon;

    nsNativeComponentLoader *mNativeComponentLoader;
    nsIComponentLoader  *mStaticComponentLoader;
    nsCOMPtr<nsIFile>   mComponentsDir;
    PRInt32             mComponentsOffset;

    // Shutdown
    #define NS_SHUTDOWN_NEVERHAPPENED 0
    #define NS_SHUTDOWN_INPROGRESS 1
    #define NS_SHUTDOWN_COMPLETE 2
    PRUint32 mShuttingDown;

    nsLoaderdata *mLoaderData;
    int mNLoaderData;
    int mMaxNLoaderData;

    PRBool              mRegistryDirty;
    nsVoidArray         mAutoRegEntries;
    nsCOMPtr<nsICategoryManager>  mCategoryManager;

    PLArenaPool   mArena;
};


#define NS_MAX_FILENAME_LEN	1024

#define NS_ERROR_IS_DIR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XPCOM, 24)

#if defined(XP_UNIX) && !defined(XP_MACOSX)
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
#define NS_MOZILLA_DIR_NAME		"Mozilla"
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
    char *location;
    nsCOMPtr<nsIFactory> factory;
    // This is an index into the mLoaderData array that holds the type string and the loader 
    int typeIndex;
    nsCOMPtr<nsISupports> mServiceObject;
};

////////////////////////////////////////////////////////////////////////////////

struct nsFactoryTableEntry : public PLDHashEntryHdr {
    nsFactoryEntry *mFactoryEntry;    
};

struct nsContractIDTableEntry : public PLDHashEntryHdr {
    char           *mContractID;
    nsFactoryEntry *mFactoryEntry;    
};
class AutoRegEntry
{
public:
    AutoRegEntry(){};
    AutoRegEntry(const char* name, PRInt64* modDate);
    virtual ~AutoRegEntry();

    char*   GetName() {return mName;}
    PRInt64 GetDate() {return mModDate;}
    void    SetDate(PRInt64 *date) { mModDate = *date;}
    PRBool  Modified(PRInt64 *date);

private:
    char*   mName;
    PRInt64 mModDate;
};
#endif // nsComponentManager_h__

