/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
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

#include "prmem.h"
#include "prerror.h"
#include "prsystem.h"           // PR_GetDirectorySeparator
#include "nsNativeComponentLoader.h"
#include "nsComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"
#include "xcDll.h"
#include "nsHashtable.h"
#include "nsHashtableEnumerator.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"

#include "nsIObserverService.h"
#include "nsObserverService.h"

#include "nsITimelineService.h"

#ifdef  XP_MAC  // sdagley dougt fix
#include <Files.h>
#include <Errors.h>
#include "nsILocalFileMac.h"
#endif

#define PRINT_CRITICAL_ERROR_TO_SCREEN 1
#define USE_REGISTRY 1
#define XPCOM_USE_NSGETFACTORY 1

// Logging of debug output
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"
extern PRLogModuleInfo *nsComponentManagerLog;

nsNativeComponentLoader::nsNativeComponentLoader() :
    mRegistry(nsnull), mCompMgr(nsnull), mDllStore(nsnull)
{
    NS_INIT_REFCNT();
}

static PRBool PR_CALLBACK
nsDll_Destroy(nsHashKey *aKey, void *aData, void* closure)
{
    nsDll* entry = NS_STATIC_CAST(nsDll*, aData);
    delete entry;
    return PR_TRUE;
}

nsNativeComponentLoader::~nsNativeComponentLoader()
{
    mRegistry = nsnull;
    mCompMgr = nsnull;

    delete mDllStore;
}
    
NS_IMPL_ISUPPORTS1(nsNativeComponentLoader, nsIComponentLoader);

NS_IMETHODIMP
nsNativeComponentLoader::GetFactory(const nsIID & aCID,
                                    const char *aLocation,
                                    const char *aType,
                                    nsIFactory **_retval)
{
    NS_TIMELINE_MARKV(("nsNativeComponentLoader::GetFactory(%s)...",
                       aLocation));
    NS_TIMELINE_INDENT();
    NS_TIMELINE_START_TIMER("nsNativeComponentLoader::GetFactory");

    nsresult rv;

    if (!_retval)
        return NS_ERROR_NULL_POINTER;
    
    /* use a hashtable of WeakRefs to store the factory object? */

    /* Should this all live in xcDll? */
    nsDll *dll;
    PRInt64 mod = LL_Zero(), size = LL_Zero();
    rv = CreateDll(nsnull, aLocation, &mod, &size, &dll);
    if (NS_FAILED(rv))
        return rv;

    if (!dll)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!dll->IsLoaded()) {
        PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
               ("nsNativeComponentLoader: loading \"%s\"",
                dll->GetDisplayPath()));

        if (!dll->Load()) {

            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("nsNativeComponentLoader: load FAILED"));
        
            char errorMsg[1024] = "<unknown; can't get error from NSPR>";

            if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
                PR_GetErrorText(errorMsg);

            DumpLoadError(dll, "GetFactory", errorMsg);

            return NS_ERROR_FAILURE;
        }
    }

    /* Get service manager for factory */
    nsCOMPtr<nsIServiceManager> serviceMgr;
    rv = NS_GetServiceManager(getter_AddRefs(serviceMgr));
    if (NS_FAILED(rv))
        return rv;      // XXX translate error code?
    
    rv = GetFactoryFromModule(dll, aCID, _retval);
#ifdef XPCOM_USE_NSGETFACTORY
    if (NS_FAILED(rv)) {
        if (rv == NS_ERROR_FACTORY_NOT_LOADED) {
            rv = GetFactoryFromNSGetFactory(dll, aCID, serviceMgr, _retval);
        }
    }
#endif
    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
           ("nsNativeComponentLoader: Factory creation %s for %s",
            (NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"),
            aLocation));

    // If the dll failed to get us a factory. But the dll registered that
    // it would be able to create a factory for this CID. mmh!
    // We cannot just delete the dll as the dll could be hosting
    // other CID for which factory creation can pass.
    // We will just let it be. The effect will be next time we try
    // creating the object, we will query the dll again. Since the
    // dll is loaded, this aint a big hit. So for optimized builds
    // this is ok to limp along.
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Factory creation failed");
    
    NS_TIMELINE_STOP_TIMER("nsNativeComponentLoader::GetFactory");
    NS_TIMELINE_OUTDENT();
    NS_TIMELINE_MARKV(("...nsNativeComponentLoader::GetFactory(%s) done",
                       aLocation));
    NS_TIMELINE_MARK_TIMER("nsNativeComponentLoader::GetFactory");

    return rv;
}

NS_IMETHODIMP
nsNativeComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg)
{
    nsresult rv;

    mCompMgr = aCompMgr;
    mRegistry = do_QueryInterface(aReg);
    if (!mCompMgr || !mRegistry)
        return NS_ERROR_INVALID_ARG;

    rv = mRegistry->GetSubtree(nsIRegistry::Common, xpcomComponentsKeyName,
                               &mXPCOMKey);
    if (NS_FAILED(rv))
        return rv;

    if (!mDllStore) {
        mDllStore = new nsObjectHashtable(nsnull, nsnull, // never copy
                                          nsDll_Destroy, nsnull,
                                          256, /* Thead Safe */ PR_TRUE);
        if (!mDllStore)
            return NS_ERROR_OUT_OF_MEMORY;
    }


    // Read in all dll entries and populate the mDllStore
    nsCOMPtr<nsIEnumerator> dllEnum;
    rv = mRegistry->EnumerateSubtrees( mXPCOMKey, getter_AddRefs(dllEnum));
    if (NS_FAILED(rv)) return rv;

    rv = dllEnum->First();
    for (; NS_SUCCEEDED(rv) && (dllEnum->IsDone() != NS_OK); (rv = dllEnum->Next()))
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
        nsXPIDLCString library;
        rv = node->GetNameUTF8(getter_Copies(library));

        if (NS_FAILED(rv)) continue;

        char* uLibrary;
        char* eLibrary = (char*)library.operator const char*();
        PRUint32 length = strlen(eLibrary);
        rv = mRegistry->UnescapeKey((PRUint8*)eLibrary, 1, &length, (PRUint8**)&uLibrary);
        
        if (NS_FAILED(rv)) continue;

        if (uLibrary == nsnull)
            uLibrary = eLibrary;

        // Get key associated with library
        nsRegistryKey libKey;
        rv = node->GetKey(&libKey);
        if (NS_SUCCEEDED(rv)) //  Cannot continue here, because we have to free unescape
        {
            // Create nsDll with this name
            nsDll *dll = NULL;
            PRInt64 lastModTime;
            PRInt64 fileSize;
            GetRegistryDllInfo(libKey, &lastModTime, &fileSize);
            rv = CreateDll(NULL, uLibrary, &lastModTime, &fileSize, &dll);
        }
        if (uLibrary && (uLibrary != eLibrary))
            nsMemory::Free(uLibrary);

        if (NS_FAILED(rv)) continue;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsNativeComponentLoader::AutoRegisterComponents(PRInt32 aWhen,
                                                nsIFile *aDirectory)
{
#ifdef DEBUG
    /* do we _really_ want to print this every time? */
    printf("nsNativeComponentLoader: autoregistering begins.\n");
#endif

    nsresult rv = RegisterComponentsInDir(aWhen, aDirectory);

#ifdef DEBUG
    printf("nsNativeComponentLoader: autoregistering %s\n",
           NS_FAILED(rv) ? "FAILED" : "succeeded");
#endif

    return rv;
}

nsresult
nsNativeComponentLoader::RegisterComponentsInDir(PRInt32 when,
                                                 nsIFile *dir)
{
    nsresult rv = NS_ERROR_FAILURE;
    PRBool isDir = PR_FALSE;

#if 0
    // Going to many of these checks is a performance hit on the mac.
    // Since these routines are called relatively infrequently and
    // we will fail anyway down the line if a directory aint there,
    // we are commenting this check out.

    // Make sure we are dealing with a directory
    rv = dir->IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;

    if (!isDir)
        return NS_ERROR_INVALID_ARG;
#endif /* 0 */

    // Create a directory iterator
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = dir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    
    if (NS_FAILED(rv)) return rv;
    
    // whip through the directory to register every file
    nsCOMPtr<nsIFile> dirEntry;
    PRBool more = PR_FALSE;

    rv = dirIterator->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
    while (more == PR_TRUE)
    {
        rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
        if (NS_SUCCEEDED(rv))
        {
            rv = dirEntry->IsDirectory(&isDir);
            if (NS_SUCCEEDED(rv))
            {
                if (isDir == PR_TRUE)
                {
                    // This is a directory. Grovel for components into the directory.
                    rv = RegisterComponentsInDir(when, dirEntry);
                }
                else
                {
                    PRBool registered;
                    // This is a file. Try to register it.
                    rv = AutoRegisterComponent(when, dirEntry, &registered);
                }
            }
        }
        rv = dirIterator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
    }
    
    return rv;
}

static nsresult PR_CALLBACK
nsFreeLibrary(nsDll *dll, nsIServiceManager *serviceMgr, PRInt32 when)
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

    PRBool canUnload = PR_FALSE;

    // Get the module object
    nsCOMPtr<nsIModule> mobj;
    /* XXXshaver cheat and use the global component manager */
    rv = dll->GetModule(NS_STATIC_CAST(nsIComponentManager*, nsComponentManagerImpl::gComponentManager),
                        getter_AddRefs(mobj));
    if (NS_SUCCEEDED(rv))
    {
        rv = mobj->CanUnload(nsComponentManagerImpl::gComponentManager, &canUnload);
    }
#ifndef OBSOLETE_MODULE_LOADING
    else
    {
        // Try the old method of module unloading
        nsCanUnloadProc proc = (nsCanUnloadProc)dll->FindSymbol("NSCanUnload");
        if (proc)
        {
            canUnload = proc(serviceMgr);
            rv = NS_OK;    // No error status returned by call.
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("nsNativeComponentLoader: Unload cant get nsIModule or NSCanUnload for %s",
                    dll->GetDisplayPath()));
            return rv;
        }
    }
#endif /* OBSOLETE_MODULE_LOADING */

    mobj = nsnull; // Release our reference to the module object

    // When shutting down, whether we can unload the dll or not,
    // we will shutdown the dll to release any memory it has got
    if (when == nsIComponentManager::NS_Shutdown)
    {
        dll->Shutdown();
    }
        
    // Check error status on CanUnload() call
    if (NS_FAILED(rv))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("nsNativeComponentLoader: nsIModule::CanUnload() returned error for %s.",
                dll->GetDisplayPath()));
        return rv;
    }

    if (canUnload)
    {
        if (dllMarkedForUnload)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, 
                   ("nsNativeComponentLoader: + Unloading \"%s\".", dll->GetDisplayPath()));
#if 0
            // XXX dlls aren't counting their outstanding instances correctly
            // XXX hence, dont unload until this gets enforced.
            rv = dll->Unload();
#endif /* 0 */
        }
        else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
                   ("nsNativeComponentLoader: Ready for unload \"%s\".", dll->GetDisplayPath()));
        }
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
               ("nsNativeComponentLoader: NOT ready for unload %s", dll->GetDisplayPath()));
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

struct freeLibrariesClosure
{
    nsIServiceManager *serviceMgr;
    PRInt32 when;
};

static PRBool PR_CALLBACK
nsFreeLibraryEnum(nsHashKey *aKey, void *aData, void* closure) 
{
    nsDll *dll = (nsDll *) aData;
    struct freeLibrariesClosure *callData = (struct freeLibrariesClosure *) closure;
    nsFreeLibrary(dll,
                  (callData ? callData->serviceMgr : NULL),
                  (callData ? callData->when : nsIComponentManager::NS_Timer));
    return PR_TRUE;
}

/*
 * SelfRegisterDll
 *
 * Given a dll abstraction, this will load, selfregister the dll and
 * unload the dll.
 *
 */
nsresult
nsNativeComponentLoader::SelfRegisterDll(nsDll *dll,
                                         const char *registryLocation,
                                         PRBool deferred)
{
    // Precondition: dll is not loaded already, unless we're deferred
    PR_ASSERT(deferred || dll->IsLoaded() == PR_FALSE);

    nsresult res;
    nsCOMPtr<nsIServiceManager> serviceMgr;
    res = NS_GetServiceManager(getter_AddRefs(serviceMgr));
    if (NS_FAILED(res)) return res;

    if (dll->Load() == PR_FALSE)
    {
        // Cannot load. Probably not a dll.
        char errorMsg[1024] = "Cannot get error from nspr. Not enough memory.";
        if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
            PR_GetErrorText(errorMsg);

        DumpLoadError(dll, "SelfRegisterDll", errorMsg);

        return NS_ERROR_FAILURE;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
           ("nsNativeComponentLoader: Loaded \"%s\".", dll->GetDisplayPath()));

    // Tell the module to self register
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(mCompMgr, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
    {
        nsCOMPtr<nsIFile> fs;
        /*************************************************************
         * WARNING: Why are use introducing 'res2' here and then     *
         * later assigning it to 'res' rather than just using 'res'? *
         * This is because this code turns up a code-generation bug  *
         * in VC6 on NT. Assigning to 'res' on the next line causes  *
         * the value of 'dll' to get nulled out! The two seem to be  *
         * getting aliased together during compilation.              *
         *************************************************************/
        nsresult res2 = dll->GetDllSpec(getter_AddRefs(fs));    // don't change 'res2' -- see warning, above
        if (NS_SUCCEEDED(res2)) {
            res = mobj->RegisterSelf(mCompMgr, fs, registryLocation,
                                     nativeComponentType);
        }
        else
        {
            res = res2;         // don't take this out -- see warning, above
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                   ("nsNativeComponentLoader: dll->GetDllSpec() on %s FAILED.",
                    dll->GetDisplayPath()));
        }
        mobj = NULL;    // Force a release of the Module object before unload()
    }
#ifndef OBSOLETE_MODULE_LOADING
    else
    {
        res = NS_ERROR_NO_INTERFACE;
        nsRegisterProc regproc = (nsRegisterProc)dll->FindSymbol("NSRegisterSelf");
        if (regproc)
        {
            // Call the NSRegisterSelfProc to enable dll registration
            res = regproc(serviceMgr, registryLocation);
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                   ("nsNativeComponentLoader: %s using OBSOLETE NSRegisterSelf()",
                    dll->GetDisplayPath()));
        }
    }
#endif /* OBSOLETE_MODULE_LOADING */

    // Update the timestamp and size of the dll in registry
    // Don't enter deferred modules in the registry, because it might only be
    // able to register on some later autoreg, after another component has been
    // installed.
    if (res != NS_ERROR_FACTORY_REGISTER_AGAIN) {
        dll->Sync();
        
        PRInt64 modTime, size;
        dll->GetLastModifiedTime(&modTime);
        dll->GetSize(&size);

        SetRegistryDllInfo(registryLocation, &modTime, &size);
    }

    return res;
}

//
// MOZ_DEMANGLE_SYMBOLS is only a linux + MOZ_DEBUG thing.
//

#if defined(MOZ_DEMANGLE_SYMBOLS)
#include "nsTraceRefcnt.h" // for nsTraceRefcnt::DemangleSymbol()
#endif

nsresult 
nsNativeComponentLoader::DumpLoadError(nsDll *dll, 
                                       const char *aCallerName,
                                       const char *aNsprErrorMsg)
{
    PR_ASSERT(aCallerName != NULL);
    
    if (nsnull == dll || nsnull == aNsprErrorMsg)
        return NS_OK;

    nsCAutoString errorMsg(aNsprErrorMsg);

#if defined(MOZ_DEMANGLE_SYMBOLS)
    // Demangle undefined symbols
    nsCAutoString undefinedMagicString("undefined symbol:");
    
    PRInt32 offset = errorMsg.Find(undefinedMagicString, PR_TRUE);
    
    if (offset != kNotFound)
    {
        nsCAutoString symbol(errorMsg);
        nsCAutoString demangledSymbol;
        
        symbol.Cut(0,offset);
        
        symbol.Cut(0,undefinedMagicString.Length());
        
        symbol.StripWhitespace();
        
        char demangled[4096] = "\0";
        
        nsTraceRefcnt::DemangleSymbol(symbol,demangled,sizeof(demangled));
        
        if (demangled && *demangled != '\0')
            demangledSymbol = demangled;
        
        if (!demangledSymbol.IsEmpty())
        {
            nsCAutoString tmp(errorMsg);
            
            
            tmp.Cut(offset + undefinedMagicString.Length(),
                    tmp.Length() - offset - undefinedMagicString.Length());
            
            tmp += " \n";
            
            tmp += demangledSymbol;
            
            errorMsg = tmp;
        }    
    }
#endif // MOZ_DEMANGLE_SYMBOLS
    
    // Do NSPR log
    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
           ("nsNativeComponentLoader: %s(%s) Load FAILED with error:%s", 
            aCallerName,
            dll->GetDisplayPath(), 
            (const char *) errorMsg));
    

    // Dump to screen if needed
#ifdef PRINT_CRITICAL_ERROR_TO_SCREEN
    printf("**************************************************\n"
           "nsNativeComponentLoader: %s(%s) Load FAILED with error: %s\n"
           "**************************************************\n",
           aCallerName,
           dll->GetDisplayPath(), 
           (const char *) errorMsg);
#endif

    return NS_OK;
}

nsresult
nsNativeComponentLoader::SelfUnregisterDll(nsDll *dll)
{
    nsresult res;
    nsCOMPtr<nsIServiceManager> serviceMgr;
    res = NS_GetServiceManager(getter_AddRefs(serviceMgr));
    if (NS_FAILED(res)) return res;

    if (dll->Load() == PR_FALSE)
    {
        // Cannot load. Probably not a dll.
        return(NS_ERROR_FAILURE);
    }
        
    // Tell the module to self register
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(mCompMgr, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
               ("nsNativeComponentLoader: %s using nsIModule to unregister self.", dll->GetDisplayPath()));
        nsCOMPtr<nsIFile> fs;
        res = dll->GetDllSpec(getter_AddRefs(fs));
        if (NS_FAILED(res)) return res;
        // Get registry location for spec
        nsXPIDLCString registryName;
        res = mCompMgr->RegistryLocationForSpec(fs, getter_Copies(registryName));
        if (NS_FAILED(res)) return res;
        mobj->UnregisterSelf(mCompMgr, fs, registryName);
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
    return res;
}

nsresult
nsNativeComponentLoader::AutoUnregisterComponent(PRInt32 when,
                                                 nsIFile *component,
                                                 PRBool *unregistered)
{
    nsresult rv = NS_ERROR_FAILURE;

    nsXPIDLCString persistentDescriptor;
    rv = mCompMgr->RegistryLocationForSpec(component,
                                           getter_Copies(persistentDescriptor));
    if (NS_FAILED(rv)) return rv;

    // Notify observers, if any, of autoregistration work
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIServiceManager> mgr;
      rv = NS_GetServiceManager(getter_AddRefs(mgr));
      if (NS_SUCCEEDED(rv))
      {
        (void) observerService->NotifyObservers(mgr,
                                                NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                                NS_ConvertASCIItoUCS2("Unregistering native component").get());
      }
    }

    nsDll *dll = NULL;
    PRInt64 mod = LL_Zero(), size = LL_Zero();
    rv = CreateDll(component, persistentDescriptor, &mod, &size, &dll);
    if (NS_FAILED(rv) || dll == NULL) return rv;

    rv = SelfUnregisterDll(dll);

    // Remove any autoreg info about this dll
    if (NS_SUCCEEDED(rv))
        RemoveRegistryDllInfo(persistentDescriptor);

    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
           ("nsNativeComponentLoader: AutoUnregistration for %s %s.",
            (NS_FAILED(rv) ? "FAILED" : "succeeded"), dll->GetDisplayPath()));
    return rv;
}

nsresult
nsNativeComponentLoader::AutoRegisterComponent(PRInt32 when,
                                               nsIFile *component,
                                               PRBool *registered)
{
    nsresult rv;
    if (!registered)
        return NS_ERROR_NULL_POINTER;

    /* this should be a pref or registry entry, or something */
    static const char *ValidDllExtensions[] = {
        ".dll",     /* Windows */
        ".so",      /* Unix */
        ".shlb",    /* Mac ? */
        ".dso",     /* Unix ? */
        ".dylib",   /* Unix: Rhapsody */
        ".so.1.0",  /* Unix: BSD */
        ".sl",      /* Unix: HP-UX */
#if defined(VMS)
        ".exe",     /* Open VMS */
#endif
        ".dlm",     /* new for all platforms */
        NULL
    };

    *registered = PR_FALSE;

#if 0
    // This is a performance hit on mac. Since we have already checked
    // this; plus is we dont, load will fail anyway later on, this
    // is being commented out.

    // Ensure we are dealing with a file as opposed to a dir
    PRBool b = PR_FALSE;

    rv = component->IsFile(&b);
    if (NS_FAILED(rv) || !b)
        return rv;
#endif /* 0 */

    // deal only with files that have a valid extension
    PRBool validExtension = PR_FALSE;

#ifdef  XP_MAC  // sdagley dougt fix
    // rjc - on Mac, check the file's type code (skip checking the creator code)
    
    nsCOMPtr<nsILocalFileMac> localFileMac = do_QueryInterface(component);
    if (localFileMac)
    {
      OSType    type, creator;
      rv = localFileMac->GetFileTypeAndCreator(&type, &creator);
      if (NS_SUCCEEDED(rv))
      {
        // on Mac, Mozilla shared libraries are of type 'shlb'
        // Note: we don't check the creator (which for Mozilla is 'MOZZ')
        // so that 3rd party shared libraries will be noticed!
        validExtension = ((type == 'shlb') || (type == 'NSPL'));
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
    if (leafName) nsMemory::Free(leafName);
#endif
        
    if (validExtension == PR_FALSE)
        // Skip invalid extensions
        return NS_OK;

    nsXPIDLCString persistentDescriptor;
    rv = mCompMgr->RegistryLocationForSpec(component, 
                                           getter_Copies(persistentDescriptor));
    if (NS_FAILED(rv))
        return rv;

    nsCStringKey key(persistentDescriptor);

    // Get the registry representation of the dll, if any
    nsDll *dll;
    PRInt64 mod = LL_Zero(), size = LL_Zero();
    rv = CreateDll(component, persistentDescriptor, &mod, &size, &dll);
    if (NS_FAILED(rv))
        return rv;

    if (dll != NULL)
    {
        // Make sure the dll is OK
        if (dll->GetStatus() != NS_OK)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsNativeComponentLoader: + nsDll not NS_OK \"%s\". Skipping...",
                    dll->GetDisplayPath()));
            return NS_ERROR_FAILURE;
        }
            
        // We already have seen this dll. Check if this dll changed
        if (!dll->HasChanged())
        {
            // Dll hasn't changed. Skip.
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                   ("nsNativeComponentLoader: + nsDll not changed \"%s\". Skipping...",
                    dll->GetDisplayPath()));
            return NS_OK;
        }

        // Aagh! the dll has changed since the last time we saw it.
        // re-register dll


        // Notify observers, if any, of autoregistration work
        nsCOMPtr<nsIObserverService> observerService = 
                 do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIServiceManager> mgr;
          rv = NS_GetServiceManager(getter_AddRefs(mgr));
          if (NS_SUCCEEDED(rv))
          {
            // this string can't come from a string bundle, because we don't have string
            // bundles yet.
            NS_ConvertASCIItoUCS2 statusMsg("Registering native component ");            
            NS_ConvertASCIItoUCS2 fileName("(no name)");
            
            // get the file name
            nsCOMPtr<nsIFile> dllSpec;
            if (NS_SUCCEEDED(dll->GetDllSpec(getter_AddRefs(dllSpec))) && dllSpec)
            {
              nsXPIDLCString dllLeafName;
              dllSpec->GetLeafName(getter_Copies(dllLeafName));
              fileName.AssignWithConversion(dllLeafName);
            }
            statusMsg.Append(fileName);
            
            (void) observerService->NotifyObservers(mgr,
                                                    NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                                    statusMsg.get());
          }
        }

        if (dll->IsLoaded())
        {
            // We loaded the old version of the dll and now we find that the
            // on-disk copy if newer. Try to unload the dll.
            nsCOMPtr<nsIServiceManager> serviceMgr;
            rv = NS_GetServiceManager(getter_AddRefs(serviceMgr));
          
            rv = nsFreeLibrary(dll, serviceMgr, when);
            if (NS_FAILED(rv))
            {
                // THIS IS THE WORST SITUATION TO BE IN.
                // Dll doesn't want to be unloaded. Cannot re-register
                // this dll.
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsNativeComponentLoader: *** Dll already loaded. "
                        "Cannot unload either. Hence cannot re-register "
                        "\"%s\". Skipping...", dll->GetDisplayPath()));
                return rv;
            }
            else {
                                // dll doesn't have a CanUnload proc. Guess it is
                                // ok to unload it.
                dll->Unload();
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                       ("nsNativeComponentLoader: + Unloading \"%s\". (no CanUnloadProc).",
                        dll->GetDisplayPath()));
            }
                
        } // dll isloaded
            
        // Sanity.
        if (dll->IsLoaded())
        {
            // We went through all the above to make sure the dll
            // is unloaded. And here we are with the dll still
            // loaded. Whoever taught dp programming...
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsNativeComponentLoader: Dll still loaded. Cannot re-register "
                    "\"%s\". Skipping...", dll->GetDisplayPath()));
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
    nsresult res = SelfRegisterDll(dll, persistentDescriptor, PR_FALSE);
    if (NS_FAILED(res))
    {
        if (res == NS_ERROR_FACTORY_REGISTER_AGAIN) {
            /* defer for later loading */
            mDeferredComponents.AppendElement(dll);
            return NS_OK;
        } else {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("nsNativeComponentLoader: Autoregistration FAILED for "
                    "\"%s\". Skipping...", dll->GetDisplayPath()));
            return NS_ERROR_FACTORY_NOT_REGISTERED;
        }
    }
    else
    {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsNativeComponentLoader: Autoregistration Passed for "
                "\"%s\".", dll->GetDisplayPath()));
        // Marking dll along with modified time and size in the
        // registry happens at PlatformRegister(). No need to do it
        // here again.
    }
    return NS_OK;
}

nsresult
nsNativeComponentLoader::RegisterDeferredComponents(PRInt32 aWhen,
                                                    PRBool *aRegistered)
{
#ifdef DEBUG 
    fprintf(stderr, "nNCL: registering deferred (%d)\n",
            mDeferredComponents.Count());
#endif
    *aRegistered = PR_FALSE;
    if (!mDeferredComponents.Count())
        return NS_OK;
    
    for (int i = mDeferredComponents.Count() - 1; i >= 0; i--) {
        nsDll *dll = NS_STATIC_CAST(nsDll *, mDeferredComponents[i]);
        nsresult rv = SelfRegisterDll(dll,
                                      dll->GetRegistryLocation(),
                                      PR_TRUE);
        if (rv != NS_ERROR_FACTORY_REGISTER_AGAIN) {
            if (NS_SUCCEEDED(rv))
                *aRegistered = PR_TRUE;
            mDeferredComponents.RemoveElementAt(i);
        }
    }

    if (*aRegistered)
        fprintf(stderr, "nNCL: registered deferred, %d left\n",
                mDeferredComponents.Count());
    else
        fprintf(stderr, "nNCL: didn't register any components, %d left\n",
                mDeferredComponents.Count());
    /* are there any fatal errors? */
    return NS_OK;
}

nsresult
nsNativeComponentLoader::OnRegister(const nsIID &aCID, const char *aType,
                                    const char *aClassName,
                                    const char *aContractID, const char *aLocation,
                                    PRBool aReplace, PRBool aPersist)
{
    return NS_OK;
}

nsresult
nsNativeComponentLoader::UnloadAll(PRInt32 aWhen)
{
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsNativeComponentLoader: Unloading...."));

    struct freeLibrariesClosure callData;
    callData.serviceMgr = NULL; // XXX need to get this as a parameter
    callData.when = aWhen;

    // Cycle through the dlls checking to see if they want to be unloaded
    if (mDllStore) {
        mDllStore->Enumerate(nsFreeLibraryEnum, &callData);
    }
    return NS_OK;
}

nsresult
nsNativeComponentLoader::GetRegistryDllInfo(const char *aLocation,
                                            PRInt64 *lastModifiedTime,
                                            PRInt64 *fileSize)
{
    nsresult rv;
    PRUint32 length = strlen(aLocation);
    char* eLocation;
    rv = mRegistry->EscapeKey((PRUint8*)aLocation, 1, &length, (PRUint8**)&eLocation);
    if (rv != NS_OK)
    {
    return rv;
    }
    if (eLocation == nsnull)    //  No escaping required
    eLocation = (char*)aLocation;


    nsRegistryKey key;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, eLocation, &key);
    if (NS_FAILED(rv)) return rv;

    rv = GetRegistryDllInfo(key, lastModifiedTime, fileSize);
    if (aLocation != eLocation)
        nsMemory::Free(eLocation);
    return rv;
}

nsresult
nsNativeComponentLoader::GetRegistryDllInfo(nsRegistryKey key,
                                            PRInt64 *lastModifiedTime,
                                            PRInt64 *fileSize)
{
    PRInt64 lastMod;
    nsresult rv = mRegistry->GetLongLong(key, lastModValueName, &lastMod);
    if (NS_FAILED(rv)) return rv;
    *lastModifiedTime = lastMod;
        
    PRInt64 fsize;
    rv = mRegistry->GetLongLong(key, fileSizeValueName, &fsize);
    if (NS_FAILED(rv)) return rv;
        *fileSize = fsize;
    return NS_OK;
}

nsresult
nsNativeComponentLoader::SetRegistryDllInfo(const char *aLocation,
                                            PRInt64 *lastModifiedTime,
                                            PRInt64 *fileSize)
{
    nsresult rv;
    PRUint32 length = strlen(aLocation);
    char* eLocation;
    rv = mRegistry->EscapeKey((PRUint8*)aLocation, 1, &length, (PRUint8**)&eLocation);
    if (rv != NS_OK)
    {
        return rv;
    }
    if (eLocation == nsnull)    //  No escaping required
    eLocation = (char*)aLocation;

    nsRegistryKey key;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, eLocation, &key);
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->SetLongLong(key, lastModValueName, lastModifiedTime);
    if (NS_FAILED(rv)) return rv;
    rv = mRegistry->SetLongLong(key, fileSizeValueName, fileSize);
    if (aLocation != eLocation)
        nsMemory::Free(eLocation);
    return rv;
}

nsresult
nsNativeComponentLoader::RemoveRegistryDllInfo(const char *aLocation)
{
    nsresult rv;
    PRUint32 length = strlen(aLocation);
    char* eLocation;
    rv = mRegistry->EscapeKey((PRUint8*)aLocation, 1, &length, (PRUint8**)&eLocation);
    if (rv != NS_OK)
    {
        return rv;
    }
    if (eLocation == nsnull)    //  No escaping required
    eLocation = (char*)aLocation;

    rv = mRegistry->RemoveSubtree(mXPCOMKey, eLocation);
    if (aLocation != eLocation)
        nsMemory::Free(eLocation);
    return rv;
}

//
// CreateDll
// The only way to create a dll or get it from the dll cache. This will
// be called in multiple situations:
//
// 1. Autoregister will create one for each dll it is trying to register. This
//    call will be passing a spec in.
//    {spec, NULL, 0, 0}
//
// 2. GetFactory() This will call CreateDll() with a null spec but will give
//    the registry represented name of the dll. If modtime and size are zero,
//    we will go the registry to find the right modtime and size.
//    {NULL, rel:libpref.so, 0, 0}
//
// 3. Prepopulation of dllCache A dll object created off a registry entry.
//    Specifically dll name is stored    in rel: or abs: or lib: formats in the
//    registry along with its    lastModTime and fileSize.
//    {NULL, rel:libpref.so, 8985659, 20987}
nsresult
nsNativeComponentLoader::CreateDll(nsIFile *aSpec, const char *aLocation,
                                   PRInt64 *modificationTime, PRInt64 *fileSize,
                                   nsDll **aDll)
{
    nsDll *dll;
    nsCOMPtr<nsIFile> dllSpec;
    nsCOMPtr<nsIFile> spec;
    nsresult rv;

    nsCStringKey key(aLocation);
    dll = (nsDll *)mDllStore->Get(&key);
    if (dll)
    {
        *aDll = dll;
        return NS_OK;
    }

    if (!aSpec)
    {
        if (!nsCRT::strncmp(aLocation, XPCOM_LIB_PREFIX, 4)) {
            dll = new nsDll(aLocation+4, 1 /* dumb magic flag */);
            if (!dll) return NS_ERROR_OUT_OF_MEMORY;
        } else {
            rv = mCompMgr->SpecForRegistryLocation(aLocation,
                                                   getter_AddRefs(spec));
            if (NS_FAILED(rv))
                return rv;
        }
    }
    else
    {
        spec = aSpec;
    }

    if (!dll)
    {
        PR_ASSERT(modificationTime != NULL);
        PR_ASSERT(fileSize != NULL);

        PRInt64 zit = LL_Zero();
        
        if (LL_EQ(*modificationTime,zit) && LL_EQ(*fileSize,zit))
        {
            // Get the modtime and filesize from the registry
            rv = GetRegistryDllInfo(aLocation, modificationTime, fileSize);
        }
        dll = new nsDll(spec, aLocation, modificationTime, fileSize);
    }

    if (!dll)
        return NS_ERROR_OUT_OF_MEMORY;

    *aDll = dll;
    mDllStore->Put(&key, dll);
    return NS_OK;
}

nsresult
nsNativeComponentLoader::GetFactoryFromModule(nsDll *aDll, const nsCID &aCID,
                                              nsIFactory **aFactory)
{
    nsresult rv;

    nsCOMPtr<nsIModule> module;
    rv = aDll->GetModule(mCompMgr, getter_AddRefs(module));

    if (NS_FAILED(rv))
        return rv;

    return module->GetClassObject(mCompMgr, aCID, NS_GET_IID(nsIFactory),
                                  (void **)aFactory);
}

nsresult
nsNativeComponentLoader::GetFactoryFromNSGetFactory(nsDll *aDll,
                                                    const nsCID &aCID,
                                                    nsIServiceManager *aServMgr,
                                                    nsIFactory **aFactory)
{
#ifdef XPCOM_USE_NSGETFACTORY
    nsFactoryProc getFactory =
        (nsFactoryProc) aDll->FindSymbol("NSGetFactory");

    if (!getFactory)
        return NS_ERROR_FACTORY_NOT_LOADED;
    
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
           ("nsNativeComponentLoader: %s using OBSOLETE NSGetFactory()\n",
            aDll->GetDisplayPath()));

    /*
     * There was a time when CLSIDToContractID was used to get className
     * and contractID, but that day is long past.  This code is not long
     * for this earth, so we just pass nsnull.
     */

    return getFactory(aServMgr, aCID, nsnull /*className */,
                      nsnull /* contractID */, aFactory);

#else /* !XPCOM_USE_NSGETFACTORY */
    return NS_ERROR_FACTORY_NOT_LOADED;
#endif /* XPCOM_USE_NSGETFACTORY */
}
