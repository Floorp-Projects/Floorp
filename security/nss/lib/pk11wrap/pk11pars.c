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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
 * The following handles the loading, unloading and management of
 * various PCKS #11 modules
 */

#include <ctype.h>
#include "pkcs11.h"
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pki3hack.h"
#include "secerr.h"
   
#include "pk11pars.h" 

/* create a new module */
static  SECMODModule *
secmod_NewModule(void)
{
    SECMODModule *newMod;
    PRArenaPool *arena;


    /* create an arena in which dllName and commonName can be
     * allocated.
     */
    arena = PORT_NewArena(512);
    if (arena == NULL) {
	return NULL;
    }

    newMod = (SECMODModule *)PORT_ArenaAlloc(arena,sizeof (SECMODModule));
    if (newMod == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }

    /*
     * initialize of the fields of the module
     */
    newMod->arena = arena;
    newMod->internal = PR_FALSE;
    newMod->loaded = PR_FALSE;
    newMod->isFIPS = PR_FALSE;
    newMod->dllName = NULL;
    newMod->commonName = NULL;
    newMod->library = NULL;
    newMod->functionList = NULL;
    newMod->slotCount = 0;
    newMod->slots = NULL;
    newMod->slotInfo = NULL;
    newMod->slotInfoCount = 0;
    newMod->refCount = 1;
    newMod->ssl[0] = 0;
    newMod->ssl[1] = 0;
    newMod->libraryParams = NULL;
    newMod->moduleDBFunc = NULL;
    newMod->parent = NULL;
    newMod->isCritical = PR_FALSE;
    newMod->isModuleDB = PR_FALSE;
    newMod->moduleDBOnly = PR_FALSE;
    newMod->trustOrder = 0;
    newMod->cipherOrder = 0;
    newMod->evControlMask = 0;
#ifdef PKCS11_USE_THREADS
    newMod->refLock = (void *)PZ_NewLock(nssILockRefLock);
    if (newMod->refLock == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
#else
    newMod->refLock = NULL;
#endif
    return newMod;
    
}

/*
 * for 3.4 we continue to use the old SECMODModule structure
 */
SECMODModule *
SECMOD_CreateModule(const char *library, const char *moduleName, 
				const char *parameters, const char *nss)
{
    SECMODModule *mod = secmod_NewModule();
    char *slotParams,*ciphers;
    /* pk11pars.h still does not have const char * interfaces */
    char *nssc = (char *)nss;
    if (mod == NULL) return NULL;

    mod->commonName = PORT_ArenaStrdup(mod->arena,moduleName ? moduleName : "");
    if (library) {
	mod->dllName = PORT_ArenaStrdup(mod->arena,library);
    }
    /* new field */
    if (parameters) {
	mod->libraryParams = PORT_ArenaStrdup(mod->arena,parameters);
    }
    mod->internal = pk11_argHasFlag("flags","internal",nssc);
    mod->isFIPS = pk11_argHasFlag("flags","FIPS",nssc);
    mod->isCritical = pk11_argHasFlag("flags","critical",nssc);
    slotParams = pk11_argGetParamValue("slotParams",nssc);
    mod->slotInfo = pk11_argParseSlotInfo(mod->arena,slotParams,
							&mod->slotInfoCount);
    if (slotParams) PORT_Free(slotParams);
    /* new field */
    mod->trustOrder = pk11_argReadLong("trustOrder",nssc,
						PK11_DEFAULT_TRUST_ORDER,NULL);
    /* new field */
    mod->cipherOrder = pk11_argReadLong("cipherOrder",nssc,
						PK11_DEFAULT_CIPHER_ORDER,NULL);
    /* new field */
    mod->isModuleDB = pk11_argHasFlag("flags","moduleDB",nssc);
    mod->moduleDBOnly = pk11_argHasFlag("flags","moduleDBOnly",nssc);
    if (mod->moduleDBOnly) mod->isModuleDB = PR_TRUE;

    ciphers = pk11_argGetParamValue("ciphers",nssc);
    pk11_argSetNewCipherFlags(&mod->ssl[0],ciphers);
    if (ciphers) PORT_Free(ciphers);

    secmod_PrivateModuleCount++;

    return mod;
}

static char *
pk11_mkModuleSpec(SECMODModule * module)
{
    char *nss = NULL, *modSpec = NULL, **slotStrings = NULL;
    int slotCount, i, si;
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();

    /* allocate target slot info strings */
    slotCount = 0;

    SECMOD_GetReadLock(moduleLock);
    if (module->slotCount) {
	for (i=0; i < module->slotCount; i++) {
	    if (module->slots[i]->defaultFlags !=0) {
		slotCount++;
	    }
	}
    } else {
	slotCount = module->slotInfoCount;
    }

    slotStrings = (char **)PORT_ZAlloc(slotCount*sizeof(char *));
    if (slotStrings == NULL) {
        SECMOD_ReleaseReadLock(moduleLock);
	goto loser;
    }


    /* build the slot info strings */
    if (module->slotCount) {
	for (i=0, si= 0; i < module->slotCount; i++) {
	    if (module->slots[i]->defaultFlags) {
		PORT_Assert(si < slotCount);
		if (si >= slotCount) break;
		slotStrings[si] = pk11_mkSlotString(module->slots[i]->slotID,
			module->slots[i]->defaultFlags,
			module->slots[i]->timeout,
			module->slots[i]->askpw,
			module->slots[i]->hasRootCerts,
			module->slots[i]->hasRootTrust);
		si++;
	    }
	}
     } else {
	for (i=0; i < slotCount; i++) {
		slotStrings[i] = pk11_mkSlotString(module->slotInfo[i].slotID,
			module->slotInfo[i].defaultFlags,
			module->slotInfo[i].timeout,
			module->slotInfo[i].askpw,
			module->slotInfo[i].hasRootCerts,
			module->slotInfo[i].hasRootTrust);
	}
    }

    SECMOD_ReleaseReadLock(moduleLock);
    nss = pk11_mkNSS(slotStrings,slotCount,module->internal, module->isFIPS,
	module->isModuleDB, module->moduleDBOnly, module->isCritical,
	module->trustOrder,module->cipherOrder,module->ssl[0],module->ssl[1]);
    modSpec= pk11_mkNewModuleSpec(module->dllName,module->commonName,
						module->libraryParams,nss);
    PORT_Free(slotStrings);
    PR_smprintf_free(nss);
loser:
    return (modSpec);
}
    

char **
SECMOD_GetModuleSpecList(SECMODModule *module)
{
    SECMODModuleDBFunc func = (SECMODModuleDBFunc) module->moduleDBFunc;
    if (func) {
	return (*func)(SECMOD_MODULE_DB_FUNCTION_FIND,
		module->libraryParams,NULL);
    }
    return NULL;
}

SECStatus
SECMOD_AddPermDB(SECMODModule *module)
{
    SECMODModuleDBFunc func;
    char *moduleSpec;
    char **retString;

    if (module->parent == NULL) return SECFailure;

    func  = (SECMODModuleDBFunc) module->parent->moduleDBFunc;
    if (func) {
	moduleSpec = pk11_mkModuleSpec(module);
	retString = (*func)(SECMOD_MODULE_DB_FUNCTION_ADD,
		module->parent->libraryParams,moduleSpec);
	PORT_Free(moduleSpec);
	if (retString != NULL) return SECSuccess;
    }
    return SECFailure;
}

SECStatus
SECMOD_DeletePermDB(SECMODModule *module)
{
    SECMODModuleDBFunc func;
    char *moduleSpec;
    char **retString;

    if (module->parent == NULL) return SECFailure;

    func  = (SECMODModuleDBFunc) module->parent->moduleDBFunc;
    if (func) {
	moduleSpec = pk11_mkModuleSpec(module);
	retString = (*func)(SECMOD_MODULE_DB_FUNCTION_DEL,
		module->parent->libraryParams,moduleSpec);
	PORT_Free(moduleSpec);
	if (retString != NULL) return SECSuccess;
    }
    return SECFailure;
}

SECStatus
SECMOD_FreeModuleSpecList(SECMODModule *module, char **moduleSpecList)
{
    SECMODModuleDBFunc func = (SECMODModuleDBFunc) module->moduleDBFunc;
    char **retString;
    if (func) {
	retString = (*func)(SECMOD_MODULE_DB_FUNCTION_RELEASE,
		module->libraryParams,moduleSpecList);
	if (retString != NULL) return SECSuccess;
    }
    return SECFailure;
}

/*
 * load a PKCS#11 module but do not add it to the default NSS trust domain
 */
SECMODModule *
SECMOD_LoadModule(char *modulespec,SECMODModule *parent, PRBool recurse)
{
    char *library = NULL, *moduleName = NULL, *parameters = NULL, *nss= NULL;
    SECStatus status;
    SECMODModule *module = NULL;
    SECStatus rv;

    /* initialize the underlying module structures */
    SECMOD_Init();

    status = pk11_argParseModuleSpec(modulespec, &library, &moduleName, 
							&parameters, &nss);
    if (status != SECSuccess) {
	goto loser;
    }

    module = SECMOD_CreateModule(library, moduleName, parameters, nss);
    if (library) PORT_Free(library);
    if (moduleName) PORT_Free(moduleName);
    if (parameters) PORT_Free(parameters);
    if (nss) PORT_Free(nss);
    if (!module) {
	goto loser;
    }
    if (parent) {
    	module->parent = SECMOD_ReferenceModule(parent);
    }

    /* load it */
    rv = SECMOD_LoadPKCS11Module(module);
    if (rv != SECSuccess) {
	goto loser;
    }

    if (recurse && module->isModuleDB) {
	char ** moduleSpecList;
	PORT_SetError(0);

	moduleSpecList = SECMOD_GetModuleSpecList(module);
	if (moduleSpecList) {
	    char **index;

	    for (index = moduleSpecList; *index; index++) {
		SECMODModule *child;
		child = SECMOD_LoadModule(*index,module,PR_TRUE);
		if (!child) break;
		if (child->isCritical && !child->loaded) {
		    int err = PORT_GetError();
		    if (!err)  
			err = SEC_ERROR_NO_MODULE;
		    SECMOD_DestroyModule(child);
		    PORT_SetError(err);
		    rv = SECFailure;
		    break;
		}
		SECMOD_DestroyModule(child);
	    }
	    SECMOD_FreeModuleSpecList(module,moduleSpecList);
	} else {
	    if (!PORT_GetError())
		PORT_SetError(SEC_ERROR_NO_MODULE);
	    rv = SECFailure;
	}
    }

    if (rv != SECSuccess) {
	goto loser;
    }


    /* inherit the reference */
    if (!module->moduleDBOnly) {
	SECMOD_AddModuleToList(module);
    } else {
	SECMOD_AddModuleToDBOnlyList(module);
    }
   
    /* handle any additional work here */
    return module;

loser:
    if (module) {
	if (module->loaded) {
	    SECMOD_UnloadModule(module);
	}
	SECMOD_AddModuleToUnloadList(module);
    }
    return module;
}

/*
 * load a PKCS#11 module and add it to the default NSS trust domain
 */
SECMODModule *
SECMOD_LoadUserModule(char *modulespec,SECMODModule *parent, PRBool recurse)
{
    SECStatus rv = SECSuccess;
    SECMODModule * newmod = SECMOD_LoadModule(modulespec, parent, recurse);
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();

    if (newmod) {
	SECMOD_GetReadLock(moduleLock);
        rv = STAN_AddModuleToDefaultTrustDomain(newmod);
	SECMOD_ReleaseReadLock(moduleLock);
        if (SECSuccess != rv) {
            SECMOD_DestroyModule(newmod);
            return NULL;
        }
    }
    return newmod;
}

/*
 * remove the PKCS#11 module from the default NSS trust domain, call
 * C_Finalize, and destroy the module structure
 */
SECStatus SECMOD_UnloadUserModule(SECMODModule *mod)
{
    SECStatus rv = SECSuccess;
    int atype = 0;
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
    if (!mod) {
        return SECFailure;
    }

    SECMOD_GetReadLock(moduleLock);
    rv = STAN_RemoveModuleFromDefaultTrustDomain(mod);
    SECMOD_ReleaseReadLock(moduleLock);
    if (SECSuccess != rv) {
        return SECFailure;
    }
    return SECMOD_DeleteModuleEx(NULL, mod, &atype, PR_FALSE);
}

