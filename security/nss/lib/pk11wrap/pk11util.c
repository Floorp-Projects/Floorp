/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/*
 * Initialize the PCKS 11 subsystem
 */
#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "pk11func.h"

/* these are for displaying error messages */

static  SECMODModuleList *modules = NULL;
static  SECMODModule *internalModule = NULL;
static SECMODListLock *moduleLock = NULL;

extern SECStatus
PK11_UpdateSlotAttribute(PK11SlotInfo *slot, PK11DefaultArrayEntry *entry,
                        PRBool add);
                        

extern int XP_SEC_MODULE_NO_LIB;

extern PK11DefaultArrayEntry PK11_DefaultArray[];
extern int num_pk11_default_mechanisms;

static PRBool secmod_ModuleHasRoots(SECMODModule *module)
{
    int i;

    for (i=0; i < module->slotInfoCount; i++) {
	if (module->slotInfo[i].hasRootCerts) {
	    return PR_TRUE;
	}
    }
    return PR_FALSE;
}


/*
 * The following code is an attempt to automagically find the external root
 * module. NOTE: This code should be checked out on the MAC! There must be
 * some cross platform support out there to help out with this?
 * Note: Keep the #if-defined chunks in order. HPUX must select before UNIX.
 */

static char *dllnames[]= {
#if defined(XP_WIN32) || defined(XP_OS2)
	"nssckbi.dll",
	"roots.dll", 
	"netckbi.dll",
	"mozckbi.dll",
#elif defined(HPUX)
	"libnssckbi.sl",
	"libroots.sl",
	"libnetckbi.sl",
	"libmozckbi.sl",
#elif defined(XP_UNIX)
	"libnssckbi.so",
	"libroots.so",
	"libnetckbi.so",
	"libmozckbi.so",
#elif defined(XP_MAC)
	"NSS Builtin Root Certs",
	"Root Certs",
	"Netscape Builtin Root Certs",
	"Mozilla Builtin Root Certs",
#else
	#error "Uh! Oh! I don't know about this platform."
#endif
	0 };
	

#define MAXDLLNAME 40

/* Should we have platform ifdefs here??? */
#define FILE_SEP '/'

static void
secmod_FindExternalRoot(char *dbname)
{
	char *path, *cp, **cur_name;
	int len = PORT_Strlen(dbname);
	int path_len;

	
	path = PORT_Alloc(len+MAXDLLNAME);
	if (path == NULL) return;

	/* back up to the top of the directory */
	for (cp = &dbname[len]; cp != dbname && (*cp != FILE_SEP); cp--) ;
	path_len = cp-dbname;
	PORT_Memcpy(path,dbname,path_len);
	path[path_len++] = FILE_SEP;

	/* now walk our tree of dll names looking for the file of interest. */
	for (cur_name= dllnames; *cur_name != 0; cur_name++) {
	    PORT_Memcpy(&path[path_len],*cur_name,PORT_Strlen(*cur_name)+1);
	    if (SECMOD_AddNewModule("Root Certs",path, 0, 0) == SECSuccess) {
		break;
	    }
	}
	PORT_Free(path);
	return;
}

void SECMOD_init(char *dbname) {
    SECMODModuleList *thisModule;
    int found=0;
    int rootFound=0;
    SECStatus rv = SECFailure;


    /* don't initialize twice */
    if (modules) return;

    PK11_InitSlotLists();

    SECMOD_InitDB(dbname);

    /*
     * read in the current modules from the database
     */
    modules = SECMOD_ReadPermDB();

    /* make sure that the internal module is loaded */
    for (thisModule = modules; thisModule ; thisModule = thisModule->next) {
	if (thisModule->module->internal) {
	    found++;
	    internalModule = SECMOD_ReferenceModule(thisModule->module);
	}
	if (secmod_ModuleHasRoots(thisModule->module)) {
	    rootFound++;
	}
    }

    if (!found) {
	thisModule = modules;
	modules = SECMOD_NewModuleListElement();
	modules->module = SECMOD_NewInternal();
	PORT_Assert(modules->module != NULL);
	modules->next = thisModule;
	SECMOD_AddPermDB(modules->module);
	internalModule = SECMOD_ReferenceModule(modules->module);
    }

    /* load it first... we need it to verify the external modules
     * which we are loading.... */
    rv = SECMOD_LoadModule(internalModule);
    if( rv != SECSuccess )
        internalModule = NULL;

    if (! rootFound ) {
	secmod_FindExternalRoot(dbname);
    }
    /* Load each new module */
    for (thisModule = modules; thisModule ; thisModule = thisModule->next) {
        if( !( thisModule->module->internal ) )
	    SECMOD_LoadModule(thisModule->module);
    }

    moduleLock = SECMOD_NewListLock();
}


void SECMOD_Shutdown() {
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
    /* destroy the list */
    if (modules) {
	SECMOD_DestroyModuleList(modules);
	modules = NULL;
    }

    /* make all the slots and the lists go away */
    PK11_DestroySlotLists();
}


/*
 * retrieve the internal module
 */
SECMODModule *
SECMOD_GetInternalModule(void) {
   return internalModule;
}

/* called from  security/cmd/swfort/instinit, which doesn't need a full 
 * security LIBRARY (it used the swfortezza code, but it does have to verify
 * cert chains against it's own list of certs. We need to initialize the 
 * security code without any database.
 */
void
SECMOD_SetInternalModule( SECMODModule *mod) {
   internalModule = SECMOD_ReferenceModule(mod);
   modules = SECMOD_NewModuleListElement();
   modules->module = SECMOD_ReferenceModule(mod);
   modules->next = NULL;
   if (!moduleLock) {
       moduleLock = SECMOD_NewListLock();
   }
}

/*
 * get the list of PKCS11 modules that are available.
 */
SECMODModuleList *SECMOD_GetDefaultModuleList() { return modules; }
SECMODListLock *SECMOD_GetDefaultModuleListLock() { return moduleLock; }



/*
 * find a module by name, and add a reference to it.
 * return that module.
 */
SECMODModule *SECMOD_FindModule(char *name) {
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
 * find a module by name and delete it of the module list
 */
SECStatus
SECMOD_DeleteModule(char *name, int *type) {
    SECMODModuleList *mlp;
    SECMODModuleList **mlpp;
    SECStatus rv = SECFailure;


    *type = SECMOD_EXTERNAL;

    SECMOD_GetWriteLock(moduleLock);
    for(mlpp = &modules,mlp = modules; 
				mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
	if (PORT_Strcmp(name,mlp->module->commonName) == 0) {
	    /* don't delete the internal module */
	    if (!mlp->module->internal) {
		SECMOD_RemoveList(mlpp,mlp);
		/* delete it after we release the lock */
		rv = SECSuccess;
	    } else if (mlp->module->isFIPS) {
		*type = SECMOD_FIPS;
	    } else {
		*type = SECMOD_INTERNAL;
	    }
	    break;
	}
    }
    SECMOD_ReleaseWriteLock(moduleLock);


    if (rv == SECSuccess) {
 	SECMOD_DeletePermDB(mlp->module);
	SECMOD_DestroyModuleListElement(mlp);
    }
    return rv;
}

/*
 * find a module by name and delete it of the module list
 */
SECStatus
SECMOD_DeleteInternalModule(char *name) {
    SECMODModuleList *mlp;
    SECMODModuleList **mlpp;
    SECStatus rv = SECFailure;

    SECMOD_GetWriteLock(moduleLock);
    for(mlpp = &modules,mlp = modules; 
				mlp != NULL; mlpp = &mlp->next, mlp = *mlpp) {
	if (PORT_Strcmp(name,mlp->module->commonName) == 0) {
	    /* don't delete the internal module */
	    if (mlp->module->internal) {
		rv = SECSuccess;
		SECMOD_RemoveList(mlpp,mlp);
	    } 
	    break;
	}
    }
    SECMOD_ReleaseWriteLock(moduleLock);

    if (rv == SECSuccess) {
	SECMODModule *newModule,*oldModule;

	if (mlp->module->isFIPS) {
	    newModule = SECMOD_NewInternal();
	} else {
	    newModule = SECMOD_GetFIPSInternal();
	}
	if (newModule == NULL) {
	    SECMODModuleList *last,*mlp2;
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
	oldModule = internalModule;
	internalModule = SECMOD_ReferenceModule(newModule);
	SECMOD_AddModule(internalModule);
	SECMOD_DestroyModule(oldModule);
 	SECMOD_DeletePermDB(mlp->module);
	SECMOD_DestroyModuleListElement(mlp);
    }
    return rv;
}

SECStatus
SECMOD_AddModule(SECMODModule *newModule) {
    SECStatus rv;
    SECMODModuleList *mlp, *newListElement, *last = NULL;

    /* Test if a module w/ the same name already exists */
    /* and return SECWouldBlock if so. */
    /* We should probably add a new return value such as */
    /* SECDublicateModule, but to minimize ripples, I'll */
    /* give SECWouldBlock a new meaning */
    if (SECMOD_FindModule(newModule->commonName)) {
        return SECWouldBlock;
        /* module already exists. */
    }

    rv = SECMOD_LoadModule(newModule);
    if (rv != SECSuccess) {
	return rv;
    }

    newListElement = SECMOD_NewModuleListElement();
    if (newListElement == NULL) {
	return SECFailure;
    }

    SECMOD_AddPermDB(newModule);

    newListElement->module = newModule;

    SECMOD_GetWriteLock(moduleLock);
    /* Added it to the end (This is very inefficient, but Adding a module
     * on the fly should happen maybe 2-3 times through the life this program
     * on a given computer, and this list should be *SHORT*. */
    for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	last = mlp;
    }

    if (last == NULL) {
	modules = newListElement;
    } else {
	SECMOD_AddList(last,newListElement,NULL);
    }
    SECMOD_ReleaseWriteLock(moduleLock);
    return SECSuccess;
}

PK11SlotInfo *SECMOD_FindSlot(SECMODModule *module,char *name) {
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
PK11_GetModInfo(SECMODModule *mod,CK_INFO *info) {
    CK_RV crv;

    if (mod->functionList == NULL) return SECFailure;
    crv = PK11_GETTAB(mod)->C_GetInfo(info);
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
SECStatus SECMOD_AddNewModule(char* moduleName, char* dllPath,
                              unsigned long defaultMechanismFlags,
                              unsigned long cipherEnableFlags) {
    SECMODModule *module;
    SECStatus result = SECFailure;
    int s,i;
    PK11SlotInfo* slot;

    module = SECMOD_NewModule();
    
    if (moduleName) {	
        module->commonName=PORT_ArenaStrdup(module->arena,moduleName);
    } else {
        module->commonName=NULL;
    }

    if (dllPath) {
        module->dllName=PORT_ArenaStrdup(module->arena,dllPath);
    } else {
        module->dllName=NULL;
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
                result = SECMOD_DeletePermDB(module);
                
                if (result == SECSuccess) {          
                    result = SECMOD_AddPermDB(module);
                    if (result == SECSuccess) {
                        return SECSuccess;
                    }                    
                }
                
            }
        }
    }
    SECMOD_DestroyModule(module);
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
    SECMODListLock *modsLock = SECMOD_GetDefaultModuleListLock();
    SECMOD_GetReadLock(moduleLock);


    for ( ; mods != NULL; mods = mods->next) {
        if (mods->module->ssl[0] & SECMOD_PubCipherFlagstoInternal(pubCipherEnableFlags)) {
            result = PR_TRUE;
        }
    }

    SECMOD_ReleaseReadLock(moduleLock);
    return result;
}
