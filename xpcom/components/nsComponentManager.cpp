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
const char xpcomKeyName[] ="Software/Netscape/XPCOM";
const char netscapeKeyName[]="Software/Netscape";
const char classesKeyName[]="Classes";
const char classIDKeyName[]="CLSID";
const char classesClassIDKeyName[]="Classes/CLSID";


// Common Value Names
const char classIDValueName[]="CLSID";
const char versionValueName[]="VersionString";
const char lastModValueName[]="LastModTimeStamp";
const char fileSizeValueName[]="FileSize";
const char componentCountValueName[]="ComponentsCount";
const char progIDValueName[]="ProgID";
const char classNameValueName[]="ClassName";
const char inprocServerValueName[]="InprocServer";

// We define a CID that is used to indicate the non-existence of a
// progid in the hash table.
#define NS_NO_CID { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }
    static NS_DEFINE_CID(kNoCID, NS_NO_CID);

// Build is using USE_NSREG to turn off xpcom using registry
// but internally we use USE_REGISTRY. Map them propertly.
#ifdef USE_NSREG
#define USE_REGISTRY
#endif /* USE_NSREG */


////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass, nsDll *aDll)
    : cid(aClass), factory(NULL), dll(aDll)
{
}

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory)
    : cid(aClass), factory(aFactory), dll(NULL)

{
    NS_ADDREF(aFactory);
}

nsFactoryEntry::~nsFactoryEntry(void)
{
    if (factory != NULL) {
        NS_RELEASE(factory);
    }
    // DO NOT DELETE nsDll *dll;
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
    : mFactories(NULL), mProgIDs(NULL), mMon(NULL), mDllStore(NULL), 
      mRegistry(NULL), mPrePopulationDone(PR_FALSE)
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
    delete entry;
    return PR_TRUE;
}

PRBool
nsDll_Destroy(nsHashKey *aKey, void *aData, void* closure)
{
    nsDll* entry = NS_STATIC_CAST(nsDll*, aData);
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
    if (mDllStore == NULL) {
        mDllStore = new nsObjectHashtable(nsnull, nsnull,      // should never be copied
                                          nsDll_Destroy, nsnull,
                                          256, /* Thead Safe */ PR_TRUE);
        if (mDllStore == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
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

    // Release dll abstraction storage
    if (mDllStore)
        delete mDllStore;

#ifdef USE_REGISTRY
    // Release registry
    if(mRegistry)
        NS_RELEASE(mRegistry);
#endif /* USE_REGISTRY */
    
    // Destroy the Lock
    if (mMon)
        PR_DestroyMonitor(mMon);

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Destroyed."));

}

NS_IMPL_ISUPPORTS(nsComponentManagerImpl, nsIComponentManager::GetIID());

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
        nsIRegistry::Key netscapeKey;
        rv = mRegistry->GetSubtree(nsIRegistry::Common,netscapeKeyName, &netscapeKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Get Subtree (%s)",netscapeKeyName));         
        }
        else
        {
            rv = mRegistry->RemoveSubtreeRaw(netscapeKey, xpcomBaseName);
            if(NS_FAILED(rv))
            {
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsComponentManager: Failed To Nuke Subtree (%s)",xpcomKeyName));
                return rv;
            }
        }

        rv = mRegistry->GetSubtree(nsIRegistry::Common,classesKeyName, &netscapeKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Get Subtree (%s)",classesKeyName));
        }
        else
        {
            rv = mRegistry->RemoveSubtreeRaw(netscapeKey, classIDKeyName);
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

    return NS_OK;
}


void
nsComponentManagerImpl::PlatformGetFileInfo(nsIRegistry::Key key, PRUint32 *lastModifiedTime, PRUint32 *fileSize)
{
    PR_ASSERT(lastModifiedTime);
    PR_ASSERT(fileSize);

    nsresult rv;
    int32 lastMod;
    rv = mRegistry->GetInt(key, lastModValueName, &lastMod);
    if (NS_SUCCEEDED(rv))
    {
        *lastModifiedTime = lastMod;
    }
    	
    int32 fsize = 0;
    rv = mRegistry->GetInt(key, fileSizeValueName, &fsize);
    if (NS_SUCCEEDED(rv))
    {
        *fileSize = fsize;
    }
}

void
nsComponentManagerImpl::PlatformSetFileInfo(nsIRegistry::Key key, PRUint32 lastModifiedTime, PRUint32 fileSize)
{
    mRegistry->SetInt(key, lastModValueName, lastModifiedTime);
    mRegistry->SetInt(key, fileSizeValueName, fileSize);
}

/**
 * PlatformCreateDll(const char *fullname)
 *
 * Creates a nsDll from the registry representation of dll 'fullname'.
 * Looks under
 *		ROOTKEY_COMMON/Software/Netscape/XPCOM/fullname
 */
nsresult
nsComponentManagerImpl::PlatformCreateDll(const char *fullname, nsDll* *result)
{
    PR_ASSERT(mRegistry!=NULL);

    nsresult rv;

    nsIRegistry::Key fullnameKey;

    rv = mRegistry->GetSubtreeRaw(mXPCOMKey,fullname, &fullnameKey);
    if(NS_FAILED(rv))
    {        
        return rv;
    }
    	
    PRUint32 lastModTime = 0;
    PRUint32 fileSize = 0;
    PlatformGetFileInfo(fullnameKey, &lastModTime, &fileSize);
    	
    nsDll *dll = new nsDll(fullname, lastModTime, fileSize);
    if (dll == NULL)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    *result = dll;
    return NS_OK;
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

  // XXX PlatformRegister needs to deal with dllName type dlls
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

    // Get the library name, modifiedtime and size
    PRUint32 lastModTime = 0;
    PRUint32 fileSize = 0;

    nsIRegistry::Key key;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, library, &key);
    if (NS_SUCCEEDED(rv))
    {
        PlatformGetFileInfo(key, &lastModTime, &fileSize);			
    }

    nsDll *dll = CreateCachedDll(library, lastModTime, fileSize);
    if (dll == NULL) return NS_ERROR_OUT_OF_MEMORY;
    nsFactoryEntry *res = new nsFactoryEntry(aCID, dll);
    if (res == NULL) return NS_ERROR_OUT_OF_MEMORY;

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
    // Read in all dll entries and populate the mDllStore
    nsCOMPtr<nsIEnumerator> dllEnum;
    nsresult rv = mRegistry->EnumerateSubtrees( mXPCOMKey, getter_AddRefs(dllEnum));
    if (NS_FAILED(rv)) return rv;

    rv = dllEnum->First();
    for (; NS_SUCCEEDED(rv) && !dllEnum->IsDone(); (rv = dllEnum->Next()))
    {
        nsCOMPtr<nsISupports> base;
        rv = dllEnum->CurrentItem(getter_AddRefs(base));
        if (NS_FAILED(rv))  continue;

        // Get specific interface.
        nsIID nodeIID = NS_IREGISTRYNODE_IID;
        nsCOMPtr<nsIRegistryNode> node;
        rv = base->QueryInterface( nodeIID, getter_AddRefs(node) );
        if (NS_FAILED(rv)) continue;

        // Get library name
        char *library = NULL;
        rv = node->GetName(&library);
        if (NS_FAILED(rv)) continue;
        // XXX make sure the following will work
        autoStringFree delete_library(library, autoStringFree::nsCRT_String_Delete);

        // Get key associated with library
        nsIRegistry::Key libKey;
        rv = node->GetKey(&libKey);
        if (NS_FAILED(rv)) continue;

        // Create nsDll with this name
        PRUint32 lastModTime = 0;
        PRUint32 fileSize = 0;
        PlatformGetFileInfo(libKey, &lastModTime, &fileSize);

        //        printf("Populating dll: %s, %d, %d\n", library, lastModTime, fileSize);

        nsDll *dll = CreateCachedDll(library, lastModTime, fileSize);
        if (dll == NULL)
        {
            rv = NS_ERROR_OUT_OF_MEMORY;
            continue;
        }
    }

    // Then read in all CID entries and populate the mFactories
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
        nsIID nodeIID = NS_IREGISTRYNODE_IID;
        nsCOMPtr<nsIRegistryNode> node;
        rv = base->QueryInterface( nodeIID, getter_AddRefs(node) );
        if (NS_FAILED(rv)) continue;

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

        nsDll *dll = CreateCachedDll(library, 0, 0);
        if (!dll) continue;
        nsFactoryEntry* entry = new nsFactoryEntry(aClass, dll);
        if (!entry) continue;

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


nsDll* nsComponentManagerImpl::CreateCachedDll(const char *persistentDescriptor,
                                               PRUint32 modDate, PRUint32 fileSize)
{
    // Check our dllCollection for a dll with matching name
    nsStringKey key(persistentDescriptor);
    nsDll *dll = (nsDll *) mDllStore->Get(&key);
    
    if (dll == NULL)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: New dll \"%s\".", persistentDescriptor));

        // Add a new Dll into the nsDllStore
        dll = new nsDll(persistentDescriptor, modDate, fileSize);
        if (dll == NULL) return NULL;
        if (dll->GetStatus() != DLL_OK)
        {
            // Cant create a nsDll. Backoff.
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: ERROR in creating nsDll from \"%s\".", persistentDescriptor));
            delete dll;
            dll = NULL;
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Adding New dll \"%s\" to mDllStore.",
                    persistentDescriptor));

            mDllStore->Put(&key, (void *)dll);
        }
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Found in mDllStore \"%s\".", persistentDescriptor));
        // XXX We found the dll in the dllCollection.
        // XXX Consistency check: dll needs to have the same
        // XXX lastModTime and fileSize. If not that would mean
        // XXX that the dll wasn't registered properly.
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
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: LoadFactory() \"%s\".", aEntry->dll->GetNativePath()));
    if (aFactory == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aFactory = NULL;

    // LoadFactory() cannot be called for entries that are CID<->factory
    // mapping entries for the session.
    PR_ASSERT(aEntry->dll != NULL);
    	
    if (aEntry->dll->IsLoaded() == PR_FALSE)
    {
        // Load the dll
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsComponentManager: + Loading \"%s\".", aEntry->dll->GetNativePath()));
        if (aEntry->dll->Load() == PR_FALSE)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("nsComponentManager: Library load unsuccessful."));
            
            char errorMsg[1024] = "Cannot get error from nspr. Not enough memory.";
            if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
                PR_GetErrorText(errorMsg);
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Load(%s) FAILED with error:%s", aEntry->dll->GetNativePath(), errorMsg));

#ifdef PRINT_CRITICAL_ERROR_TO_SCREEN
            // Put the error message on the screen.
            printf("**************************************************\n"
    		   "nsComponentManager: Load(%s) FAILED with error: %s\n"
            	   "**************************************************\n",
    		   aEntry->dll->GetNativePath(), errorMsg);
#endif
            return NS_ERROR_FAILURE;
        }
    }
    	
#ifdef MOZ_TRACE_XPCOM_REFCNT
    // Inform refcnt tracer of new library so that calls through the
    // new library can be traced.
    nsTraceRefcnt::LoadLibrarySymbols(aEntry->dll->GetNativePath(), aEntry->dll->GetInstance());
#endif
    nsIServiceManager* serviceMgr = NULL;
    nsresult rv = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(rv)) return rv;

    // Create the module object and get the factory
    nsCOMPtr<nsIModule> mobj;
    rv = aEntry->dll->GetModule(gComponentManager, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsISupports> classObject;
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
               ("nsComponentManager: %s using nsIModule to get factory.",
                aEntry->dll->GetNativePath()));
        rv = mobj->GetClassObject(gComponentManager, aEntry->cid, getter_AddRefs(classObject));
        if (NS_SUCCEEDED(rv))
        {
            rv = classObject->QueryInterface(nsIFactory::GetIID(), (void **)aFactory);
        }
        return rv;
    }

#ifndef OBSOLETE_MODULE_LOADING
    // Try the older OBSOLETE method
    nsFactoryProc proc = (nsFactoryProc) aEntry->dll->FindSymbol("NSGetFactory");
    if (proc != NULL)
    {
        char* className = NULL;
        char* progID = NULL;

        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("nsComponentManager: %s using OBSOLETE method NSGetFactory().", aEntry->dll->GetNativePath()));

        // XXX dp, warren: deal with this!
#if 0
        rv = CLSIDToProgID(&aEntry->cid, &className, &progID);
        // if CLSIDToProgID fails, just pass null to NSGetFactory
#endif

        rv = proc(serviceMgr, aEntry->cid, className, progID, aFactory);
        if (NS_FAILED(rv)) return rv;

        if (className)
            delete[] className;
        if (progID)
            delete[] progID;
        return rv;
    }
#endif /* OBSOLETE_MODULE_LOADING */

    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
           ("nsComponentManager: Module entrypoint not found."));
    return NS_ERROR_FACTORY_NOT_LOADED;
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass, PRBool checkRegistry)
{
    nsIDKey key(aClass);
    nsFactoryEntry *entry = (nsFactoryEntry*) mFactories->Get(&key);

    if (entry)
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("\t\tfound in factory cache."));
    else
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

    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;    	
    	
    if (entry)
    {
        if (entry->factory == NULL)
        {
            res = LoadFactory(entry, aFactory);
            // XXX Cache factory that we created for performance.
            // XXX Need a way to release this factory else dlls will never
            // XXX get unloaded
            if (NS_SUCCEEDED(res))
            {
                entry->factory = *aFactory;
                NS_ADDREF(entry->factory);
            }
        }
        else
        {
            *aFactory = entry->factory;
            NS_ADDREF(*aFactory);
            res = NS_OK;
        }
    }
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFindFactory() %s", NS_SUCCEEDED(res) ? "succeeded" : "FAILED"));
    	
    return res;
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
            mProgIDs->Put(&key, (void*) &kNoCID);
        }
    }
#endif /* USE_REGISTRY */

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) {
        char *buf;
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
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: CreateInstance(%s)", buf);
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
 * XXX This uses FindFactory() to test if a factory already exists. This
 * XXX has the bad side effect of loading the factory if the previous
 * XXX registration was a dll for this class. We might be able to do away
 * XXX with such a load.
 */
nsresult
nsComponentManagerImpl::RegisterFactory(const nsCID &aClass,
                                        const char *aClassName,
                                        const char *aProgID,
                                        nsIFactory *aFactory, 
                                        PRBool aReplace)
{
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: RegisterFactory(%s, %s, %s), replace = %d.",
                    buf, aClassName, aProgID, (int)aReplace);
        delete [] buf;
    }

    nsFactoryEntry *entry = NULL;
    nsIDKey key(aClass);
    entry = GetFactoryEntry(aClass, PR_TRUE);

    if (entry && !aReplace)
    {
        // Already registered
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

    nsFactoryEntry *newEntry = new nsFactoryEntry(aClass, aFactory);
    if (newEntry == NULL) return NS_ERROR_OUT_OF_MEMORY;
    if (entry)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tdeleting old Factory Entry."));
        mFactories->RemoveAndDelete(&key);
        entry = NULL;
    }
    mFactories->Put(&key, newEntry);

    // Update the ProgID->CLSID Map
    if (aProgID)
    {
        nsresult rv = HashProgID(aProgID, aClass);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tFactory register succeeded. PROGID->CLSID mapping failed."));
            return (rv);
        }
    }
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tFactory register succeeded."));
    	
    return NS_OK;
}

nsresult
nsComponentManagerImpl::RegisterComponent(const nsCID &aClass,
                                          const char *aClassName,
                                          const char *aProgID,
                                          const char *aLibraryPersistentDescriptor,
                                          PRBool aReplace,
                                          PRBool aPersist)
{
    nsresult rv = NS_OK;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: RegisterComponent(%s, %s, %s, %s), replace = %d, persist = %d.",
                    buf, aClassName, aProgID, aLibraryPersistentDescriptor, (int)aReplace, (int)aPersist);
        delete [] buf;
    }

    // Convert the persistent descriptor into a nsIFileSpec
    nsCOMPtr<nsIFileSpec>libSpec;
    NS_DEFINE_IID(kFileSpecIID, NS_IFILESPEC_IID);
    rv = CreateInstance(NS_FILESPEC_PROGID, NULL, kFileSpecIID, getter_AddRefs(libSpec));
    if (NS_FAILED(rv)) return rv;
    rv = libSpec->SetPersistentDescriptorString((char *)aLibraryPersistentDescriptor);
    if (NS_FAILED(rv)) return rv;

    // Call the register component with FileSpec routine
    rv = RegisterComponentSpec(aClass, aClassName, aProgID, libSpec, aReplace, aPersist);
    return rv;
}


nsresult
nsComponentManagerImpl::RegisterComponentSpec(const nsCID &aClass,
                                              const char *aClassName,
                                              const char *aProgID,
                                              nsIFileSpec *aLibrarySpec,
                                              PRBool aReplace,
                                              PRBool aPersist)
{
    nsresult rv = NS_OK;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *fullName = NULL;
        aLibrarySpec->GetNativePath(&fullName);
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: RegisterComponentSpec(%s, %s, %s, %s), replace = %d, persist = %d.",
                    buf, aClassName, aProgID, fullName, (int)aReplace, (int)aPersist);
        delete [] buf;
        if (fullName) delete [] fullName;
    }

    nsIDKey key(aClass);
    nsFactoryEntry *entry = GetFactoryEntry(aClass, PR_TRUE);

    if (entry && !aReplace)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }
    
#ifdef USE_REGISTRY
    if (aPersist)
    {
        // Add it to the registry
        nsDll *dll = new nsDll(aLibrarySpec);
        if (dll == NULL) return NS_ERROR_OUT_OF_MEMORY;
        char *cidString = aClass.ToString();
        if (cidString == NULL)
        {
            delete dll;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        PlatformRegister(cidString, aClassName, aProgID, dll);
        delete [] cidString;
        delete dll;
    } 
    else
#endif
    {
        // Create a dll from the librarySpec
        nsDll *dll = CreateCachedDll(aLibrarySpec);
        if (dll == NULL) return NS_ERROR_OUT_OF_MEMORY;
        
        // Use the dll to create a factoryEntry
        nsFactoryEntry *newEntry = new nsFactoryEntry(aClass, dll);
        if (newEntry == NULL)
        {
            delete dll;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (entry)
        {
            mFactories->RemoveAndDelete(&key);
            entry = NULL;
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tdeleting registered Factory."));
        }
        mFactories->Put(&key, newEntry);
    }
    	
    // Update the ProgID->CLSID Map if this isnt stored in registry
#ifdef USE_REGISTRY
    if (aProgID && aPersist != PR_TRUE)
        rv = HashProgID(aProgID, aClass);
#else /* USE_REGISTRY */
    if (aProgID)
        rv = HashProgID(aProgID, aClass);
#endif /* USE_REGISTRY */

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register %s.", NS_SUCCEEDED(rv) ? "succeeded" : "failed"));
    return rv;
}

nsresult
nsComponentManagerImpl::RegisterComponentLib(const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aProgID,
                                             const char *aDllName,
                                             PRBool aReplace,
                                             PRBool aPersist /* has to be PR_FALSE */)
{
    nsresult rv = NS_OK;

    // XXX We cant store dllName registrations in the registry yet.
    PR_ASSERT(aPersist == PR_FALSE);

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: RegisterComponentLib(%s, %s, %s, %s), replace = %d, persist = %d.",
                    buf, aClassName, aProgID, aDllName, (int)aReplace, (int)aPersist);
        delete [] buf;
    }

    nsIDKey key(aClass);
    nsFactoryEntry *entry = GetFactoryEntry(aClass, PR_TRUE);

    if (entry && !aReplace)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

#ifdef USE_REGISTRY
    if (aPersist)
    {
        // Add it to the registry
        nsDll *dll = new nsDll(aDllName, 1/* dummy */);
        if (dll == NULL) rv = NS_ERROR_OUT_OF_MEMORY;
        char *cidString = aClass.ToString();
        if (cidString == NULL)
        {
            delete dll;
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
        PlatformRegister(cidString, aClassName, aProgID, dll);
        delete [] cidString;
        delete dll;
    } 
    else
#endif
    {
        // Create a dll from the DllName
        nsDll *dll = CreateCachedDllName(aDllName);
        if (dll == NULL) return NS_ERROR_OUT_OF_MEMORY;
        
        // Use the dll to create a factoryEntry
        nsFactoryEntry* newEntry = new nsFactoryEntry(aClass, dll);
        if (newEntry == NULL)
        {
            delete dll;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (entry)
        {
            mFactories->RemoveAndDelete(&key);
            entry = NULL;
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING, ("\t\tdeleting registered Factory."));
        }
        mFactories->Put(&key, newEntry);
    }
    	
        // Update the ProgID->CLSID Map if this isn't stored in registry
#ifdef USE_REGISTRY
    if (aProgID && aPersist != PR_TRUE)
        rv = HashProgID(aProgID, aClass);
#else /* USE_REGISTRY */
    if (aProgID)
        rv = HashProgID(aProgID, aClass);
#endif /* USE_REGISTRY */

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register %s.", rv == NS_OK ? "succeeded" : "failed"));
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
        if (old->factory == aFactory)
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
    	
    if (old != NULL && old->dll != NULL)
    {
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
    }
    	
    PR_ExitMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: ! Factory unregister %s.", 
            NS_SUCCEEDED(res) ? "succeeded" : "failed"));
    	
    return res;
}

static nsresult
nsFreeLibrary(nsDll *dll, nsIServiceManager *serviceMgr)
{
    nsresult rv = NS_ERROR_FAILURE;

    if (!dll || dll->IsLoaded() == PR_FALSE)
    {
        return NS_ERROR_INVALID_ARG;
    }

    // Get if the dll was marked for unload in an earlier round
    PRBool dllMarkedForUnload = dll->IsMarkedForUnload();

    // Reset dll marking for unload just in case we return with
    // an error.
    dll->MarkForUnload(PR_FALSE);

    
    // Get the module object
    nsCOMPtr<nsIModule> mobj;
    rv = dll->GetModule(nsComponentManagerImpl::gComponentManager, getter_AddRefs(mobj));
    if (NS_FAILED(rv))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsComponentManager: Cannot get module object for %s", dll->GetNativePath()));
        return  rv;
    }

    PRBool canUnload;
    rv = mobj->CanUnload(nsComponentManagerImpl::gComponentManager, &canUnload);
    if (NS_FAILED(rv))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsComponentManager: nsIModule::CanUnload() returned error for %s.", dll->GetNativePath()));
        return rv;
    }
        
    if (canUnload)
    {
        if (dllMarkedForUnload)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsComponentManager: + Unloading \"%s\".", dll->GetNativePath()));
#if 0
            // XXX dlls aren't counting their outstanding instances correctly
            // XXX hence, dont unload until this gets enforced.
            rv = dll->Unload();
#endif /* 0 */
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Ready for unload \"%s\".", dll->GetNativePath()));
        }
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsComponentManager: + NOT Unloading %s", dll->GetNativePath()));
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

static PRBool
nsFreeLibraryEnum(nsHashKey *aKey, void *aData, void* closure) 
{
    nsDll *dll = (nsDll *) aData;
    nsIServiceManager* serviceMgr = (nsIServiceManager*)closure;
    nsFreeLibrary(dll, serviceMgr);
    return PR_TRUE;
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

    mDllStore->Enumerate(nsFreeLibraryEnum, serviceMgr);
    	
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

nsresult
nsComponentManagerImpl::AutoRegister(RegistrationTime when, nsIFileSpec *inDirSpec)
{
    nsCOMPtr<nsIFileSpec> dirSpec;

    if (inDirSpec == NULL)
    {
        // Do default components directory
        nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::XPCOM_CurrentProcessComponentDirectory);
        nsresult rv = NS_NewFileSpecWithSpec(sysdir, getter_AddRefs(dirSpec));
        if (NS_FAILED(rv)) return rv;
    }
    else
        dirSpec = inDirSpec;

    char *nativePath = NULL;
    nsresult rv = dirSpec->GetNativePath(&nativePath);
    if (NS_FAILED(rv)) return rv;

    if (nativePath == NULL)
    {
        return NS_ERROR_INVALID_POINTER;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: Autoregistration begins. dir = %s", nativePath));
#ifdef DEBUG
    printf("nsComponentManager: Autoregistration begins. dir = %s\n", nativePath);
#endif /* DEBUG */

    rv = SyncComponentsInDir(when, dirSpec);

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: Autoregistration ends. dir = %s", nativePath));
#ifdef DEBUG
    printf("nsComponentManager: Autoregistration ends. dir = %s\n", nativePath);
#endif /* DEBUG */

    nsCRT::free(nativePath);

    return rv;
}


nsresult
nsComponentManagerImpl::SyncComponentsInDir(RegistrationTime when, nsIFileSpec *dirSpec)
{
    nsresult rv = NS_ERROR_FAILURE;
    PRBool isDir = PR_FALSE;

    // Maker sure we are dealing with a directory
    rv = dirSpec->isDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;
    if (isDir == PR_FALSE)
    {
        return NS_ERROR_INVALID_POINTER;
    }

    // Create a directory iterator
    NS_DEFINE_IID(kDirectoryIteratorIID, NS_IDIRECTORYITERATOR_IID);
    nsCOMPtr<nsIDirectoryIterator>dirIterator;
    rv = CreateInstance(NS_DIRECTORYITERATOR_PROGID, NULL, kDirectoryIteratorIID, getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;
    rv = dirIterator->Init(dirSpec, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // whip through the directory to register every file
    nsIFileSpec *dirEntry = NULL;
    PRBool more = PR_FALSE;
    rv = dirIterator->exists(&more);
    if (NS_FAILED(rv)) return rv;
    while (more == PR_TRUE)
    {
        rv = dirIterator->GetCurrentSpec(&dirEntry);
        if (NS_FAILED(rv)) return rv;

        rv = dirEntry->isDirectory(&isDir);
        if (NS_FAILED(rv)) return rv;
        if (isDir == PR_TRUE)
        {
            // This is a directory. Grovel for components into the directory.
            rv = SyncComponentsInDir(when, dirEntry);
        }
        else
        {
            // This is a file. Try to register it.
            rv = AutoRegisterComponent(when, dirEntry);
        }
        if (NS_FAILED(rv))
        {
            // This means either of AutoRegisterComponent or
            // SyncComponentsInDir failed. It could be because
            // the file isn't a component like initpref.js
            // So dont break on these errors.
        }

        NS_RELEASE(dirEntry);
        
        rv = dirIterator->next();
        if (NS_FAILED(rv)) return rv;
        rv = dirIterator->exists(&more);
        if (NS_FAILED(rv)) return rv;
    }
    
    return rv;
}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(RegistrationTime when, nsIFileSpec *component)
{
    const char *ValidDllExtensions[] = {
        ".dll",		/* Windows */
        ".dso",		/* Unix ? */
        ".dylib",	/* Unix: Rhapsody */
        ".so",		/* Unix */
        ".so.1.0",	/* Unix: BSD */
        ".sl",		/* Unix: HP-UX */
        ".shlb",	/* Mac ? */
#if defined(VMS)
        ".exe",		/* Open VMS */
#endif
        ".dlm",		/* new for all platforms */
        NULL
    };
    nsresult rv;

    // Ensure we are dealing with a file as opposed to a dir
    PRBool b = PR_FALSE;

    rv = component->isFile(&b);
    if (NS_FAILED(rv)) return rv;

    if (b == PR_FALSE) return NS_ERROR_FAILURE;

    // deal only with files that have a valid extension
    PRBool validExtension = PR_FALSE;

#ifdef	XP_MAC
	// rjc - on Mac, check the file's type code (skip checking the creator code)
	const nsFileSpec	fs;
	if (NS_FAILED(rv = component->GetFileSpec(&fs)))
			return(rv);

	CInfoPBRec	catInfo;
	OSErr 		err = fs.GetCatInfo(catInfo);
	if (!err)
	{
		// on Mac, Mozilla shared libraries are of type 'shlb'
		// Note: we don't check the creator (which for Mozilla is 'MOZZ')
		// so that 3rd party shared libraries will be noticed!
		if ((catInfo.hFileInfo.ioFlFndrInfo.fdType == 'shlb')
			/* && (catInfo.hFileInfo.ioFlFndrInfo.fdCreator == 'MOZZ') */ )
		{
			validExtension = PR_TRUE;
		}
	}
#else
    char *leafName = NULL;
    rv = component->GetLeafName(&leafName);
    if (NS_FAILED(rv)) return rv;
    int flen = PL_strlen(leafName);
    for (int i=0; ValidDllExtensions[i] != NULL; i++)
    {
        int extlen = PL_strlen(ValidDllExtensions[i]);
    		
        // Does fullname end with this extension
        if (flen >= extlen &&
            !PL_strcasecmp(&(leafName[flen - extlen]), ValidDllExtensions[i])
            )
        {
            validExtension = PR_TRUE;
            break;
        }
    }
    if (leafName) nsCRT::free(leafName);
#endif
    	
    if (validExtension == PR_FALSE)
    {
        // Skip invalid extensions
        return NS_ERROR_FAILURE;
    }

    // Get the name of the dll
    char *persistentDescriptor = NULL;
    rv = component->GetPersistentDescriptorString(&persistentDescriptor);
    if (NS_FAILED(rv)) return rv;
    autoStringFree delete_persistentDescriptor(persistentDescriptor, autoStringFree::nsCRT_String_Delete);
    nsStringKey key(persistentDescriptor);

    // Check if dll is one that we have already seen
    nsDll *dll = (nsDll *) mDllStore->Get(&key);
    rv = NS_OK;
    if (dll == NULL)
    {
        // Create nsDll for this from registry and
        // add it to our dll cache mDllStore.
#ifdef USE_REGISTRY
        rv = PlatformCreateDll(persistentDescriptor, &dll);
        if (NS_SUCCEEDED(rv))
        {
            mDllStore->Put(&key, (void *) dll);
        }
#endif /* USE_REGISTRY */
    }

    if (dll != NULL)
    {
        // Make sure the dll is OK
        if (dll->GetStatus() != NS_OK)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsComponentManager: + nsDll not NS_OK \"%s\". Skipping...",
                    dll->GetNativePath()));
            return NS_ERROR_FAILURE;
        }
    		
        // We already have seen this dll. Check if this dll changed
        if (!dll->HasChanged())
        {
            // Dll hasn't changed. Skip.
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsComponentManager: + nsDll not changed \"%s\". Skipping...",
                    dll->GetNativePath()));
            return NS_OK;
        }
    		
        // Aagh! the dll has changed since the last time we saw it.
        // re-register dll
        if (dll->IsLoaded())
        {
            // We loaded the old version of the dll and now we find that the
            // on-disk copy if newer. Try to unload the dll.
            nsIServiceManager* serviceMgr = NULL;
            nsServiceManager::GetGlobalServiceManager(&serviceMgr);
            // if getting the serviceMgr failed, we can still pass NULL to FreeLibraries
            rv = nsFreeLibrary(dll, serviceMgr);
            if (NS_FAILED(rv))
            {
                // THIS IS THE WORST SITUATION TO BE IN.
                // Dll doesn't want to be unloaded. Cannot re-register
                // this dll.
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsComponentManager: *** Dll already loaded. "
                        "Cannot unload either. Hence cannot re-register "
                        "\"%s\". Skipping...", dll->GetNativePath()));
                return rv;
            }
            else {
                // dll doesn't have a CanUnload proc. Guess it is
                // ok to unload it.
                dll->Unload();
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                       ("nsComponentManager: + Unloading \"%s\". (no CanUnloadProc).",
                        dll->GetNativePath()));
            }
    			
        } // dll isloaded
    		
        // Sanity.
        if (dll->IsLoaded())
        {
            // We went through all the above to make sure the dll
            // is unloaded. And here we are with the dll still
            // loaded. Whoever taught dp programming...
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Dll still loaded. Cannot re-register "
                    "\"%s\". Skipping...", dll->GetNativePath()));
            return NS_ERROR_FAILURE;
        }
    } // dll != NULL
    else
    {
        // Create and add the dll to the mDllStore
        // It is ok to do this even if the creation of nsDll
        // didnt succeed. That way we wont do this again
        // when we encounter the same dll.
        dll = new nsDll(persistentDescriptor);
        if (dll == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        mDllStore->Put(&key, (void *) dll);
    } // dll == NULL
    	
    // Either we are seeing the dll for the first time or the dll has
    // changed since we last saw it and it is unloaded successfully.
    //
    // Now we can try register the dll for sure. 
    nsresult res = SelfRegisterDll(dll);
    nsresult ret = NS_OK;
    if (NS_FAILED(res))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Autoregistration FAILED for "
                "\"%s\". Skipping...", dll->GetNativePath()));
        // Mark dll as not xpcom dll along with modified time and size in
        // the registry so that we wont need to load the dll again every
        // session until the dll changes.
#ifdef USE_REGISTRY
        PlatformMarkNoComponents(dll);
#endif /* USE_REGISTRY */
        ret = NS_ERROR_FAILURE;
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Autoregistration Passed for "
                "\"%s\".", dll->GetNativePath()));
        // Marking dll along with modified time and size in the
        // registry happens at PlatformRegister(). No need to do it
        // here again.
    }
    return ret;
}


/*
 * SelfRegisterDll
 *
 * Given a dll abstraction, this will load, selfregister the dll and
 * unload the dll.
 *
 */
nsresult
nsComponentManagerImpl::SelfRegisterDll(nsDll *dll)
{
    // Precondition: dll is not loaded already
    PR_ASSERT(dll->IsLoaded() == PR_FALSE);

    nsIServiceManager* serviceMgr = NULL;
    nsresult res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(res)) return res;

    if (dll->Load() == PR_FALSE)
    {
        // Cannot load. Probably not a dll.
        char errorMsg[1024] = "Cannot get error from nspr. Not enough memory.";
        if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
            PR_GetErrorText(errorMsg);

        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: SelfRegisterDll(%s) Load FAILED with error:%s", dll->GetNativePath(), errorMsg));

#ifdef PRINT_CRITICAL_ERROR_TO_SCREEN
        printf("**************************************************\n"
               "nsComponentManager: Load(%s) FAILED with error: %s\n"
               "**************************************************\n",
               dll->GetNativePath(), errorMsg);
#endif
        return NS_ERROR_FAILURE;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
      ("nsComponentManager: + Loaded \"%s\".", dll->GetNativePath()));

    // Tell the module to self register
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(gComponentManager, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
               ("nsComponentManager: %s using nsIModule to register self.", dll->GetNativePath()));
        nsCOMPtr<nsIFileSpec> fs;
        res = dll->GetDllSpec(getter_AddRefs(fs));
        if (NS_SUCCEEDED(res))
            res = mobj->RegisterSelf(gComponentManager, fs);
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                   ("nsComponentManager: dll->GetDllSpec() on %s FAILED.", dll->GetNativePath()));
        }
        mobj = NULL;	// Force a release of the Module object before unload()
    }
#ifndef OBSOLETE_MODULE_LOADING
    else
    {
        res = NS_ERROR_NO_INTERFACE;
        nsRegisterProc regproc = (nsRegisterProc)dll->FindSymbol("NSRegisterSelf");
        if (regproc)
        {
            // Call the NSRegisterSelfProc to enable dll registration
            res = regproc(serviceMgr, dll->GetPersistentDescriptorString());
        }
    }
#endif /* OBSOLETE_MODULE_LOADING */

    dll->Unload();
    return res;
}

nsresult
nsComponentManagerImpl::SelfUnregisterDll(nsDll *dll)
{
    // Precondition: dll is not loaded
    PR_ASSERT(dll->IsLoaded() == PR_FALSE);

    nsIServiceManager* serviceMgr = NULL;
    nsresult res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(res)) return res;

    if (dll->Load() == PR_FALSE)
    {
        // Cannot load. Probably not a dll.
        return(NS_ERROR_FAILURE);
    }
    	
    // Tell the module to self register
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(gComponentManager, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
               ("nsComponentManager: %s using nsIModule to unregister self.", dll->GetNativePath()));
        nsCOMPtr<nsIFileSpec> fs;
        res = dll->GetDllSpec(getter_AddRefs(fs));
        if (NS_SUCCEEDED(res))
            res = mobj->UnregisterSelf(gComponentManager, fs);
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                   ("nsComponentManager: dll->GetDllSpec() on %s FAILED.", dll->GetNativePath()));
        }
        mobj = NULL;	// Force a release of the Module object before unload()
    }
#ifndef OBSOLETE_MODULE_LOADING
    else
    {
        res = NS_ERROR_NO_INTERFACE;
        nsUnregisterProc unregproc =
            (nsUnregisterProc) dll->FindSymbol("NSUnregisterSelf");
        if (unregproc)
        {
            // Call the NSUnregisterSelfProc to enable dll de-registration
            res = unregproc(serviceMgr, dll->GetPersistentDescriptorString());
        }
    }
#endif /* OBSOLETE_MODULE_LOADING */
    dll->Unload();
    return res;
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
