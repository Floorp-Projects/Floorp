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
 */

#include "prlog.h"
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

#define PRINT_CRITICAL_ERROR_TO_SCREEN 1
#define USE_REGISTRY 1
#define XPCOM_USE_NSGETFACTORY 1

extern PRLogModuleInfo *nsComponentManagerLog;

/* XXX consolidate with one in nsComponentManager.cpp */
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

nsNativeComponentLoader::nsNativeComponentLoader() :
    mRegistry(nsnull), mCompMgr(nsnull), mDllStore(nsnull)
{
    NS_INIT_REFCNT();
}

static PRBool
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
    
NS_IMPL_ISUPPORTS(nsNativeComponentLoader, NS_GET_IID(nsIComponentLoader));

NS_IMETHODIMP
nsNativeComponentLoader::GetFactory(const nsIID & aCID,
                                    const char *aLocation,
                                    const char *aType,
                                    nsIFactory **_retval)
{
    nsresult rv;

    if (!_retval)
        return NS_ERROR_NULL_POINTER;
    
    /* use a hashtable of WeakRefs to store the factory object? */

    /* Should this all live in xcDll? */
    nsDll *dll;
    rv = CreateDll(nsnull, aLocation, &dll);
    if (NS_FAILED(rv))
        return rv;

    if (!dll)
        return NS_ERROR_OUT_OF_MEMORY;
    
    if (!dll->IsLoaded()) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsNativeComponentLoader: loading \"%s\"",
                dll->GetNativePath()));

        if (!dll->Load()) {

            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("nsNativeComponentLoader: load FAILED"));
        
            char errorMsg[1024] = "<unknown; can't get error from NSPR>";

            if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
                PR_GetErrorText(errorMsg);

            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Load(\"%s\") FAILED with error: %s",
                    dll->GetNativePath(), errorMsg));

#ifdef PRINT_CRITICAL_ERROR_TO_SCREEN
            // Put the error message on the screen.
            printf("****************************************************\n"
                   "nsComponentManager: Load(\"%s\") FAILED with error: %s\n"
                   "****************************************************\n",
                   dll->GetNativePath(), errorMsg);
#endif
            return NS_ERROR_FAILURE;
        }
    }

#ifdef MOZ_TRACE_XPCOM_REFCNT
    // Inform refcnt tracer of new library so that calls through the
    // new library can be traced.
    nsTraceRefcnt::LoadLibrarySymbols(dll->GetNativePath(),
                                      dll->GetInstance());
#endif

    /* Get service manager for factory */
    nsIServiceManager* serviceMgr = NULL;
    rv = nsServiceManager::GetGlobalServiceManager(&serviceMgr);
    if (NS_FAILED(rv))
        return rv;      // XXX translate error code?
    
    rv = GetFactoryFromModule(dll, aCID, _retval);
    if (NS_FAILED(rv)) {
#ifdef XPCOM_USE_NSGETFACTORY
        if (rv == NS_ERROR_FACTORY_NOT_LOADED) {
            rv = GetFactoryFromNSGetFactory(dll, aCID, serviceMgr, _retval);
            if (NS_SUCCEEDED(rv))
                goto out;
        }
#endif
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("nsNativeComponentLoader: failed to get factory from "
                "%s -> %s\n",
                aLocation, dll->GetNativePath()));
    }

 out:
    if (NS_FAILED(rv)) {
        rv = NS_ERROR_FACTORY_NOT_LOADED;
        delete dll;
    }
    
    return rv;
}

NS_IMETHODIMP
nsNativeComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg)
{
    nsresult rv;

#ifdef DEBUG_shaver
    fprintf(stderr, "nNCL: Init()\n");
#endif

    mCompMgr = aCompMgr;
    mRegistry = do_QueryInterface(aReg);
    if (!mCompMgr.get() || !mRegistry)
        return NS_ERROR_INVALID_ARG;

    rv = mRegistry->GetSubtree(nsIRegistry::Common, xpcomKeyName,
                               &mXPCOMKey);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "nsNCL::Init: registry is hosed\n");
#endif
        return rv;
    }

    if (!mDllStore) {
        mDllStore = new nsObjectHashtable(nsnull, nsnull, // never copy
                                          nsDll_Destroy, nsnull,
                                          256, /* Thead Safe */ PR_TRUE);
        if (!mDllStore) {
#ifdef DEBUG_shaver
            fprintf(stderr, "nNCL::Init: couldn't build hash table\n");
#endif            
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsNativeComponentLoader::AutoRegisterComponents(PRInt32 aWhen,
                                                nsIFileSpec *aDirectory)
{
    char *nativePath = NULL;
    nsresult rv = aDirectory->GetNativePath(&nativePath);
    if (NS_FAILED(rv))
        return rv;
    if (!nativePath)
        return NS_ERROR_INVALID_POINTER;

#ifdef DEBUG
    /* do we _really_ want to print this every time? */
    printf("nsNativeComponentLoader: autoregistering %s\n", nativePath);
#endif

    rv = RegisterComponentsInDir(aWhen, aDirectory);

#ifdef DEBUG
    printf("nsNativeComponentLoader: autoregistering %s\n",
           NS_FAILED(rv) ? "FAILED" : "succeeded");
#endif

    nsCRT::free(nativePath);

    return rv;
}

nsresult
nsNativeComponentLoader::RegisterComponentsInDir(PRInt32 when,
                                                 nsIFileSpec *dir)
{
    nsresult rv = NS_ERROR_FAILURE;
    PRBool isDir = PR_FALSE;

    // Make sure we are dealing with a directory
    rv = dir->IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;

    if (!isDir)
        return NS_ERROR_INVALID_ARG;

    // Create a directory iterator
    nsCOMPtr<nsIDirectoryIterator>dirIterator;
    rv = nsComponentManager::CreateInstance(NS_DIRECTORYITERATOR_PROGID, NULL,
                                            NS_GET_IID(nsIDirectoryIterator),
                                            getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;
    rv = dirIterator->Init(dir, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // whip through the directory to register every file
    nsIFileSpec *dirEntry = NULL;
    PRBool more = PR_FALSE;
    rv = dirIterator->Exists(&more);
    if (NS_FAILED(rv)) return rv;
    while (more == PR_TRUE)
        {
            rv = dirIterator->GetCurrentSpec(&dirEntry);
            if (NS_FAILED(rv)) return rv;

            rv = dirEntry->IsDirectory(&isDir);
            if (NS_FAILED(rv)) return rv;
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
            if (NS_FAILED(rv))
                {
                    // This means either of AutoRegisterComponent or
                    // SyncComponentsInDir failed. It could be because
                    // the file isn't a component like initpref.js
                    // So dont break on these errors.

                    // Update: actually, we return NS_OK for the wrong file
                    // types, but we should never fail hard because just one
                    // component didn't work.
                }
                    
            NS_RELEASE(dirEntry);
        
            rv = dirIterator->Next();
            if (NS_FAILED(rv)) return rv;
            rv = dirIterator->Exists(&more);
            if (NS_FAILED(rv)) return rv;
        }
    
    return rv;
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
    /* XXXshaver cheat and use the global component manager */
    rv = dll->GetModule(nsComponentManagerImpl::gComponentManager,
                        getter_AddRefs(mobj));
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

/*
 * SelfRegisterDll
 *
 * Given a dll abstraction, this will load, selfregister the dll and
 * unload the dll.
 *
 */
nsresult
nsNativeComponentLoader::SelfRegisterDll(nsDll *dll, const char *registryLocation)
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
                   ("nsNativeComponentLoader: SelfRegisterDll(%s) Load FAILED with error:%s", dll->GetNativePath(), errorMsg));

#ifdef PRINT_CRITICAL_ERROR_TO_SCREEN
            printf("**************************************************\n"
                   "nsNativeComponentLoader: Load(%s) FAILED with error: %s\n"
                   "**************************************************\n",
                   dll->GetNativePath(), errorMsg);
#endif
            return NS_ERROR_FAILURE;
        }

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsNativeComponentLoader: + Loaded \"%s\".", dll->GetNativePath()));

    // Tell the module to self register
    nsCOMPtr<nsIModule> mobj;
    res = dll->GetModule(mCompMgr, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                   ("nsNativeComponentLoader: %s using nsIModule to register self.", dll->GetNativePath()));
            nsCOMPtr<nsIFileSpec> fs;
            res = dll->GetDllSpec(getter_AddRefs(fs));
            if (NS_SUCCEEDED(res))
                res = mobj->RegisterSelf(mCompMgr, fs, registryLocation);
            else
                {
                    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                           ("nsNativeComponentLoader: dll->GetDllSpec() on %s FAILED.", dll->GetNativePath()));
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
                    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
                           ("NSRegisterSelf(%s, %s) %s",
                            serviceMgr ? "serviceMgr" : "(null)",
                            registryLocation,
                            NS_FAILED(res) ? "FAILED" : "succeeded"));
                }
        }
#endif /* OBSOLETE_MODULE_LOADING */

    // Update the timestamp and size of the dll in registry
    dll->Sync();
    SetRegistryDllInfo(registryLocation, dll->GetLastModifiedTime(), dll->GetSize());

    return res;
}

nsresult
nsNativeComponentLoader::SelfUnregisterDll(nsDll *dll)
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
    res = dll->GetModule(mCompMgr, getter_AddRefs(mobj));
    if (NS_SUCCEEDED(res))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                   ("nsNativeComponentLoader: %s using nsIModule to unregister self.", dll->GetNativePath()));
            nsCOMPtr<nsIFileSpec> fs;
            res = dll->GetDllSpec(getter_AddRefs(fs));
            if (NS_SUCCEEDED(res))
                res = mobj->UnregisterSelf(mCompMgr, fs, /* XXX location */ "");
            else
                {
                    PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, 
                           ("nsNativeComponentLoader: dll->GetDllSpec() on %s FAILED.", dll->GetNativePath()));
                }
            mobj = NULL;    // Force a release of the Module object before unload()
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
nsNativeComponentLoader::AutoRegisterComponent(PRInt32 when,
                                               nsIFileSpec *component,
                                               PRBool *registered)
{
    nsresult rv;
    if (!registered)
        return NS_ERROR_NULL_POINTER;

    /* this should be a pref or registry entry, or something */
    const char *ValidDllExtensions[] = {
        ".dll",     /* Windows */
        ".dso",     /* Unix ? */
        ".dylib",   /* Unix: Rhapsody */
        ".so",      /* Unix */
        ".so.1.0",  /* Unix: BSD */
        ".sl",      /* Unix: HP-UX */
        ".shlb",    /* Mac ? */
#if defined(VMS)
        ".exe",     /* Open VMS */
#endif
        ".dlm",     /* new for all platforms */
        NULL
    };

    *registered = PR_FALSE;

    // Ensure we are dealing with a file as opposed to a dir
    PRBool b = PR_FALSE;

    rv = component->IsFile(&b);
    if (NS_FAILED(rv) || !b)
        return rv;

    // deal only with files that have a valid extension
    PRBool validExtension = PR_FALSE;

#ifdef  XP_MAC
    // rjc - on Mac, check the file's type code (skip checking the creator code)
    nsFileSpec  fs;
    if (NS_FAILED(rv = component->GetFileSpec(&fs)))
        return(rv);

    CInfoPBRec  catInfo;
    OSErr       err = fs.GetCatInfo(catInfo);
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
        // Skip invalid extensions
        return NS_OK;


    /*
     * Get the name of the dll
     * I think I get to go through every type of string here.
     * Pink would be so proud.
     */
    char *persistentDescriptor;
    rv = mCompMgr->RegistryLocationForSpec(component, &persistentDescriptor);
    if (NS_FAILED(rv))
        return rv;

    autoStringFree
        delete_persistentDescriptor(persistentDescriptor,
                                    autoStringFree::nsCRT_String_Delete);

    nsStringKey key(persistentDescriptor);

    // Check if dll is one that we have already seen
    nsDll *dll;
    rv = CreateDll(component, persistentDescriptor, &dll);
    if (NS_FAILED(rv))
        return rv;

    if (dll != NULL)
        {
            // Make sure the dll is OK
            if (dll->GetStatus() != NS_OK)
                {
                    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                           ("nsNativeComponentLoader: + nsDll not NS_OK \"%s\". Skipping...",
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
                                   ("nsNativeComponentLoader: *** Dll already loaded. "
                                    "Cannot unload either. Hence cannot re-register "
                                    "\"%s\". Skipping...", dll->GetNativePath()));
                            return rv;
                        }
                    else {
                                // dll doesn't have a CanUnload proc. Guess it is
                                // ok to unload it.
                        dll->Unload();
                        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
                               ("nsNativeComponentLoader: + Unloading \"%s\". (no CanUnloadProc).",
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
                           ("nsNativeComponentLoader: Dll still loaded. Cannot re-register "
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
    nsresult res = SelfRegisterDll(dll, persistentDescriptor);
    nsresult ret = NS_OK;
    if (NS_FAILED(res))
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsNativeComponentLoader: Autoregistration FAILED for "
                    "\"%s\". Skipping...", dll->GetNativePath()));
            // Mark dll as not xpcom dll along with modified time and size in
            // the registry so that we wont need to load the dll again every
            // session until the dll changes.
#ifdef USE_REGISTRY
#endif /* USE_REGISTRY */
            ret = NS_ERROR_FAILURE;
        }
    else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsNativeComponentLoader: Autoregistration Passed for "
                    "\"%s\".", dll->GetNativePath()));
            // Marking dll along with modified time and size in the
            // registry happens at PlatformRegister(). No need to do it
            // here again.
        }
    return ret;
}

#if 0
nsDll*
nsNativeComponentLoader::CreateCachedDll(const char *location,
                                         PRUint32 modificationDate,
                                         PRUint32 fileSize)
{
    // Check our dllCollection for a dll with matching name
    nsStringKey key(location);
    nsDll *dll = (nsDll *) mDllStore->Get(&key);
    
    if (dll == NULL)
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsNativeComponentLoader: New dll \"%s\".", location));

            // Add a new Dll into the nsDllStore
            dll = new nsDll(location, modificationDate, fileSize);
            if (dll == NULL) return NULL;
            if (dll->GetStatus() != DLL_OK) {
                // Cant create a nsDll. Backoff.
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsNativeComponentLoader: ERROR in creating nsDll from \"%s\".",
                        location));
                delete dll;
                dll = NULL;
            } else {
                PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                       ("nsNativeComponentLoader: Adding New dll \"%s\" to mDllStore.",
                        location));

                mDllStore->Put(&key, (void *)dll);
            }
        }
    else
        {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
                   ("nsComponentManager: Found in mDllStore \"%s\".", location));
            // XXX We found the dll in the dllCollection.
            // XXX Consistency check: dll needs to have the same
            // XXX lastModTime and fileSize. If not that would mean
            // XXX that the dll wasn't registered properly.
        }

    return dll;
}
#endif

nsresult
nsNativeComponentLoader::OnRegister(const nsIID &aCID, const char *aType,
                                    const char *aClassName,
                                    const char *aProgID, const char *aLocation,
                                    PRBool aReplace, PRBool aPersist)
{
    return NS_OK;
}

nsresult
nsNativeComponentLoader::UnloadAll(PRInt32 aWhen)
{
#ifdef DEBUG_shaver
    fprintf(stderr, "nNCL:UnloadAll(%d)\n", aWhen);
#endif
    return NS_OK;
}

/*
 * There are no interesting failures here, so we don't propagate errors back.
 */
nsresult
nsNativeComponentLoader::GetRegistryDllInfo(const char *aLocation,
                                            PRUint32 *lastModifiedTime,
                                            PRUint32 *fileSize)
{
    nsresult rv;

    *lastModifiedTime = 0;
    *fileSize = 0;

    nsIRegistry::Key key;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, aLocation, &key);
    if (NS_FAILED(rv)) return rv;

    int32 lastMod;
    rv = mRegistry->GetInt(key, lastModValueName, &lastMod);
    if (NS_FAILED(rv)) return rv;
    *lastModifiedTime = lastMod;
        
    int32 fsize;
    rv = mRegistry->GetInt(key, fileSizeValueName, &fsize);
    if (NS_FAILED(rv)) return rv;
    *fileSize = fsize;

    return NS_OK;
}

nsresult
nsNativeComponentLoader::SetRegistryDllInfo(const char *aLocation,
                                            PRUint32 lastModifiedTime,
                                            PRUint32 fileSize)
{
    nsresult rv;
    nsIRegistry::Key key;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, aLocation, &key);
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->SetInt(key, lastModValueName, lastModifiedTime);
    if (NS_FAILED(rv)) return rv;
    rv = mRegistry->SetInt(key, fileSizeValueName, fileSize);
    return rv;
}
 
nsresult
nsNativeComponentLoader::CreateDll(nsIFileSpec *aSpec,
                                   const char *aLocation, nsDll **aDll)
{
    PRUint32 modificationTime = 0, fileSize = 0;
    nsDll *dll;
    nsFileSpec dllSpec;
    nsCOMPtr<nsIFileSpec> spec;
    nsresult rv;

    nsStringKey key(aLocation);
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
            rv = NS_OK;
            if (mRegistry)
                rv = GetRegistryDllInfo(aLocation, &modificationTime, &fileSize);
            else
                PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                       ("nsNativeComponentLoader: no registry, eep!"));
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

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
           ("nsNativeComponentLoader: %s using nsIModule to get factory",
            aDll->GetNativePath()));
    
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
           ("nsNativeComponentLoader: %s using OBSOLETE NSGetFactory\n",
            aDll->GetNativePath()));

    /*
     * There was a time when CLSIDToProgID was used to get className
     * and progID, but that day is long past.  This code is not long
     * for this earth, so we just pass nsnull.
     */

    return getFactory(aServMgr, aCID, nsnull /*className */,
                      nsnull /* progID */, aFactory);

#else /* !XPCOM_USE_NSGETFACTORY */
    return NS_ERROR_FACTORY_NOT_LOADED;
#endif /* XPCOM_USE_NSGETFACTORY */
}
