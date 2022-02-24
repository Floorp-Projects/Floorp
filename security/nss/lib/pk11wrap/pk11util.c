/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Initialize the PCKS 11 subsystem
 */
#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pk11func.h"
#include "pki3hack.h"
#include "secerr.h"
#include "dev.h"
#include "dev3hack.h"
#include "utilpars.h"
#include "pkcs11uri.h"

/* these are for displaying error messages */

static SECMODModuleList *modules = NULL;
static SECMODModuleList *modulesDB = NULL;
static SECMODModuleList *modulesUnload = NULL;
static SECMODModule *internalModule = NULL;
static SECMODModule *defaultDBModule = NULL;
static SECMODModule *pendingModule = NULL;
static SECMODListLock *moduleLock = NULL;

int secmod_PrivateModuleCount = 0;

extern const PK11DefaultArrayEntry PK11_DefaultArray[];
extern const int num_pk11_default_mechanisms;

void
SECMOD_Init()
{
    /* don't initialize twice */
    if (moduleLock)
        return;

    moduleLock = SECMOD_NewListLock();
    PK11_InitSlotLists();
}

SECStatus
SECMOD_Shutdown()
{
    /* destroy the lock */
    if (moduleLock) {
        SECMOD_DestroyListLock(moduleLock);
        moduleLock = NULL;
    }
    /* free the internal module */
    if (internalModule) {
        SECMOD_DestroyModule(internalModule);
        internalModule = NULL;
    }

    /* free the default database module */
    if (defaultDBModule) {
        SECMOD_DestroyModule(defaultDBModule);
        defaultDBModule = NULL;
    }

    /* destroy the list */
    if (modules) {
        SECMOD_DestroyModuleList(modules);
        modules = NULL;
    }

    if (modulesDB) {
        SECMOD_DestroyModuleList(modulesDB);
        modulesDB = NULL;
    }

    if (modulesUnload) {
        SECMOD_DestroyModuleList(modulesUnload);
        modulesUnload = NULL;
    }

    /* make all the slots and the lists go away */
    PK11_DestroySlotLists();

    nss_DumpModuleLog();

#ifdef DEBUG
    if (PR_GetEnvSecure("NSS_STRICT_SHUTDOWN")) {
        PORT_Assert(secmod_PrivateModuleCount == 0);
    }
#endif
    if (secmod_PrivateModuleCount) {
        PORT_SetError(SEC_ERROR_BUSY);
        return SECFailure;
    }
    return SECSuccess;
}

PRBool
SECMOD_GetSystemFIPSEnabled(void)
{
#ifdef LINUX
#ifndef NSS_FIPS_DISABLED
    FILE *f;
    char d;
    size_t size;

    f = fopen("/proc/sys/crypto/fips_enabled", "r");
    if (!f) {
        return PR_FALSE;
    }

    size = fread(&d, 1, sizeof(d), f);
    fclose(f);
    if (size != sizeof(d)) {
        return PR_FALSE;
    }
    if (d == '1') {
        return PR_TRUE;
    }
#endif
#endif
    return PR_FALSE;
}

/*
 * retrieve the internal module
 */
SECMODModule *
SECMOD_GetInternalModule(void)
{
    return internalModule;
}

SECStatus
secmod_AddModuleToList(SECMODModuleList **moduleList, SECMODModule *newModule)
{
    SECMODModuleList *mlp, *newListElement, *last = NULL;

    newListElement = SECMOD_NewModuleListElement();
    if (newListElement == NULL) {
        return SECFailure;
    }

    newListElement->module = SECMOD_ReferenceModule(newModule);

    SECMOD_GetWriteLock(moduleLock);
    /* Added it to the end (This is very inefficient, but Adding a module
     * on the fly should happen maybe 2-3 times through the life this program
     * on a given computer, and this list should be *SHORT*. */
    for (mlp = *moduleList; mlp != NULL; mlp = mlp->next) {
        last = mlp;
    }

    if (last == NULL) {
        *moduleList = newListElement;
    } else {
        SECMOD_AddList(last, newListElement, NULL);
    }
    SECMOD_ReleaseWriteLock(moduleLock);
    return SECSuccess;
}

SECStatus
SECMOD_AddModuleToList(SECMODModule *newModule)
{
    if (newModule->internal && !internalModule) {
        internalModule = SECMOD_ReferenceModule(newModule);
    }
    return secmod_AddModuleToList(&modules, newModule);
}

SECStatus
SECMOD_AddModuleToDBOnlyList(SECMODModule *newModule)
{
    if (defaultDBModule && SECMOD_GetDefaultModDBFlag(newModule)) {
        SECMOD_DestroyModule(defaultDBModule);
        defaultDBModule = SECMOD_ReferenceModule(newModule);
    } else if (defaultDBModule == NULL) {
        defaultDBModule = SECMOD_ReferenceModule(newModule);
    }
    return secmod_AddModuleToList(&modulesDB, newModule);
}

SECStatus
SECMOD_AddModuleToUnloadList(SECMODModule *newModule)
{
    return secmod_AddModuleToList(&modulesUnload, newModule);
}

/*
 * get the list of PKCS11 modules that are available.
 */
SECMODModuleList *
SECMOD_GetDefaultModuleList()
{
    return modules;
}
SECMODModuleList *
SECMOD_GetDeadModuleList()
{
    return modulesUnload;
}
SECMODModuleList *
SECMOD_GetDBModuleList()
{
    return modulesDB;
}

/*
 * This lock protects the global module lists.
 * it also protects changes to the slot array (module->slots[]) and slot count
 * (module->slotCount) in each module. It is a read/write lock with multiple
 * readers or one writer. Writes are uncommon.
 * Because of legacy considerations protection of the slot array and count is
 * only necessary in applications if the application calls
 * SECMOD_UpdateSlotList() or SECMOD_WaitForAnyTokenEvent(), though all new
 * applications are encouraged to acquire this lock when reading the
 * slot array information directly.
 */
SECMODListLock *
SECMOD_GetDefaultModuleListLock()
{
    return moduleLock;
}

/*
 * find a module by name, and add a reference to it.
 * return that module.
 */
SECMODModule *
SECMOD_FindModule(const char *name)
{
    SECMODModuleList *mlp;
    SECMODModule *module = NULL;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return module;
    }
    SECMOD_GetReadLock(moduleLock);
    for (mlp = modules; mlp != NULL; mlp = mlp->next) {
        if (PORT_Strcmp(name, mlp->module->commonName) == 0) {
            module = mlp->module;
            SECMOD_ReferenceModule(module);
            break;
        }
    }
    if (module) {
        goto found;
    }
    for (mlp = modulesUnload; mlp != NULL; mlp = mlp->next) {
        if (PORT_Strcmp(name, mlp->module->commonName) == 0) {
            module = mlp->module;
            SECMOD_ReferenceModule(module);
            break;
        }
    }

found:
    SECMOD_ReleaseReadLock(moduleLock);

    return module;
}

/*
 * find a module by ID, and add a reference to it.
 * return that module.
 */
SECMODModule *
SECMOD_FindModuleByID(SECMODModuleID id)
{
    SECMODModuleList *mlp;
    SECMODModule *module = NULL;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return module;
    }
    SECMOD_GetReadLock(moduleLock);
    for (mlp = modules; mlp != NULL; mlp = mlp->next) {
        if (id == mlp->module->moduleID) {
            module = mlp->module;
            SECMOD_ReferenceModule(module);
            break;
        }
    }
    SECMOD_ReleaseReadLock(moduleLock);
    if (module == NULL) {
        PORT_SetError(SEC_ERROR_NO_MODULE);
    }
    return module;
}

/*
 * find the function pointer.
 */
SECMODModule *
secmod_FindModuleByFuncPtr(void *funcPtr)
{
    SECMODModuleList *mlp;
    SECMODModule *module = NULL;

    SECMOD_GetReadLock(moduleLock);
    for (mlp = modules; mlp != NULL; mlp = mlp->next) {
        /* paranoia, shouldn't ever happen */
        if (!mlp->module) {
            continue;
        }
        if (funcPtr == mlp->module->functionList) {
            module = mlp->module;
            SECMOD_ReferenceModule(module);
            break;
        }
    }
    SECMOD_ReleaseReadLock(moduleLock);
    if (module == NULL) {
        PORT_SetError(SEC_ERROR_NO_MODULE);
    }
    return module;
}

/*
 * Find the Slot based on ID and the module.
 */
PK11SlotInfo *
SECMOD_FindSlotByID(SECMODModule *module, CK_SLOT_ID slotID)
{
    int i;
    PK11SlotInfo *slot = NULL;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return slot;
    }
    SECMOD_GetReadLock(moduleLock);
    for (i = 0; i < module->slotCount; i++) {
        PK11SlotInfo *cSlot = module->slots[i];

        if (cSlot->slotID == slotID) {
            slot = PK11_ReferenceSlot(cSlot);
            break;
        }
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (slot == NULL) {
        PORT_SetError(SEC_ERROR_NO_SLOT_SELECTED);
    }
    return slot;
}

/*
 * lookup the Slot module based on it's module ID and slot ID.
 */
PK11SlotInfo *
SECMOD_LookupSlot(SECMODModuleID moduleID, CK_SLOT_ID slotID)
{
    SECMODModule *module;
    PK11SlotInfo *slot;

    module = SECMOD_FindModuleByID(moduleID);
    if (module == NULL)
        return NULL;

    slot = SECMOD_FindSlotByID(module, slotID);
    SECMOD_DestroyModule(module);
    return slot;
}

/*
 * find a module by name or module pointer and delete it off the module list.
 * optionally remove it from secmod.db.
 */
SECStatus
SECMOD_DeleteModuleEx(const char *name, SECMODModule *mod,
                      int *type, PRBool permdb)
{
    SECMODModuleList *mlp;
    SECMODModuleList **mlpp;
    SECStatus rv = SECFailure;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return rv;
    }

    *type = SECMOD_EXTERNAL;

    SECMOD_GetWriteLock(moduleLock);
    for (mlpp = &modules, mlp = modules;
         mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
        if ((name && (PORT_Strcmp(name, mlp->module->commonName) == 0)) ||
            mod == mlp->module) {
            /* don't delete the internal module */
            if (!mlp->module->internal) {
                SECMOD_RemoveList(mlpp, mlp);
                /* delete it after we release the lock */
                rv = STAN_RemoveModuleFromDefaultTrustDomain(mlp->module);
            } else if (mlp->module->isFIPS) {
                *type = SECMOD_FIPS;
            } else {
                *type = SECMOD_INTERNAL;
            }
            break;
        }
    }
    if (mlp) {
        goto found;
    }
    /* not on the internal list, check the unload list */
    for (mlpp = &modulesUnload, mlp = modulesUnload;
         mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
        if ((name && (PORT_Strcmp(name, mlp->module->commonName) == 0)) ||
            mod == mlp->module) {
            /* don't delete the internal module */
            if (!mlp->module->internal) {
                SECMOD_RemoveList(mlpp, mlp);
                rv = SECSuccess;
            } else if (mlp->module->isFIPS) {
                *type = SECMOD_FIPS;
            } else {
                *type = SECMOD_INTERNAL;
            }
            break;
        }
    }
found:
    SECMOD_ReleaseWriteLock(moduleLock);

    if (rv == SECSuccess) {
        if (permdb) {
            SECMOD_DeletePermDB(mlp->module);
        }
        SECMOD_DestroyModuleListElement(mlp);
    }
    return rv;
}

/*
 * find a module by name and delete it off the module list
 */
SECStatus
SECMOD_DeleteModule(const char *name, int *type)
{
    return SECMOD_DeleteModuleEx(name, NULL, type, PR_TRUE);
}

/*
 * find a module by name and delete it off the module list
 */
SECStatus
SECMOD_DeleteInternalModule(const char *name)
{
    SECMODModuleList *mlp;
    SECMODModuleList **mlpp;
    SECStatus rv = SECFailure;

    if (SECMOD_GetSystemFIPSEnabled() || pendingModule) {
        PORT_SetError(SEC_ERROR_MODULE_STUCK);
        return rv;
    }
    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return rv;
    }

#ifdef NSS_FIPS_DISABLED
    PORT_SetError(PR_OPERATION_NOT_SUPPORTED_ERROR);
    return rv;
#endif

    SECMOD_GetWriteLock(moduleLock);
    for (mlpp = &modules, mlp = modules;
         mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
        if (PORT_Strcmp(name, mlp->module->commonName) == 0) {
            /* don't delete the internal module */
            if (mlp->module->internal) {
                SECMOD_RemoveList(mlpp, mlp);
                rv = STAN_RemoveModuleFromDefaultTrustDomain(mlp->module);
            }
            break;
        }
    }
    SECMOD_ReleaseWriteLock(moduleLock);

    if (rv == SECSuccess) {
        SECMODModule *newModule, *oldModule;

        if (mlp->module->isFIPS) {
            newModule = SECMOD_CreateModule(NULL, SECMOD_INT_NAME,
                                            NULL, SECMOD_INT_FLAGS);
        } else {
            newModule = SECMOD_CreateModule(NULL, SECMOD_FIPS_NAME,
                                            NULL, SECMOD_FIPS_FLAGS);
        }
        if (newModule) {
            PK11SlotInfo *slot;
            newModule->libraryParams =
                PORT_ArenaStrdup(newModule->arena, mlp->module->libraryParams);
            /* if an explicit internal key slot has been set, reset it */
            slot = pk11_SwapInternalKeySlot(NULL);
            if (slot) {
                secmod_SetInternalKeySlotFlag(newModule, PR_TRUE);
            }
            rv = SECMOD_AddModule(newModule);
            if (rv != SECSuccess) {
                /* load failed, restore the internal key slot */
                pk11_SetInternalKeySlot(slot);
                SECMOD_DestroyModule(newModule);
                newModule = NULL;
            }
            /* free the old explicit internal key slot, we now have a new one */
            if (slot) {
                PK11_FreeSlot(slot);
            }
        }
        if (newModule == NULL) {
            SECMODModuleList *last = NULL, *mlp2;
            /* we're in pretty deep trouble if this happens...Security
             * not going to work well... try to put the old module back on
             * the list */
            SECMOD_GetWriteLock(moduleLock);
            for (mlp2 = modules; mlp2 != NULL; mlp2 = mlp->next) {
                last = mlp2;
            }

            if (last == NULL) {
                modules = mlp;
            } else {
                SECMOD_AddList(last, mlp, NULL);
            }
            SECMOD_ReleaseWriteLock(moduleLock);
            return SECFailure;
        }
        pendingModule = oldModule = internalModule;
        internalModule = NULL;
        SECMOD_DestroyModule(oldModule);
        SECMOD_DeletePermDB(mlp->module);
        SECMOD_DestroyModuleListElement(mlp);
        internalModule = newModule; /* adopt the module */
    }
    return rv;
}

SECStatus
SECMOD_AddModule(SECMODModule *newModule)
{
    SECStatus rv;
    SECMODModule *oldModule;

    /* Test if a module w/ the same name already exists */
    /* and return SECWouldBlock if so. */
    /* We should probably add a new return value such as */
    /* SECDublicateModule, but to minimize ripples, I'll */
    /* give SECWouldBlock a new meaning */
    if ((oldModule = SECMOD_FindModule(newModule->commonName)) != NULL) {
        SECMOD_DestroyModule(oldModule);
        return SECWouldBlock;
        /* module already exists. */
    }

    rv = secmod_LoadPKCS11Module(newModule, NULL);
    if (rv != SECSuccess) {
        return rv;
    }

    if (newModule->parent == NULL) {
        newModule->parent = SECMOD_ReferenceModule(defaultDBModule);
    }

    SECMOD_AddPermDB(newModule);
    SECMOD_AddModuleToList(newModule);

    rv = STAN_AddModuleToDefaultTrustDomain(newModule);

    return rv;
}

PK11SlotInfo *
SECMOD_FindSlot(SECMODModule *module, const char *name)
{
    int i;
    char *string;
    PK11SlotInfo *retSlot = NULL;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return retSlot;
    }
    SECMOD_GetReadLock(moduleLock);
    for (i = 0; i < module->slotCount; i++) {
        PK11SlotInfo *slot = module->slots[i];

        if (PK11_IsPresent(slot)) {
            string = PK11_GetTokenName(slot);
        } else {
            string = PK11_GetSlotName(slot);
        }
        if (PORT_Strcmp(name, string) == 0) {
            retSlot = PK11_ReferenceSlot(slot);
            break;
        }
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (retSlot == NULL) {
        PORT_SetError(SEC_ERROR_NO_SLOT_SELECTED);
    }
    return retSlot;
}

SECStatus
PK11_GetModInfo(SECMODModule *mod, CK_INFO *info)
{
    CK_RV crv;

    if (mod->functionList == NULL)
        return SECFailure;
    crv = PK11_GETTAB(mod)->C_GetInfo(info);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
    }
    return (crv == CKR_OK) ? SECSuccess : SECFailure;
}

char *
PK11_GetModuleURI(SECMODModule *mod)
{
    CK_INFO info;
    PK11URI *uri;
    char *ret = NULL;
    PK11URIAttribute attrs[3];
    size_t nattrs = 0;
    char libraryManufacturer[32 + 1], libraryDescription[32 + 1], libraryVersion[8];

    if (PK11_GetModInfo(mod, &info) == SECFailure) {
        return NULL;
    }

    PK11_MakeString(NULL, libraryManufacturer, (char *)info.manufacturerID,
                    sizeof(info.manufacturerID));
    if (*libraryManufacturer != '\0') {
        attrs[nattrs].name = PK11URI_PATTR_LIBRARY_MANUFACTURER;
        attrs[nattrs].value = libraryManufacturer;
        nattrs++;
    }

    PK11_MakeString(NULL, libraryDescription, (char *)info.libraryDescription,
                    sizeof(info.libraryDescription));
    if (*libraryDescription != '\0') {
        attrs[nattrs].name = PK11URI_PATTR_LIBRARY_DESCRIPTION;
        attrs[nattrs].value = libraryDescription;
        nattrs++;
    }

    PR_snprintf(libraryVersion, sizeof(libraryVersion), "%d.%d",
                info.libraryVersion.major, info.libraryVersion.minor);
    attrs[nattrs].name = PK11URI_PATTR_LIBRARY_VERSION;
    attrs[nattrs].value = libraryVersion;
    nattrs++;

    uri = PK11URI_CreateURI(attrs, nattrs, NULL, 0);
    if (uri == NULL) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL;
    }

    ret = PK11URI_FormatURI(NULL, uri);
    PK11URI_DestroyURI(uri);
    if (ret == NULL) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL;
    }

    return ret;
}

/* Determine if we have the FIP's module loaded as the default
 * module to trigger other bogus FIPS requirements in PKCS #12 and
 * SSL
 */
PRBool
PK11_IsFIPS(void)
{
    SECMODModule *mod = SECMOD_GetInternalModule();

    if (mod && mod->internal) {
        return mod->isFIPS;
    }

    return PR_FALSE;
}

/* combines NewModule() & AddModule */
/* give a string for the module name & the full-path for the dll, */
/* installs the PKCS11 module & update registry */
SECStatus
SECMOD_AddNewModuleEx(const char *moduleName, const char *dllPath,
                      unsigned long defaultMechanismFlags,
                      unsigned long cipherEnableFlags,
                      char *modparms, char *nssparms)
{
    SECMODModule *module;
    SECStatus result = SECFailure;
    int s, i;
    PK11SlotInfo *slot;

    PR_SetErrorText(0, NULL);
    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return result;
    }

    module = SECMOD_CreateModule(dllPath, moduleName, modparms, nssparms);

    if (module == NULL) {
        return result;
    }

    if (module->dllName != NULL) {
        if (module->dllName[0] != 0) {
            result = SECMOD_AddModule(module);
            if (result == SECSuccess) {
                /* turn on SSL cipher enable flags */
                module->ssl[0] = cipherEnableFlags;

                SECMOD_GetReadLock(moduleLock);
                /* check each slot to turn on appropriate mechanisms */
                for (s = 0; s < module->slotCount; s++) {
                    slot = (module->slots)[s];
                    /* for each possible mechanism */
                    for (i = 0; i < num_pk11_default_mechanisms; i++) {
                        /* we are told to turn it on by default ? */
                        PRBool add =
                            (PK11_DefaultArray[i].flag & defaultMechanismFlags) ? PR_TRUE : PR_FALSE;
                        result = PK11_UpdateSlotAttribute(slot,
                                                          &(PK11_DefaultArray[i]), add);
                        if (result != SECSuccess) {
                            SECMOD_ReleaseReadLock(moduleLock);
                            SECMOD_DestroyModule(module);
                            return result;
                        }
                    } /* for each mechanism */
                    /* disable each slot if the defaultFlags say so */
                    if (defaultMechanismFlags & PK11_DISABLE_FLAG) {
                        PK11_UserDisableSlot(slot);
                    }
                } /* for each slot of this module */
                SECMOD_ReleaseReadLock(moduleLock);

                /* delete and re-add module in order to save changes
                 * to the module */
                result = SECMOD_UpdateModule(module);
            }
        }
    }
    SECMOD_DestroyModule(module);
    return result;
}

SECStatus
SECMOD_AddNewModule(const char *moduleName, const char *dllPath,
                    unsigned long defaultMechanismFlags,
                    unsigned long cipherEnableFlags)
{
    return SECMOD_AddNewModuleEx(moduleName, dllPath, defaultMechanismFlags,
                                 cipherEnableFlags,
                                 NULL, NULL); /* don't pass module or nss params */
}

SECStatus
SECMOD_UpdateModule(SECMODModule *module)
{
    SECStatus result;

    result = SECMOD_DeletePermDB(module);

    if (result == SECSuccess) {
        result = SECMOD_AddPermDB(module);
    }
    return result;
}

/* Public & Internal(Security Library)  representation of
 * encryption mechanism flags conversion */

/* Currently, the only difference is that internal representation
 * puts RANDOM_FLAG at bit 31 (Most-significant bit), but
 * public representation puts this bit at bit 28
 */
unsigned long
SECMOD_PubMechFlagstoInternal(unsigned long publicFlags)
{
    unsigned long internalFlags = publicFlags;

    if (publicFlags & PUBLIC_MECH_RANDOM_FLAG) {
        internalFlags &= ~PUBLIC_MECH_RANDOM_FLAG;
        internalFlags |= SECMOD_RANDOM_FLAG;
    }
    return internalFlags;
}

unsigned long
SECMOD_InternaltoPubMechFlags(unsigned long internalFlags)
{
    unsigned long publicFlags = internalFlags;

    if (internalFlags & SECMOD_RANDOM_FLAG) {
        publicFlags &= ~SECMOD_RANDOM_FLAG;
        publicFlags |= PUBLIC_MECH_RANDOM_FLAG;
    }
    return publicFlags;
}

/* Public & Internal(Security Library)  representation of */
/* cipher flags conversion */
/* Note: currently they are just stubs */
unsigned long
SECMOD_PubCipherFlagstoInternal(unsigned long publicFlags)
{
    return publicFlags;
}

unsigned long
SECMOD_InternaltoPubCipherFlags(unsigned long internalFlags)
{
    return internalFlags;
}

/* Funtion reports true if module of modType is installed/configured */
PRBool
SECMOD_IsModulePresent(unsigned long int pubCipherEnableFlags)
{
    PRBool result = PR_FALSE;
    SECMODModuleList *mods;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return result;
    }
    SECMOD_GetReadLock(moduleLock);
    mods = SECMOD_GetDefaultModuleList();
    for (; mods != NULL; mods = mods->next) {
        if (mods->module->ssl[0] &
            SECMOD_PubCipherFlagstoInternal(pubCipherEnableFlags)) {
            result = PR_TRUE;
        }
    }

    SECMOD_ReleaseReadLock(moduleLock);
    return result;
}

/* create a new ModuleListElement */
SECMODModuleList *
SECMOD_NewModuleListElement(void)
{
    SECMODModuleList *newModList;

    newModList = (SECMODModuleList *)PORT_Alloc(sizeof(SECMODModuleList));
    if (newModList) {
        newModList->next = NULL;
        newModList->module = NULL;
    }
    return newModList;
}

/*
 * make a new reference to a module so It doesn't go away on us
 */
SECMODModule *
SECMOD_ReferenceModule(SECMODModule *module)
{
    PZ_Lock(module->refLock);
    PORT_Assert(module->refCount > 0);

    module->refCount++;
    PZ_Unlock(module->refLock);
    return module;
}

/* destroy an existing module */
void
SECMOD_DestroyModule(SECMODModule *module)
{
    PRBool willfree = PR_FALSE;
    int slotCount;
    int i;

    PZ_Lock(module->refLock);
    if (module->refCount-- == 1) {
        willfree = PR_TRUE;
    }
    PORT_Assert(willfree || (module->refCount > 0));
    PZ_Unlock(module->refLock);

    if (!willfree) {
        return;
    }

    if (module->parent != NULL) {
        SECMODModule *parent = module->parent;
        /* paranoia, don't loop forever if the modules are looped */
        module->parent = NULL;
        SECMOD_DestroyModule(parent);
    }

    /* slots can't really disappear until our module starts freeing them,
     * so this check is safe */
    slotCount = module->slotCount;
    if (slotCount == 0) {
        SECMOD_SlotDestroyModule(module, PR_FALSE);
        return;
    }

    /* now free all out slots, when they are done, they will cause the
     * module to disappear altogether */
    for (i = 0; i < slotCount; i++) {
        if (!module->slots[i]->disabled) {
            PK11_ClearSlotList(module->slots[i]);
        }
        PK11_FreeSlot(module->slots[i]);
    }
    /* WARNING: once the last slot has been freed is it possible (even likely)
     * that module is no more... touching it now is a good way to go south */
}

/* we can only get here if we've destroyed the module, or some one has
 * erroneously freed a slot that wasn't referenced. */
void
SECMOD_SlotDestroyModule(SECMODModule *module, PRBool fromSlot)
{
    PRBool willfree = PR_FALSE;
    if (fromSlot) {
        PORT_Assert(module->refCount == 0);
        PZ_Lock(module->refLock);
        if (module->slotCount-- == 1) {
            willfree = PR_TRUE;
        }
        PORT_Assert(willfree || (module->slotCount > 0));
        PZ_Unlock(module->refLock);
        if (!willfree)
            return;
    }

    if (module == pendingModule) {
        pendingModule = NULL;
    }

    if (module->loaded) {
        SECMOD_UnloadModule(module);
    }
    PZ_DestroyLock(module->refLock);
    PORT_FreeArena(module->arena, PR_FALSE);
    secmod_PrivateModuleCount--;
}

/* destroy a list element
 * this destroys a single element, and returns the next element
 * on the chain. It makes it easy to implement for loops to delete
 * the chain. It also make deleting a single element easy */
SECMODModuleList *
SECMOD_DestroyModuleListElement(SECMODModuleList *element)
{
    SECMODModuleList *next = element->next;

    if (element->module) {
        SECMOD_DestroyModule(element->module);
        element->module = NULL;
    }
    PORT_Free(element);
    return next;
}

/*
 * Destroy an entire module list
 */
void
SECMOD_DestroyModuleList(SECMODModuleList *list)
{
    SECMODModuleList *lp;

    for (lp = list; lp != NULL; lp = SECMOD_DestroyModuleListElement(lp))
        ;
}

PRBool
SECMOD_CanDeleteInternalModule(void)
{
#ifdef NSS_FIPS_DISABLED
    return PR_FALSE;
#else
    return (PRBool)((pendingModule == NULL) && !SECMOD_GetSystemFIPSEnabled());
#endif
}

/*
 * check to see if the module has added new slots. PKCS 11 v2.20 allows for
 * modules to add new slots, but never remove them. Slots cannot be added
 * between a call to C_GetSlotLlist(Flag, NULL, &count) and the subsequent
 * C_GetSlotList(flag, &data, &count) so that the array doesn't accidently
 * grow on the caller. It is permissible for the slots to increase between
 * successive calls with NULL to get the size.
 */
SECStatus
SECMOD_UpdateSlotList(SECMODModule *mod)
{
    CK_RV crv;
    CK_ULONG count;
    CK_ULONG i, oldCount;
    PRBool freeRef = PR_FALSE;
    void *mark = NULL;
    CK_ULONG *slotIDs = NULL;
    PK11SlotInfo **newSlots = NULL;
    PK11SlotInfo **oldSlots = NULL;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }

    /* C_GetSlotList is not a session function, make sure
     * calls are serialized */
    PZ_Lock(mod->refLock);
    freeRef = PR_TRUE;
    /* see if the number of slots have changed */
    crv = PK11_GETTAB(mod)->C_GetSlotList(PR_FALSE, NULL, &count);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        goto loser;
    }
    /* nothing new, blow out early, we want this function to be quick
     * and cheap in the normal case  */
    if (count == mod->slotCount) {
        PZ_Unlock(mod->refLock);
        return SECSuccess;
    }
    if (count < (CK_ULONG)mod->slotCount) {
        /* shouldn't happen with a properly functioning PKCS #11 module */
        PORT_SetError(SEC_ERROR_INCOMPATIBLE_PKCS11);
        goto loser;
    }

    /* get the new slot list */
    slotIDs = PORT_NewArray(CK_SLOT_ID, count);
    if (slotIDs == NULL) {
        goto loser;
    }

    crv = PK11_GETTAB(mod)->C_GetSlotList(PR_FALSE, slotIDs, &count);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        goto loser;
    }
    freeRef = PR_FALSE;
    PZ_Unlock(mod->refLock);
    mark = PORT_ArenaMark(mod->arena);
    if (mark == NULL) {
        goto loser;
    }
    newSlots = PORT_ArenaZNewArray(mod->arena, PK11SlotInfo *, count);

    /* walk down the new slot ID list returned from the module. We keep
     * the old slots which match a returned ID, and we initialize the new
     * slots. */
    for (i = 0; i < count; i++) {
        PK11SlotInfo *slot = SECMOD_FindSlotByID(mod, slotIDs[i]);

        if (!slot) {
            /* we have a new slot create a new slot data structure */
            slot = PK11_NewSlotInfo(mod);
            if (!slot) {
                goto loser;
            }
            PK11_InitSlot(mod, slotIDs[i], slot);
            STAN_InitTokenForSlotInfo(NULL, slot);
        }
        newSlots[i] = slot;
    }
    STAN_ResetTokenInterator(NULL);
    PORT_Free(slotIDs);
    slotIDs = NULL;
    PORT_ArenaUnmark(mod->arena, mark);

    /* until this point we're still using the old slot list. Now we update
     * module slot list. We update the slots (array) first then the count,
     * since we've already guarrenteed that count has increased (just in case
     * someone is looking at the slots field of  module without holding the
     * moduleLock */
    SECMOD_GetWriteLock(moduleLock);
    oldCount = mod->slotCount;
    oldSlots = mod->slots;
    mod->slots = newSlots; /* typical arena 'leak'... old mod->slots is
                            * allocated out of the module arena and won't
                            * be freed until the module is freed */
    mod->slotCount = count;
    SECMOD_ReleaseWriteLock(moduleLock);
    /* free our old references before forgetting about oldSlot*/
    for (i = 0; i < oldCount; i++) {
        PK11_FreeSlot(oldSlots[i]);
    }
    return SECSuccess;

loser:
    if (freeRef) {
        PZ_Unlock(mod->refLock);
    }
    if (slotIDs) {
        PORT_Free(slotIDs);
    }
    /* free all the slots we allocated. newSlots are part of the
     * mod arena. NOTE: the newSlots array contain both new and old
     * slots, but we kept a reference to the old slots when we built the new
     * array, so we need to free all the slots in newSlots array. */
    if (newSlots) {
        for (i = 0; i < count; i++) {
            if (newSlots[i] == NULL) {
                break; /* hit the last one */
            }
            PK11_FreeSlot(newSlots[i]);
        }
    }
    /* must come after freeing newSlots */
    if (mark) {
        PORT_ArenaRelease(mod->arena, mark);
    }
    return SECFailure;
}

/*
 * this handles modules that do not support C_WaitForSlotEvent().
 * The internal flags are stored. Note that C_WaitForSlotEvent() does not
 * have a timeout, so we don't have one for handleWaitForSlotEvent() either.
 */
PK11SlotInfo *
secmod_HandleWaitForSlotEvent(SECMODModule *mod, unsigned long flags,
                              PRIntervalTime latency)
{
    PRBool removableSlotsFound = PR_FALSE;
    int i;
    int error = SEC_ERROR_NO_EVENT;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return NULL;
    }
    PZ_Lock(mod->refLock);
    if (mod->evControlMask & SECMOD_END_WAIT) {
        mod->evControlMask &= ~SECMOD_END_WAIT;
        PZ_Unlock(mod->refLock);
        PORT_SetError(SEC_ERROR_NO_EVENT);
        return NULL;
    }
    mod->evControlMask |= SECMOD_WAIT_SIMULATED_EVENT;
    while (mod->evControlMask & SECMOD_WAIT_SIMULATED_EVENT) {
        PZ_Unlock(mod->refLock);
        /* now is a good time to see if new slots have been added */
        SECMOD_UpdateSlotList(mod);

        /* loop through all the slots on a module */
        SECMOD_GetReadLock(moduleLock);
        for (i = 0; i < mod->slotCount; i++) {
            PK11SlotInfo *slot = mod->slots[i];
            PRUint16 series;
            PRBool present;

            /* perm modules do not change */
            if (slot->isPerm) {
                continue;
            }
            removableSlotsFound = PR_TRUE;
            /* simulate the PKCS #11 module flags. are the flags different
             * from the last time we called? */
            series = slot->series;
            present = PK11_IsPresent(slot);
            if ((slot->flagSeries != series) || (slot->flagState != present)) {
                slot->flagState = present;
                slot->flagSeries = series;
                SECMOD_ReleaseReadLock(moduleLock);
                PZ_Lock(mod->refLock);
                mod->evControlMask &= ~SECMOD_END_WAIT;
                PZ_Unlock(mod->refLock);
                return PK11_ReferenceSlot(slot);
            }
        }
        SECMOD_ReleaseReadLock(moduleLock);
        /* if everything was perm modules, don't lock up forever */
        if ((mod->slotCount != 0) && !removableSlotsFound) {
            error = SEC_ERROR_NO_SLOT_SELECTED;
            PZ_Lock(mod->refLock);
            break;
        }
        if (flags & CKF_DONT_BLOCK) {
            PZ_Lock(mod->refLock);
            break;
        }
        PR_Sleep(latency);
        PZ_Lock(mod->refLock);
    }
    mod->evControlMask &= ~SECMOD_END_WAIT;
    PZ_Unlock(mod->refLock);
    PORT_SetError(error);
    return NULL;
}

/*
 * this function waits for a token event on any slot of a given module
 * This function should not be called from more than one thread of the
 * same process (though other threads can make other library calls
 * on this module while this call is blocked).
 */
PK11SlotInfo *
SECMOD_WaitForAnyTokenEvent(SECMODModule *mod, unsigned long flags,
                            PRIntervalTime latency)
{
    CK_SLOT_ID id;
    CK_RV crv;
    PK11SlotInfo *slot;

    if (!pk11_getFinalizeModulesOption() ||
        ((mod->cryptokiVersion.major == 2) &&
         (mod->cryptokiVersion.minor < 1))) {
        /* if we are sharing the module with other software in our
         * address space, we can't reliably use C_WaitForSlotEvent(),
         * and if the module is version 2.0, C_WaitForSlotEvent() doesn't
         * exist */
        return secmod_HandleWaitForSlotEvent(mod, flags, latency);
    }
    /* first the the PKCS #11 call */
    PZ_Lock(mod->refLock);
    if (mod->evControlMask & SECMOD_END_WAIT) {
        goto end_wait;
    }
    mod->evControlMask |= SECMOD_WAIT_PKCS11_EVENT;
    PZ_Unlock(mod->refLock);
    crv = PK11_GETTAB(mod)->C_WaitForSlotEvent(flags, &id, NULL);
    PZ_Lock(mod->refLock);
    mod->evControlMask &= ~SECMOD_WAIT_PKCS11_EVENT;
    /* if we are in end wait, short circuit now, don't even risk
     * going into secmod_HandleWaitForSlotEvent */
    if (mod->evControlMask & SECMOD_END_WAIT) {
        goto end_wait;
    }
    PZ_Unlock(mod->refLock);
    if (crv == CKR_FUNCTION_NOT_SUPPORTED) {
        /* module doesn't support that call, simulate it */
        return secmod_HandleWaitForSlotEvent(mod, flags, latency);
    }
    if (crv != CKR_OK) {
        /* we can get this error if finalize was called while we were
         * still running. This is the only way to force a C_WaitForSlotEvent()
         * to return in PKCS #11. In this case, just return that there
         * was no event. */
        if (crv == CKR_CRYPTOKI_NOT_INITIALIZED) {
            PORT_SetError(SEC_ERROR_NO_EVENT);
        } else {
            PORT_SetError(PK11_MapError(crv));
        }
        return NULL;
    }
    slot = SECMOD_FindSlotByID(mod, id);
    if (slot == NULL) {
        /* possibly a new slot that was added? */
        SECMOD_UpdateSlotList(mod);
        slot = SECMOD_FindSlotByID(mod, id);
    }
    /* if we are in the delay period for the "isPresent" call, reset
     * the delay since we know things have probably changed... */
    if (slot) {
        NSSToken *nssToken = PK11Slot_GetNSSToken(slot);
        if (nssToken) {
            if (nssToken->slot) {
                nssSlot_ResetDelay(nssToken->slot);
            }
            (void)nssToken_Destroy(nssToken);
        }
    }
    return slot;

/* must be called with the lock on. */
end_wait:
    mod->evControlMask &= ~SECMOD_END_WAIT;
    PZ_Unlock(mod->refLock);
    PORT_SetError(SEC_ERROR_NO_EVENT);
    return NULL;
}

/*
 * This function "wakes up" WaitForAnyTokenEvent. It's a pretty drastic
 * function, possibly bringing down the pkcs #11 module in question. This
 * should be OK because 1) it does reinitialize, and 2) it should only be
 * called when we are on our way to tear the whole system down anyway.
 */
SECStatus
SECMOD_CancelWait(SECMODModule *mod)
{
    unsigned long controlMask;
    SECStatus rv = SECSuccess;
    CK_RV crv;

    PZ_Lock(mod->refLock);
    mod->evControlMask |= SECMOD_END_WAIT;
    controlMask = mod->evControlMask;
    if (controlMask & SECMOD_WAIT_PKCS11_EVENT) {
        if (!pk11_getFinalizeModulesOption()) {
            /* can't get here unless pk11_getFinalizeModulesOption is set */
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            rv = SECFailure;
            goto loser;
        }
        /* NOTE: this call will drop all transient keys, in progress
         * operations, and any authentication. This is the only documented
         * way to get WaitForSlotEvent to return. Also note: for non-thread
         * safe tokens, we need to hold the module lock, this is not yet at
         * system shutdown/startup time, so we need to protect these calls */
        crv = PK11_GETTAB(mod)->C_Finalize(NULL);
        /* ok, we slammed the module down, now we need to reinit it in case
         * we intend to use it again */
        if (CKR_OK == crv) {
            PRBool alreadyLoaded;
            secmod_ModuleInit(mod, NULL, &alreadyLoaded);
        } else {
            /* Finalized failed for some reason,  notify the application
             * so maybe it has a prayer of recovering... */
            PORT_SetError(PK11_MapError(crv));
            rv = SECFailure;
        }
    } else if (controlMask & SECMOD_WAIT_SIMULATED_EVENT) {
        mod->evControlMask &= ~SECMOD_WAIT_SIMULATED_EVENT;
        /* Simulated events will eventually timeout
         * and wake up in the loop */
    }
loser:
    PZ_Unlock(mod->refLock);
    return rv;
}

/*
 * check to see if the module has removable slots that we may need to
 * watch for.
 */
PRBool
SECMOD_HasRemovableSlots(SECMODModule *mod)
{
    int i;
    PRBool ret = PR_FALSE;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return ret;
    }
    SECMOD_GetReadLock(moduleLock);
    for (i = 0; i < mod->slotCount; i++) {
        PK11SlotInfo *slot = mod->slots[i];
        /* perm modules are not inserted or removed */
        if (slot->isPerm) {
            continue;
        }
        ret = PR_TRUE;
        break;
    }
    if (mod->slotCount == 0) {
        ret = PR_TRUE;
    }
    SECMOD_ReleaseReadLock(moduleLock);
    return ret;
}

/*
 * helper function to actually create and destroy user defined slots
 */
static SECStatus
secmod_UserDBOp(PK11SlotInfo *slot, CK_OBJECT_CLASS objClass,
                const char *sendSpec)
{
    CK_OBJECT_HANDLE dummy;
    CK_ATTRIBUTE template[2];
    CK_ATTRIBUTE *attrs = template;
    CK_RV crv;

    PK11_SETATTRS(attrs, CKA_CLASS, &objClass, sizeof(objClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_NSS_MODULE_SPEC, (unsigned char *)sendSpec,
                  strlen(sendSpec) + 1);
    attrs++;

    PORT_Assert(attrs - template <= 2);

    PK11_EnterSlotMonitor(slot);
    crv = PK11_CreateNewObject(slot, slot->session,
                               template, attrs - template, PR_FALSE, &dummy);
    PK11_ExitSlotMonitor(slot);

    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        return SECFailure;
    }
    return SECMOD_UpdateSlotList(slot->module);
}

/*
 * return true if the selected slot ID is not present or doesn't exist
 */
static PRBool
secmod_SlotIsEmpty(SECMODModule *mod, CK_SLOT_ID slotID)
{
    PK11SlotInfo *slot = SECMOD_LookupSlot(mod->moduleID, slotID);
    if (slot) {
        PRBool present = PK11_IsPresent(slot);
        PK11_FreeSlot(slot);
        if (present) {
            return PR_FALSE;
        }
    }
    /* it doesn't exist or isn't present, it's available */
    return PR_TRUE;
}

/*
 * Find an unused slot id in module.
 */
static CK_SLOT_ID
secmod_FindFreeSlot(SECMODModule *mod)
{
    CK_SLOT_ID i, minSlotID, maxSlotID;

    /* look for a free slot id on the internal module */
    if (mod->internal && mod->isFIPS) {
        minSlotID = SFTK_MIN_FIPS_USER_SLOT_ID;
        maxSlotID = SFTK_MAX_FIPS_USER_SLOT_ID;
    } else {
        minSlotID = SFTK_MIN_USER_SLOT_ID;
        maxSlotID = SFTK_MAX_USER_SLOT_ID;
    }
    for (i = minSlotID; i < maxSlotID; i++) {
        if (secmod_SlotIsEmpty(mod, i)) {
            return i;
        }
    }
    PORT_SetError(SEC_ERROR_NO_SLOT_SELECTED);
    return (CK_SLOT_ID)-1;
}

/*
 * Attempt to open a new slot.
 *
 * This works the same os OpenUserDB except it can be called against
 * any module that understands the softoken protocol for opening new
 * slots, not just the softoken itself. If the selected module does not
 * understand the protocol, C_CreateObject will fail with
 * CKR_INVALID_ATTRIBUTE, and SECMOD_OpenNewSlot will return NULL and set
 * SEC_ERROR_BAD_DATA.
 *
 * NewSlots can be closed with SECMOD_CloseUserDB();
 *
 * Modulespec is module dependent.
 */
PK11SlotInfo *
SECMOD_OpenNewSlot(SECMODModule *mod, const char *moduleSpec)
{
    CK_SLOT_ID slotID = 0;
    PK11SlotInfo *slot;
    char *escSpec;
    char *sendSpec;
    SECStatus rv;

    slotID = secmod_FindFreeSlot(mod);
    if (slotID == (CK_SLOT_ID)-1) {
        return NULL;
    }

    if (mod->slotCount == 0) {
        return NULL;
    }

    /* just grab the first slot in the module, any present slot should work */
    slot = PK11_ReferenceSlot(mod->slots[0]);
    if (slot == NULL) {
        return NULL;
    }

    /* we've found the slot, now build the moduleSpec */
    escSpec = NSSUTIL_DoubleEscape(moduleSpec, '>', ']');
    if (escSpec == NULL) {
        PK11_FreeSlot(slot);
        return NULL;
    }
    sendSpec = PR_smprintf("tokens=[0x%x=<%s>]", slotID, escSpec);
    PORT_Free(escSpec);

    if (sendSpec == NULL) {
        /* PR_smprintf does not set SEC_ERROR_NO_MEMORY on failure. */
        PK11_FreeSlot(slot);
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }
    rv = secmod_UserDBOp(slot, CKO_NSS_NEWSLOT, sendSpec);
    PR_smprintf_free(sendSpec);
    PK11_FreeSlot(slot);
    if (rv != SECSuccess) {
        return NULL;
    }

    slot = SECMOD_FindSlotByID(mod, slotID);
    if (slot) {
        /* if we are in the delay period for the "isPresent" call, reset
         * the delay since we know things have probably changed... */
        NSSToken *nssToken = PK11Slot_GetNSSToken(slot);
        if (nssToken) {
            if (nssToken->slot) {
                nssSlot_ResetDelay(nssToken->slot);
            }
            (void)nssToken_Destroy(nssToken);
        }
        /* force the slot info structures to properly reset */
        (void)PK11_IsPresent(slot);
    }
    return slot;
}

/*
 * given a module spec, find the slot in the module for it.
 */
PK11SlotInfo *
secmod_FindSlotFromModuleSpec(const char *moduleSpec, SECMODModule *module)
{
    CK_SLOT_ID slot_id = secmod_GetSlotIDFromModuleSpec(moduleSpec, module);
    if (slot_id == -1) {
        return NULL;
    }

    return SECMOD_FindSlotByID(module, slot_id);
}

/*
 * Open a new database using the softoken. The caller is responsible for making
 * sure the module spec is correct and usable. The caller should ask for one
 * new database per call if the caller wants to get meaningful information
 * about the new database.
 *
 * moduleSpec is the same data that you would pass to softoken at
 * initialization time under the 'tokens' options. For example, if you were
 * to specify tokens=<0x4=[configdir='./mybackup' tokenDescription='Backup']>
 * You would specify "configdir='./mybackup' tokenDescription='Backup'" as your
 * module spec here. The slot ID will be calculated for you by
 * SECMOD_OpenUserDB().
 *
 * Typical parameters here are configdir, tokenDescription and flags.
 *
 * a Full list is below:
 *
 *
 *  configDir - The location of the databases for this token. If configDir is
 *         not specified, and noCertDB and noKeyDB is not specified, the load
 *         will fail.
 *   certPrefix - Cert prefix for this token.
 *   keyPrefix - Prefix for the key database for this token. (if not specified,
 *         certPrefix will be used).
 *   tokenDescription - The label value for this token returned in the
 *         CK_TOKEN_INFO structure with an internationalize string (UTF8).
 *         This value will be truncated at 32 bytes (no NULL, partial UTF8
 *         characters dropped). You should specify a user friendly name here
 *         as this is the value the token will be referred to in most
 *         application UI's. You should make sure tokenDescription is unique.
 *   slotDescription - The slotDescription value for this token returned
 *         in the CK_SLOT_INFO structure with an internationalize string
 *         (UTF8). This value will be truncated at 64 bytes (no NULL, partial
 *         UTF8 characters dropped). This name will not change after the
 *         database is closed. It should have some number to make this unique.
 *   minPWLen - minimum password length for this token.
 *   flags - comma separated list of flag values, parsed case-insensitive.
 *         Valid flags are:
 *              readOnly - Databases should be opened read only.
 *              noCertDB - Don't try to open a certificate database.
 *              noKeyDB - Don't try to open a key database.
 *              forceOpen - Don't fail to initialize the token if the
 *                databases could not be opened.
 *              passwordRequired - zero length passwords are not acceptable
 *                (valid only if there is a keyDB).
 *              optimizeSpace - allocate smaller hash tables and lock tables.
 *                When this flag is not specified, Softoken will allocate
 *                large tables to prevent lock contention.
 */
PK11SlotInfo *
SECMOD_OpenUserDB(const char *moduleSpec)
{
    SECMODModule *mod;
    SECMODConfigList *conflist = NULL;
    int count = 0;

    if (moduleSpec == NULL) {
        return NULL;
    }

    /* NOTE: unlike most PK11 function, this does not return a reference
     * to the module */
    mod = SECMOD_GetInternalModule();
    if (!mod) {
        /* shouldn't happen */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL;
    }

    /* make sure we don't open the same database twice. We only understand 
     * the moduleSpec for internal databases well enough to do this, so only
     * do this in OpenUserDB */
    conflist = secmod_GetConfigList(mod->isFIPS, mod->libraryParams, &count);
    if (conflist) {
        PK11SlotInfo *slot = NULL;
        if (secmod_MatchConfigList(moduleSpec, conflist, count)) {
            slot = secmod_FindSlotFromModuleSpec(moduleSpec, mod);
        }
        secmod_FreeConfigList(conflist, count);
        if (slot) {
            return slot;
        }
    }
    return SECMOD_OpenNewSlot(mod, moduleSpec);
}

/*
 * close an already opened user database. NOTE: the database must be
 * in the internal token, and must be one created with SECMOD_OpenUserDB().
 * Once the database is closed, the slot will remain as an empty slot
 * until it's used again with SECMOD_OpenUserDB() or SECMOD_OpenNewSlot().
 */
SECStatus
SECMOD_CloseUserDB(PK11SlotInfo *slot)
{
    SECStatus rv;
    char *sendSpec;

    sendSpec = PR_smprintf("tokens=[0x%x=<>]", slot->slotID);
    if (sendSpec == NULL) {
        /* PR_smprintf does not set no memory error */
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = secmod_UserDBOp(slot, CKO_NSS_DELSLOT, sendSpec);
    PR_smprintf_free(sendSpec);
    /* if we are in the delay period for the "isPresent" call, reset
     * the delay since we know things have probably changed... */
    NSSToken *nssToken = PK11Slot_GetNSSToken(slot);
    if (nssToken) {
        if (nssToken->slot) {
            nssSlot_ResetDelay(nssToken->slot);
        }
        (void)nssToken_Destroy(nssToken);
        /* force the slot info structures to properly reset */
        (void)PK11_IsPresent(slot);
    }
    return rv;
}

/*
 * Restart PKCS #11 modules after a fork(). See secmod.h for more information.
 */
SECStatus
SECMOD_RestartModules(PRBool force)
{
    SECMODModuleList *mlp;
    SECStatus rrv = SECSuccess;
    int lastError = 0;

    if (!moduleLock) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }

    /* Only need to restart the PKCS #11 modules that were initialized */
    SECMOD_GetReadLock(moduleLock);
    for (mlp = modules; mlp != NULL; mlp = mlp->next) {
        SECMODModule *mod = mlp->module;
        CK_ULONG count;
        SECStatus rv;
        int i;

        /* If the module needs to be reset, do so */
        if (force || (PK11_GETTAB(mod)->C_GetSlotList(CK_FALSE, NULL, &count) != CKR_OK)) {
            PRBool alreadyLoaded;
            /* first call Finalize. This is not required by PKCS #11, but some
             * older modules require it, and it doesn't hurt (compliant modules
             * will return CKR_NOT_INITIALIZED */
            (void)PK11_GETTAB(mod)->C_Finalize(NULL);
            /* now initialize the module, this function reinitializes
             * a module in place, preserving existing slots (even if they
             * no longer exist) */
            rv = secmod_ModuleInit(mod, NULL, &alreadyLoaded);
            if (rv != SECSuccess) {
                /* save the last error code */
                lastError = PORT_GetError();
                rrv = rv;
                /* couldn't reinit the module, disable all its slots */
                for (i = 0; i < mod->slotCount; i++) {
                    mod->slots[i]->disabled = PR_TRUE;
                    mod->slots[i]->reason = PK11_DIS_COULD_NOT_INIT_TOKEN;
                }
                continue;
            }
            for (i = 0; i < mod->slotCount; i++) {
                /* get new token sessions, bump the series up so that
                 * we refresh other old sessions. This will tell much of
                 * NSS to flush cached handles it may hold as well */
                rv = PK11_InitToken(mod->slots[i], PR_TRUE);
                /* PK11_InitToken could fail if the slot isn't present.
                 * If it is present, though, something is wrong and we should
                 * disable the slot and let the caller know. */
                if (rv != SECSuccess && PK11_IsPresent(mod->slots[i])) {
                    /* save the last error code */
                    lastError = PORT_GetError();
                    rrv = rv;
                    /* disable the token */
                    mod->slots[i]->disabled = PR_TRUE;
                    mod->slots[i]->reason = PK11_DIS_COULD_NOT_INIT_TOKEN;
                }
            }
        }
    }
    SECMOD_ReleaseReadLock(moduleLock);

    /*
     * on multiple failures, we are only returning the lastError. The caller
     * can determine which slots are bad by calling PK11_IsDisabled().
     */
    if (rrv != SECSuccess) {
        /* restore the last error code */
        PORT_SetError(lastError);
    }

    return rrv;
}
