/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * The following handles the loading, unloading and management of
 * various PCKS #11 modules
 */
#define FORCE_PR_LOG 1
#include "seccomon.h"
#include "pkcs11.h"
#include "secmod.h"
#include "prlink.h"
#include "pk11func.h"
#include "secmodi.h"
#include "secmodti.h"
#include "nssilock.h"
#include "secerr.h"
#include "prenv.h"
#include "utilparst.h"
#include "prio.h"
#include "prprf.h"
#include <stdio.h>
#include "prsystem.h"

#define DEBUG_MODULE 1

#ifdef DEBUG_MODULE
static char *modToDBG = NULL;

#include "debug_module.c"
#endif

/* build the PKCS #11 2.01 lock files */
CK_RV PR_CALLBACK
secmodCreateMutext(CK_VOID_PTR_PTR pmutex)
{
    *pmutex = (CK_VOID_PTR)PZ_NewLock(nssILockOther);
    if (*pmutex)
        return CKR_OK;
    return CKR_HOST_MEMORY;
}

CK_RV PR_CALLBACK
secmodDestroyMutext(CK_VOID_PTR mutext)
{
    PZ_DestroyLock((PZLock *)mutext);
    return CKR_OK;
}

CK_RV PR_CALLBACK
secmodLockMutext(CK_VOID_PTR mutext)
{
    PZ_Lock((PZLock *)mutext);
    return CKR_OK;
}

CK_RV PR_CALLBACK
secmodUnlockMutext(CK_VOID_PTR mutext)
{
    PZ_Unlock((PZLock *)mutext);
    return CKR_OK;
}

static SECMODModuleID nextModuleID = 1;
static const CK_C_INITIALIZE_ARGS secmodLockFunctions = {
    secmodCreateMutext, secmodDestroyMutext, secmodLockMutext,
    secmodUnlockMutext, CKF_LIBRARY_CANT_CREATE_OS_THREADS | CKF_OS_LOCKING_OK,
    NULL
};
static const CK_C_INITIALIZE_ARGS secmodNoLockArgs = {
    NULL, NULL, NULL, NULL,
    CKF_LIBRARY_CANT_CREATE_OS_THREADS, NULL
};

static PRBool loadSingleThreadedModules = PR_TRUE;
static PRBool enforceAlreadyInitializedError = PR_TRUE;
static PRBool finalizeModules = PR_TRUE;

/* set global options for NSS PKCS#11 module loader */
SECStatus
pk11_setGlobalOptions(PRBool noSingleThreadedModules,
                      PRBool allowAlreadyInitializedModules,
                      PRBool dontFinalizeModules)
{
    if (noSingleThreadedModules) {
        loadSingleThreadedModules = PR_FALSE;
    } else {
        loadSingleThreadedModules = PR_TRUE;
    }
    if (allowAlreadyInitializedModules) {
        enforceAlreadyInitializedError = PR_FALSE;
    } else {
        enforceAlreadyInitializedError = PR_TRUE;
    }
    if (dontFinalizeModules) {
        finalizeModules = PR_FALSE;
    } else {
        finalizeModules = PR_TRUE;
    }
    return SECSuccess;
}

PRBool
pk11_getFinalizeModulesOption(void)
{
    return finalizeModules;
}

/*
 * Allow specification loading the same module more than once at init time.
 * This enables 2 things.
 *
 *    1) we can load additional databases by manipulating secmod.db/pkcs11.txt.
 *    2) we can handle the case where some library has already initialized NSS
 *    before the main application.
 *
 * oldModule is the module we have already initialized.
 * char *modulespec is the full module spec for the library we want to
 * initialize.
 */
static SECStatus
secmod_handleReload(SECMODModule *oldModule, SECMODModule *newModule)
{
    PK11SlotInfo *slot;
    char *modulespec;
    char *newModuleSpec;
    char **children;
    CK_SLOT_ID *ids;
    SECMODConfigList *conflist = NULL;
    SECStatus rv = SECFailure;
    int count = 0;

    /* first look for tokens= key words from the module spec */
    modulespec = newModule->libraryParams;
    newModuleSpec = secmod_ParseModuleSpecForTokens(PR_TRUE,
                                                    newModule->isFIPS, modulespec, &children, &ids);
    if (!newModuleSpec) {
        return SECFailure;
    }

    /*
     * We are now trying to open a new slot on an already loaded module.
     * If that slot represents a cert/key database, we don't want to open
     * multiple copies of that same database. Unfortunately we understand
     * the softoken flags well enough to be able to do this, so we can only get
     * the list of already loaded databases if we are trying to open another
     * internal module.
     */
    if (oldModule->internal) {
        conflist = secmod_GetConfigList(oldModule->isFIPS,
                                        oldModule->libraryParams, &count);
    }

    /* don't open multiple of the same db */
    if (conflist && secmod_MatchConfigList(newModuleSpec, conflist, count)) {
        rv = SECSuccess;
        goto loser;
    }
    slot = SECMOD_OpenNewSlot(oldModule, newModuleSpec);
    if (slot) {
        int newID;
        char **thisChild;
        CK_SLOT_ID *thisID;
        char *oldModuleSpec;

        if (secmod_IsInternalKeySlot(newModule)) {
            pk11_SetInternalKeySlotIfFirst(slot);
        }
        newID = slot->slotID;
        PK11_FreeSlot(slot);
        for (thisChild = children, thisID = ids; thisChild && *thisChild;
             thisChild++, thisID++) {
            if (conflist &&
                secmod_MatchConfigList(*thisChild, conflist, count)) {
                *thisID = (CK_SLOT_ID)-1;
                continue;
            }
            slot = SECMOD_OpenNewSlot(oldModule, *thisChild);
            if (slot) {
                *thisID = slot->slotID;
                PK11_FreeSlot(slot);
            } else {
                *thisID = (CK_SLOT_ID)-1;
            }
        }

        /* update the old module initialization string in case we need to
         * shutdown and reinit the whole mess (this is rare, but can happen
         * when trying to stop smart card insertion/removal threads)... */
        oldModuleSpec = secmod_MkAppendTokensList(oldModule->arena,
                                                  oldModule->libraryParams, newModuleSpec, newID,
                                                  children, ids);
        if (oldModuleSpec) {
            oldModule->libraryParams = oldModuleSpec;
        }

        rv = SECSuccess;
    }

loser:
    secmod_FreeChildren(children, ids);
    PORT_Free(newModuleSpec);
    if (conflist) {
        secmod_FreeConfigList(conflist, count);
    }
    return rv;
}

/*
 * collect the steps we need to initialize a module in a single function
 */
SECStatus
secmod_ModuleInit(SECMODModule *mod, SECMODModule **reload,
                  PRBool *alreadyLoaded)
{
    CK_C_INITIALIZE_ARGS moduleArgs;
    CK_VOID_PTR pInitArgs;
    CK_RV crv;

    if (reload) {
        *reload = NULL;
    }

    if (!mod || !alreadyLoaded) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (mod->libraryParams == NULL) {
        if (mod->isThreadSafe) {
            pInitArgs = (void *)&secmodLockFunctions;
        } else {
            pInitArgs = NULL;
        }
    } else {
        if (mod->isThreadSafe) {
            moduleArgs = secmodLockFunctions;
        } else {
            moduleArgs = secmodNoLockArgs;
        }
        moduleArgs.LibraryParameters = (void *)mod->libraryParams;
        pInitArgs = &moduleArgs;
    }
    crv = PK11_GETTAB(mod)->C_Initialize(pInitArgs);
    if (CKR_CRYPTOKI_ALREADY_INITIALIZED == crv) {
        SECMODModule *oldModule = NULL;

        /* Library has already been loaded once, if caller expects it, and it
         * has additional configuration, try reloading it as well. */
        if (reload != NULL && mod->libraryParams) {
            oldModule = secmod_FindModuleByFuncPtr(mod->functionList);
        }
        /* Library has been loaded by NSS. It means it may be capable of
         * reloading */
        if (oldModule) {
            SECStatus rv;
            rv = secmod_handleReload(oldModule, mod);
            if (rv == SECSuccess) {
                /* This module should go away soon, since we've
                 * simply expanded the slots on the old module.
                 * When it goes away, it should not Finalize since
                 * that will close our old module as well. Setting
                 * the function list to NULL will prevent that close */
                mod->functionList = NULL;
                *reload = oldModule;
                return SECSuccess;
            }
            SECMOD_DestroyModule(oldModule);
        }
        /* reload not possible, fall back to old semantics */
        if (!enforceAlreadyInitializedError) {
            *alreadyLoaded = PR_TRUE;
            return SECSuccess;
        }
    }
    if (crv != CKR_OK) {
        if (!mod->isThreadSafe ||
            crv == CKR_NETSCAPE_CERTDB_FAILED ||
            crv == CKR_NETSCAPE_KEYDB_FAILED) {
            PORT_SetError(PK11_MapError(crv));
            return SECFailure;
        }
        /* If we had attempted to init a single threaded module "with"
         * parameters and it failed, should we retry "without" parameters?
         * (currently we don't retry in this scenario) */

        if (!loadSingleThreadedModules) {
            PORT_SetError(SEC_ERROR_INCOMPATIBLE_PKCS11);
            return SECFailure;
        }
        /* If we arrive here, the module failed a ThreadSafe init. */
        mod->isThreadSafe = PR_FALSE;
        if (!mod->libraryParams) {
            pInitArgs = NULL;
        } else {
            moduleArgs = secmodNoLockArgs;
            moduleArgs.LibraryParameters = (void *)mod->libraryParams;
            pInitArgs = &moduleArgs;
        }
        crv = PK11_GETTAB(mod)->C_Initialize(pInitArgs);
        if ((CKR_CRYPTOKI_ALREADY_INITIALIZED == crv) &&
            (!enforceAlreadyInitializedError)) {
            *alreadyLoaded = PR_TRUE;
            return SECSuccess;
        }
        if (crv != CKR_OK) {
            PORT_SetError(PK11_MapError(crv));
            return SECFailure;
        }
    }
    return SECSuccess;
}

/*
 * set the hasRootCerts flags in the module so it can be stored back
 * into the database.
 */
void
SECMOD_SetRootCerts(PK11SlotInfo *slot, SECMODModule *mod)
{
    PK11PreSlotInfo *psi = NULL;
    int i;

    if (slot->hasRootCerts) {
        for (i = 0; i < mod->slotInfoCount; i++) {
            if (slot->slotID == mod->slotInfo[i].slotID) {
                psi = &mod->slotInfo[i];
                break;
            }
        }
        if (psi == NULL) {
            /* allocate more slots */
            PK11PreSlotInfo *psi_list = (PK11PreSlotInfo *)
                PORT_ArenaAlloc(mod->arena,
                                (mod->slotInfoCount + 1) * sizeof(PK11PreSlotInfo));
            /* copy the old ones */
            if (mod->slotInfoCount > 0) {
                PORT_Memcpy(psi_list, mod->slotInfo,
                            (mod->slotInfoCount) * sizeof(PK11PreSlotInfo));
            }
            /* assign psi to the last new slot */
            psi = &psi_list[mod->slotInfoCount];
            psi->slotID = slot->slotID;
            psi->askpw = 0;
            psi->timeout = 0;
            psi->defaultFlags = 0;

            /* increment module count & store new list */
            mod->slotInfo = psi_list;
            mod->slotInfoCount++;
        }
        psi->hasRootCerts = 1;
    }
}

#ifndef NSS_STATIC_SOFTOKEN
static const char *my_shlib_name =
    SHLIB_PREFIX "nss" SHLIB_VERSION "." SHLIB_SUFFIX;
static const char *softoken_shlib_name =
    SHLIB_PREFIX "softokn" SOFTOKEN_SHLIB_VERSION "." SHLIB_SUFFIX;
static const PRCallOnceType pristineCallOnce;
static PRCallOnceType loadSoftokenOnce;
static PRLibrary *softokenLib;
static PRInt32 softokenLoadCount;

/* This function must be run only once. */
/*  determine if hybrid platform, then actually load the DSO. */
static PRStatus
softoken_LoadDSO(void)
{
    PRLibrary *handle;

    handle = PORT_LoadLibraryFromOrigin(my_shlib_name,
                                        (PRFuncPtr)&softoken_LoadDSO,
                                        softoken_shlib_name);
    if (handle) {
        softokenLib = handle;
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}
#else
CK_RV NSC_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList);
char **NSC_ModuleDBFunc(unsigned long function, char *parameters, void *args);
#endif

/*
 * load a new module into our address space and initialize it.
 */
SECStatus
secmod_LoadPKCS11Module(SECMODModule *mod, SECMODModule **oldModule)
{
    PRLibrary *library = NULL;
    CK_C_GetFunctionList entry = NULL;
    CK_INFO info;
    CK_ULONG slotCount = 0;
    SECStatus rv;
    PRBool alreadyLoaded = PR_FALSE;
    char *disableUnload = NULL;

    if (mod->loaded)
        return SECSuccess;

    /* internal modules get loaded from their internal list */
    if (mod->internal && (mod->dllName == NULL)) {
#ifdef NSS_STATIC_SOFTOKEN
        entry = (CK_C_GetFunctionList)NSC_GetFunctionList;
#else
        /*
         * Loads softoken as a dynamic library,
         * even though the rest of NSS assumes this as the "internal" module.
         */
        if (!softokenLib &&
            PR_SUCCESS != PR_CallOnce(&loadSoftokenOnce, &softoken_LoadDSO))
            return SECFailure;

        PR_ATOMIC_INCREMENT(&softokenLoadCount);

        if (mod->isFIPS) {
            entry = (CK_C_GetFunctionList)
                PR_FindSymbol(softokenLib, "FC_GetFunctionList");
        } else {
            entry = (CK_C_GetFunctionList)
                PR_FindSymbol(softokenLib, "NSC_GetFunctionList");
        }

        if (!entry)
            return SECFailure;
#endif

        if (mod->isModuleDB) {
            mod->moduleDBFunc = (CK_C_GetFunctionList)
#ifdef NSS_STATIC_SOFTOKEN
                NSC_ModuleDBFunc;
#else
                PR_FindSymbol(softokenLib, "NSC_ModuleDBFunc");
#endif
        }

        if (mod->moduleDBOnly) {
            mod->loaded = PR_TRUE;
            return SECSuccess;
        }
    } else {
        /* Not internal, load the DLL and look up C_GetFunctionList */
        if (mod->dllName == NULL) {
            return SECFailure;
        }

        /* load the library. If this succeeds, then we have to remember to
         * unload the library if anything goes wrong from here on out...
         */
        library = PR_LoadLibrary(mod->dllName);
        mod->library = (void *)library;

        if (library == NULL) {
            return SECFailure;
        }

        /*
         * now we need to get the entry point to find the function pointers
         */
        if (!mod->moduleDBOnly) {
            entry = (CK_C_GetFunctionList)
                PR_FindSymbol(library, "C_GetFunctionList");
        }
        if (mod->isModuleDB) {
            mod->moduleDBFunc = (void *)
                PR_FindSymbol(library, "NSS_ReturnModuleSpecData");
        }
        if (mod->moduleDBFunc == NULL)
            mod->isModuleDB = PR_FALSE;
        if (entry == NULL) {
            if (mod->isModuleDB) {
                mod->loaded = PR_TRUE;
                mod->moduleDBOnly = PR_TRUE;
                return SECSuccess;
            }
            PR_UnloadLibrary(library);
            return SECFailure;
        }
    }

    /*
     * We need to get the function list
     */
    if ((*entry)((CK_FUNCTION_LIST_PTR *)&mod->functionList) != CKR_OK)
        goto fail;

#ifdef DEBUG_MODULE
    if (PR_TRUE) {
        modToDBG = PR_GetEnvSecure("NSS_DEBUG_PKCS11_MODULE");
        if (modToDBG && strcmp(mod->commonName, modToDBG) == 0) {
            mod->functionList = (void *)nss_InsertDeviceLog(
                (CK_FUNCTION_LIST_PTR)mod->functionList);
        }
    }
#endif

    mod->isThreadSafe = PR_TRUE;

    /* Now we initialize the module */
    rv = secmod_ModuleInit(mod, oldModule, &alreadyLoaded);
    if (rv != SECSuccess) {
        goto fail;
    }

    /* module has been reloaded, this module itself is done,
     * return to the caller */
    if (mod->functionList == NULL) {
        mod->loaded = PR_TRUE; /* technically the module is loaded.. */
        return SECSuccess;
    }

    /* check the version number */
    if (PK11_GETTAB(mod)->C_GetInfo(&info) != CKR_OK)
        goto fail2;
    if (info.cryptokiVersion.major != 2)
        goto fail2;
    /* all 2.0 are a priori *not* thread safe */
    if (info.cryptokiVersion.minor < 1) {
        if (!loadSingleThreadedModules) {
            PORT_SetError(SEC_ERROR_INCOMPATIBLE_PKCS11);
            goto fail2;
        } else {
            mod->isThreadSafe = PR_FALSE;
        }
    }
    mod->cryptokiVersion = info.cryptokiVersion;

    /* If we don't have a common name, get it from the PKCS 11 module */
    if ((mod->commonName == NULL) || (mod->commonName[0] == 0)) {
        mod->commonName = PK11_MakeString(mod->arena, NULL,
                                          (char *)info.libraryDescription, sizeof(info.libraryDescription));
        if (mod->commonName == NULL)
            goto fail2;
    }

    /* initialize the Slots */
    if (PK11_GETTAB(mod)->C_GetSlotList(CK_FALSE, NULL, &slotCount) == CKR_OK) {
        CK_SLOT_ID *slotIDs;
        int i;
        CK_RV crv;

        mod->slots = (PK11SlotInfo **)PORT_ArenaAlloc(mod->arena,
                                                      sizeof(PK11SlotInfo *) * slotCount);
        if (mod->slots == NULL)
            goto fail2;

        slotIDs = (CK_SLOT_ID *)PORT_Alloc(sizeof(CK_SLOT_ID) * slotCount);
        if (slotIDs == NULL) {
            goto fail2;
        }
        crv = PK11_GETTAB(mod)->C_GetSlotList(CK_FALSE, slotIDs, &slotCount);
        if (crv != CKR_OK) {
            PORT_Free(slotIDs);
            goto fail2;
        }

        /* Initialize each slot */
        for (i = 0; i < (int)slotCount; i++) {
            mod->slots[i] = PK11_NewSlotInfo(mod);
            PK11_InitSlot(mod, slotIDs[i], mod->slots[i]);
            /* look down the slot info table */
            PK11_LoadSlotList(mod->slots[i], mod->slotInfo, mod->slotInfoCount);
            SECMOD_SetRootCerts(mod->slots[i], mod);
            /* explicitly mark the internal slot as such if IsInternalKeySlot()
             * is set */
            if (secmod_IsInternalKeySlot(mod) && (i == (mod->isFIPS ? 0 : 1))) {
                pk11_SetInternalKeySlotIfFirst(mod->slots[i]);
            }
        }
        mod->slotCount = slotCount;
        mod->slotInfoCount = 0;
        PORT_Free(slotIDs);
    }

    mod->loaded = PR_TRUE;
    mod->moduleID = nextModuleID++;
    return SECSuccess;
fail2:
    if (enforceAlreadyInitializedError || (!alreadyLoaded)) {
        PK11_GETTAB(mod)->C_Finalize(NULL);
    }
fail:
    mod->functionList = NULL;
    disableUnload = PR_GetEnvSecure("NSS_DISABLE_UNLOAD");
    if (library && !disableUnload) {
        PR_UnloadLibrary(library);
    }
    return SECFailure;
}

SECStatus
SECMOD_UnloadModule(SECMODModule *mod)
{
    PRLibrary *library;
    char *disableUnload = NULL;

    if (!mod->loaded) {
        return SECFailure;
    }
    if (finalizeModules) {
        if (mod->functionList && !mod->moduleDBOnly) {
            PK11_GETTAB(mod)->C_Finalize(NULL);
        }
    }
    mod->moduleID = 0;
    mod->loaded = PR_FALSE;

    /* do we want the semantics to allow unloading the internal library?
     * if not, we should change this to SECFailure and move it above the
     * mod->loaded = PR_FALSE; */
    if (mod->internal && (mod->dllName == NULL)) {
#ifndef NSS_STATIC_SOFTOKEN
        if (0 == PR_ATOMIC_DECREMENT(&softokenLoadCount)) {
            if (softokenLib) {
                disableUnload = PR_GetEnvSecure("NSS_DISABLE_UNLOAD");
                if (!disableUnload) {
#ifdef DEBUG
                    PRStatus status = PR_UnloadLibrary(softokenLib);
                    PORT_Assert(PR_SUCCESS == status);
#else
                    PR_UnloadLibrary(softokenLib);
#endif
                }
                softokenLib = NULL;
            }
            loadSoftokenOnce = pristineCallOnce;
        }
#endif
        return SECSuccess;
    }

    library = (PRLibrary *)mod->library;
    /* paranoia */
    if (library == NULL) {
        return SECFailure;
    }

    disableUnload = PR_GetEnvSecure("NSS_DISABLE_UNLOAD");
    if (!disableUnload) {
        PR_UnloadLibrary(library);
    }
    return SECSuccess;
}

void
nss_DumpModuleLog(void)
{
#ifdef DEBUG_MODULE
    if (modToDBG) {
        print_final_statistics();
    }
#endif
}
