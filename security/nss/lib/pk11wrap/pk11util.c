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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

/* these are for displaying error messages */

static  SECMODModuleList *modules = NULL;
static  SECMODModuleList *modulesDB = NULL;
static  SECMODModuleList *modulesUnload = NULL;
static  SECMODModule *internalModule = NULL;
static  SECMODModule *defaultDBModule = NULL;
static  SECMODModule *pendingModule = NULL;
static SECMODListLock *moduleLock = NULL;

int secmod_PrivateModuleCount = 0;

extern PK11DefaultArrayEntry PK11_DefaultArray[];
extern int num_pk11_default_mechanisms;


void
SECMOD_Init() 
{
    /* don't initialize twice */
    if (moduleLock) return;

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
    if (PR_GetEnv("NSS_STRICT_SHUTDOWN")) {
	PORT_Assert(secmod_PrivateModuleCount == 0);
    }
#endif
    if (secmod_PrivateModuleCount) {
    	PORT_SetError(SEC_ERROR_BUSY);
	return SECFailure;
    }
    return SECSuccess;
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
secmod_AddModuleToList(SECMODModuleList **moduleList,SECMODModule *newModule)
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
    for(mlp = *moduleList; mlp != NULL; mlp = mlp->next) {
	last = mlp;
    }

    if (last == NULL) {
	*moduleList = newListElement;
    } else {
	SECMOD_AddList(last,newListElement,NULL);
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
    return secmod_AddModuleToList(&modules,newModule);
}

SECStatus
SECMOD_AddModuleToDBOnlyList(SECMODModule *newModule)
{
    if (defaultDBModule == NULL) {
	defaultDBModule = SECMOD_ReferenceModule(newModule);
    }
    return secmod_AddModuleToList(&modulesDB,newModule);
}

SECStatus
SECMOD_AddModuleToUnloadList(SECMODModule *newModule)
{
    return secmod_AddModuleToList(&modulesUnload,newModule);
}

/*
 * get the list of PKCS11 modules that are available.
 */
SECMODModuleList * SECMOD_GetDefaultModuleList() { return modules; }
SECMODModuleList *SECMOD_GetDeadModuleList() { return modulesUnload; }
SECMODModuleList *SECMOD_GetDBModuleList() { return modulesDB; }

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
SECMODListLock *SECMOD_GetDefaultModuleListLock() { return moduleLock; }



/*
 * find a module by name, and add a reference to it.
 * return that module.
 */
SECMODModule *
SECMOD_FindModule(const char *name)
{
    SECMODModuleList *mlp;
    SECMODModule *module = NULL;

    SECMOD_GetReadLock(moduleLock);
    for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	if (PORT_Strcmp(name,mlp->module->commonName) == 0) {
	    module = mlp->module;
	    SECMOD_ReferenceModule(module);
	    break;
	}
    }
    if (module) {
	goto found;
    }
    for(mlp = modulesUnload; mlp != NULL; mlp = mlp->next) {
	if (PORT_Strcmp(name,mlp->module->commonName) == 0) {
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

    SECMOD_GetReadLock(moduleLock);
    for(mlp = modules; mlp != NULL; mlp = mlp->next) {
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
 * Find the Slot based on ID and the module.
 */
PK11SlotInfo *
SECMOD_FindSlotByID(SECMODModule *module, CK_SLOT_ID slotID)
{
    int i;
    PK11SlotInfo *slot = NULL;

    SECMOD_GetReadLock(moduleLock);
    for (i=0; i < module->slotCount; i++) {
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
SECMOD_LookupSlot(SECMODModuleID moduleID,CK_SLOT_ID slotID) 
{
    SECMODModule *module;
    PK11SlotInfo *slot;

    module = SECMOD_FindModuleByID(moduleID);
    if (module == NULL) return NULL;

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

    *type = SECMOD_EXTERNAL;

    SECMOD_GetWriteLock(moduleLock);
    for (mlpp = &modules,mlp = modules; 
				mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
	if ((name && (PORT_Strcmp(name,mlp->module->commonName) == 0)) ||
							mod == mlp->module) {
	    /* don't delete the internal module */
	    if (!mlp->module->internal) {
		SECMOD_RemoveList(mlpp,mlp);
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
    for (mlpp = &modulesUnload,mlp = modulesUnload; 
				mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
	if ((name && (PORT_Strcmp(name,mlp->module->commonName) == 0)) ||
							mod == mlp->module) {
	    /* don't delete the internal module */
	    if (!mlp->module->internal) {
		SECMOD_RemoveList(mlpp,mlp);
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

    if (pendingModule) {
	PORT_SetError(SEC_ERROR_MODULE_STUCK);
	return rv;
    }

    SECMOD_GetWriteLock(moduleLock);
    for(mlpp = &modules,mlp = modules; 
				mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
	if (PORT_Strcmp(name,mlp->module->commonName) == 0) {
	    /* don't delete the internal module */
	    if (mlp->module->internal) {
		SECMOD_RemoveList(mlpp,mlp);
		rv = STAN_RemoveModuleFromDefaultTrustDomain(mlp->module);
	    } 
	    break;
	}
    }
    SECMOD_ReleaseWriteLock(moduleLock);

    if (rv == SECSuccess) {
	SECMODModule *newModule,*oldModule;

	if (mlp->module->isFIPS) {
    	    newModule = SECMOD_CreateModule(NULL, SECMOD_INT_NAME,
				NULL, SECMOD_INT_FLAGS);
	} else {
    	    newModule = SECMOD_CreateModule(NULL, SECMOD_FIPS_NAME,
				NULL, SECMOD_FIPS_FLAGS);
	}
	if (newModule) {
	    newModule->libraryParams = 
	     PORT_ArenaStrdup(newModule->arena,mlp->module->libraryParams);
	    rv = SECMOD_AddModule(newModule);
	    if (rv != SECSuccess) {
		SECMOD_DestroyModule(newModule);
		newModule = NULL;
	    }
	}
	if (newModule == NULL) {
	    SECMODModuleList *last = NULL,*mlp2;
	   /* we're in pretty deep trouble if this happens...Security
	    * not going to work well... try to put the old module back on
	    * the list */
	   SECMOD_GetWriteLock(moduleLock);
	   for(mlp2 = modules; mlp2 != NULL; mlp2 = mlp->next) {
		last = mlp2;
	   }

	   if (last == NULL) {
		modules = mlp;
	   } else {
		SECMOD_AddList(last,mlp,NULL);
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

    rv = SECMOD_LoadPKCS11Module(newModule);
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
SECMOD_FindSlot(SECMODModule *module,const char *name) 
{
    int i;
    char *string;
    PK11SlotInfo *retSlot = NULL;

    SECMOD_GetReadLock(moduleLock);
    for (i=0; i < module->slotCount; i++) {
	PK11SlotInfo *slot = module->slots[i];

	if (PK11_IsPresent(slot)) {
	    string = PK11_GetTokenName(slot);
	} else {
	    string = PK11_GetSlotName(slot);
	}
	if (PORT_Strcmp(name,string) == 0) {
	    retSlot = PK11_ReferenceSlot(slot);
	    break;
	}
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (retSlot == NULL) {
	PORT_SetError(SEC_ERROR_NO_SLOT_SELECTED);
    }
    return NULL;
    return retSlot;
}

SECStatus
PK11_GetModInfo(SECMODModule *mod,CK_INFO *info)
{
    CK_RV crv;

    if (mod->functionList == NULL) return SECFailure;
    crv = PK11_GETTAB(mod)->C_GetInfo(info);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
    }	
    return (crv == CKR_OK) ? SECSuccess : SECFailure;
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
SECMOD_AddNewModuleEx(const char* moduleName, const char* dllPath,
                              unsigned long defaultMechanismFlags,
                              unsigned long cipherEnableFlags,
                              char* modparms, char* nssparms)
{
    SECMODModule *module;
    SECStatus result = SECFailure;
    int s,i;
    PK11SlotInfo* slot;

    PR_SetErrorText(0, NULL);

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
                    for (i=0; i < num_pk11_default_mechanisms; i++) {
                        /* we are told to turn it on by default ? */
			PRBool add = 
			 (PK11_DefaultArray[i].flag & defaultMechanismFlags) ?
						PR_TRUE: PR_FALSE;
                        result = PK11_UpdateSlotAttribute(slot, 
					&(PK11_DefaultArray[i]),  add);
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
SECMOD_AddNewModule(const char* moduleName, const char* dllPath,
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
SECMOD_IsModulePresent( unsigned long int pubCipherEnableFlags )
{
    PRBool result = PR_FALSE;
    SECMODModuleList *mods = SECMOD_GetDefaultModuleList();
    SECMOD_GetReadLock(moduleLock);


    for ( ; mods != NULL; mods = mods->next) {
        if (mods->module->ssl[0] & 
		SECMOD_PubCipherFlagstoInternal(pubCipherEnableFlags)) {
            result = PR_TRUE;
        }
    }

    SECMOD_ReleaseReadLock(moduleLock);
    return result;
}

/* create a new ModuleListElement */
SECMODModuleList *SECMOD_NewModuleListElement(void) 
{
    SECMODModuleList *newModList;

    newModList= (SECMODModuleList *) PORT_Alloc(sizeof(SECMODModuleList));
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
    PK11_USE_THREADS(PZ_Lock((PZLock *)module->refLock);)
    PORT_Assert(module->refCount > 0);

    module->refCount++;
    PK11_USE_THREADS(PZ_Unlock((PZLock*)module->refLock);)
    return module;
}


/* destroy an existing module */
void
SECMOD_DestroyModule(SECMODModule *module) 
{
    PRBool willfree = PR_FALSE;
    int slotCount;
    int i;

    PK11_USE_THREADS(PZ_Lock((PZLock *)module->refLock);)
    if (module->refCount-- == 1) {
	willfree = PR_TRUE;
    }
    PORT_Assert(willfree || (module->refCount > 0));
    PK11_USE_THREADS(PZ_Unlock((PZLock *)module->refLock);)

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
	SECMOD_SlotDestroyModule(module,PR_FALSE);
	return;
    }

    /* now free all out slots, when they are done, they will cause the
     * module to disappear altogether */
    for (i=0 ; i < slotCount; i++) {
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
	PK11_USE_THREADS(PZ_Lock((PZLock *)module->refLock);)
	if (module->slotCount-- == 1) {
	    willfree = PR_TRUE;
	}
	PORT_Assert(willfree || (module->slotCount > 0));
	PK11_USE_THREADS(PZ_Unlock((PZLock *)module->refLock);)
        if (!willfree) return;
    }

    if (module == pendingModule) {
	pendingModule = NULL;
    }

    if (module->loaded) {
	SECMOD_UnloadModule(module);
    }
    PK11_USE_THREADS(PZ_DestroyLock((PZLock *)module->refLock);)
    PORT_FreeArena(module->arena,PR_FALSE);
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

    for ( lp = list; lp != NULL; lp = SECMOD_DestroyModuleListElement(lp)) ;
}

PRBool
SECMOD_CanDeleteInternalModule(void)
{
    return (PRBool) (pendingModule == NULL);
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
    int count,i, oldCount;
    PRBool freeRef = PR_FALSE;
    void *mark;
    CK_ULONG *slotIDs = NULL;
    PK11SlotInfo **newSlots = NULL;
    PK11SlotInfo **oldSlots = NULL;

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
    if (count < mod->slotCount) {
	/* shouldn't happen with a properly functioning PKCS #11 module */
	PORT_SetError( SEC_ERROR_INCOMPATIBLE_PKCS11 );
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
    newSlots = PORT_ArenaZNewArray(mod->arena,PK11SlotInfo *,count);

    /* walk down the new slot ID list returned from the module. We keep
     * the old slots which match a returned ID, and we initialize the new 
     * slots. */
    for (i=0; i < count; i++) {
	PK11SlotInfo *slot = SECMOD_FindSlotByID(mod,slotIDs[i]);

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
    oldCount =mod->slotCount;
    oldSlots = mod->slots;
    mod->slots = newSlots; /* typical arena 'leak'... old mod->slots is
			    * allocated out of the module arena and won't
			    * be freed until the module is freed */
    mod->slotCount = count;
    SECMOD_ReleaseWriteLock(moduleLock);
    /* free our old references before forgetting about oldSlot*/
    for (i=0; i < oldCount; i++) {
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
	for (i=0; i < count; i++) {
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
secmod_HandleWaitForSlotEvent(SECMODModule *mod,  unsigned long flags,
						PRIntervalTime latency)
{
    PRBool removableSlotsFound = PR_FALSE;
    int i;
    int error = SEC_ERROR_NO_EVENT;

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
	for (i=0; i < mod->slotCount; i++) {
	    PK11SlotInfo *slot = mod->slots[i];
	    uint16 series;
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
	if (!removableSlotsFound) {
	    error =SEC_ERROR_NO_SLOT_SELECTED;
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
    unsigned long controlMask = mod->evControlMask;
    SECStatus rv = SECSuccess;
    CK_RV crv;

    PZ_Lock(mod->refLock);
    mod->evControlMask |= SECMOD_END_WAIT;
    controlMask = mod->evControlMask;
    if (controlMask & SECMOD_WAIT_PKCS11_EVENT) {
	/* NOTE: this call will drop all transient keys, in progress
	 * operations, and any authentication. This is the only documented
	 * way to get WaitForSlotEvent to return. Also note: for non-thread
	 * safe tokens, we need to hold the module lock, this is not yet at
	 * system shutdown/starup time, so we need to protect these calls */
	crv = PK11_GETTAB(mod)->C_Finalize(NULL);
	/* ok, we slammed the module down, now we need to reinit it in case
	 * we intend to use it again */
	if (crv = CKR_OK) {
	    secmod_ModuleInit(mod);
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

    SECMOD_GetReadLock(moduleLock);
    for (i=0; i < mod->slotCount; i++) {
	PK11SlotInfo *slot = mod->slots[i];
	/* perm modules are not inserted or removed */
	if (slot->isPerm) {
	    continue;
	}
	ret = PR_TRUE;
	break;
    }
    SECMOD_ReleaseReadLock(moduleLock);
    return ret;
}
