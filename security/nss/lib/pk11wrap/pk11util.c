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
SECMOD_Init() {
    /* don't initialize twice */
    if (moduleLock) return;

    moduleLock = SECMOD_NewListLock();
    PK11_InitSlotLists();
}


SECStatus
SECMOD_Shutdown() {
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
SECMOD_GetInternalModule(void) {
   return internalModule;
}


SECStatus
secmod_AddModuleToList(SECMODModuleList **moduleList,SECMODModule *newModule) {
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
SECMOD_AddModuleToList(SECMODModule *newModule) {
    if (newModule->internal && !internalModule) {
	internalModule = SECMOD_ReferenceModule(newModule);
    }
    return secmod_AddModuleToList(&modules,newModule);
}

SECStatus
SECMOD_AddModuleToDBOnlyList(SECMODModule *newModule) {
    if (defaultDBModule == NULL) {
	defaultDBModule = SECMOD_ReferenceModule(newModule);
    }
    return secmod_AddModuleToList(&modulesDB,newModule);
}

SECStatus
SECMOD_AddModuleToUnloadList(SECMODModule *newModule) {
    return secmod_AddModuleToList(&modulesUnload,newModule);
}

/*
 * get the list of PKCS11 modules that are available.
 */
SECMODModuleList *SECMOD_GetDefaultModuleList() { return modules; }
SECMODModuleList *SECMOD_GetDeadModuleList() { return modulesUnload; }
SECMODModuleList *SECMOD_GetDBModuleList() { return modulesDB; }
SECMODListLock *SECMOD_GetDefaultModuleListLock() { return moduleLock; }



/*
 * find a module by name, and add a reference to it.
 * return that module.
 */
SECMODModule *SECMOD_FindModule(const char *name) {
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
SECMODModule *SECMOD_FindModuleByID(SECMODModuleID id) {
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

    return module;
}

/*
 * lookup the Slot module based on it's module ID and slot ID.
 */
PK11SlotInfo *SECMOD_LookupSlot(SECMODModuleID moduleID,CK_SLOT_ID slotID) {
    int i;
    SECMODModule *module;

    module = SECMOD_FindModuleByID(moduleID);
    if (module == NULL) return NULL;

    for (i=0; i < module->slotCount; i++) {
	PK11SlotInfo *slot = module->slots[i];

	if (slot->slotID == slotID) {
	    SECMOD_DestroyModule(module);
	    return PK11_ReferenceSlot(slot);
	}
    }
    SECMOD_DestroyModule(module);
    return NULL;
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
SECMOD_DeleteModule(const char *name, int *type) {
    return SECMOD_DeleteModuleEx(name, NULL, type, PR_TRUE);
}

/*
 * find a module by name and delete it off the module list
 */
SECStatus
SECMOD_DeleteInternalModule(const char *name) {
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
SECMOD_AddModule(SECMODModule *newModule) {
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

PK11SlotInfo *SECMOD_FindSlot(SECMODModule *module,const char *name) {
    int i;
    char *string;

    for (i=0; i < module->slotCount; i++) {
	PK11SlotInfo *slot = module->slots[i];

	if (PK11_IsPresent(slot)) {
	    string = PK11_GetTokenName(slot);
	} else {
	    string = PK11_GetSlotName(slot);
	}
	if (PORT_Strcmp(name,string) == 0) {
	    return PK11_ReferenceSlot(slot);
	}
    }
    return NULL;
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
SECStatus SECMOD_AddNewModuleEx(const char* moduleName, const char* dllPath,
                              unsigned long defaultMechanismFlags,
                              unsigned long cipherEnableFlags,
                              char* modparms,
                              char* nssparms) {
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

                /* check each slot to turn on appropriate mechanisms */
                for (s = 0; s < module->slotCount; s++) {
                    slot = (module->slots)[s];
                    /* for each possible mechanism */
                    for (i=0; i < num_pk11_default_mechanisms; i++) {
                        /* we are told to turn it on by default ? */
                        if (PK11_DefaultArray[i].flag & defaultMechanismFlags) {                            
                            /* it ignores if slot attribute update failes */
                            result = PK11_UpdateSlotAttribute(slot, &(PK11_DefaultArray[i]), PR_TRUE);
                        } else { /* turn this mechanism of the slot off by default */
                            result = PK11_UpdateSlotAttribute(slot, &(PK11_DefaultArray[i]), PR_FALSE);
                        }
                    } /* for each mechanism */
                    /* disable each slot if the defaultFlags say so */
                    if (defaultMechanismFlags & PK11_DISABLE_FLAG) {
                        PK11_UserDisableSlot(slot);
                    }
                } /* for each slot of this module */

                /* delete and re-add module in order to save changes to the module */
		result = SECMOD_UpdateModule(module);
            }
        }
    }
    SECMOD_DestroyModule(module);
    return result;
}

SECStatus SECMOD_AddNewModule(const char* moduleName, const char* dllPath,
                              unsigned long defaultMechanismFlags,
                              unsigned long cipherEnableFlags)
{
    return SECMOD_AddNewModuleEx(moduleName, dllPath, defaultMechanismFlags,
                  cipherEnableFlags, 
                  NULL, NULL); /* don't pass module or nss params */
}

SECStatus SECMOD_UpdateModule(SECMODModule *module)
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
unsigned long SECMOD_PubMechFlagstoInternal(unsigned long publicFlags) {
    unsigned long internalFlags = publicFlags;

    if (publicFlags & PUBLIC_MECH_RANDOM_FLAG) {
        internalFlags &= ~PUBLIC_MECH_RANDOM_FLAG;
        internalFlags |= SECMOD_RANDOM_FLAG;
    }
    return internalFlags;
}

unsigned long SECMOD_InternaltoPubMechFlags(unsigned long internalFlags) {
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
unsigned long SECMOD_PubCipherFlagstoInternal(unsigned long publicFlags) {
    return publicFlags;
}

unsigned long SECMOD_InternaltoPubCipherFlags(unsigned long internalFlags) {
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
        if (mods->module->ssl[0] & SECMOD_PubCipherFlagstoInternal(pubCipherEnableFlags)) {
            result = PR_TRUE;
        }
    }

    SECMOD_ReleaseReadLock(moduleLock);
    return result;
}

/* create a new ModuleListElement */
SECMODModuleList *SECMOD_NewModuleListElement(void) {
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
SECMOD_ReferenceModule(SECMODModule *module) {
    PK11_USE_THREADS(PZ_Lock((PZLock *)module->refLock);)
    PORT_Assert(module->refCount > 0);

    module->refCount++;
    PK11_USE_THREADS(PZ_Unlock((PZLock*)module->refLock);)
    return module;
}


/* destroy an existing module */
void
SECMOD_DestroyModule(SECMODModule *module) {
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
SECMOD_SlotDestroyModule(SECMODModule *module, PRBool fromSlot) {
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
SECMOD_DestroyModuleListElement(SECMODModuleList *element) {
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
SECMOD_DestroyModuleList(SECMODModuleList *list) {
    SECMODModuleList *lp;

    for ( lp = list; lp != NULL; lp = SECMOD_DestroyModuleListElement(lp)) ;
}

PRBool
SECMOD_CanDeleteInternalModule(void)
{
    return (PRBool) (pendingModule == NULL);
}
