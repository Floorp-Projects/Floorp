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

#include <stdlib.h>
#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsIServiceManager.h"
#include "nsSpecialSystemDirectory.h"
#include "nsCRT.h"
#include "nsIEnumerator.h"
#include "nsIModule.h"
#include "nsHashtableEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentLoader.h"
#include "nsNativeComponentLoader.h"

#include "plstr.h"
#include "prlink.h"
#include "prsystem.h"
#include "prprf.h"
#include "xcDll.h"
#include "prerror.h"
#include "prmem.h"

#include "prcmon.h"
#include "prthread.h" /* XXX: only used for the NSPR initialization hack (rick) */

#ifdef XP_BEOS
#include <FindDirectory.h>
#include <Path.h>
#endif

// Logging of debug output
#define FORCE_PR_LOG /* Allow logging in the release build */
#include "prlog.h"
PRLogModuleInfo* nsComponentManagerLog = NULL;

// Enable printing of critical errors on screen even for release builds
#define PRINT_CRITICAL_ERROR_TO_SCREEN

// Common Key Names 
const char xpcomBaseName[]="XPCOM";
const char xpcomKeyName[] ="Software/Mozilla/XPCOM";
const char mozillaKeyName[]="Software/Mozilla";
const char classesKeyName[]="Classes";
const char classIDKeyName[]="CLSID";
const char classesClassIDKeyName[]="Classes/CLSID";
const char componentLoadersKeyName[]="ComponentLoaders";

// Common Value Names
const char classIDValueName[]="CLSID";
const char versionValueName[]="VersionString";
const char lastModValueName[]="LastModTimeStamp";
const char fileSizeValueName[]="FileSize";
const char componentCountValueName[]="ComponentsCount";
const char progIDValueName[]="ProgID";
const char classNameValueName[]="ClassName";
const char inprocServerValueName[]="InprocServer";
const char componentTypeValueName[]="ComponentType";
const char nativeComponentType[]="application/x-mozilla-native";

// We define a CID that is used to indicate the non-existence of a
// progid in the hash table.
#define NS_NO_CID { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }
    static NS_DEFINE_CID(kNoCID, NS_NO_CID);

// Build is using USE_NSREG to turn off xpcom using registry
// but internally we use USE_REGISTRY. Map them propertly.
#ifdef USE_NSREG
#define USE_REGISTRY
#endif /* USE_NSREG */

/* prototypes for the Mac */
PRBool
nsFactoryEntry_Destroy(nsHashKey *aKey, void *aData, void* closure);

PRBool
nsCID_Destroy(nsHashKey *aKey, void *aData, void* closure);
////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass,
                               char *aLocation,
                               char *aType,
                               nsIComponentLoader *aLoader)
    : cid(aClass), factory(nsnull), loader(aLoader)
{
    loader = aLoader;
    type = nsCRT::strdup(aType);
    location = nsCRT::strdup(aLocation);
}

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory)
    : cid(aClass), location(nsnull), type(nsnull), loader(nsnull)

{
    factory = aFactory;
}

nsFactoryEntry::~nsFactoryEntry(void)
{
    factory = 0;
    loader = 0;
    if (location)
	nsAllocator::Free(location);
    if (type)
	nsAllocator::Free(type);
}

////////////////////////////////////////////////////////////////////////////////
// autoStringFree
////////////////////////////////////////////////////////////////////////////////

//
// To prevent leaks, we are using this class. Typical use would be
// for each ptr to be deleted, create an object of these types with that ptr.
// Once that object goes out of scope, deletion and hence memory free will
// automatically happen.
//
class autoStringFree
{
public:
    enum DeleteModel {
        NSPR_Delete = 1,
        nsCRT_String_Delete = 2
    };
    autoStringFree(char *Ptr, DeleteModel whichDelete): mPtr(Ptr), mWhichDelete(whichDelete) {}
    ~autoStringFree() {
        if (mPtr)
            if (mWhichDelete == NSPR_Delete) { PR_FREEIF(mPtr); }
            else if (mWhichDelete == nsCRT_String_Delete) nsCRT::free(mPtr);
            else PR_ASSERT(0);
    }
private:
    char *mPtr;
    DeleteModel mWhichDelete;
    
};


////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////


nsComponentManagerImpl::nsComponentManagerImpl()
    : mFactories(NULL), mProgIDs(NULL), mLoaders(0), mMon(NULL), 
      mRegistry(NULL), mPrePopulationDone(PR_FALSE),
      mNativeComponentLoader(0)
{
    NS_INIT_REFCNT();
}

PRBool
nsFactoryEntry_Destroy(nsHashKey *aKey, void *aData, void* closure)
{
    nsFactoryEntry* entry = NS_STATIC_CAST(nsFactoryEntry*, aData);
    delete entry;
    return PR_TRUE;
}

PRBool
nsCID_Destroy(nsHashKey *aKey, void *aData, void* closure)
{
    nsCID* entry = NS_STATIC_CAST(nsCID*, aData);
    // nasty hack. We "know" that kNoCID was entered into the hash table.
    if (entry != &kNoCID)
	    delete entry;
    return PR_TRUE;
}

nsresult nsComponentManagerImpl::Init(void) 
{
    if (nsComponentManagerLog == NULL)
    {
        nsComponentManagerLog = PR_NewLogModule("nsComponentManager");
    }

    if (mFactories == NULL) {
        mFactories = new nsObjectHashtable(nsnull, nsnull,      // should never be copied
                                           nsFactoryEntry_Destroy, nsnull, 
                                           256, /* Thread Safe */ PR_TRUE);
        if (mFactories == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mProgIDs == NULL) {
        mProgIDs = new nsObjectHashtable(nsnull, nsnull,      // should never be copied
                                         nsCID_Destroy, nsnull,
                                         256, /* Thread Safe */ PR_TRUE);
        if (mProgIDs == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mMon == NULL) {
        mMon = PR_NewMonitor();
        if (mMon == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mNativeComponentLoader == nsnull) {
        /* Create the NativeComponentLoader */
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("creating native ComponentLoader"));
        mNativeComponentLoader = new nsNativeComponentLoader();
        if (!mNativeComponentLoader)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mNativeComponentLoader);
    }
    
    if (mLoaders == nsnull) {
	PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
	       ("creating component loader table"));
	mLoaders = new nsSupportsHashtable(16, /* Thread safe */ PR_TRUE);
	if (mLoaders == nsnull)
	    return NS_ERROR_OUT_OF_MEMORY;
	nsStringKey loaderKey(nativeComponentType);
	mLoaders->Put(&loaderKey, mNativeComponentLoader);
    }
    
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("nsComponentManager: Initialized."));

#ifdef USE_REGISTRY
    PlatformInit();
#endif

    return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Beginning destruction."));

    // Release all cached factories
    if (mFactories)
        delete mFactories;

    // Unload libraries
    UnloadLibraries(NULL);

    // Release Progid hash tables
    if (mProgIDs)
        delete mProgIDs;

#ifdef USE_REGISTRY
    // Release registry
    NS_IF_RELEASE(mRegistry);
#endif /* USE_REGISTRY */

    // Release all the component loaders
    if (mLoaders)
	delete mLoaders;

    // we have an extra reference on this one, which is probably a good thing
    NS_IF_RELEASE(mNativeComponentLoader);
    
    // Destroy the Lock
    if (mMon)
        PR_DestroyMonitor(mMon);

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Destroyed."));

}

NS_IMPL_ISUPPORTS(nsComponentManagerImpl, NS_GET_IID(nsIComponentManager));

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl: Platform methods
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_REGISTRY

nsresult
nsComponentManagerImpl::PlatformInit(void)
{
    nsresult rv = NS_ERROR_FAILURE;

    // We need to create our registry. Since we are in the constructor
    // we haven't gone as far as registering the registry factory.
    // Hence, we hand create a registry.
    if (mRegistry == NULL) {		
        nsIFactory *registryFactory = NULL;
        rv = NS_RegistryGetFactory(&registryFactory);
        if (NS_SUCCEEDED(rv))
        {
            NS_DEFINE_IID(kRegistryIID, NS_IREGISTRY_IID);
            rv = registryFactory->CreateInstance(NULL, kRegistryIID,(void **)&mRegistry);
            if (NS_FAILED(rv)) return rv;
            NS_RELEASE(registryFactory);
        }
    }

#ifdef XP_UNIX
    // Create ~/.mozilla as that is the default place for the registry file

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
    char *home = getenv("HOME");
    if (home != NULL)
    {
        char dotMozillaDir[1024];
        PR_snprintf(dotMozillaDir, sizeof(dotMozillaDir),
                    "%s/" NS_MOZILLA_DIR_NAME, home);
        if (PR_Access(dotMozillaDir, PR_ACCESS_EXISTS) != PR_SUCCESS)
        {
            PR_MkDir(dotMozillaDir, NS_MOZILLA_DIR_PERMISSION);
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Creating Directory %s", dotMozillaDir));
        }
    }
#endif /* XP_UNIX */

#ifdef XP_BEOS
    BPath p;
    const char *settings = "/boot/home/config/settings";
    if(find_directory(B_USER_SETTINGS_DIRECTORY, &p) == B_OK)
        settings = p.Path();
    char settingsMozillaDir[1024];
    PR_snprintf(settingsMozillaDir, sizeof(settingsMozillaDir),
                "%s/" NS_MOZILLA_DIR_NAME, settings);
    if (PR_Access(settingsMozillaDir, PR_ACCESS_EXISTS) != PR_SUCCESS) {
        PR_MkDir(settingsMozillaDir, NS_MOZILLA_DIR_PERMISSION);
        printf("nsComponentManager: Creating Directory %s\n", settingsMozillaDir);
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Creating Directory %s", settingsMozillaDir));
    }
#endif

    // Open the App Components registry. We will keep it open forever!
    rv = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(rv)) return rv;

    // Check the version of registry. Nuke old versions.
    PlatformVersionCheck();

    // Open common registry keys here to speed access
    // Do this after PlatformVersionCheck as it may re-create our keys
    rv = mRegistry->AddSubtree(nsIRegistry::Common, xpcomKeyName, &mXPCOMKey);
    		
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->AddSubtree(nsIRegistry::Common, classesKeyName, &mClassesKey);
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->AddSubtree(nsIRegistry::Common, classIDKeyName, &mCLSIDKey);
    if (NS_FAILED(rv)) return rv;

    if (mNativeComponentLoader) {
        /* now that we have the registry, Init the native loader */
        rv = mNativeComponentLoader->Init(this, mRegistry);
    } else {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("no native component loader available for init"));
    }
    return rv;
}

/**
 * PlatformVersionCheck()
 *
 * Checks to see if the XPCOM hierarchy in the registry is the same as that of
 * the software as defined by NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING
 */
nsresult
nsComponentManagerImpl::PlatformVersionCheck()
{

    nsIRegistry::Key xpcomKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, xpcomKeyName, &xpcomKey);
    		
    if (NS_FAILED(rv)) return rv;
    
    char *buf;
    nsresult err = mRegistry->GetString(xpcomKey, versionValueName, &buf);
    autoStringFree delete_buf(buf, autoStringFree::NSPR_Delete);

    // If there is a version mismatch or no version string, we got an old registry.
    // Delete the old repository hierarchies and recreate version string
    if (NS_FAILED(err) || PL_strcmp(buf, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Registry version mismatch (%s vs %s). Nuking xpcom "
                "registry hierarchy.", buf, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING));

        // Delete the XPCOM and CLSID hierarchy
        nsIRegistry::Key mozillaKey;
        rv = mRegistry->GetSubtree(nsIRegistry::Common, mozillaKeyName,
                                   &mozillaKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Get Subtree (%s)",
                    mozillaKeyName));         
        }
        else
        {
            rv = mRegistry->RemoveSubtreeRaw(mozillaKey, xpcomBaseName);
            if(NS_FAILED(rv))
            {
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsComponentManager: Failed To Nuke Subtree (%s)",xpcomKeyName));
                return rv;
            }
        }

        rv = mRegistry->GetSubtree(nsIRegistry::Common,classesKeyName, &mozillaKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Get Subtree (%s)",classesKeyName));
        }
        else
        {
            rv = mRegistry->RemoveSubtreeRaw(mozillaKey, classIDKeyName);
            if(NS_FAILED(rv))
            {
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsComponentManager: Failed To Nuke Subtree (%s/%s)",classesKeyName,classIDKeyName));
                return rv;
            }
        }
        
        // Recreate XPCOM and CLSID keys		
        rv = mRegistry->AddSubtree(nsIRegistry::Common,xpcomKeyName, &xpcomKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Add Subtree (%s)",xpcomKeyName));
            return rv;

        }

        rv = mRegistry->AddSubtree(nsIRegistry::Common,classesClassIDKeyName, NULL);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Add Subtree (%s)",classesClassIDKeyName));
            return rv;

        }

        rv = mRegistry->SetString(xpcomKey,versionValueName, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Set String (Version) Under (%s)",xpcomKeyName));
            return rv;
        }
    }
    else 
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: platformVersionCheck() passed."));
    }

    rv = mRegistry->AddSubtree(xpcomKey, componentLoadersKeyName,
                               &mLoadersKey);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

#if 0
void
nsComponentManagerImpl::PlatformSetFileInfo(nsIRegistry::Key key, PRUint32 lastModifiedTime, PRUint32 fileSize)
{
    mRegistry->SetInt(key, lastModValueName, lastModifiedTime);
    mRegistry->SetInt(key, fileSizeValueName, fileSize);
}

/**
 * PlatformMarkNoComponents(nsDll *dll)
 *
 * Stores the dll name, last modified time, size and 0 for number of
 * components in dll in the registry at location
 *		ROOTKEY_COMMON/Software/Netscape/XPCOM/dllname
 */
nsresult
nsComponentManagerImpl::PlatformMarkNoComponents(nsDll *dll)
{
    PR_ASSERT(mRegistry!=NULL);
    
    nsresult rv;

    nsIRegistry::Key dllPathKey;
    rv = mRegistry->AddSubtreeRaw(mXPCOMKey, dll->GetPersistentDescriptorString(), &dllPathKey);    
    if(NS_FAILED(rv))
    {
        return rv;
    }
    	
    PlatformSetFileInfo(dllPathKey, dll->GetLastModifiedTime(), dll->GetSize());
    rv = mRegistry->SetInt(dllPathKey, componentCountValueName, 0);
      
    return rv;
}

nsresult
nsComponentManagerImpl::PlatformRegister(const char *cidString,
                                         const char *className,
                                         const char * progID, nsDll *dll)
{
    // Preconditions
    PR_ASSERT(cidString != NULL);
    PR_ASSERT(dll != NULL);
    PR_ASSERT(mRegistry !=NULL);

    nsresult rv;
    
    nsIRegistry::Key IDkey;
    rv = mRegistry->AddSubtreeRaw(mCLSIDKey, cidString, &IDkey);
    if (NS_FAILED(rv)) return (rv);


    rv = mRegistry->SetString(IDkey,classNameValueName, className);
    if (progID)
    {
        rv = mRegistry->SetString(IDkey,progIDValueName, progID);        
    }
    rv = mRegistry->SetString(IDkey, inprocServerValueName, dll->GetPersistentDescriptorString());
    
    if (progID)
    {
        nsIRegistry::Key progIDKey;
        rv = mRegistry->AddSubtreeRaw(mClassesKey, progID, &progIDKey);
        rv = mRegistry->SetString(progIDKey, classIDValueName, cidString);
    }

    // XXX Gross. LongLongs dont have a serialization format. This makes
    // XXX the registry non-xp. Someone beat on the nspr people to get
    // XXX a longlong serialization function please!
    
    nsIRegistry::Key dllPathKey;
    rv = mRegistry->AddSubtreeRaw(mXPCOMKey,dll->GetPersistentDescriptorString(), &dllPathKey);

    PlatformSetFileInfo(dllPathKey, dll->GetLastModifiedTime(), dll->GetSize());

    int32 nComponents = 0;
    rv = mRegistry->GetInt(dllPathKey, componentCountValueName, &nComponents);
    nComponents++;
    rv = mRegistry->SetInt(dllPathKey,componentCountValueName, nComponents);

    return rv;
}
#endif

nsresult
nsComponentManagerImpl::PlatformUnregister(const char *cidString,
                                           const char *aLibrary)
{  
    PR_ASSERT(mRegistry!=NULL);

    nsresult rv;

    nsIRegistry::Key cidKey;
    rv = mRegistry->AddSubtreeRaw(mCLSIDKey, cidString, &cidKey);

    char *progID = NULL;
    rv = mRegistry->GetString(cidKey, progIDValueName, &progID);
    if(NS_SUCCEEDED(rv))
    {
        mRegistry->RemoveSubtreeRaw(mClassesKey, progID);
        PR_FREEIF(progID);
    }

    mRegistry->RemoveSubtree(mCLSIDKey, cidString);
    	
    nsIRegistry::Key libKey;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, aLibrary, &libKey);
    if(NS_FAILED(rv)) return rv;

    // We need to reduce the ComponentCount by 1.
    // If the ComponentCount hits 0, delete the entire key.
    int32 nComponents = 0;
    rv = mRegistry->GetInt(libKey, componentCountValueName, &nComponents);
    if(NS_FAILED(rv)) return rv;
    nComponents--;
    
    if (nComponents <= 0)
    {
        rv = mRegistry->RemoveSubtreeRaw(mXPCOMKey, aLibrary);
    }
    else
    {
        rv = mRegistry->SetInt(libKey, componentCountValueName, nComponents);
    }

    return rv;
}

nsresult
nsComponentManagerImpl::PlatformFind(const nsCID &aCID, nsFactoryEntry* *result)
{
    PR_ASSERT(mRegistry!=NULL);

    nsresult rv;

    char *cidString = aCID.ToString();

    nsIRegistry::Key cidKey;
    rv = mRegistry->GetSubtreeRaw(mCLSIDKey, cidString, &cidKey);
    delete [] cidString;

    if (NS_FAILED(rv)) return rv;

    char *library = NULL;
    rv = mRegistry->GetString(cidKey, inprocServerValueName, &library);
    if (NS_FAILED(rv))
    {
        // Registry inconsistent. No File name for CLSID.
        return rv;
    }

    autoStringFree delete_library(library, autoStringFree::NSPR_Delete);

    char *componentType = NULL;
    rv = mRegistry->GetString(cidKey, componentTypeValueName, &componentType);
    if (rv == NS_ERROR_REG_NOT_FOUND)  {
        /* missing componentType, we assume application/x-moz-native */
        componentType = PL_strdup(nativeComponentType);
        if (!componentType)
            return NS_ERROR_OUT_OF_MEMORY;
    } else if (NS_FAILED(rv)) {
        return rv;              // XXX translate error code?
    }

    autoStringFree delete_type(componentType, autoStringFree::NSPR_Delete);

    nsCOMPtr<nsIComponentLoader> loader;

    rv = GetLoaderForType(componentType, getter_AddRefs(loader));
    if (NS_FAILED(rv))
        return rv;

    nsFactoryEntry *res = new nsFactoryEntry(aCID, library, componentType,
                                             loader);
    if (res == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    *result = res;
    return NS_OK;
}

nsresult
nsComponentManagerImpl::PlatformProgIDToCLSID(const char *aProgID, nsCID *aClass) 
{
    PR_ASSERT(aClass != NULL);
    PR_ASSERT(mRegistry);

    nsresult rv;
    	
    nsIRegistry::Key progIDKey;
    rv = mRegistry->GetSubtreeRaw(mClassesKey, aProgID, &progIDKey);
    if (NS_FAILED(rv)) return rv;

    char *cidString;
    rv = mRegistry->GetString(progIDKey, classIDValueName, &cidString);
    if(NS_FAILED(rv)) return rv;
    if (!(aClass->Parse(cidString)))
    {
        rv = NS_ERROR_FAILURE;
    }

    PR_FREEIF(cidString);
    return NS_OK;
}

nsresult
nsComponentManagerImpl::PlatformCLSIDToProgID(nsCID *aClass,
                                              char* *aClassName, char* *aProgID)
{
    	
    PR_ASSERT(aClass);
    PR_ASSERT(mRegistry);

    nsresult rv;

    char* cidStr = aClass->ToString();
    nsIRegistry::Key cidKey;
    rv = mRegistry->GetSubtreeRaw(mCLSIDKey, cidStr, &cidKey);
    if(NS_FAILED(rv)) return rv;
    PR_FREEIF(cidStr);

    char* classnameString;
    rv = mRegistry->GetString(cidKey, classNameValueName, &classnameString);
    if(NS_FAILED(rv)) return rv;
    *aClassName = classnameString;

    char* progidString;
    rv = mRegistry->GetString(cidKey,progIDValueName,&progidString);
    if (NS_FAILED(rv)) return rv;
    *aProgID = progidString;

    return NS_OK;

}

nsresult nsComponentManagerImpl::PlatformPrePopulateRegistry()
{
    nsresult rv;

    // Read in all CID entries and populate the mFactories
    nsCOMPtr<nsIEnumerator> cidEnum;
    rv = mRegistry->EnumerateSubtrees( mCLSIDKey, getter_AddRefs(cidEnum));
    if (NS_FAILED(rv)) return rv;

    rv = cidEnum->First();
    for (; NS_SUCCEEDED(rv) && !cidEnum->IsDone(); (rv = cidEnum->Next()))
    {
        nsCOMPtr<nsISupports> base;
        rv = cidEnum->CurrentItem(getter_AddRefs(base));
        if (NS_FAILED(rv))  continue;

        // Get specific interface.
        nsCOMPtr<nsIRegistryNode> node;
        node = do_QueryInterface(base);
        if (!node) continue;

        // Get library name
        char *cidString = NULL;
        rv = node->GetName(&cidString);
        if (NS_FAILED(rv)) continue;
        autoStringFree delete_cidString(cidString, autoStringFree::nsCRT_String_Delete);

        // Get key associated with library
        nsIRegistry::Key cidKey;
        rv = node->GetKey(&cidKey);
        if (NS_FAILED(rv)) continue;

        // Create the CID entry
        char *library = NULL;
        rv = mRegistry->GetString(cidKey, inprocServerValueName, &library);
        if (NS_FAILED(rv)) continue;
        autoStringFree delete_library(library, autoStringFree::NSPR_Delete);
        nsCID aClass;
        if (!(aClass.Parse(cidString))) continue;

        nsFactoryEntry* entry = 
            new nsFactoryEntry(aClass, PL_strdup(library),
                               PL_strdup(nativeComponentType),
                               mNativeComponentLoader);
        if (!entry)
            continue;

        nsIDKey key(aClass);
        mFactories->Put(&key, entry);
    }

    // Finally read in PROGID -> CID mappings
    nsCOMPtr<nsIEnumerator> progidEnum;
    rv = mRegistry->EnumerateSubtrees( mClassesKey, getter_AddRefs(progidEnum));
    if (NS_FAILED(rv)) return rv;

    rv = progidEnum->First();
    for (; NS_SUCCEEDED(rv) && !progidEnum->IsDone(); (rv = progidEnum->Next()))
    {
        nsCOMPtr<nsISupports> base;
        rv = progidEnum->CurrentItem(getter_AddRefs(base));
        if (NS_FAILED(rv))  continue;

        // Get specific interface.
        nsIID nodeIID = NS_IREGISTRYNODE_IID;
        nsCOMPtr<nsIRegistryNode> node;
        rv = base->QueryInterface(nodeIID, getter_AddRefs(node));
        if (NS_FAILED(rv)) continue;

        // Get the progid string
        char *progidString = NULL;
        rv = node->GetName(&progidString);
        if (NS_FAILED(rv)) continue;
        autoStringFree delete_progidString(progidString, autoStringFree::nsCRT_String_Delete);

        // Get cid string
        nsIRegistry::Key progidKey;
        rv = node->GetKey(&progidKey);
        if (NS_FAILED(rv)) continue;
        char *cidString = NULL;
        rv = mRegistry->GetString(progidKey, classIDValueName, &cidString);
        if (NS_FAILED(rv)) continue;
        autoStringFree delete_cidString(cidString, autoStringFree::NSPR_Delete);
        nsCID *aClass = new nsCID();
        if (!aClass) continue;		// Protect against out of memory.
        if (!(aClass->Parse(cidString)))
        {
            delete aClass;
            continue;
        }

        // put the {progid, Cid} mapping into our map
        nsStringKey key(progidString);
        mProgIDs->Put(&key, aClass);
        //  printf("Populating [ %s, %s ]\n", cidString, progidString);
    }

    mPrePopulationDone = PR_TRUE;
    return NS_OK;
}

#endif /* USE_REGISTRY */

//
// HashProgID
//
nsresult 
nsComponentManagerImpl::HashProgID(const char *aProgID, const nsCID &aClass)
{
    if(!aProgID)
    {
        return NS_ERROR_NULL_POINTER;
    }
    
    nsStringKey key(aProgID);
    nsCID* cid = (nsCID*) mProgIDs->Get(&key);
    if (cid)
    {
        if (cid == &kNoCID)
        {
            // we don't delete this ptr as it's static (ugh)
        }
        else
        {
            delete cid;
        }
    }
    
    cid = new nsCID(aClass);
    if (!cid)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
        
    mProgIDs->Put(&key, cid);
    return NS_OK;
}

#if 0
nsDll* nsComponentManagerImpl::CreateCachedDllName(const char *dllName)
{
    // Check our dllCollection for a dll with matching name
    nsStringKey key(dllName);
    nsDll *dll = (nsDll *) mDllStore->Get(&key);
    
    if (dll == NULL)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: New dll \"%s\".", dllName));

        // Add a new Dll into the nsDllStore
        dll = new nsDll(dllName, 1 /* dummy */);
        if (dll == NULL) return NULL;
        if (dll->GetStatus() != DLL_OK)
        {
            // Cant create a nsDll. Backoff.
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: ERROR in creating nsDll from \"%s\".", dllName));
            delete dll;
            dll = NULL;
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Adding New dll \"%s\" to mDllStore.",
                    dllName));

            mDllStore->Put(&key, (void *)dll);
        }
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Found in mDllStore \"%s\".", dllName));
    }

    return dll;
}




nsDll* nsComponentManagerImpl::CreateCachedDll(nsIFileSpec *dllSpec)
{
    nsDll *dll = NULL;
    PRUint32 modDate;
    PRUint32 size;

    if (NS_FAILED(dllSpec->GetModDate(&modDate)) ||
        NS_FAILED(dllSpec->GetFileSize(&size)))
        return NULL;

    char *persistentDescriptor = NULL;
    if (NS_FAILED(dllSpec->GetPersistentDescriptorString(&persistentDescriptor)))
        return NULL;
    dll = CreateCachedDll(persistentDescriptor, modDate, size);
    nsCRT::free(persistentDescriptor);

    return dll;
}
#endif


////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl: Public methods
////////////////////////////////////////////////////////////////////////////////

/**
 * LoadFactory()
 *
 * Given a FactoryEntry, this loads the dll if it has to, find the NSGetFactory
 * symbol, calls the routine to create a new factory and returns it to the
 * caller.
 *
 * No attempt is made to store the factory in any form anywhere.
 */
nsresult
nsComponentManagerImpl::LoadFactory(nsFactoryEntry *aEntry,
                                    nsIFactory **aFactory)
{

    if (!aFactory)
        return NS_ERROR_NULL_POINTER;
    *aFactory = NULL;

    nsresult rv = aEntry->GetFactory(aFactory);
    if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("nsComponentManager: failed to load factory from %s (%s)\n",
                aEntry->location, aEntry->type));
        return rv;
    }
    	
    return NS_OK;
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass, PRBool checkRegistry)
{
    nsIDKey key(aClass);
    nsFactoryEntry *entry = (nsFactoryEntry*) mFactories->Get(&key);

    if (entry) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
	       ("\t\tfound %s as %p in factory cache.", entry->location,
                entry));
    } else {
#ifdef USE_REGISTRY
        if (checkRegistry)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("\t\tnot found in factory cache. Looking in registry"));

            nsresult rv = PlatformFind(aClass, &entry);

            // If we got one, cache it in our hashtable
            if (NS_SUCCEEDED(rv))
            {
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("\t\tfound in registry."));
                mFactories->Put(&key, entry);
            }
        }
#endif /* USE_REGISTRY */
    }

    return (entry);
}

/**
 * FindFactory()
 *
 * Given a classID, this finds the factory for this CID by first searching the
 * local CID<->factory mapping. Next it searches for a Dll that implements
 * this classID and calls LoadFactory() to create the factory.
 *
 * Again, no attempt is made at storing the factory.
 */
nsresult
nsComponentManagerImpl::FindFactory(const nsCID &aClass,
                                    nsIFactory **aFactory) 
{
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: FindFactory(%s)", buf);
        delete [] buf;
    }

    PR_ASSERT(aFactory != NULL);

    nsFactoryEntry *entry = GetFactoryEntry(aClass, PR_TRUE);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    return entry->GetFactory(aFactory);
}

/**
 * GetClassObject()
 *
 * Given a classID, this finds the singleton ClassObject that implements the CID.
 * Returns an interface of type aIID off the singleton classobject.
 */
nsresult
nsComponentManagerImpl::GetClassObject(const nsCID &aClass, const nsIID &aIID,
                                       void **aResult) 
{
    nsresult rv;

    nsCOMPtr<nsIFactory> factory;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", buf);
        delete [] buf;
    }

    PR_ASSERT(aResult != NULL);
    
    rv = FindFactory(aClass, getter_AddRefs(factory));
    if (NS_FAILED(rv)) return rv;

    rv = factory->QueryInterface(aIID, aResult);

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));
    	
    return rv;
}

/**
 * ProgIDToCLSID()
 *
 * Mapping function from a ProgID to a classID. Directly talks to the registry.
 *
 */
nsresult
nsComponentManagerImpl::ProgIDToCLSID(const char *aProgID, nsCID *aClass) 
{
    NS_PRECONDITION(aProgID != NULL, "null ptr");
    if (! aProgID)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aClass != NULL, "null ptr");
    if (! aClass)
        return NS_ERROR_NULL_POINTER;

    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef USE_REGISTRY
    // XXX This isn't quite the best way to do this: we should
    // probably move an nsArray<ProgID> into the FactoryEntry class,
    // and then have the construct/destructor of the factory entry
    // keep the ProgID to CID cache up-to-date. However, doing this
    // significantly improves performance, so it'll do for now.

    nsStringKey key(aProgID);
    nsCID* cid = (nsCID*) mProgIDs->Get(&key);
    if (cid) {
        if (cid == &kNoCID) {
            // we've already tried to map this ProgID to a CLSID, and found
            // that there _was_ no such mapping in the registry.
        }
        else {
            *aClass = *cid;
            res = NS_OK;
        }
    }
    else {
        // This is the first time someone has asked for this
        // ProgID. Go to the registry to find the CID.
        res = PlatformProgIDToCLSID(aProgID, aClass);

        if (NS_SUCCEEDED(res)) {
            // Found it. So put it into the cache.
            cid = new nsCID(*aClass);
            if (!cid)
                return NS_ERROR_OUT_OF_MEMORY;

            mProgIDs->Put(&key, cid);
        }
        else {
            // Didn't find it. Put a special CID in the cache so we
            // don't need to hit the registry on subsequent requests
            // for the same ProgID.
            mProgIDs->Put(&key, (void *)&kNoCID);
        }
    }
#endif /* USE_REGISTRY */

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) {
        char *buf = 0;
        if (NS_SUCCEEDED(res))
            buf = aClass->ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: ProgIDToCLSID(%s)->%s", aProgID,
                NS_SUCCEEDED(res) ? buf : "[FAILED]"));
        if (NS_SUCCEEDED(res))
            delete [] buf;
    }

    return res;
}

/**
 * CLSIDToProgID()
 *
 * Translates a classID to a {ProgID, Class Name}. Does direct registry
 * access to do the translation.
 *
 * XXX Would be nice to hook in a cache here too.
 */
nsresult
nsComponentManagerImpl::CLSIDToProgID(nsCID *aClass,
                                      char* *aClassName,
                                      char* *aProgID)
{
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef USE_REGISTRY
    res = PlatformCLSIDToProgID(aClass, aClassName, aProgID);
#endif /* USE_REGISTRY */

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass->ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: CLSIDToProgID(%s)->%s", buf,
                NS_SUCCEEDED(res) ? *aProgID : "[FAILED]"));
        delete [] buf;
    }

    return res;
}

/**
 * CreateInstance()
 *
 * Create an instance of an object that implements an interface and belongs
 * to the implementation aClass using the factory. The factory is immediately
 * released and not held onto for any longer.
 */
nsresult 
nsComponentManagerImpl::CreateInstance(const nsCID &aClass, 
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void **aResult)
{
#ifdef DEBUG_shaver
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        fprintf(stderr, "nsComponentManager: CreateInstance(%s)\n", buf);
        delete [] buf;
    }
#endif

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
        NS_RELEASE(factory);
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("\t\tFactory CreateInstance() %s.",
                NS_SUCCEEDED(res) ? "succeeded" : "FAILED"));
        return res;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("\t\tCreateInstance() FAILED."));
    return NS_ERROR_FACTORY_NOT_REGISTERED;
}

/**
 * CreateInstance()
 *
 * An overload of CreateInstance() that creates an instance of the object that
 * implements the interface aIID and whose implementation has a progID aProgID.
 *
 * This is only a convenience routine that turns around can calls the
 * CreateInstance() with classid and iid.
 *
 * XXX This is a function overload. We need to remove it.
 */
nsresult
nsComponentManagerImpl::CreateInstance(const char *aProgID,
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void **aResult)
{
    nsCID clsid;
    nsresult rv = ProgIDToCLSID(aProgID, &clsid);
    if (NS_FAILED(rv)) return rv; 
    return CreateInstance(clsid, aDelegate, aIID, aResult);
}


/**
 * RegisterFactory()
 *
 * Register a factory to be responsible for creation of implementation of
 * classID aClass. Plus creates as association of aClassName and aProgID
 * to the classID. If replace is PR_TRUE, we replace any existing registrations
 * with this one.
 *
 * Once registration is complete, we add the class to the factories cache
 * that we maintain. The factories cache is the ONLY place where these
 * registrations are ever kept.
 *
 * The other RegisterFunctions create a loader mapping and persistent
 * location, but we just slam it into the cache here.  And we don't call the
 * loader's OnRegister function, either.
 */
nsresult
nsComponentManagerImpl::RegisterFactory(const nsCID &aClass,
                                        const char *aClassName,
                                        const char *aProgID,
                                        nsIFactory *aFactory, 
                                        PRBool aReplace)
{
    nsFactoryEntry *entry = NULL;

    nsIDKey key(aClass);
    entry = (nsFactoryEntry *)mFactories->Get(&key);
    

    if (entry && !aReplace) {
        // Already registered
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

    nsFactoryEntry *newEntry = new nsFactoryEntry(aClass, aFactory);
    if (newEntry == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    if (entry) {                // aReplace implied by above check
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("\t\tdeleting old Factory Entry."));
        mFactories->RemoveAndDelete(&key);
        entry = NULL;
    }
    mFactories->Put(&key, newEntry);

    // Update the ProgID->CLSID Map
    if (aProgID) {
        nsresult rv = HashProgID(aProgID, aClass);
        if(NS_FAILED(rv)) {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tFactory register succeeded. "
                    "PROGID(%s)->CLSID mapping failed.", aProgID));
            return rv;
        }
    }
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register succeeded progid=%s.",
            aProgID ? aProgID : "<none>"));
    	
    return NS_OK;
}

/* Create a spec then hand off to spec version */
nsresult
nsComponentManagerImpl::RegisterComponent(const nsCID &aClass,
                                          const char *aClassName,
                                          const char *aProgID,
                                          const char *aPersistentDescriptor,
                                          PRBool aReplace,
                                          PRBool aPersist)
{
    char *registryName = nsCRT::strdup(aPersistentDescriptor);
    if (!registryName)
        return NS_ERROR_OUT_OF_MEMORY;
    return RegisterComponentCommon(aClass, aClassName, aProgID, registryName,
                                   aReplace, aPersist, nativeComponentType);
}


/*
 * Register a component, using whatever they stuck in the FileSpec.
 */
nsresult
nsComponentManagerImpl::RegisterComponentSpec(const nsCID &aClass,
                                              const char *aClassName,
                                              const char *aProgID,
                                              nsIFileSpec *aLibrarySpec,
                                              PRBool aReplace,
                                              PRBool aPersist)
{
    nsresult rv = NS_OK;
    char *registryName;

    rv = mNativeComponentLoader->RegistryNameForSpec(aLibrarySpec,
                                                     &registryName);
    if (NS_FAILED(rv))
        return rv;

    return RegisterComponentCommon(aClass, aClassName, aProgID, registryName,
                                   aReplace, aPersist,
                                   nativeComponentType);
}

/*
 * Register a ``library'', which is a DLL location named by a simple filename
 * such as ``libnsappshell.so'', rather than a relative or absolute path.
 *
 * It implies application/x-moz-dll as the component type, and skips the
 * FindLoaderForType phase.
 */
nsresult
nsComponentManagerImpl::RegisterComponentLib(const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aProgID,
                                             const char *aDllName,
                                             PRBool aReplace,
                                             PRBool aPersist)
{
    char *registryName;
    nsresult rv = mNativeComponentLoader->RegistryNameForLib(aDllName,
                                                             &registryName);
    if (NS_FAILED(rv))
        return rv;
    return RegisterComponentCommon(aClass, aClassName, aProgID, registryName,
                                   aReplace, aPersist, nativeComponentType);
}

/*
 * Add a component to the known universe of components.

 * Once we enter this function, we own aRegistryName, and must free it
 * or hand it to nsFactoryEntry.  Common exit point ``out'' helps keep us
 * sane.
 */
nsresult
nsComponentManagerImpl::RegisterComponentCommon(const nsCID &aClass,
                                                const char *aClassName,
                                                const char *aProgID,
                                                char *aRegistryName,
                                                PRBool aReplace,
                                                PRBool aPersist,
                                                const char *aType)
{
    nsresult rv = NS_OK;
    nsFactoryEntry* newEntry = nsnull;

    nsIDKey key(aClass);
    nsFactoryEntry *entry = GetFactoryEntry(aClass, PR_TRUE);
    nsCOMPtr<nsIComponentLoader> loader;
    PRBool sanity;

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
           ("RegisterComponentCommon: %s %s %s %s",
            aClassName ? aClassName : "(null)",
            aProgID ? aProgID : "(null)",
            aType, aRegistryName));

    if (entry && !aReplace) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        rv = NS_ERROR_FACTORY_EXISTS;
        goto out;
    }


#ifdef USE_REGISTRY
    if (aPersist) {
        /* Add to the registry */
        rv = AddComponentToRegistry(aClass, aClassName, aProgID,
                                    aRegistryName, aType);
        if (NS_FAILED(rv)) {
	    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
		   ("\t\tadding %s %s to reg failed", aClassName, aProgID));
            goto out;
	}
    }
#endif

    rv = GetLoaderForType(aType, getter_AddRefs(loader));
    if (NS_FAILED(rv)) {
	PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
	       ("\t\tcouldn't get loader for %s\n", aType));
        goto out;
    }

    newEntry = new nsFactoryEntry(aClass, aRegistryName,
                                  nsCRT::strdup(aType),
                                  loader);
    if (!newEntry) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto out;
    }

    aRegistryName = nsnull;     // handed to nsFactoryEntry, so don't free

    if (entry) {                // aReplace implicit from test above
	delete entry;
    }

    /* unless the fabric of the universe bends, we'll get entry back */
    sanity = (entry == mFactories->Put(&key, newEntry));
    PR_ASSERT(sanity);
    /* don't try to clean up, just drop everything and run */
    if (!sanity)
	return NS_ERROR_FACTORY_NOT_REGISTERED;

    /* we've put the new entry in the hash table, so don't delete on error */
    newEntry = nsnull;
 
   // Update the ProgID->CLSID Map
    if (aProgID
#ifdef USE_REGISTRY
        && !aPersist
#endif
        ) {
        rv = HashProgID(aProgID, aClass);
        if (NS_FAILED(rv)) {
	    char *cidString = aClass.ToString();
	    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
		   ("\t\tHashProgID(%s,%s) failed\n", cidString, aProgID));
	    delete [] cidString;
            goto out;
	}
    }

    // Let the loader do magic things now
    rv = loader->OnRegister(aClass, aType, aClassName, aProgID, aRegistryName,
                            aReplace, aPersist);
    if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("loader->OnRegister failed for %s \"%s\" %s %s", aType,
                aClassName, aProgID, aRegistryName));
        goto out;
    }
    
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register %s progID=%s.",
            NS_SUCCEEDED(rv) ? "succeeded" : "failed",
            aProgID ? aProgID : "<none>"));

 out:
    if (NS_FAILED(rv)) {
        if (aRegistryName)
            nsAllocator::Free(aRegistryName);
        if (newEntry)
            delete newEntry;
    }
    return rv;
}

nsresult
nsComponentManagerImpl::GetLoaderForType(const char *aType,
                                         nsIComponentLoader **aLoader)
{
    nsStringKey typeKey(aType);
    nsIComponentLoader *loader;
    nsresult rv;

    loader = (nsIComponentLoader *)mLoaders->Get(&typeKey);
    if (loader) {
	// nsSupportsHashtable does the AddRef
	*aLoader = loader;
	return NS_OK;
    }

    nsIRegistry::Key loaderKey;
    rv = mRegistry->GetSubtreeRaw(mLoadersKey, aType, &loaderKey);
    if (NS_FAILED(rv))
        return rv;
    
    char *progID;
    rv = mRegistry->GetString(loaderKey, progIDValueName, &progID);
    if (NS_FAILED(rv))
        return rv;

#ifdef DEBUG_shaver
    fprintf(stderr, "nCMI: constructing loader for type %s = %s\n", aType, progID);
#endif

    rv = CreateInstance(progID, nsnull, NS_GET_IID(nsIComponentLoader),	(void **)&loader);
    PR_FREEIF(progID);

    if (NS_SUCCEEDED(rv)) {
	mLoaders->Put(&typeKey, loader);
	*aLoader = loader;
    }
    return rv;
}

nsresult
nsComponentManagerImpl::RegisterComponentLoader(const char *aType, const char *aProgID,
                                                PRBool aReplace)
{
    nsIRegistry::Key loaderKey;
    nsresult rv = mRegistry->AddSubtreeRaw(mLoadersKey, aType, &loaderKey);
    if (NS_FAILED(rv))
        return rv;

    /* XXX honour aReplace */
    
    rv = mRegistry->SetString(loaderKey, progIDValueName, aProgID);

#ifdef DEBUG_shaver
    fprintf(stderr, "nNCI: registered %s as component loader for %s\n",
            aProgID, aType);
#endif
    return rv;
}

nsresult
nsComponentManagerImpl::AddComponentToRegistry(const nsCID &aClass,
                                               const char *aClassName,
                                               const char *aProgID,
                                               const char *aRegistryName,
                                               const char *aType)
{
    nsresult rv;
    nsIRegistry::Key IDKey;
    int32 nComponents = 0;
    
    /* so why do we use strings here rather than writing bytes, anyway? */
    char *cidString = aClass.ToString();
    if (!cidString)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mRegistry->AddSubtreeRaw(mCLSIDKey, cidString, &IDKey);
    if (NS_FAILED(rv))
        goto out;
    
    if (aClassName) {
        rv = mRegistry->SetString(IDKey, classNameValueName, aClassName);
        if (NS_FAILED(rv))
            goto out;
    }

    rv = mRegistry->SetString(IDKey, inprocServerValueName, aRegistryName);
    if (NS_FAILED(rv))
        goto out;

    rv = mRegistry->SetString(IDKey, componentTypeValueName, aType);
    if (NS_FAILED(rv))
        goto out;

    if (aProgID) {
        rv = mRegistry->SetString(IDKey, progIDValueName, aProgID);
        if (NS_FAILED(rv))
            goto out;

        nsIRegistry::Key progIDKey;
        rv = mRegistry->AddSubtreeRaw(mClassesKey, aProgID, &progIDKey);
        if (NS_FAILED(rv))
            goto out;
        rv = mRegistry->SetString(progIDKey, classIDValueName, cidString);
        if (NS_FAILED(rv))
            goto out;
    }

    nsIRegistry::Key compKey;
    rv = mRegistry->AddSubtreeRaw(mXPCOMKey, aRegistryName, &compKey);
    
    // update component count
    rv = mRegistry->GetInt(compKey, componentCountValueName, &nComponents);
    nComponents++;
    rv = mRegistry->SetInt(compKey, componentCountValueName, nComponents);
    if (NS_FAILED(rv))
        goto out;

 out:
    // XXX if failed, undo registry adds or set invalid bit?  How?
    delete [] cidString;    // XXX nsAllocator?
    return rv;
}

nsresult
nsComponentManagerImpl::UnregisterFactory(const nsCID &aClass,
                                          nsIFactory *aFactory)
{
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: Unregistering Factory.");
        PR_LogPrint("nsComponentManager: + %s.", buf);
        delete [] buf;
    }
    	
    nsIDKey key(aClass);
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
    nsFactoryEntry *old = (nsFactoryEntry *) mFactories->Get(&key);
    if (old != NULL)
    {
        if (old->factory.get() == aFactory)
        {
            PR_EnterMonitor(mMon);
            old = (nsFactoryEntry *) mFactories->RemoveAndDelete(&key);
            old = NULL;
            PR_ExitMonitor(mMon);
            res = NS_OK;
        }

    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: ! Factory unregister %s.", 
            NS_SUCCEEDED(res) ? "succeeded" : "failed"));
    	
    return res;
}

nsresult
nsComponentManagerImpl::UnregisterComponent(const nsCID &aClass,
                                            const char *aLibrary)
{
    nsresult rv;
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: UnregisterComponentSpec(%s, %s)", buf,
                    aLibrary);
        delete [] buf;
    }

    // Convert the persistent descriptor into a nsIFileSpec
    nsCOMPtr<nsIFileSpec>libSpec;
    NS_DEFINE_IID(kFileSpecIID, NS_IFILESPEC_IID);
    rv = CreateInstance(NS_FILESPEC_PROGID, NULL, kFileSpecIID, getter_AddRefs(libSpec));
    if (NS_FAILED(rv)) return rv;
    rv = libSpec->SetPersistentDescriptorString((char *)aLibrary);
    if (NS_FAILED(rv)) return rv;

    return UnregisterComponentSpec(aClass, libSpec);
}

nsresult
nsComponentManagerImpl::UnregisterComponentSpec(const nsCID &aClass,
                                                nsIFileSpec *aLibrarySpec)
{
    char *aLibrary;
    nsresult rv = aLibrarySpec->GetPersistentDescriptorString(&aLibrary);
    if (NS_FAILED(rv))
        return NS_ERROR_INVALID_ARG;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: UnregisterComponentSpec(%s, %s)", buf,
                    aLibrary);
        delete [] buf;
    }
    	
    nsIDKey key(aClass);
    nsFactoryEntry *old = (nsFactoryEntry *) mFactories->Get(&key);
    	
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
    	
    PR_EnterMonitor(mMon);
    	
    if (old != NULL)
    {
#if 0 /* use nsFactoryEntry->location */
        if (old->dll->GetPersistentDescriptorString() != NULL &&
#if defined(XP_UNIX) || defined(XP_BEOS)
            PL_strcasecmp(old->dll->GetPersistentDescriptorString(), aLibrary)
#else
            PL_strcmp(old->dll->GetPersistentDescriptorString(), aLibrary)
#endif
            )
        {
            mFactories->RemoveAndDelete(&key);
            old = NULL;
            res = NS_OK;
        }
#ifdef USE_REGISTRY
        char *cidString = aClass.ToString();
        res = PlatformUnregister(cidString, aLibrary);
        delete [] cidString;
#endif
#endif
    }
    	
    PR_ExitMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: ! Factory unregister %s.", 
            NS_SUCCEEDED(res) ? "succeeded" : "failed"));
    	
    return res;
}

nsresult
nsComponentManagerImpl::FreeLibraries(void) 
{
    nsIServiceManager* serviceMgr = NULL;
    nsresult rv = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(rv)) return rv;
    rv = UnloadLibraries(serviceMgr);
    return rv;
}

// Private implementation of unloading libraries
nsresult
nsComponentManagerImpl::UnloadLibraries(nsIServiceManager *serviceMgr)
{
    PR_EnterMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: Unloading Libraries."));

    PR_ExitMonitor(mMon);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * AutoRegister(RegistrationInstant, const char *directory)
 *
 * Given a directory in the following format, this will ensure proper registration
 * of all components. No default director is looked at.
 *
 *    Directory and fullname are what NSPR will accept. For eg.
 *     	WIN	y:/home/dp/mozilla/dist/bin
 *  	UNIX	/home/dp/mozilla/dist/bin
 *  	MAC	/Hard drive/mozilla/dist/apprunner
 *
 * This will take care not loading already registered dlls, finding and
 * registering new dlls, re-registration of modified dlls
 *
 */

struct AutoReg_closure {
    int when;
    nsIFileSpec *spec;
    nsresult status;   // this is a hack around Enumerate's void return
    nsIComponentLoader *native;
};

static PRBool
AutoRegister_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    nsIComponentLoader *loader = (nsIComponentLoader *)aData;
    struct AutoReg_closure *closure =
	(struct AutoReg_closure *)aClosure;

    if (loader == closure->native) {
#ifdef DEBUG_shaver
	fprintf(stderr, "AutoRegister_enumerate: skipping native\n");
#endif
	return PR_TRUE;
    }

    PR_ASSERT(NS_SUCCEEDED(closure->status));

    closure->status = loader->AutoRegisterComponents(closure->when, closure->spec);
    if (NS_FAILED(closure->status))
	return PR_FALSE;
    return PR_TRUE;
}

nsresult
nsComponentManagerImpl::AutoRegister(RegistrationTime when, nsIFileSpec *inDirSpec)
{
    nsCOMPtr<nsIFileSpec> dir;
    nsresult rv;

    if (inDirSpec) {
	dir = inDirSpec;
    } else {
        // Do default components directory
        nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::XPCOM_CurrentProcessComponentDirectory);
        rv = NS_NewFileSpecWithSpec(sysdir, getter_AddRefs(dir));
        if (NS_FAILED(rv)) 
	    return rv; // XXX translate error code?
    }

    /* do the native loader first, so we can find other loaders */
    rv = mNativeComponentLoader->AutoRegisterComponents((PRInt32)when, dir);
    if (NS_FAILED(rv))
	return rv;

    /* XXX eagerly instantiate all known loaders */
    nsCOMPtr<nsIEnumerator> loaderEnum;
    rv = mRegistry->EnumerateSubtrees(mLoadersKey, getter_AddRefs(loaderEnum));
    if (NS_FAILED(rv))
        return rv;

    rv = loaderEnum->First();
    if (NS_FAILED(rv))
        return rv;

    for (; NS_SUCCEEDED(rv) && !loaderEnum->IsDone();
         (rv = loaderEnum->Next())) {
        nsCOMPtr<nsISupports> base;
        rv = loaderEnum->CurrentItem(getter_AddRefs(base));
        if (NS_FAILED(rv))
            return rv;

        // Narrow
        nsCOMPtr<nsIRegistryNode> node;
        node = do_QueryInterface(base, &rv);
        if (NS_FAILED(rv))
            continue;

        char *type;
        rv = node->GetName(&type);
        if (NS_FAILED(rv))
            continue;
        autoStringFree del_type(type, autoStringFree::nsCRT_String_Delete);
        
        nsStringKey typeKey(type);
        nsCOMPtr<nsIComponentLoader> loader =
            (nsIComponentLoader *)mLoaders->Get(&typeKey);
        if (loader.get())
            continue;
        
#ifdef DEBUG_shaver
        fprintf(stderr, "nCMI: creating %s loader for enumeration\n",
                type);
#endif
        /* this will get it added to the hashtable and stuff */
        GetLoaderForType(type, getter_AddRefs(loader));
        continue;
    }

    /* iterate over all known loaders and ask them to autoregister. */
    struct AutoReg_closure closure;
    /* XXX convert when to nsIComponentLoader::(when) properly */
    closure.when = when;
    closure.spec = dir.get();
    closure.status = NS_OK;
    closure.native = mNativeComponentLoader; // prevent duplicate autoreg
    
    mLoaders->Enumerate(AutoRegister_enumerate, &closure);
    return closure.status;
}

static PRBool
AutoRegisterComponent_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    PRBool didRegister;
    nsIComponentLoader *loader = (nsIComponentLoader *)aData;
    struct AutoReg_closure *closure =
	(struct AutoReg_closure *)aData;

    closure->status = loader->AutoRegisterComponent(closure->when,
						    closure->spec,
						    &didRegister);
    
    if (NS_SUCCEEDED(closure->status) && didRegister)
	return PR_FALSE;
    if (didRegister)
	return PR_TRUE;
    return PR_FALSE;
}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(RegistrationTime when,
                                              nsIFileSpec *component)
{
    struct AutoReg_closure closure;

    /* XXX convert when to nsIComponentLoader::(when) properly */
    closure.when = (PRInt32)when;
    closure.spec = component;
    closure.status = NS_OK;

    /*
     * Do we have to give the native loader first crack at it?
     * I vote ``no''.
     */
    mLoaders->Enumerate(AutoRegisterComponent_enumerate, &closure);
    return NS_FAILED(closure.status) 
	? NS_ERROR_FACTORY_NOT_REGISTERED : NS_OK;

}

nsresult
nsComponentManagerImpl::IsRegistered(const nsCID &aClass,
                                     PRBool *aRegistered)
{
    if(!aRegistered)
    {
        NS_ASSERTION(0, "null ptr");
        return NS_ERROR_NULL_POINTER;
    }
    *aRegistered = (nsnull != GetFactoryEntry(aClass, PR_TRUE));
    return NS_OK;
}

static NS_IMETHODIMP
ConvertFactoryEntryToCID(nsHashKey *key, void *data, void *convert_data,
                         nsISupports **retval)
{
    nsComponentManagerImpl *compMgr = (nsComponentManagerImpl*) convert_data;
    nsresult rv;

    nsISupportsID* cidHolder;

    if(NS_SUCCEEDED(rv = compMgr->CreateInstance(NS_SUPPORTS_ID_PROGID, nsnull, 
                                                 NS_GET_IID(nsISupportsID),
                                                 (void **)&cidHolder)))
    {
        nsFactoryEntry *fe = (nsFactoryEntry *) data;
        cidHolder->SetData(&fe->cid);
        *retval = cidHolder;
    }
    else
        *retval = nsnull;

    return rv;
}

static NS_IMETHODIMP
ConvertProgIDKeyToString(nsHashKey *key, void *data, void *convert_data,
                         nsISupports **retval)
{
    nsComponentManagerImpl *compMgr = (nsComponentManagerImpl*) convert_data;
    nsresult rv;

    nsISupportsString* strHolder;

    if(NS_SUCCEEDED(rv = compMgr->CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                                 NS_GET_IID(nsISupportsString),
                                                 (void **)&strHolder)))
    {
        nsStringKey *strKey = (nsStringKey *) key;
        const nsString& str = strKey->GetString();
        char* yetAnotherCopyOfTheString = str.ToNewCString();
        if(yetAnotherCopyOfTheString)
        {
            strHolder->SetData(yetAnotherCopyOfTheString);
            delete [] yetAnotherCopyOfTheString;
        }
        *retval = strHolder;
    }
    else
        *retval = nsnull;

    return rv;
}

nsresult
nsComponentManagerImpl::EnumerateCLSIDs(nsIEnumerator** aEmumerator)
{
    if(!aEmumerator)
    {
        NS_ASSERTION(0, "null ptr");
        return NS_ERROR_NULL_POINTER;
    }
    *aEmumerator = nsnull;

    nsresult rv;
    if(!mPrePopulationDone)
    {
        rv = PlatformPrePopulateRegistry();
        if(NS_FAILED(rv))
            return rv;
    }

    return NS_NewHashtableEnumerator(mFactories, ConvertFactoryEntryToCID,
                                     this, aEmumerator);
}

nsresult
nsComponentManagerImpl::EnumerateProgIDs(nsIEnumerator** aEmumerator)
{
    if(!aEmumerator)
    {
        NS_ASSERTION(0, "null ptr");
        return NS_ERROR_NULL_POINTER;
    }

    *aEmumerator = nsnull;

    nsresult rv;
    if(!mPrePopulationDone)
    {
        rv = PlatformPrePopulateRegistry();
        if(NS_FAILED(rv))
            return rv;
    }

    return NS_NewHashtableEnumerator(mProgIDs, ConvertProgIDKeyToString,
                                     this, aEmumerator);
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_GetGlobalComponentManager(nsIComponentManager* *result)
{
    nsresult rv = NS_OK;

    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        // XPCOM needs initialization.
        rv = NS_InitXPCOM(NULL);
    }

    if (NS_SUCCEEDED(rv))
    {
        // NO ADDREF since this is never intended to be released.
        *result = nsComponentManagerImpl::gComponentManager;
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
