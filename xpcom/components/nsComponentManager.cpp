/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      Added PR_CALLBACK for Optlink use in OS2
 */

#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"

#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsCRT.h"
#include "nsIEnumerator.h"
#include "nsIModule.h"
#include "nsHashtableEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentLoader.h"
#include "nsNativeComponentLoader.h"
#include "nsXPIDLString.h"

#include "nsIObserverService.h"

#include "nsILocalFile.h"
#include "nsLocalFile.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

#include "plstr.h"
#include "prlink.h"
#include "prsystem.h"
#include "prprf.h"
#include "xcDll.h"
#include "prerror.h"
#include "prmem.h"
#include "nsIFile.h"
//#include "mozreg.h"
#include "NSReg.h"

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
const char xpcomKeyName[]="software/mozilla/XPCOM";
const char classesKeyName[]="progID";
const char classIDKeyName[]="classID";
const char componentsKeyName[]="components";
const char componentLoadersKeyName[]="componentLoaders";
const char xpcomComponentsKeyName[]="software/mozilla/XPCOM/components";

// Common Value Names
const char classIDValueName[]="ClassID";
const char versionValueName[]="VersionString";
const char lastModValueName[]="LastModTimeStamp";
const char fileSizeValueName[]="FileSize";
const char componentCountValueName[]="ComponentsCount";
const char progIDValueName[]="ProgID";
const char classNameValueName[]="ClassName";
const char inprocServerValueName[]="InprocServer";
const char componentTypeValueName[]="ComponentType";
const char nativeComponentType[]="application/x-mozilla-native";

const static char XPCOM_ABSCOMPONENT_PREFIX[] = "abs:";
const static char XPCOM_RELCOMPONENT_PREFIX[] = "rel:";
const char XPCOM_LIB_PREFIX[]          = "lib:";

// We define a CID that is used to indicate the non-existence of a
// progid in the hash table.
#define NS_NO_CID { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }
static NS_DEFINE_CID(kNoCID, NS_NO_CID);

// Build is using USE_NSREG to turn off xpcom using registry
// but internally we use USE_REGISTRY. Map them propertly.
#ifdef USE_NSREG
#define USE_REGISTRY
#endif /* USE_NSREG */


extern PRBool gShuttingDown;
nsresult

nsCreateInstanceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
    {
        nsresult status = nsComponentManager::CreateInstance(mCID, mOuter, aIID, aInstancePtr);
        if ( !NS_SUCCEEDED(status) )
            *aInstancePtr = 0;

        if ( mErrorPtr )
            *mErrorPtr = status;
        return status;
    }

nsresult
nsCreateInstanceByProgID::operator()( const nsIID& aIID, void** aInstancePtr ) const
    {
        nsresult status;
        if ( mProgID )
            {
              if ( !NS_SUCCEEDED(status = nsComponentManager::CreateInstance(mProgID, mOuter, aIID, aInstancePtr)) )
                  *aInstancePtr = 0;
          }
        else
          status = NS_ERROR_NULL_POINTER;

        if ( mErrorPtr )
            *mErrorPtr = status;
        return status;
    }

nsresult
nsCreateInstanceFromCategory::operator()( const nsIID& aIID,
                                          void** aInstancePtr ) const
{
    /*
     * If I were a real man, I would consolidate this with
     * nsGetServiceFromProgID::operator().
     */
    nsresult status;
    nsXPIDLCString value;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_PROGID, &status);

    if (NS_FAILED(status)) goto error;

    if (!mCategory || !mEntry) {
        // when categories have defaults, use that for null mEntry
        status = NS_ERROR_NULL_POINTER;
        goto error;
    }
    
    /* find the progID for category.entry */
    status = catman->GetCategoryEntry(mCategory, mEntry,
                                      getter_Copies(value));
    if (NS_FAILED(status)) goto error;
    if (!value) {
        status = NS_ERROR_SERVICE_NOT_FOUND;
        goto error;
    }

    status = nsComponentManager::CreateInstance(value, mOuter, aIID,
                                                aInstancePtr);
    error:
    if (NS_FAILED(status)) {
        *aInstancePtr = 0;
    }

    *mErrorPtr = status;
    return status;
}

/* prototypes for the Mac */
PRBool PR_CALLBACK
nsFactoryEntry_Destroy(nsHashKey *aKey, void *aData, void* closure);

PRBool PR_CALLBACK
nsCID_Destroy(nsHashKey *aKey, void *aData, void* closure);
////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

MOZ_DECL_CTOR_COUNTER(nsFactoryEntry);

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass,
                               const char *aLocation,
                               const char *aType,
                               nsIComponentLoader *aLoader)
    : cid(aClass), factory(nsnull), loader(aLoader)
{
    MOZ_COUNT_CTOR(nsFactoryEntry);
    loader = aLoader;
    type = aType;
    location = aLocation;
}

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory)
    : cid(aClass), loader(nsnull)

{
    MOZ_COUNT_CTOR(nsFactoryEntry);
    factory = aFactory;
}

nsFactoryEntry::~nsFactoryEntry(void)
{
    MOZ_COUNT_DTOR(nsFactoryEntry);
    factory = 0;
    loader = 0;
}

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////


nsComponentManagerImpl::nsComponentManagerImpl()
    : mFactories(NULL), mProgIDs(NULL), mLoaders(0), mMon(NULL), 
      mRegistry(NULL), mPrePopulationDone(PR_FALSE),
      mNativeComponentLoader(0), mShuttingDown(NS_SHUTDOWN_NEVERHAPPENED)
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
    PR_ASSERT(mShuttingDown != NS_SHUTDOWN_INPROGRESS);
    if (mShuttingDown == NS_SHUTDOWN_INPROGRESS)
        return NS_ERROR_FAILURE;

    mShuttingDown = NS_SHUTDOWN_NEVERHAPPENED;

    if (nsComponentManagerLog == NULL)
    {
        nsComponentManagerLog = PR_NewLogModule("nsComponentManager");
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("xpcom-log-version : " NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING));
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
        mNativeComponentLoader = new nsNativeComponentLoader();
        if (!mNativeComponentLoader)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mNativeComponentLoader);
    }
    
    if (mLoaders == nsnull) {
    mLoaders = new nsSupportsHashtable(16, /* Thread safe */ PR_TRUE);
    if (mLoaders == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsCStringKey loaderKey(nativeComponentType);
    mLoaders->Put(&loaderKey, mNativeComponentLoader);
    }

#ifdef USE_REGISTRY
        NR_StartupRegistry();
        PlatformInit();
#endif

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("nsComponentManager: Initialized."));

    return NS_OK;
}

nsresult nsComponentManagerImpl::Shutdown(void) 
{
    PR_ASSERT(mShuttingDown == NS_SHUTDOWN_NEVERHAPPENED);
    if (mShuttingDown != NS_SHUTDOWN_NEVERHAPPENED)
        return NS_ERROR_FAILURE;

    mShuttingDown = NS_SHUTDOWN_INPROGRESS;

    // Shutdown the component manager
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Beginning Shutdown."));

    // Release all cached factories
    if (mFactories)
        delete mFactories;

    // Unload libraries
    UnloadLibraries(NULL, NS_Shutdown);

    // Release Progid hash tables
    if (mProgIDs)
        delete mProgIDs;

#ifdef USE_REGISTRY
    // Release registry
    NS_IF_RELEASE(mRegistry);
#endif /* USE_REGISTRY */

    // This is were the nsFileSpec was deleted, so I am 
    // going to assign zero to 
    mComponentsDir = 0;

    // Release all the component loaders
    if (mLoaders)
    delete mLoaders;

    // we have an extra reference on this one, which is probably a good thing
    NS_IF_RELEASE(mNativeComponentLoader);
    
    // Destroy the Lock
    if (mMon)
        PR_DestroyMonitor(mMon);

#ifdef USE_REGISTRY
    NR_ShutdownRegistry();
#endif /* USE_REGISTRY */

    mShuttingDown = NS_SHUTDOWN_COMPLETE;

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Shutdown complete."));

    return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Beginning destruction."));

    if (mShuttingDown != NS_SHUTDOWN_COMPLETE)
        Shutdown();

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Destroyed."));
}

NS_IMPL_ISUPPORTS3(nsComponentManagerImpl, nsIComponentManager,
                   nsISupportsWeakReference, nsIInterfaceRequestor)

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
            rv = registryFactory->CreateInstance(NULL, NS_GET_IID(nsIRegistry),(void **)&mRegistry);
            if (NS_FAILED(rv)) return rv;
            NS_RELEASE(registryFactory);
        }
    }

    // Open the App Components registry. We will keep it open forever!
    rv = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(rv)) return rv;

    // Check the version of registry. Nuke old versions.
    nsRegistryKey xpcomRoot;
    rv = PlatformVersionCheck(&xpcomRoot);
    if (NS_FAILED(rv)) return rv;

    // Open common registry keys here to speed access
    // Do this after PlatformVersionCheck as it may re-create our keys
    rv = mRegistry->AddSubtree(xpcomRoot, componentsKeyName, &mXPCOMKey);
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->AddSubtree(xpcomRoot, classesKeyName, &mClassesKey);
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->AddSubtree(xpcomRoot, classIDKeyName, &mCLSIDKey);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIProperties> directoryService;
    rv = nsDirectoryService::Create(nsnull, 
                                    NS_GET_IID(nsIProperties), 
                                    getter_AddRefs(directoryService));  
    
    directoryService->Get(NS_XPCOM_COMPONENT_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mComponentsDir));

    if (!mComponentsDir)
        return NS_ERROR_OUT_OF_MEMORY;
    
    char* componentDescriptor;
    mComponentsDir->GetPath(&componentDescriptor);
    if (!componentDescriptor)
        return NS_ERROR_NULL_POINTER;

    mComponentsOffset = strlen(componentDescriptor);
        
    if (componentDescriptor)
        nsMemory::Free(componentDescriptor);



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
nsComponentManagerImpl::PlatformVersionCheck(nsRegistryKey *aXPCOMRootKey)
{
    nsRegistryKey xpcomKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, xpcomKeyName, &xpcomKey);
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLCString buf;
    nsresult err = mRegistry->GetStringUTF8(xpcomKey, versionValueName, 
                                        getter_Copies(buf));

    // If there is a version mismatch or no version string, we got an old registry.
    // Delete the old repository hierarchies and recreate version string
    if (NS_FAILED(err) || PL_strcmp(buf, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Registry version mismatch (old:%s vs new:%s)."
                "Nuking xpcom registry hierarchy.", (const char *)buf,
                NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING));

        // Delete the XPCOM hierarchy
        rv = mRegistry->RemoveSubtree(nsIRegistry::Common, xpcomKeyName);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Nuke Subtree (%s)",xpcomKeyName));
            return rv;
        }

        // The top-level Classes and CLSID trees are from an early alpha version,
        // we can probably remove these two deletions after the second beta or so.
        (void) mRegistry->RemoveSubtree(nsIRegistry::Common, classIDKeyName);
        (void) mRegistry->RemoveSubtree(nsIRegistry::Common, classesKeyName);

        // Recreate XPCOM key and version
        rv = mRegistry->AddSubtree(nsIRegistry::Common,xpcomKeyName, &xpcomKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Add Subtree (%s)",xpcomKeyName));
            return rv;
        }

        rv = mRegistry->SetStringUTF8(xpcomKey,versionValueName, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING);
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


    // return the XPCOM key (null check deferred so cleanup always happens)
    if (!aXPCOMRootKey)
        return NS_ERROR_NULL_POINTER;
    else
        *aXPCOMRootKey = xpcomKey;

    return NS_OK;
}

#if 0
// If ever revived, this code is not fully updated to escape the dll location
void
nsComponentManagerImpl::PlatformSetFileInfo(nsRegistryKey key, PRUint32 lastModifiedTime, PRUint32 fileSize)
{
    mRegistry->SetInt(key, lastModValueName, lastModifiedTime);
    mRegistry->SetInt(key, fileSizeValueName, fileSize);
}

/**
 * PlatformMarkNoComponents(nsDll *dll)
 *
 * Stores the dll name, last modified time, size and 0 for number of
 * components in dll in the registry at location
 *        ROOTKEY_COMMON/Software/Mozilla/XPCOM/Components/dllname
 */
nsresult
nsComponentManagerImpl::PlatformMarkNoComponents(nsDll *dll)
{
    PR_ASSERT(mRegistry!=NULL);
    
    nsresult rv;

    nsRegistryKey dllPathKey;
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
    
    nsRegistryKey IDkey;
    rv = mRegistry->AddSubtreeRaw(mCLSIDKey, cidString, &IDkey);
    if (NS_FAILED(rv)) return (rv);


    rv = mRegistry->SetStringUTF8(IDkey,classNameValueName, className);
    if (progID)
    {
        rv = mRegistry->SetStringUTF8(IDkey,progIDValueName, progID);        
    }
    rv = mRegistry->SetBytesUTF8(IDkey, inprocServerValueName, 
            strlen(dll->GetPersistentDescriptorString()) + 1, 
            NS_REINTERPRET_CAST(char*, dll->GetPersistentDescriptorString()));
    
    if (progID)
    {
        nsRegistryKey progIDKey;
        rv = mRegistry->AddSubtreeRaw(mClassesKey, progID, &progIDKey);
        rv = mRegistry->SetStringUTF8(progIDKey, classIDValueName, cidString);
    }

    nsRegistryKey dllPathKey;
    rv = mRegistry->AddSubtreeRaw(mXPCOMKey,dll->GetPersistentDescriptorString(), &dllPathKey);

    PlatformSetFileInfo(dllPathKey, dll->GetLastModifiedTime(), dll->GetSize());

    PRInt32 nComponents = 0;
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
    nsresult rv;
    PRUint32 length = strlen(aLibrary);
    char* eLibrary;
    rv = mRegistry->EscapeKey((PRUint8*)aLibrary, 1, &length, (PRUint8**)&eLibrary);
    if (rv != NS_OK)
    {
    return rv;
    }
    if (eLibrary == nsnull)    //  No escaping required
    eLibrary = (char*)aLibrary;


    PR_ASSERT(mRegistry!=NULL);


    nsRegistryKey cidKey;
    rv = mRegistry->AddSubtreeRaw(mCLSIDKey, cidString, &cidKey);

    char *progID = NULL;
    rv = mRegistry->GetStringUTF8(cidKey, progIDValueName, &progID);
    if(NS_SUCCEEDED(rv))
    {
        mRegistry->RemoveSubtreeRaw(mClassesKey, progID);
        PR_FREEIF(progID);
    }

    mRegistry->RemoveSubtree(mCLSIDKey, cidString);
        
    nsRegistryKey libKey;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, eLibrary, &libKey);
    if(NS_FAILED(rv)) return rv;

    // We need to reduce the ComponentCount by 1.
    // If the ComponentCount hits 0, delete the entire key.
    PRInt32 nComponents = 0;
    rv = mRegistry->GetInt(libKey, componentCountValueName, &nComponents);
    if(NS_FAILED(rv)) return rv;
    nComponents--;
    
    if (nComponents <= 0)
    {
        rv = mRegistry->RemoveSubtreeRaw(mXPCOMKey, eLibrary);
    }
    else
    {
        rv = mRegistry->SetInt(libKey, componentCountValueName, nComponents);
    }

    if (eLibrary != aLibrary)
    nsMemory::Free(eLibrary);

    return rv;
}

nsresult
nsComponentManagerImpl::PlatformFind(const nsCID &aCID, nsFactoryEntry* *result)
{
    PR_ASSERT(mRegistry!=NULL);

    nsresult rv;

    char *cidString = aCID.ToString();

    nsRegistryKey cidKey;
    rv = mRegistry->GetSubtreeRaw(mCLSIDKey, cidString, &cidKey);
    delete [] cidString;

    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString library;
    PRUint32 tmp;
    rv = mRegistry->GetBytesUTF8(cidKey, inprocServerValueName,
                              &tmp, (PRUint8**)getter_Copies(library).operator char**());
    if (NS_FAILED(rv))
    {
        // Registry inconsistent. No File name for CLSID.
        return rv;
    }

    nsXPIDLCString componentType;
    rv = mRegistry->GetStringUTF8(cidKey, componentTypeValueName, 
                              getter_Copies(componentType));

    if (NS_FAILED(rv))
    if (rv == NS_ERROR_REG_NOT_FOUND)
        /* missing componentType, we assume application/x-moz-native */
        componentType = nativeComponentType;
    else 
        return rv;              // XXX translate error code?

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
        
    nsRegistryKey progIDKey;
    rv = mRegistry->GetSubtreeRaw(mClassesKey, aProgID, &progIDKey);
    if (NS_FAILED(rv)) return NS_ERROR_FACTORY_NOT_REGISTERED;

    char *cidString;
    rv = mRegistry->GetStringUTF8(progIDKey, classIDValueName, &cidString);
    if(NS_FAILED(rv)) return rv;
    if (!(aClass->Parse(cidString)))
    {
        rv = NS_ERROR_FAILURE;
    }

    PR_FREEIF(cidString);
    return rv;
}

nsresult
nsComponentManagerImpl::PlatformCLSIDToProgID(const nsCID *aClass,
                                              char* *aClassName, char* *aProgID)
{
        
    PR_ASSERT(aClass);
    PR_ASSERT(mRegistry);

    nsresult rv;

    char* cidStr = aClass->ToString();
    nsRegistryKey cidKey;
    rv = mRegistry->GetSubtreeRaw(mCLSIDKey, cidStr, &cidKey);
    if(NS_FAILED(rv)) return rv;
    PR_FREEIF(cidStr);

    char* classnameString;
    rv = mRegistry->GetStringUTF8(cidKey, classNameValueName, &classnameString);
    if(NS_FAILED(rv)) return rv;
    *aClassName = classnameString;

    char* progidString;
    rv = mRegistry->GetStringUTF8(cidKey,progIDValueName,&progidString);
    if (NS_FAILED(rv)) return rv;
    *aProgID = progidString;

    return NS_OK;

}

nsresult nsComponentManagerImpl::PlatformPrePopulateRegistry()
{
    nsresult rv;

    if (mPrePopulationDone)
        return NS_OK;
    // Read in all CID entries and populate the mFactories
    nsCOMPtr<nsIEnumerator> cidEnum;
    rv = mRegistry->EnumerateSubtrees( mCLSIDKey, getter_AddRefs(cidEnum));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRegistryEnumerator> regEnum = do_QueryInterface(cidEnum, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = regEnum->First();
    for (rv = regEnum->First();
         NS_SUCCEEDED(rv) && (regEnum->IsDone() != NS_OK);
         rv = regEnum->Next())
    {
        const char *cidString;
        nsRegistryKey cidKey;
        /*
         * CurrentItemInPlaceUTF8 will give us back a _shared_ pointer in 
         * cidString.  This is bad XPCOM practice.  It is evil, and requires
         * great care with the relative lifetimes of cidString and regEnum.
         *
         * It is also faster, and less painful in the allocation department.
         */
        rv = regEnum->CurrentItemInPlaceUTF8(&cidKey, &cidString);
        if (NS_FAILED(rv))  continue;

        // Create the CID entry
        nsXPIDLCString library;
        PRUint32 tmp;
        rv = mRegistry->GetBytesUTF8(cidKey, inprocServerValueName,
                              &tmp, (PRUint8**)getter_Copies(library).operator char**());
        if (NS_FAILED(rv)) continue;
        nsCID aClass;

        if (!(aClass.Parse(cidString))) continue;

        nsXPIDLCString componentType;
        if (NS_FAILED(mRegistry->GetStringUTF8(cidKey, componentTypeValueName,
                                           getter_Copies(componentType))))
            continue;

        nsFactoryEntry* entry = 
            new nsFactoryEntry(aClass, library, componentType,
                               nsCRT::strcmp(componentType,
                                             nativeComponentType) ?
                               0 : mNativeComponentLoader);
        if (!entry)
            continue;

        nsIDKey key(aClass);
        mFactories->Put(&key, entry);
    }

    // Finally read in PROGID -> CID mappings
    nsCOMPtr<nsIEnumerator> progidEnum;
    rv = mRegistry->EnumerateSubtrees( mClassesKey, getter_AddRefs(progidEnum));
    if (NS_FAILED(rv)) return rv;

    regEnum = do_QueryInterface(progidEnum, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = regEnum->First();
    for (rv = regEnum->First();
         NS_SUCCEEDED(rv) && (regEnum->IsDone() != NS_OK);
         rv = regEnum->Next())
    {
        const char *progidString;
        nsRegistryKey progidKey;
        /*
         * CurrentItemInPlaceUTF8 will give us back a _shared_ pointer in 
         * progidString.  This is bad XPCOM practice.  It is evil, and requires
         * great care with the relative lifetimes of progidString and regEnum.
         *
         * It is also faster, and less painful in the allocation department.
         */
        rv = regEnum->CurrentItemInPlaceUTF8(&progidKey, &progidString);
        if (NS_FAILED(rv)) continue;

        nsXPIDLCString cidString;
        rv = mRegistry->GetStringUTF8(progidKey, classIDValueName,
                                      getter_Copies(cidString));
        if (NS_FAILED(rv)) continue;

        nsCID *aClass = new nsCID();
        if (!aClass) continue;        // Protect against out of memory.
        if (!(aClass->Parse(cidString)))
        {
            delete aClass;
            continue;
        }

        // put the {progid, Cid} mapping into our map
        nsCStringKey key(progidString);
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
    
    nsCStringKey key(aProgID);
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

    nsresult rv;
    rv = aEntry->GetFactory(aFactory, this);
    if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("nsComponentManager: FAILED to load factory from %s (%s)\n",
                (const char *)aEntry->location, (const char *)aEntry->type));
        return rv;
    }
        
    return NS_OK;
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass, PRBool checkRegistry)
{
    nsIDKey key(aClass);
    nsFactoryEntry *entry = (nsFactoryEntry*) mFactories->Get(&key);

#ifdef USE_REGISTRY
    if (!entry)
    {
        if (checkRegistry)
        {
            nsresult rv = PlatformFind(aClass, &entry);

            // If we got one, cache it in our hashtable
            if (NS_SUCCEEDED(rv))
            {
                mFactories->Put(&key, entry);
            }
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
    PR_ASSERT(aFactory != NULL);

    nsFactoryEntry *entry = GetFactoryEntry(aClass, !mPrePopulationDone);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    return entry->GetFactory(aFactory, this);
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
 * ProgIDToClassID()
 *
 * Mapping function from a ProgID to a classID. Directly talks to the registry.
 *
 */
nsresult
nsComponentManagerImpl::ProgIDToClassID(const char *aProgID, nsCID *aClass) 
{
    NS_PRECONDITION(aProgID != NULL, "null ptr");
    if (! aProgID)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aClass != NULL, "null ptr");
    if (! aClass)
        return NS_ERROR_NULL_POINTER;

    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef USE_REGISTRY
    nsCStringKey key(aProgID);
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
        if (!mPrePopulationDone)
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
               ("nsComponentManager: ProgIDToClassID(%s)->%s", aProgID,
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
 * NOTE: Since this isn't heavily used, we arent caching this.
 */
nsresult
nsComponentManagerImpl::CLSIDToProgID(const nsCID &aClass,
                                      char* *aClassName,
                                      char* *aProgID)
{
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef USE_REGISTRY
    res = PlatformCLSIDToProgID(&aClass, aClassName, aProgID);
#endif /* USE_REGISTRY */

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
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
    // test this first, since there's no point in creating a component during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gShuttingDown) {
        // When processing shutdown, dont process new GetService() requests
#ifdef DEBUG_dp
        NS_WARN_IF_FALSE(PR_FALSE, "Creating new instance on shutdown. Denied.");
#endif /* DEBUG_dp */
        return NS_ERROR_UNEXPECTED;
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
    }
    else
    {
        // Translate error values
        res = NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: CreateInstance(%s) %s", buf,
                NS_SUCCEEDED(res) ? "succeeded" : "FAILED"));
        delete [] buf;
    }

    return res;
}

/**
 * CreateInstanceByProgID()
 *
 * A variant of CreateInstance() that creates an instance of the object that
 * implements the interface aIID and whose implementation has a progID aProgID.
 *
 * This is only a convenience routine that turns around can calls the
 * CreateInstance() with classid and iid.
 */
nsresult
nsComponentManagerImpl::CreateInstanceByProgID(const char *aProgID,
                                               nsISupports *aDelegate,
                                               const nsIID &aIID,
                                               void **aResult)
{
    nsCID clsid;
    nsresult rv = ProgIDToClassID(aProgID, &clsid);
    if (NS_FAILED(rv)) return rv; 
    return CreateInstance(clsid, aDelegate, aIID, aResult);
}

/*
 * I want an efficient way to allocate a buffer to the right size
 * and stick the prefix and dllName in, then be able to hand that buffer
 * off to the FactoryEntry.  Is that so wrong?
 *
 * *regName is allocated on success.
 *
 * This should live in nsNativeComponentLoader.cpp, I think.
 */
static nsresult
MakeRegistryName(const char *aDllName, const char *prefix, char **regName)
{
    char *registryName;

    PRUint32 len = nsCRT::strlen(prefix);

    PRUint32 registryNameLen = nsCRT::strlen(aDllName) + len;
    registryName = (char *)nsMemory::Alloc(registryNameLen + 1);
    
    // from here on it, we want len sans terminating NUL

    if (!registryName)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsCRT::memcpy(registryName, prefix, len);
    strcpy(registryName + len, aDllName); // no nsCRT::strcpy? for shame!
    registryName[registryNameLen] = '\0';
    *regName = registryName;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "MakeRegistryName(%s, %s, &[%s])\n",
            aDllName, prefix, *regName);
#endif

    return NS_OK;
}

nsresult
nsComponentManagerImpl::RegistryNameForLib(const char *aLibName,
                                           char **aRegistryName)
{
    return MakeRegistryName(aLibName, XPCOM_LIB_PREFIX, aRegistryName);
}

nsresult
nsComponentManagerImpl::RegistryLocationForSpec(nsIFile *aSpec,
                                                char **aRegistryName)
{
    nsresult rv;
    
    if (!mComponentsDir) 
        return NS_ERROR_NOT_INITIALIZED;

    PRBool containedIn;
    mComponentsDir->Contains(aSpec, PR_TRUE, &containedIn);

    char *persistentDescriptor;

    if (containedIn){
        
        rv = aSpec->GetPath(&persistentDescriptor);
        if (NS_FAILED(rv))
            return rv;
        
        char* relativeLocation = persistentDescriptor + mComponentsOffset + 1;
        
        rv = MakeRegistryName(relativeLocation, XPCOM_RELCOMPONENT_PREFIX, 
                              aRegistryName);
    } else {
        /* absolute names include volume info on Mac, so persistent descriptor */
        rv = aSpec->GetPath(&persistentDescriptor);
        if (NS_FAILED(rv))
            return rv;
        rv = MakeRegistryName(persistentDescriptor, XPCOM_ABSCOMPONENT_PREFIX,
                              aRegistryName);
    }

    if (persistentDescriptor)
        nsMemory::Free(persistentDescriptor);
        
    return rv;

}

nsresult
nsComponentManagerImpl::SpecForRegistryLocation(const char *aLocation,
                                                nsIFile **aSpec)
{
    nsresult rv;
    if (!aLocation || !aSpec)
        return NS_ERROR_NULL_POINTER;

    /* abs:/full/path/to/libcomponent.so */
    if (!nsCRT::strncmp(aLocation, XPCOM_ABSCOMPONENT_PREFIX, 4)) {

        nsLocalFile* file = new nsLocalFile;
        if (!file) return NS_ERROR_FAILURE;
        
        rv = file->InitWithPath(((char *)aLocation + 4));
        file->QueryInterface(NS_GET_IID(nsILocalFile), (void**)aSpec);
        return rv;
    }

    if (!nsCRT::strncmp(aLocation, XPCOM_RELCOMPONENT_PREFIX, 4)) {
        
        if (!mComponentsDir)
            return NS_ERROR_NOT_INITIALIZED;

        nsILocalFile* file = nsnull;
        rv = mComponentsDir->Clone((nsIFile**)&file);       
        
        if (NS_FAILED(rv)) return rv;
        
        rv = file->AppendRelativePath(aLocation + 4);
        *aSpec = file;
        return rv;
    }
    *aSpec = nsnull;
    return NS_ERROR_INVALID_ARG;
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

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: RegisterFactory(%s, %s)", buf,
                (aProgID ? aProgID : "(null)")));
        delete [] buf;

    }
    

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
                    "Hashing progid (%s) FAILED.", aProgID));
            return rv;
        }
    }
        
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register succeeded progid=%s.",
            aProgID ? aProgID : "<none>"));
        
    return NS_OK;
}

nsresult
nsComponentManagerImpl::RegisterComponent(const nsCID &aClass,
                                          const char *aClassName,
                                          const char *aProgID,
                                          const char *aPersistentDescriptor,
                                          PRBool aReplace,
                                          PRBool aPersist)
{
    return RegisterComponentCommon(aClass, aClassName, aProgID,
                                   aPersistentDescriptor, aReplace, aPersist,
                                   nativeComponentType);
}

nsresult
nsComponentManagerImpl::RegisterComponentWithType(const nsCID &aClass,
                                                  const char *aClassName,
                                                  const char *aProgID,
                                                  nsIFile *aSpec,
                                                  const char *aLocation,
                                                  PRBool aReplace,
                                                  PRBool aPersist,
                                                  const char *aType)
{
    return RegisterComponentCommon(aClass, aClassName, aProgID, 
                                   aLocation,
                                   aReplace, aPersist,
                                   aType);
}

/*
 * Register a component, using whatever they stuck in the nsIFile.
 */
nsresult
nsComponentManagerImpl::RegisterComponentSpec(const nsCID &aClass,
                                              const char *aClassName,
                                              const char *aProgID,
                                              nsIFile *aLibrarySpec,
                                              PRBool aReplace,
                                              PRBool aPersist)
{
    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(aLibrarySpec, getter_Copies(registryName));
    if (NS_FAILED(rv))
        return rv;

    rv = RegisterComponentWithType(aClass, aClassName, aProgID, aLibrarySpec,
                                   registryName,
                                   aReplace, aPersist,
                                   nativeComponentType);
    return rv;
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
    nsXPIDLCString registryName;
    nsresult rv = RegistryNameForLib(aDllName, getter_Copies(registryName));
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
                                                const char *aRegistryName,
                                                PRBool aReplace,
                                                PRBool aPersist,
                                                const char *aType)
{
    nsresult rv = NS_OK;
    nsFactoryEntry* newEntry = nsnull;

    nsIDKey key(aClass);
    nsFactoryEntry *entry = GetFactoryEntry(aClass, !mPrePopulationDone);
    nsCOMPtr<nsIComponentLoader> loader;
    PRBool sanity;

    // Normalize proid and classname
    const char *progID = (aProgID && *aProgID) ? aProgID : NULL;
    const char *className = (aClassName && *aClassName) ? aClassName : NULL;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
               ("nsComponentManager: RegisterComponentCommon(%s, %s, %s, %s)",
                buf,
                progID ? progID : "(null)",
                aRegistryName, aType));
        delete [] buf;
    }

    if (entry && !aReplace) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        rv = NS_ERROR_FACTORY_EXISTS;
        goto out;
    }

#ifdef USE_REGISTRY
    if (aPersist) {
        /* Add to the registry */
        rv = AddComponentToRegistry(aClass, className, progID,
                                    aRegistryName, aType);
        if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
           ("\t\tadding %s %s to registry FAILED", className, progID));
            goto out;
    }
    }
#endif

    rv = GetLoaderForType(aType, getter_AddRefs(loader));
    if (NS_FAILED(rv)) {
    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
           ("\t\tgetting loader for %s FAILED\n", aType));
        goto out;
    }

    newEntry = new nsFactoryEntry(aClass, aRegistryName, aType, loader);
    if (!newEntry) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto out;
    }

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
    if (progID
#ifdef USE_REGISTRY
        && (mPrePopulationDone || !aPersist)
#endif
        ) {
        rv = HashProgID(progID, aClass);
        if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
           ("\t\tHashProgID(%s) FAILED\n", progID));
            goto out;
    }
    }

    // Let the loader do magic things now
    rv = loader->OnRegister(aClass, aType, className, progID, aRegistryName,
                            aReplace, aPersist);
    if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tloader->OnRegister FAILED for %s \"%s\" %s %s", aType,
                className, progID, aRegistryName));
        goto out;
    }
    
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("\t\tRegisterComponentCommon() %s",
            NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));
 out:
    if (NS_FAILED(rv)) {
        if (newEntry)
            delete newEntry;
    }
    return rv;
}

nsresult
nsComponentManagerImpl::GetLoaderForType(const char *aType,
                                         nsIComponentLoader **aLoader)
{
    nsCStringKey typeKey(aType);
    nsresult rv;

    nsCOMPtr<nsIComponentLoader> loader;
    loader = NS_STATIC_CAST(nsIComponentLoader *, mLoaders->Get(&typeKey));
    if (loader) {
        // nsSupportsHashtable does the AddRef
        *aLoader = loader;
        return NS_OK;
    }

    loader = do_GetServiceFromCategory("component-loader", aType, &rv);
    if (NS_FAILED(rv))
        return rv;
    
    rv = loader->Init(this, mRegistry);

    if (NS_SUCCEEDED(rv)) {
        mLoaders->Put(&typeKey, loader);
        *aLoader = loader;
        NS_ADDREF(*aLoader);
    }
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
    PRUint32 length = strlen(aRegistryName);
    char* eRegistryName;
    rv = mRegistry->EscapeKey((PRUint8*)aRegistryName, 1, &length, (PRUint8**)&eRegistryName);
    if (rv != NS_OK)
    {
    return rv;
    }
    if (eRegistryName == nsnull)    //  No escaping required
    eRegistryName = (char*)aRegistryName;

    nsRegistryKey IDKey;
    PRInt32 nComponents = 0;
    
    /* so why do we use strings here rather than writing bytes, anyway? */
    char *cidString = aClass.ToString();
    if (!cidString)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mRegistry->AddSubtreeRaw(mCLSIDKey, cidString, &IDKey);
    if (NS_FAILED(rv))
        goto out;
    
    if (aClassName) {
        rv = mRegistry->SetStringUTF8(IDKey, classNameValueName, aClassName);
        if (NS_FAILED(rv))
            goto out;
    }

    rv = mRegistry->SetBytesUTF8(IDKey, inprocServerValueName, 
            strlen(aRegistryName) + 1, 
            (PRUint8*)aRegistryName);
    if (NS_FAILED(rv))
        goto out;

    rv = mRegistry->SetStringUTF8(IDKey, componentTypeValueName, aType);
    if (NS_FAILED(rv))
        goto out;

    if (aProgID) {
        rv = mRegistry->SetStringUTF8(IDKey, progIDValueName, aProgID);
        if (NS_FAILED(rv))
            goto out;

        nsRegistryKey progIDKey;
        rv = mRegistry->AddSubtreeRaw(mClassesKey, aProgID, &progIDKey);
        if (NS_FAILED(rv))
            goto out;
        rv = mRegistry->SetStringUTF8(progIDKey, classIDValueName, cidString);
        if (NS_FAILED(rv))
            goto out;
    }

    nsRegistryKey compKey;
    rv = mRegistry->AddSubtreeRaw(mXPCOMKey, eRegistryName, &compKey);
    
    // update component count
    rv = mRegistry->GetInt(compKey, componentCountValueName, &nComponents);
    nComponents++;
    rv = mRegistry->SetInt(compKey, componentCountValueName, nComponents);
    if (NS_FAILED(rv))
        goto out;

 out:
    // XXX if failed, undo registry adds or set invalid bit?  How?
    nsCRT::free(cidString);
    if (eRegistryName != aRegistryName)
    nsMemory::Free(eRegistryName);
    return rv;
}

nsresult
nsComponentManagerImpl::UnregisterFactory(const nsCID &aClass,
                                          nsIFactory *aFactory)
{
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
               ("nsComponentManager: UnregisterFactory(%s)", buf));
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
           ("\t\tUnregisterFactory() %s",
            NS_SUCCEEDED(res) ? "succeeded" : "FAILED"));
    return res;
}

nsresult
nsComponentManagerImpl::UnregisterComponent(const nsCID &aClass,
                                            const char *registryName)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(registryName);

    PR_EnterMonitor(mMon);

    // Remove any stored factory entries
    nsIDKey key(aClass);
    nsFactoryEntry *entry = (nsFactoryEntry *) mFactories->Get(&key);
    if (entry && entry->location && PL_strcasecmp(entry->location, registryName))
    {
        mFactories->RemoveAndDelete(&key);
        entry = NULL;
    }

#ifdef USE_REGISTRY
    // Remove registry entries for this cid
    char *cidString = aClass.ToString();
    rv = PlatformUnregister(cidString, registryName);
    delete [] cidString;
#endif
        
    PR_ExitMonitor(mMon);
        
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: Factory unregister(%s) %s.", registryName,
            NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

    return rv;
}

nsresult
nsComponentManagerImpl::UnregisterComponentSpec(const nsCID &aClass,
                                                nsIFile *aLibrarySpec)
{
    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(aLibrarySpec, getter_Copies(registryName));
    if (NS_FAILED(rv)) return rv;
    return UnregisterComponent(aClass, registryName);
}

struct CanUnload_closure {
    int when;
    nsresult status;   // this is a hack around Enumerate's void return
    nsIComponentLoader *native;
};

static PRBool PR_CALLBACK
CanUnload_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    nsIComponentLoader *loader = (nsIComponentLoader *)aData;
    struct CanUnload_closure *closure =
    (struct CanUnload_closure *)aClosure;

    if (loader == closure->native) {
#ifdef DEBUG
    fprintf(stderr, "CanUnload_enumerate: skipping native\n");
#endif
    return PR_TRUE;
    }

    closure->status = loader->UnloadAll(closure->when);
    if (NS_FAILED(closure->status))
    return PR_FALSE;
    return PR_TRUE;
}

// XXX Need to pass in aWhen and servicemanager
nsresult
nsComponentManagerImpl::FreeLibraries(void) 
{
    nsIServiceManager* serviceMgr = NULL;
    nsresult rv = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(rv)) return rv;
    rv = UnloadLibraries(serviceMgr, NS_Timer); // XXX when
    return rv;
}

// Private implementation of unloading libraries
nsresult
nsComponentManagerImpl::UnloadLibraries(nsIServiceManager *serviceMgr, PRInt32 aWhen)
{
    nsresult rv = NS_OK;

    PR_EnterMonitor(mMon);
        
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: Unloading Libraries."));

    // UnloadAll the loaders
    /* iterate over all known loaders and ask them to autoregister. */
    struct CanUnload_closure closure;
    closure.when = aWhen;
    closure.status = NS_OK;
    closure.native = mNativeComponentLoader;
    mLoaders->Enumerate(CanUnload_enumerate, &closure);

    // UnloadAll the native loader
    rv = mNativeComponentLoader->UnloadAll(aWhen);

    PR_ExitMonitor(mMon);

    return rv;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * AutoRegister(RegistrationInstant, const char *directory)
 *
 * Given a directory in the following format, this will ensure proper registration
 * of all components. No default director is looked at.
 *
 *    Directory and fullname are what NSPR will accept. For eg.
 *         WIN    y:/home/dp/mozilla/dist/bin
 *      UNIX    /home/dp/mozilla/dist/bin
 *      MAC    /Hard drive/mozilla/dist/apprunner
 *
 * This will take care not loading already registered dlls, finding and
 * registering new dlls, re-registration of modified dlls
 *
 */

struct AutoReg_closure {
    int when;
    nsIFile *spec;
    nsresult status;   // this is a hack around Enumerate's void return
    nsIComponentLoader *native;
    PRBool registered;
};

static PRBool PR_CALLBACK
AutoRegister_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    nsIComponentLoader *loader = NS_STATIC_CAST(nsIComponentLoader *, aData);
    struct AutoReg_closure *closure =
    (struct AutoReg_closure *)aClosure;

    if (loader == closure->native)
    return PR_TRUE;

    PR_ASSERT(NS_SUCCEEDED(closure->status));

    closure->status = loader->AutoRegisterComponents(closure->when,
                                                     closure->spec);
    return NS_SUCCEEDED(closure->status) ? PR_TRUE : PR_FALSE;
}

static PRBool PR_CALLBACK
RegisterDeferred_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    nsIComponentLoader *loader = NS_STATIC_CAST(nsIComponentLoader *, aData);
    struct AutoReg_closure *closure =
    (struct AutoReg_closure *)aClosure;
    PR_ASSERT(NS_SUCCEEDED(closure->status));
    
    PRBool registered;
    closure->status = loader->RegisterDeferredComponents(closure->when,
                                                         &registered);
    closure->registered |= registered;
    return NS_SUCCEEDED(closure->status) ? PR_TRUE : PR_FALSE;
}

nsresult
nsComponentManagerImpl::AutoRegister(PRInt32 when, nsIFile *inDirSpec)
{
    nsCOMPtr<nsIFile> dir;
    nsresult rv;

#ifdef DEBUG
    // testing release behaviour
    if (getenv("XPCOM_NO_AUTOREG"))
        return NS_OK;
#endif
    if (inDirSpec) 
    {
        // Use supplied components' directory   
        dir = inDirSpec;
    
        // Set components' directory for AutoRegisterInterfces to query
        NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
        if (NS_FAILED(rv)) return rv;

        // Don't care if undefining fails
        directoryService->Undefine(NS_XPCOM_COMPONENT_DIR); 
        rv = directoryService->Define(NS_XPCOM_COMPONENT_DIR, dir);
        if (NS_FAILED(rv)) return rv;
    } 
    else 
    {
        // Do default components directory
        NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = directoryService->Get(NS_XPCOM_COMPONENT_DIR, NS_GET_IID(nsIFile), getter_AddRefs(dir));
        if (NS_FAILED(rv)) return rv; // XXX translate error code?
    }

    // Force the InterfaceInfoManager to do its AutoReg first.
    nsCOMPtr<nsIInterfaceInfoManager> iim = 
        dont_AddRef(XPTI_GetInterfaceInfoManager());


    if (!iim)
        return NS_ERROR_UNEXPECTED;    
    
    // Notify observers of xpcom autoregistration start
    NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_FAILED(rv))
    {

        nsIServiceManager *mgr;    // NO COMPtr as we dont release the service manager
        rv = nsServiceManager::GetGlobalServiceManager(&mgr);
        if (NS_SUCCEEDED(rv))
        {
            (void) observerService->Notify(mgr,
                NS_ConvertASCIItoUCS2(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID).GetUnicode(),
                NS_ConvertASCIItoUCS2("Starting component registration").GetUnicode());
        }
    }

    rv = iim->AutoRegisterInterfaces();
    if (NS_FAILED(rv)) return rv;

    /* do the native loader first, so we can find other loaders */
    rv = mNativeComponentLoader->AutoRegisterComponents((PRInt32)when, dir);

    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> loaderEnum;
    rv = catman->EnumerateCategory("component-loader",
                                   getter_AddRefs(loaderEnum));
    if (NS_FAILED(rv)) return rv;

    PRBool hasMore;
    while (NS_SUCCEEDED(loaderEnum->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        if (NS_FAILED(loaderEnum->GetNext(getter_AddRefs(supports))))
            continue;

        nsCOMPtr<nsISupportsString> supStr = do_QueryInterface(supports);
        if (!supStr)
            continue;
        
        nsXPIDLCString loaderType;
        if (NS_FAILED(supStr->GetData(getter_Copies(loaderType))))
            continue;

        
        nsCOMPtr<nsIComponentLoader> loader;
        /* this will create it if we haven't already */
        GetLoaderForType(loaderType, getter_AddRefs(loader));
    }

    /* iterate over all known loaders and ask them to autoregister. */
    /* XXX convert when to nsIComponentLoader::(when) properly */
    struct AutoReg_closure closure;
    closure.when = when;
    closure.spec = dir.get();
    closure.status = NS_OK;
    closure.native = mNativeComponentLoader; // prevent duplicate autoreg
    
    mLoaders->Enumerate(AutoRegister_enumerate, &closure);
    rv = closure.status;

    if (NS_SUCCEEDED(rv))
    {
        do {
            closure.registered = PR_FALSE;
            mLoaders->Enumerate(RegisterDeferred_enumerate, &closure);
        } while (NS_SUCCEEDED(closure.status) && closure.registered);
        rv = closure.status;

    }

  	nsIServiceManager *mgr;    // NO COMPtr as we dont release the service manager
  	rv = nsServiceManager::GetGlobalServiceManager(&mgr);
  	if (NS_SUCCEEDED(rv))
  	{
      (void) observerService->Notify(mgr,
          NS_ConvertASCIItoUCS2(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID).GetUnicode(),
          NS_ConvertASCIItoUCS2("Component registration finished").GetUnicode());
  	}
  
    return rv;
}

static PRBool PR_CALLBACK
AutoRegisterComponent_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    PRBool didRegister;
    nsIComponentLoader *loader = (nsIComponentLoader *)aData;
    struct AutoReg_closure *closure =
    (struct AutoReg_closure *)aClosure;

    closure->status = loader->AutoRegisterComponent(closure->when,
                            closure->spec,
                            &didRegister);
    
    if (NS_SUCCEEDED(closure->status) && didRegister)
    return PR_FALSE; // Stop enumeration as we are done
    return PR_TRUE;
}

static PRBool PR_CALLBACK
AutoUnregisterComponent_enumerate(nsHashKey *key, void *aData, void *aClosure)
{
    PRBool didUnregister;
    nsIComponentLoader *loader = (nsIComponentLoader *)aData;
    struct AutoReg_closure *closure =
    (struct AutoReg_closure *)aClosure;

    closure->status = loader->AutoUnregisterComponent(closure->when,
                                                      closure->spec,
                                                      &didUnregister);
    if (NS_SUCCEEDED(closure->status) && didUnregister)
        return PR_FALSE; // Stop enumeration as we are done
    return PR_TRUE; // Let enumeration continue

}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(PRInt32 when,
                                              nsIFile *component)
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
nsComponentManagerImpl::AutoUnregisterComponent(PRInt32 when,
                                                nsIFile *component)
{
    struct AutoReg_closure closure;

    /* XXX convert when to nsIComponentLoader::(when) properly */
    closure.when = (PRInt32)when;
    closure.spec = component;
    closure.status = NS_OK;

    mLoaders->Enumerate(AutoUnregisterComponent_enumerate, &closure);

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
    *aRegistered = (nsnull != GetFactoryEntry(aClass, !mPrePopulationDone));
    return NS_OK;
}

static NS_IMETHODIMP
ConvertFactoryEntryToCID(nsHashKey *key, void *data, void *convert_data,
                         nsISupports **retval)
{
    nsComponentManagerImpl *compMgr = (nsComponentManagerImpl*) convert_data;
    nsresult rv;

    nsISupportsID* cidHolder;

    if(NS_SUCCEEDED(rv = 
                    compMgr->CreateInstanceByProgID(NS_SUPPORTS_ID_PROGID,
                                                    nsnull, 
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


    rv = compMgr->CreateInstanceByProgID(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                         NS_GET_IID(nsISupportsString),
                                         (void **)&strHolder);
    if(NS_SUCCEEDED(rv))
    {
        nsCStringKey *strKey = (nsCStringKey *) key;
        strHolder->SetData(strKey->GetString());
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


nsresult
nsComponentManagerImpl::GetInterface(const nsIID & uuid, void **result)
{
    nsresult rv = NS_OK;
    if (uuid.Equals(NS_GET_IID(nsIServiceManager)))
    {
        // Return the global service manager
        rv = nsServiceManager::GetGlobalServiceManager((nsIServiceManager **)result);
    }
    else
    {
        // fall through to QI as anything QIable is a superset of what canbe
        // got via the GetInterface()
        rv = QueryInterface(uuid, result);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_GetGlobalComponentManager(nsIComponentManager* *result)
{
    nsresult rv = NS_OK;

    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        // XPCOM needs initialization.
        rv = NS_InitXPCOM(NULL, NULL);
    }

    if (NS_SUCCEEDED(rv))
    {
        // NO ADDREF since this is never intended to be released.
        *result = nsComponentManagerImpl::gComponentManager;
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
