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

#include "nsComponentManager.h"
#include "nsIServiceManager.h"
#include <stdlib.h>

extern "C" NS_EXPORT nsresult
NS_RegistryGetFactory(nsISupports* servMgr, nsIFactory** aFactory);

#ifndef XP_MAC
// including this on mac causes odd link errors in static initialization
// stuff that we (pinkerton & scc) don't yet understand. If you want to
// turn this on for mac, talk to one of us.
#include <iostream.h>
#endif

#ifdef	XP_MAC
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#include <TextUtils.h>
#ifndef PR_PATH_SEPARATOR
#define PR_PATH_SEPARATOR       ':' 
#endif
#elif defined(XP_UNIX)
#ifndef PR_PATH_SEPARATOR
#define PR_PATH_SEPARATOR       ':'     // nspr doesn't like sun386i?
#endif
#else
#include "private/primpl.h"     // XXX for PR_PATH_SEPARATOR 
#endif

#include "plstr.h"
#include "prlink.h"
#include "prsystem.h"
#include "prprf.h"
#include "xcDll.h"
#include "prlog.h"
#include "prerror.h"
#include "prmem.h"

#include "prcmon.h"
#include "prthread.h" /* XXX: only used for the NSPR initialization hack (rick) */

#ifdef NS_DEBUG
PRLogModuleInfo* nsComponentManagerLog = NULL;
#endif

////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry()
    : factory(NULL), dll(NULL)
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

nsresult
nsFactoryEntry::Init(nsHashtable* dllCollection, 
                     const nsCID &aClass, const char *aLibrary,
                     PRTime lastModTime, PRUint32 fileSize)
{
    if (aLibrary == NULL) {
        return NS_OK;
    }
    cid = aClass;
      
    nsCStringKey key(aLibrary);

    // If dll not already in dllCollection, add it.
    // PR_EnterMonitor(mMon);
    dll = (nsDll *) dllCollection->Get(&key);
    // PR_ExitMonitor(mMon);
    	
    if (dll == NULL) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: New dll \"%s\".", aLibrary));

        // Add a new Dll into the nsDllStore
        dll = new nsDll(aLibrary, lastModTime, fileSize);
        if (dll == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        if (dll->GetStatus() != DLL_OK) {
            // Cant create a nsDll. Backoff.
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: New dll status error. \"%s\".", aLibrary));
            delete dll;
            dll = NULL;
        }
        else {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Adding New dll \"%s\" to mDllStore.",
                    aLibrary));

            // PR_EnterMonitor(mMon);
            dllCollection->Put(&key, (void *)dll);
            // PR_ExitMonitor(mMon);
        }
    }
    else {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Found in mDllStore \"%s\".", aLibrary));
        // XXX We found the dll in the dllCollection.
        // XXX Consistency check: dll needs to have the same
        // XXX lastModTime and fileSize. If not that would mean
        // XXX that the dll wasn't registered properly.
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// autoFree
////////////////////////////////////////////////////////////////////////////////

//
// To prevent string leaks, we are using these classes. Typical use would be
// for each ptr to be deleted, create an object of these types with that ptr.
// Once that object goes out of scope, deletion and hence memory free will
// automatically happen.
//
class autoFree
{
private:
    void *mPtr;
public:
    autoFree(void *Ptr=NULL): mPtr(Ptr) {}
    ~autoFree() { PR_FREEIF(mPtr); }
};


////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////


nsComponentManagerImpl::nsComponentManagerImpl()
    : mFactories(NULL), mProgIDs(NULL), mMon(NULL), mDllStore(NULL), mRegistry(NULL)
{
    NS_INIT_REFCNT();
}

nsresult nsComponentManagerImpl::Init(void) 
{
    if (mFactories == NULL) {
        mFactories = new nsHashtable(256, PR_TRUE);
        if (mFactories == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mProgIDs == NULL) {
        mProgIDs = new nsHashtable(256, PR_TRUE);
        if (mProgIDs == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mMon == NULL) {
        mMon = PR_NewMonitor();
        if (mMon == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mDllStore == NULL) {
        mDllStore = new nsHashtable(256, PR_TRUE);
        if (mDllStore == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if(mRegistry==NULL) {		
        nsIFactory *registryFactory = NULL;
        nsresult rv;

        nsIServiceManager *srvMgr;

        rv = nsServiceManager::GetGlobalServiceManager(&srvMgr);
        if(NS_FAILED(rv))
        {
            return rv;
        }
        rv = NS_RegistryGetFactory(srvMgr, &registryFactory);
        if (NS_SUCCEEDED(rv))
        {
            NS_DEFINE_IID(kRegistryIID, NS_IREGISTRY_IID);
            rv = registryFactory->CreateInstance(NULL, kRegistryIID,(void **)&mRegistry);
            if (NS_FAILED(rv))
            {
                return rv;
            }

            NS_RELEASE(registryFactory);
        }
    }

#ifdef NS_DEBUG
    if (nsComponentManagerLog == NULL) {
        nsComponentManagerLog = PR_NewLogModule("nsComponentManager");
    }
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("nsComponentManager: Initialized."));
#endif

#ifdef USE_NSREG

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
            printf("nsComponentManager: Creating Directory %s\n", dotMozillaDir);
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Creating Directory %s", dotMozillaDir));
        }
    }
#endif /* XP_UNIX */
    
    // No error checking on the following because the program
    // can work perfectly well even if no registry is available.

    // Open the default registry. We will keep it open forever!
    mRegistry->Open();

    // Check the version of registry. Nuke old versions.
    PlatformVersionCheck();
#endif

    return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
    if (mFactories)
        delete mFactories;
    if (mProgIDs)
        delete mProgIDs;
    if (mMon)
        PR_DestroyMonitor(mMon);
    if (mDllStore)
        delete mDllStore;
    if(mRegistry)
        NS_RELEASE(mRegistry);
}

NS_IMPL_ISUPPORTS(nsComponentManagerImpl, nsIComponentManager::GetIID());

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl: Platform methods
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_NSREG
#define USE_REGISTRY

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
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Software/Netscape/XPCOM", &xpcomKey);
    		
    if (NS_FAILED(rv))
    {        
        return rv;
    }
    	
    char *buf;
    nsresult err = mRegistry->GetString(xpcomKey, "VersionString", &buf);
    autoFree bufAutoFree(buf);

    // If there is a version mismatch or no version string, we got an old registry.
    // Delete the old repository hierarchies and recreate version string
    if (err != NS_OK || PL_strcmp(buf, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: Registry version mismatch (%s vs %s). Nuking xpcom "
                "registry hierarchy.", buf, NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING));

        // Delete the XPCOM and CLSID hierarchy
        nsIRegistry::Key netscapeKey;

        rv = mRegistry->GetSubtree(nsIRegistry::Common,"Software/Netscape", &netscapeKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Get Subtree (Software/Netscape)"));
            return rv;
        }

        rv = mRegistry->RemoveSubtreeRaw(netscapeKey, "XPCOM");
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Nuke Subtree (Software/Netscape/XPCOM)"));
            return rv;
        }

        rv = mRegistry->GetSubtree(nsIRegistry::Common,"Classes", &netscapeKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Get Subtree (Classes)"));
            return rv;
        }

        rv = mRegistry->RemoveSubtreeRaw(netscapeKey, "CLSID");
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Nuke Subtree (Classes/CLSID)"));
            return rv;
        }
        
        // Recreate XPCOM and CLSID keys
        rv = mRegistry->AddSubtree(nsIRegistry::Common,"Software/Netscape/XPCOM", &xpcomKey);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Add Subtree (Software/Netscape/XPCOM)"));
            return rv;

        }

        rv = mRegistry->AddSubtree(nsIRegistry::Common,"Classes/CLSID", NULL);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Add Subtree (Classes/CLSID)"));
            return rv;

        }

        rv = mRegistry->SetString(xpcomKey,"VersionString", NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING);
        if(NS_FAILED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Failed To Set String (Version) Under (Software/Netscape/XPCOM)"));
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
nsComponentManagerImpl::PlatformGetFileInfo(nsIRegistry::Key key,PRTime *lastModifiedTime,PRUint32 *fileSize)
{
    PR_ASSERT(lastModifiedTime);
    PR_ASSERT(fileSize);

    nsresult rv;
    PRTime *lastModPtr;
    uint32 n = sizeof(PRTime);
    rv = mRegistry->GetBytes(key, "LastModTimeStamp", (void **)&lastModPtr, &n);
    if(NS_SUCCEEDED(rv))
    {
        *lastModifiedTime = *lastModPtr;
        PR_Free(lastModPtr);
    }
    	
    int32 fsize = 0;
    rv = mRegistry->GetInt(key, "FileSize", &fsize);
    if (NS_SUCCEEDED(rv))
    {
        *fileSize = fsize;
    }
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

    nsIRegistry::Key xpcomKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Software/Netscape/XPCOM", &xpcomKey);
    if (NS_FAILED(rv))
    {        
        return rv;
    }

    nsIRegistry::Key fullnameKey;

    rv = mRegistry->GetSubtreeRaw(xpcomKey,(char *)fullname, &fullnameKey);
    if(NS_FAILED(rv))
    {        
        return rv;
    }
    	
    PRTime lastModTime=LL_ZERO;
    PRUint32 fileSize=0;

    PlatformGetFileInfo(fullnameKey,&lastModTime,&fileSize);

    	
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
    
    // XXX Gross. LongLongs dont have a serialization format. This makes
    // XXX the registry non-xp. Someone beat on the nspr people to get
    // XXX a longlong serialization function please!

    nsIRegistry::Key xpcomKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Software/Netscape/XPCOM", &xpcomKey);
    if (NS_FAILED(rv))
    {        
        return rv;
    }

    nsIRegistry::Key dllPathKey;
    rv = mRegistry->AddSubtreeRaw(xpcomKey, (char *)dll->GetFullPath(), &dllPathKey);    
    if(NS_FAILED(rv))
    {
        return rv;
    }
    	
    PRTime lastModTime = dll->GetLastModifiedTime();
    PRUint32 fileSize = dll->GetSize();
    int32 nComponents = 0;

    rv = mRegistry->SetBytes(dllPathKey,"LastModTimeStamp",&lastModTime, sizeof(lastModTime));
    rv = mRegistry->SetInt(dllPathKey,"FileSize",fileSize);
    rv = mRegistry->SetInt(dllPathKey, "ComponentsCount", nComponents);
      
    return rv;
}

nsresult
nsComponentManagerImpl::PlatformRegister(QuickRegisterData* regd, nsDll *dll)
{
    // Preconditions
    PR_ASSERT(regd != NULL);
    PR_ASSERT(regd->CIDString != NULL);
    PR_ASSERT(dll != NULL);
    PR_ASSERT(mRegistry!=NULL);

    nsIRegistry::Key classesKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Classes", &classesKey);
    if (NS_FAILED(rv))
    {        
        return rv;
    }

    nsIRegistry::Key clsIDkey;
    rv = mRegistry->AddSubtree(classesKey, "CLSID", &clsIDkey);
    if (NS_FAILED(rv)) return (rv);

    nsIRegistry::Key IDkey;
    rv = mRegistry->AddSubtreeRaw(clsIDkey, (char *)regd->CIDString, &IDkey);
    if (NS_FAILED(rv)) return (rv);


    rv = mRegistry->SetString(IDkey,"ClassName", (char *)regd->className);
    if (regd->progID)
    {
        rv = mRegistry->SetString(IDkey,"ProgID", (char *)(regd->progID));        
    }
    rv = mRegistry->SetString(IDkey, "InprocServer", (char *)dll->GetFullPath());
    
    if (regd->progID)
    {
        nsIRegistry::Key progIDKey;
        rv = mRegistry->AddSubtreeRaw(classesKey, (char *)regd->progID, &progIDKey);
        rv = mRegistry->SetString(progIDKey, "CLSID", (char *)regd->CIDString);
    }

    // XXX Gross. LongLongs dont have a serialization format. This makes
    // XXX the registry non-xp. Someone beat on the nspr people to get
    // XXX a longlong serialization function please!
    nsIRegistry::Key xpcomKey;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Software/Netscape/XPCOM", &xpcomKey);
    if(NS_FAILED(rv))
    {
        // This aint a fatal error. It causes autoregistration to fail
        // and hence the dll would be registered again the next time
        // we startup. Let us run with it.
        return NS_OK;
    }

    nsIRegistry::Key dllPathKey;
    rv = mRegistry->AddSubtreeRaw(xpcomKey,(char *)dll->GetFullPath(), &dllPathKey);

    PRTime lastModTime = dll->GetLastModifiedTime();
    int32 fileSize = dll->GetSize();

    rv = mRegistry->SetBytes(dllPathKey,"LastModTimeStamp",&lastModTime, sizeof(lastModTime));
    rv = mRegistry->SetInt(dllPathKey,"FileSize", fileSize);

    int32 nComponents = 0;
    rv = mRegistry->GetInt(dllPathKey, "ComponentsCount", &nComponents);
    nComponents++;
    rv = mRegistry->SetInt(dllPathKey,"ComponentsCount", nComponents);

    return rv;
}

nsresult
nsComponentManagerImpl::PlatformUnregister(QuickRegisterData* regd, const char *aLibrary)
{  
    PR_ASSERT(mRegistry!=NULL);

    nsIRegistry::Key classesKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Classes", &classesKey);
    if (NS_FAILED(rv))
    {        
        return rv;
    }

    nsIRegistry::Key clsIDKey;
    rv = mRegistry->AddSubtree(classesKey, "CLSID", &clsIDKey);
    if(NS_FAILED(rv)) return rv;
    	
    nsIRegistry::Key cidKey;
    rv = mRegistry->AddSubtreeRaw(clsIDKey,(char *)regd->CIDString, &cidKey);

    char *progID = NULL;
    rv = mRegistry->GetString(cidKey, "ProgID", &progID);
    if(NS_SUCCEEDED(rv))
    {
        // XXX RemoveSubtreeRaw
        mRegistry->RemoveSubtree(classesKey, progID);
        PR_FREEIF(progID);
    }

    mRegistry->RemoveSubtree(clsIDKey, (char *)regd->CIDString);
    	
    nsIRegistry::Key xpcomKey;
    rv = mRegistry->AddSubtree(nsIRegistry::Common,"Software/Netscape/XPCOM", &xpcomKey);
    if(NS_FAILED(rv)) return rv;

    nsIRegistry::Key libKey;
    rv = mRegistry->GetSubtreeRaw(xpcomKey,(char *)aLibrary, &libKey);
    if(NS_FAILED(rv)) return rv;

    // We need to reduce the ComponentCount by 1.
    // If the ComponentCount hits 0, delete the entire key.
    int32 nComponents = 0;
    rv = mRegistry->GetInt(libKey, "ComponentsCount", &nComponents);
    if(NS_FAILED(rv)) return rv;
    nComponents--;
    
    if (nComponents <= 0)
    {
        // XXX RemoveSubtreeRaw
        rv = mRegistry->RemoveSubtree(libKey, (char *)aLibrary);
    }
    else
    {
        rv = mRegistry->SetInt(libKey, "ComponentsCount", nComponents);
    }

    return rv;
}

nsresult
nsComponentManagerImpl::PlatformFind(const nsCID &aCID, nsFactoryEntry* *result)
{
    PR_ASSERT(mRegistry!=NULL);

    nsIRegistry::Key classesKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Classes", &classesKey);
    if (NS_FAILED(rv)) return rv;

    nsIRegistry::Key clsIDKey;
    rv = mRegistry->AddSubtree(classesKey, "CLSID", &clsIDKey);
    if (NS_FAILED(rv)) return rv;

    char *cidString = aCID.ToString();
    nsIRegistry::Key cidKey;
    rv = mRegistry->GetSubtreeRaw(clsIDKey, cidString, &cidKey);
    delete [] cidString;

    if (NS_FAILED(rv)) return rv;
    
    char *library = NULL;
    rv = mRegistry->GetString(cidKey, "InprocServer", &library);
    if (NS_FAILED(rv))
    {
        // Registry inconsistent. No File name for CLSID.
        return rv;
    }
    autoFree libAutoFree(library);

    // XXX Gross. LongLongs dont have a serialization format. This makes
    // XXX the registry non-xp. Someone beat on the nspr people to get
    // XXX a longlong serialization function please!
    nsIRegistry::Key xpcomKey;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Software/Netscape/XPCOM", &xpcomKey);

    // Get the library name, modifiedtime and size
    PRTime lastModTime = LL_ZERO;
    PRUint32 fileSize = 0;

    if (NS_SUCCEEDED(rv))
    {
        nsIRegistry::Key key;
        rv = mRegistry->GetSubtreeRaw(xpcomKey, library, &key);
        if (NS_SUCCEEDED(rv))
        {
            PlatformGetFileInfo(key, &lastModTime, &fileSize);			
        }
    }

    nsFactoryEntry *res = new nsFactoryEntry;
    if (res == NULL) return NS_ERROR_OUT_OF_MEMORY;
    rv = res->Init(mDllStore, aCID, library, lastModTime, fileSize);
    if (NS_FAILED(rv)) return rv;

    *result = res;
    return NS_OK;
}

nsresult
nsComponentManagerImpl::PlatformProgIDToCLSID(const char *aProgID, nsCID *aClass) 
{
    PR_ASSERT(aClass != NULL);
    PR_ASSERT(mRegistry);

    nsIRegistry::Key classesKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Classes", &classesKey);
    if (NS_FAILED(rv)) return rv;
    	
    nsIRegistry::Key progIDKey;
    rv = mRegistry->GetSubtreeRaw(classesKey, (char *)aProgID, &progIDKey);
    if (NS_FAILED(rv)) return rv;

    char *cidString;
    rv = mRegistry->GetString(progIDKey, "CLSID", &cidString);
    if(NS_FAILED(rv)) return rv;
    autoFree cidAutoFree(cidString);

    if (!(aClass->Parse(cidString)))
    {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

nsresult
nsComponentManagerImpl::PlatformCLSIDToProgID(nsCID *aClass,
                                              char* *aClassName, char* *aProgID)
{
    	
    PR_ASSERT(aClass);
    PR_ASSERT(mRegistry);

    nsIRegistry::Key classesKey;
    nsresult rv;
    rv = mRegistry->AddSubtree(nsIRegistry::Common, "Classes", &classesKey);
    if (NS_FAILED(rv)) return rv;

    char* cidStr = aClass->ToString();
    nsIRegistry::Key cidKey;
    rv = mRegistry->GetSubtreeRaw(classesKey,cidStr,&cidKey);
    if(NS_FAILED(rv)) return rv;
    PR_FREEIF(cidStr);

    char* classnameString;
    rv = mRegistry->GetString(cidKey, "ClassName", &classnameString);
    if(NS_FAILED(rv)) return rv;
    *aClassName = classnameString;

    char* progidString;
    rv = mRegistry->GetString(cidKey,"ProgID",&progidString);
    if (NS_FAILED(rv)) return rv;
    *aProgID = progidString;

    return NS_OK;

}

#endif // USE_NSREG

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
               ("nsComponentManager: + Loading \"%s\".", aEntry->dll->GetFullPath()));
        if (aEntry->dll->Load() == PR_FALSE)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("nsComponentManager: Library load unsuccessful."));
            
            char errorMsg[1024] = "Cannot get error from nspr. Not enough memory.";
            if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
                PR_GetErrorText(errorMsg);
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Load(%s) FAILED with error:%s", aEntry->dll->GetFullPath(), errorMsg));
#if defined(XP_UNIX) || defined(XP_PC)
            // Put the error message on the screen.
            printf("**************************************************\n"
    		   "nsComponentManager: Load(%s) FAILED with error: %s\n"
            	   "**************************************************\n",
    		   aEntry->dll->GetFullPath(), errorMsg);
#endif

            return NS_ERROR_FAILURE;
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
        char* className = NULL;
        char* progID = NULL;
        nsresult rv;

        // XXX dp, warren: deal with this!
#if 0
        rv = CLSIDToProgID(&aEntry->cid, &className, &progID);
        // if CLSIDToProgID fails, just pass null to NSGetFactory
#endif

        nsIServiceManager* serviceMgr = NULL;
        rv = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
        if (NS_FAILED(rv)) return rv;

        rv = proc(serviceMgr, aEntry->cid, className, progID, aFactory);
        if (NS_FAILED(rv)) return rv;

        if (className)
            delete[] className;
        if (progID)
            delete[] progID;
        return rv;
    }
    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
           ("nsComponentManager: NSGetFactory entrypoint not found."));
    return NS_ERROR_FACTORY_NOT_LOADED;
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
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: FindFactory(%s)", buf);
        delete [] buf;
    }
#endif
    	
    PR_ASSERT(aFactory != NULL);
    	
    PR_EnterMonitor(mMon);
    	
    nsIDKey key(aClass);
    nsFactoryEntry *entry = (nsFactoryEntry*) mFactories->Get(&key);
    	
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
    	
#ifdef USE_REGISTRY
    if (entry == NULL)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("\t\tnot found in factory cache. Looking in registry"));
        nsresult rv = PlatformFind(aClass, &entry);

        // XXX This should go into PlatformFind(), and PlatformFind()
        // should just become a static method on nsComponentManager.

        // If we got one, cache it in our hashtable
        if (NS_SUCCEEDED(rv))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("\t\tfound in registry."));
            mFactories->Put(&key, entry);
        }
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("\t\tfound in factory cache."));
    }
#endif
    	
    PR_ExitMonitor(mMon);
    	
    if (entry != NULL)
    {
        if ((entry)->factory == NULL)
        {
            res = LoadFactory(entry, aFactory);
        }
        else
        {
            *aFactory = entry->factory;
            NS_ADDREF(*aFactory);
            res = NS_OK;
        }
    }
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFindFactory() %s",
            NS_SUCCEEDED(res) ? "succeeded" : "FAILED"));
    	
    return res;
}

/**
 * ProgIDToCLSID()
 *
 * Mapping function from a ProgID to a classID. Directly talks to the registry.
 *
 * XXX We need to add a cache this translation as this is done a lot.
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

#define NS_NO_CID { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }
    static NS_DEFINE_CID(kNoCID, NS_NO_CID);

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

#ifdef NS_DEBUG
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
#endif
    	
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

#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass->ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: CLSIDToProgID(%s)->%s", buf,
                NS_SUCCEEDED(res) ? *aProgID : "[FAILED]"));
        delete [] buf;
    }
#endif

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
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: CreateInstance(%s)", buf);
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
               ("\t\tCreateInstance() succeeded."));		
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

#if 0
nsresult 
nsComponentManagerImpl::CreateInstance2(const nsCID &aClass, 
                                        nsISupports *aDelegate,
                                        const nsIID &aIID,
                                        void *aSignature,
                                        void **aResult)
{
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: Creating Instance.");
        PR_LogPrint("nsComponentManager: + %s.", 
                    buf);
        PR_LogPrint("nsComponentManager: + Signature = %p.",
                    aSignature);
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
        nsIFactory2 *factory2 = NULL;
        res = NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT;
    		
        factory->QueryInterface(kFactory2IID, (void **) &factory2);
    		
        if (factory2 != NULL)
        {
            res = factory2->CreateInstance2(aDelegate, aIID, aSignature, aResult);
            NS_RELEASE(factory2);
        }
    		
        NS_RELEASE(factory);
        return res;
    }
    	
    return NS_ERROR_FACTORY_NOT_REGISTERED;
}
#endif /* 0 */

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
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: RegisterFactory(%s, factory), replace = %d.", buf, (int)aReplace);
        delete [] buf;
    }
#endif

    nsIFactory *old = NULL;
    FindFactory(aClass, &old);

    if (old != NULL)
    {
        NS_RELEASE(old);
        if (!aReplace)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tFactory already registered."));
            return NS_ERROR_FACTORY_EXISTS;
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tdeleting old Factory Entry."));
        }
    }
    	
    PR_EnterMonitor(mMon);

    nsIDKey key(aClass);
    nsFactoryEntry* entry = new nsFactoryEntry(aClass, aFactory);
    if (entry == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    mFactories->Put(&key, entry);
    	
    PR_ExitMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register succeeded."));
    	
    return NS_OK;
}

nsresult
nsComponentManagerImpl::RegisterComponent(const nsCID &aClass,
                                          const char *aClassName,
                                          const char *aProgID,
                                          const char *aLibrary,
                                          PRBool aReplace,
                                          PRBool aPersist)
{
    nsresult rv = NS_OK;
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: RegisterComponent(%s, %s, %s, %s), replace = %d, persist = %d.",
                    buf, aClassName, aProgID, aLibrary, (int)aReplace, (int)aPersist);
        delete [] buf;
    }
#endif

    nsIFactory *old = NULL;
    FindFactory(aClass, &old);
    	
    if (old != NULL)
    {
        NS_RELEASE(old);
        if (!aReplace)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tFactory already registered."));
            return NS_ERROR_FACTORY_EXISTS;
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tdeleting registered Factory."));
        }
    }
    	
    PR_EnterMonitor(mMon);
    	
#ifdef USE_REGISTRY
    if (aPersist == PR_TRUE)
    {
        // Add it to the registry
        nsDll *dll = new nsDll(aLibrary);
        if (dll == NULL) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto done;
        }
        // XXX temp hack until we get the dll to give us the entire
        // XXX NSQuickRegisterClassData
        QuickRegisterData cregd = {0};
        cregd.CIDString = aClass.ToString();
        cregd.className = aClassName;
        cregd.progID = aProgID;
        PlatformRegister(&cregd, dll);
        delete [] (char *)cregd.CIDString;
        delete dll;
    } 
    else
#endif
    {
        nsDll *dll = new nsDll(aLibrary);
        if (dll == NULL) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto done;
        }
        nsIDKey key(aClass);
        nsFactoryEntry* entry = new nsFactoryEntry;
        if (entry == NULL) {
            delete dll;
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto done;
        }
        rv = entry->Init(mDllStore, aClass, aLibrary,
                         dll->GetLastModifiedTime(), dll->GetSize());
        delete dll;
        if (NS_FAILED(rv)) {
            goto done;
        }
        mFactories->Put(&key, entry);
    }
    	
 done:	
    PR_ExitMonitor(mMon);
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register %s.",
            rv == NS_OK ? "succeeded" : "failed"));
    return rv;
}

nsresult
nsComponentManagerImpl::UnregisterFactory(const nsCID &aClass,
                                          nsIFactory *aFactory)
{
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: Unregistering Factory.");
        PR_LogPrint("nsComponentManager: + %s.", buf);
        delete [] buf;
    }
#endif
    	
    nsIDKey key(aClass);
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
    nsFactoryEntry *old = (nsFactoryEntry *) mFactories->Get(&key);
    if (old != NULL)
    {
        if (old->factory == aFactory)
        {
            PR_EnterMonitor(mMon);
            old = (nsFactoryEntry *) mFactories->Remove(&key);
            PR_ExitMonitor(mMon);
            delete old;
            res = NS_OK;
        }

    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: ! Factory unregister %s.", 
            res == NS_OK ? "succeeded" : "failed"));
    	
    return res;
}

nsresult
nsComponentManagerImpl::UnregisterComponent(const nsCID &aClass,
                                            const char *aLibrary)
{
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: Unregistering Factory.");
        PR_LogPrint("nsComponentManager: + %s in \"%s\".", buf, aLibrary);
        delete [] buf;
    }
#endif
    	
    nsIDKey key(aClass);
    nsFactoryEntry *old = (nsFactoryEntry *) mFactories->Get(&key);
    	
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
    	
    PR_EnterMonitor(mMon);
    	
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
            nsFactoryEntry *entry = (nsFactoryEntry *) mFactories->Remove(&key);
            delete entry;
            res = NS_OK;
        }
#ifdef USE_REGISTRY
        // XXX temp hack until we get the dll to give us the entire
        // XXX NSQuickRegisterClassData
        QuickRegisterData cregd = {0};
        cregd.CIDString = aClass.ToString();
        res = PlatformUnregister(&cregd, aLibrary);
        delete [] (char *)cregd.CIDString;
#endif
    }
    	
    PR_ExitMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: ! Factory unregister %s.", 
            res == NS_OK ? "succeeded" : "failed"));
    	
    return res;
}

nsresult
nsComponentManagerImpl::UnregisterFactory(const nsCID &aClass,
                                          const char *aLibrary)
{
#ifdef NS_DEBUG
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: Unregistering Factory.");
        PR_LogPrint("nsComponentManager: + %s in \"%s\".", buf, aLibrary);
        delete [] buf;
    }
#endif
    	
    nsIDKey key(aClass);
    nsFactoryEntry *old = (nsFactoryEntry *) mFactories->Get(&key);
    	
    nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
    	
    PR_EnterMonitor(mMon);
    	
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
            nsFactoryEntry *entry = (nsFactoryEntry *) mFactories->Remove(&key);
            delete entry;
            res = NS_OK;
        }
#ifdef USE_REGISTRY
        // XXX temp hack until we get the dll to give us the entire
        // XXX NSQuickRegisterClassData
        QuickRegisterData cregd = {0};
        cregd.CIDString = aClass.ToString();
        res = PlatformUnregister(&cregd, aLibrary);
        delete [] (char *)cregd.CIDString;
#endif
    }
    	
    PR_ExitMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: ! Factory unregister %s.", 
            res == NS_OK ? "succeeded" : "failed"));
    	
    return res;
}

static PRBool
nsFreeLibraryEnum(nsHashKey *aKey, void *aData, void* closure) 
{
    nsFactoryEntry *entry = (nsFactoryEntry *) aData;
    nsIServiceManager* serviceMgr = (nsIServiceManager*)closure;
    	
    if (entry->dll && entry->dll->IsLoaded() == PR_TRUE)
    {
        nsCanUnloadProc proc = (nsCanUnloadProc) entry->dll->FindSymbol("NSCanUnload");
        if (proc != NULL) {
            nsresult rv = proc(serviceMgr);
            if (NS_FAILED(rv)) {
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                       ("nsComponentManager: + Unloading \"%s\".", entry->dll->GetFullPath()));
                entry->dll->Unload();
            }
        }
    }
    	
    return PR_TRUE;
}

nsresult
nsComponentManagerImpl::FreeLibraries(void) 
{
    PR_EnterMonitor(mMon);
    	
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: Freeing Libraries."));

    nsIServiceManager* serviceMgr = NULL;
    nsresult rv = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(rv)) return rv;
    mFactories->Enumerate(nsFreeLibraryEnum, serviceMgr);
    	
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
nsComponentManagerImpl::AutoRegister(RegistrationTime when, const char* dir)
{

    if (dir != NULL)
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsComponentManager: Autoregistration begins. dir = %s", dir));
        SyncComponentsInDir(when, dir);
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsComponentManager: Autoregistration ends.", dir));
    }

#ifdef XP_MAC
    // The below code is being moved out into the applications using using
    // nsSpecialSystemDirectory platform by platform.

    // get info for the the current process to determine the directory its located in
    CInfoPBRec	catInfo;
    Handle		pathH;
    OSErr			err;
    ProcessSerialNumber	psn;
    ProcessInfoRec	pInfo;
    FSSpec			appFSSpec;
    FSSpec			tempSpec;
    long              theDirID;
    Str255			name;

    if (!(err = GetCurrentProcess(&psn)))
    {
        // initialize ProcessInfoRec before calling GetProcessInformation() or die horribly.
        pInfo.processName = nil;
        pInfo.processAppSpec = &tempSpec;
        pInfo.processInfoLength = sizeof(ProcessInfoRec);
        if (!(err = GetProcessInformation(&psn, &pInfo)))
        {
            appFSSpec = *(pInfo.processAppSpec);
            if ((pathH = NewHandle(1)) != NULL)
            {
                **pathH = '\0';						// initially null terminate the string
                HNoPurge(pathH);
                HUnlock(pathH);
                theDirID = appFSSpec.parID;
                do
                {
                    catInfo.dirInfo.ioCompletion = NULL;
                    catInfo.dirInfo.ioNamePtr = (StringPtr)&name;
                    catInfo.dirInfo.ioVRefNum = appFSSpec.vRefNum;
                    catInfo.dirInfo.ioDrDirID = theDirID;
                    catInfo.dirInfo.ioFDirIndex = -1;		// -1 = query dir in ioDrDirID
                    if (!(err = PBGetCatInfoSync(&catInfo)))
                    {
                        // build up a Unix style pathname due to NSPR
                        // XXX Note: this breaks if any of the parent
                        // directories contain a "slash" (blame NSPR)

                        Munger(pathH, 0L, NULL, 0L, (const void *)&name[1], (long)name[0]);	// prepend dir name
                        Munger(pathH, 0L, NULL, 0L, "/", 1);					// prepend slash

                        // move up to parent directory
                        theDirID = catInfo.dirInfo.ioDrParID;
                    }
                } while ((!err) && (catInfo.dirInfo.ioDrDirID != 2));	// 2 = root
                if (!err)
                {
                    Munger(pathH, GetHandleSize(pathH)-1, NULL, 0L, "/components", 11);	// append "/components"
                    HLock(pathH);
                    SyncComponentsInDir(when, (const char *)(*pathH));
                    HUnlock(pathH);
                }
                DisposeHandle(pathH);
            }
        }
    }
#endif /* XP_MAC */
    return NS_OK;
}


nsresult
nsComponentManagerImpl::SyncComponentsInDir(RegistrationTime when, const char *dir)
{
    PRDir *prdir = PR_OpenDir(dir);
    if (prdir == NULL)
        return NS_ERROR_FAILURE;
    	
    // Create a buffer that has dir/ in it so we can append
    // the filename each time in the loop
    char fullname[NS_MAX_FILENAME_LEN];
    PL_strncpyz(fullname, dir, sizeof(fullname));
    unsigned int n = strlen(fullname);
    if (n+1 < sizeof(fullname))
    {
        fullname[n] = '/';
        n++;
    }
    char *filepart = fullname + n;
    	
    PRDirEntry *dirent = NULL;
    while ((dirent = PR_ReadDir(prdir, PR_SKIP_BOTH)) != NULL)
    {
        PL_strncpyz(filepart, dirent->name, sizeof(fullname)-n);
        nsresult ret = AutoRegisterComponent(when, fullname);
        if (NS_FAILED(ret) && ret == NS_ERROR_IS_DIR) {
            SyncComponentsInDir(when, fullname);
        }
    } // foreach file
    PR_CloseDir(prdir);
    return NS_OK;
}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(RegistrationTime when, const char *fullname)
{
    const char *ValidDllExtensions[] = {
        ".dll",	/* Windows */
        ".dso",	/* Unix */
        ".so",	/* Unix */
        ".sl",	/* Unix: HP */
        ".shlb",	/* Mac ? */
        ".dlm",	/* new for all platforms */
        NULL
    };
    	
    	
    PRFileInfo statbuf;
    if (PR_GetFileInfo(fullname,&statbuf) != PR_SUCCESS)
    {
        // Skip files that cannot be stat
        return NS_ERROR_FAILURE;
    }

    if (statbuf.type == PR_FILE_DIRECTORY)
    {
        // Cant register a directory
        return NS_ERROR_IS_DIR;
    }
    else if (statbuf.type != PR_FILE_FILE)
    {
        // Skip non-files
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
    }
    	
    // Check if dll is one that we have already seen
    nsCStringKey key(fullname);
    nsDll *dll = (nsDll *) mDllStore->Get(&key);
    nsresult rv = NS_OK;
    if (dll == NULL)
    {
        // XXX Create nsDll for this from registry and
        // XXX add it to our dll cache mDllStore.
#ifdef USE_REGISTRY
        rv = PlatformCreateDll(fullname, &dll);
#endif /* USE_REGISTRY */
    }
    	
    if (NS_SUCCEEDED(rv))
    {
        // Make sure the dll is OK
        if (dll->GetStatus() != NS_OK)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsComponentManager: + nsDll not NS_OK \"%s\". Skipping...",
                    dll->GetFullPath()));
            return NS_ERROR_FAILURE;
        }
    		
        // We already have seen this dll. Check if this dll changed
        if (LL_EQ(dll->GetLastModifiedTime(), statbuf.modifyTime) &&
            (dll->GetSize() == statbuf.size))
        {
            // Dll hasn't changed. Skip.
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsComponentManager: + nsDll not changed \"%s\". Skipping...",
                    dll->GetFullPath()));
            return NS_OK;
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
                PRBool res = proc(this /*, PR_TRUE*/);
                if (res)
                {
                    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                           ("nsComponentManager: + Unloading \"%s\".",
                            dll->GetFullPath()));
                    dll->Unload();
                }
                else
                {
                    // THIS IS THE WORST SITUATION TO BE IN.
                    // Dll doesn't want to be unloaded. Cannot re-register
                    // this dll.
                    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                           ("nsComponentManager: *** Dll already loaded. "
                            "Cannot unload either. Hence cannot re-register "
                            "\"%s\". Skipping...", dll->GetFullPath()));
                    return NS_ERROR_FAILURE;
                }
            }
            else {
                // dll doesn't have a CanUnload proc. Guess it is
                // ok to unload it.
                dll->Unload();
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                       ("nsComponentManager: + Unloading \"%s\". (no CanUnloadProc).",
                        dll->GetFullPath()));
    				
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
                    "\"%s\". Skipping...", dll->GetFullPath()));
            return NS_ERROR_FAILURE;
        }
    } // dll != NULL
    else
    {
        // Create and add the dll to the mDllStore
        // It is ok to do this even if the creation of nsDll
        // didnt succeed. That way we wont do this again
        // when we encounter the same dll.
        dll = new nsDll(fullname);
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
                "\"%s\". Skipping...", dll->GetFullPath()));
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
                "\"%s\". Skipping...", dll->GetFullPath()));
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

    nsresult res = NS_ERROR_FAILURE;
    	
    if (dll->Load() == PR_FALSE)
    {
        // Cannot load. Probably not a dll.
        char errorMsg[1024] = "Cannot get error from nspr. Not enough memory.";
        if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
            PR_GetErrorText(errorMsg);
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: SelfRegisterDll(%s) Load FAILED with error:%s", dll->GetFullPath(), errorMsg));
#if defined(XP_UNIX) || defined(XP_PC)
    	// Put the error message on the screen.
        printf("**************************************************\n"
               "nsComponentManager: Load(%s) FAILED with error: %s\n"
               "**************************************************\n",
               dll->GetFullPath(), errorMsg);
#endif

        return(NS_ERROR_FAILURE);
    }
    	
    nsRegisterProc regproc = (nsRegisterProc)dll->FindSymbol("NSRegisterSelf");

    if (regproc == NULL)
    {
        // Smart registration
        QuickRegisterData* qr =
            (QuickRegisterData*)dll->FindSymbol(NS_QUICKREGISTER_DATA_SYMBOL);
        if (qr == NULL)
        {
            res = NS_ERROR_NO_INTERFACE;
        }
        else
        {
            // XXX register the quick registration data on behalf of the dll
            // XXX for now return failure
            res = NS_ERROR_FAILURE;
        }
    }
    else
    {
        // Call the NSRegisterSelfProc to enable dll registration
        nsIServiceManager* serviceMgr = NULL;
        res = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
        if (NS_SUCCEEDED(res)) {
            res = regproc(serviceMgr, dll->GetFullPath());
        }
    }
    dll->Unload();
    return res;
}

nsresult
nsComponentManagerImpl::SelfUnregisterDll(nsDll *dll)
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
        QuickRegisterData* qr = (QuickRegisterData*)
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
        if (NS_SUCCEEDED(res)) {
            res = unregproc(serviceMgr, dll->GetFullPath());
        }
    }
    dll->Unload();
    return res;
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
