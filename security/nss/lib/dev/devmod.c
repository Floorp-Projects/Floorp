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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: devmod.c,v $ $Revision: 1.5 $ $Date: 2003/08/01 02:02:43 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifdef PURE_STAN_CODE

extern void FC_GetFunctionList(void);
extern void NSC_GetFunctionList(void);
extern void NSC_ModuleDBFunc(void);

/* The list of boolean flags used to describe properties of a
 * module.
 */
#define NSSMODULE_FLAGS_NOT_THREADSAFE 0x0001 /* isThreadSafe */
#define NSSMODULE_FLAGS_INTERNAL       0x0002 /* isInternal   */
#define NSSMODULE_FLAGS_FIPS           0x0004 /* isFIPS       */
#define NSSMODULE_FLAGS_MODULE_DB      0x0008 /* isModuleDB   */
#define NSSMODULE_FLAGS_MODULE_DB_ONLY 0x0010 /* moduleDBOnly */
#define NSSMODULE_FLAGS_CRITICAL       0x0020 /* isCritical   */

struct NSSModuleStr {
  struct nssDeviceBaseStr base;
  NSSUTF8 *libraryName;
  PRLibrary *library;
  char *libraryParams;
  void *moduleDBFunc;
  void *epv;
  CK_INFO info;
  NSSSlot **slots;
  PRUint32 numSlots;
  PRBool isLoaded;
  struct {
    PRInt32 trust;
    PRInt32 cipher;
    PRInt32 certStorage;
  } order;
};

#define NSSMODULE_IS_THREADSAFE(module) \
    (!(module->base.flags & NSSMODULE_FLAGS_NOT_THREADSAFE))

#define NSSMODULE_IS_INTERNAL(module) \
    (module->base.flags & NSSMODULE_FLAGS_INTERNAL)

#define NSSMODULE_IS_FIPS(module) \
    (module->base.flags & NSSMODULE_FLAGS_FIPS)

#define NSSMODULE_IS_MODULE_DB(module) \
    (module->base.flags & NSSMODULE_FLAGS_MODULE_DB)

#define NSSMODULE_IS_MODULE_DB_ONLY(module) \
    (module->base.flags & NSSMODULE_FLAGS_MODULE_DB_ONLY)

#define NSSMODULE_IS_CRITICAL(module) \
    (module->base.flags & NSSMODULE_FLAGS_CRITICAL)

/* Threading callbacks for C_Initialize.  Use NSPR threads. */

CK_RV PR_CALLBACK 
nss_ck_CreateMutex(CK_VOID_PTR_PTR pMutex)
{
    CK_VOID_PTR mutex = (CK_VOID_PTR)PZ_NewLock(nssILockOther);
    if (mutex != NULL) {
	*pMutex = (CK_VOID_PTR)mutex;
	return CKR_OK;
    }
    return CKR_HOST_MEMORY;
}

CK_RV PR_CALLBACK
nss_ck_DestroyMutex(CK_VOID_PTR mutex)
{
    PZ_DestroyLock((PZLock *)mutex);
    return CKR_OK;
}

CK_RV PR_CALLBACK
nss_ck_LockMutex(CK_VOID_PTR mutex)
{
    PZ_Lock((PZLock *)mutex);
    return CKR_OK;
}

CK_RV PR_CALLBACK
nss_ck_UnlockMutex(CK_VOID_PTR mutex)
{
    return (PZ_Unlock((PZLock *)mutex) == PR_SUCCESS) ? 
	CKR_OK : CKR_MUTEX_NOT_LOCKED;
}

/* Default callback args to C_Initialize */
/* XXX not const because we are modifying the pReserved argument in order
 *     to use the libraryParams extension.
 */
static CK_C_INITIALIZE_ARGS 
s_ck_initialize_args = {
    nss_ck_CreateMutex,         /* CreateMutex  */
    nss_ck_DestroyMutex,        /* DestroyMutex */
    nss_ck_LockMutex,           /* LockMutex    */
    nss_ck_UnlockMutex,         /* UnlockMutex  */
    CKF_LIBRARY_CANT_CREATE_OS_THREADS |
    CKF_OS_LOCKING_OK,          /* flags        */
    NULL                        /* pReserved    */
};

/* load all slots in a module. */
static PRStatus
module_load_slots(NSSModule *mod)
{
    CK_ULONG i, ulNumSlots;
    CK_SLOT_ID *slotIDs;
    nssArenaMark *mark = NULL;
    NSSSlot **slots;
    PRStatus nssrv;
    CK_RV ckrv;
    /* Get the number of slots */
    ckrv = CKAPI(mod->epv)->C_GetSlotList(CK_FALSE, NULL, &ulNumSlots);
    if (ckrv != CKR_OK) {
	/* what is the error? */
	return PR_FAILURE;
    }
    /* Alloc memory for the array of slot ID's */
    slotIDs = nss_ZNEWARRAY(NULL, CK_SLOT_ID, ulNumSlots);
    if (!slotIDs) {
	goto loser;
    }
    /* Get the actual slot list */
    ckrv = CKAPI(mod->epv)->C_GetSlotList(CK_FALSE, slotIDs, &ulNumSlots);
    if (ckrv != CKR_OK) {
	/* what is the error? */
	goto loser;
    }
    /* Alloc memory for the array of slots, in the module's arena */
    mark = nssArena_Mark(mod->base.arena); /* why mark? it'll be destroyed */
    if (!mark) {
	return PR_FAILURE;
    }
    slots = nss_ZNEWARRAY(mod->base.arena, NSSSlot *, ulNumSlots);
    if (!slots) {
	goto loser;
    }
    /* Initialize each slot */
    for (i=0; i<ulNumSlots; i++) {
	slots[i] = nssSlot_Create(slotIDs[i], mod);
    }
    nss_ZFreeIf(slotIDs);
    nssrv = nssArena_Unmark(mod->base.arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    mod->slots = slots;
    mod->numSlots = ulNumSlots;
    return PR_SUCCESS;
loser:
    if (mark) {
	nssArena_Release(mod->base.arena, mark);
    }
    nss_ZFreeIf(slotIDs);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssModule_Load (
  NSSModule *mod
)
{
    PRLibrary *library = NULL;
    CK_C_GetFunctionList epv;
    CK_RV ckrv;
    if (NSSMODULE_IS_INTERNAL(mod)) {
	/* internal, statically get the C_GetFunctionList function */
	if (NSSMODULE_IS_FIPS(mod)) {
	    epv = (CK_C_GetFunctionList) FC_GetFunctionList;
	} else {
	    epv = (CK_C_GetFunctionList) NSC_GetFunctionList;
	}
	if (NSSMODULE_IS_MODULE_DB(mod)) {
	    mod->moduleDBFunc = (void *) NSC_ModuleDBFunc;
	}
	if (NSSMODULE_IS_MODULE_DB_ONLY(mod)) {
	    mod->isLoaded = PR_TRUE; /* XXX needed? */
	    return PR_SUCCESS;
	}
    } else {
	/* Use NSPR to load the library */
	library = PR_LoadLibrary(mod->libraryName);
	if (!library) {
	    /* what's the error to set? */
	    return PR_FAILURE;
	}
	mod->library = library;
	/* Skip if only getting the db loader function */
	if (!NSSMODULE_IS_MODULE_DB_ONLY(mod)) {
	    /* Load the cryptoki entry point function */
	    epv = (CK_C_GetFunctionList)PR_FindSymbol(library, 
	                                              "C_GetFunctionList");
	}
	/* Load the module database loader function */
	if (NSSMODULE_IS_MODULE_DB(mod)) {
	    mod->moduleDBFunc = (void *)PR_FindSymbol(library, 
	                                          "NSS_ReturnModuleSpecData");
	}
    }
    if (epv == NULL) {
	goto loser;
    }
    /* Load the cryptoki entry point vector (function list) */
    ckrv = (*epv)((CK_FUNCTION_LIST_PTR *)&mod->epv);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    /* Initialize the module */
    if (mod->libraryParams) {
	s_ck_initialize_args.LibraryParameters = (void *)mod->libraryParams;
    } else {
	s_ck_initialize_args.LibraryParameters = NULL;
    }
    ckrv = CKAPI(mod->epv)->C_Initialize(&s_ck_initialize_args);
    if (ckrv != CKR_OK) {
	/* Apparently the token is not thread safe.  Retry without 
	 * threading parameters. 
	 */
        mod->base.flags |= NSSMODULE_FLAGS_NOT_THREADSAFE;
	ckrv = CKAPI(mod->epv)->C_Initialize((CK_VOID_PTR)NULL);
	if (ckrv != CKR_OK) {
	    goto loser;
	}
    }
    /* TODO: check the version # using C_GetInfo */
    ckrv = CKAPI(mod->epv)->C_GetInfo(&mod->info);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    /* TODO: if the name is not set, get it from info.libraryDescription */
    /* Now load the slots */
    if (module_load_slots(mod) != PR_SUCCESS) {
	goto loser;
    }
    /* Module has successfully loaded */
    mod->isLoaded = PR_TRUE;
    return PR_SUCCESS;
loser:
    if (library) {
	PR_UnloadLibrary(library);
    }
    /* clear all values set above, they are invalid now */
    mod->library = NULL;
    mod->epv = NULL;
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssModule_Unload (
  NSSModule *mod
)
{
    PRStatus nssrv = PR_SUCCESS;
    if (mod->library) {
	(void)CKAPI(mod->epv)->C_Finalize(NULL);
	nssrv = PR_UnloadLibrary(mod->library);
    }
    /* Free the slots, yes? */
    mod->library = NULL;
    mod->epv = NULL;
    mod->isLoaded = PR_FALSE;
    return nssrv;
}

/* Alloc memory for a module.  Copy in the module name and library path
 *  if provided.  XXX use the opaque arg also, right? 
 */
NSS_IMPLEMENT NSSModule *
nssModule_Create (
  NSSUTF8 *moduleOpt,
  NSSUTF8 *uriOpt,
  NSSUTF8 *opaqueOpt,
  void    *reserved
)
{
    NSSArena *arena;
    NSSModule *rvMod;
    arena = NSSArena_Create();
    if(!arena) {
	return (NSSModule *)NULL;
    }
    rvMod = nss_ZNEW(arena, NSSModule);
    if (!rvMod) {
	goto loser;
    }
    if (moduleOpt) {
	/* XXX making the gross assumption this is just the module name */
	/* if the name is a duplicate, should that be tested here?  or
	 *  wait for Load? 
	 */
	rvMod->base.name = nssUTF8_Duplicate(moduleOpt, arena);
	if (!rvMod->base.name) {
	    goto loser;
	}
    }
    if (uriOpt) {
	/* Load the module from a URI. */
	/* XXX at this time - only file URI (even worse, no file:// for now) */
	rvMod->libraryName = nssUTF8_Duplicate(uriOpt, arena);
	if (!rvMod->libraryName) {
	    goto loser;
	}
    }
    rvMod->base.arena = arena;
    rvMod->base.refCount = 1;
    rvMod->base.lock = PZ_NewLock(nssNSSILockOther);
    if (!rvMod->base.lock) {
	goto loser;
    }
    /* everything else is 0/NULL at this point. */
    return rvMod;
loser:
    nssArena_Destroy(arena);
    return (NSSModule *)NULL;
}

NSS_EXTERN PRStatus
nssCryptokiArgs_ParseNextPair (
  NSSUTF8 *start,
  NSSUTF8 **attrib,
  NSSUTF8 **value,
  NSSUTF8 **remainder,
  NSSArena *arenaOpt
);

static PRStatus
parse_slot_flags (
  NSSSlot *slot,
  NSSUTF8 *slotFlags
)
{
    PRStatus nssrv = PR_SUCCESS;
#if 0
    PRBool done = PR_FALSE;
    NSSUTF8 *mark, *last;
    last = mark = slotFlags;
    while (PR_TRUE) {
	while (*mark && *mark != ',') ++mark;
	if (!*mark) done = PR_TRUE;
	*mark = '\0';
	if (nssUTF8_Equal(last, "RANDOM", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_HAS_RANDOM;
	} else if (nssUTF8_Equal(last, "RSA", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_RSA;
	} else if (nssUTF8_Equal(last, "DSA", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_DSA;
	} else if (nssUTF8_Equal(last, "DH", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_DH;
	} else if (nssUTF8_Equal(last, "RC2", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_RC2;
	} else if (nssUTF8_Equal(last, "RC4", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_RC4;
	} else if (nssUTF8_Equal(last, "RC5", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_RC5;
	} else if (nssUTF8_Equal(last, "DES", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_DES;
	} else if (nssUTF8_Equal(last, "AES", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_AES;
	} else if (nssUTF8_Equal(last, "SHA1", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_SHA1;
	} else if (nssUTF8_Equal(last, "MD2", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_MD2;
	} else if (nssUTF8_Equal(last, "MD5", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_MD5;
	} else if (nssUTF8_Equal(last, "SSL", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_SSL;
	} else if (nssUTF8_Equal(last, "TLS", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_TLS;
	} else if (nssUTF8_Equal(last, "PublicCerts", &nssrv)) {
	    slot->base.flags |= NSSSLOT_FLAGS_FRIENDLY;
	} else {
	    return PR_FAILURE;
	}
	if (done) break;
	last = ++mark;
    }
#endif
    return nssrv;
}

static PRStatus
parse_slot_parameters (
  NSSSlot *slot,
  NSSUTF8 *slotParams,
  NSSArena *tmparena
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSUTF8 *current, *remainder;
    NSSUTF8 *attrib, *value;
    current = slotParams;
    while (nssrv == PR_SUCCESS) {
	nssrv = nssCryptokiArgs_ParseNextPair(current, 
	                                      &attrib, &value, 
	                                      &remainder, tmparena);
	if (nssrv != PR_SUCCESS) break;
	if (value) {
	    if (nssUTF8_Equal(attrib, "slotFlags", &nssrv)) {
		nssrv = parse_slot_flags(slot, value);
	    } else if (nssUTF8_Equal(attrib, "askpw", &nssrv)) {
	    } else if (nssUTF8_Equal(attrib, "timeout", &nssrv)) {
	    }
	}
	if (*remainder == '\0') break;
	current = remainder;
    }
    return nssrv;
}

/* softoken seems to use "0x0000001", but no standard yet...  perhaps this
 * should store the number as an ID, in case the input isn't 1,2,3,...?
 */
static PRIntn
get_slot_number(NSSUTF8* snString)
{
    /* XXX super big hack */
    return atoi(&snString[strlen(snString)-1]);
}

static PRStatus
parse_module_slot_parameters (
  NSSModule *mod,
  NSSUTF8 *slotParams
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSUTF8 *current, *remainder;
    NSSUTF8 *attrib, *value;
    NSSArena *tmparena;
    PRIntn slotNum;
    tmparena = nssArena_Create();
    if (!tmparena) {
	return PR_FAILURE;
    }
    current = slotParams;
    while (nssrv == PR_SUCCESS) {
	nssrv = nssCryptokiArgs_ParseNextPair(current, 
	                                      &attrib, &value, 
	                                      &remainder, tmparena);
	if (nssrv != PR_SUCCESS) break;
	if (value) {
	    slotNum = get_slot_number(attrib);
	    if (slotNum < 0 || slotNum > mod->numSlots) {
		return PR_FAILURE;
	    }
	    nssrv = parse_slot_parameters(mod->slots[slotNum], 
	                                  value, tmparena);
	    if (nssrv != PR_SUCCESS) break;
	}
	if (*remainder == '\0') break;
	current = remainder;
    }
    return nssrv;
}

static PRStatus
parse_nss_flags (
  NSSModule *mod,
  NSSUTF8 *nssFlags
)
{
    PRStatus nssrv = PR_SUCCESS;
    PRBool done = PR_FALSE;
    NSSUTF8 *mark, *last;
    last = mark = nssFlags;
    while (PR_TRUE) {
	while (*mark && *mark != ',') ++mark;
	if (!*mark) done = PR_TRUE;
	*mark = '\0';
	if (nssUTF8_Equal(last, "internal", &nssrv)) {
	    mod->base.flags |= NSSMODULE_FLAGS_INTERNAL;
	} else if (nssUTF8_Equal(last, "moduleDB", &nssrv)) {
	    mod->base.flags |= NSSMODULE_FLAGS_MODULE_DB;
	} else if (nssUTF8_Equal(last, "moduleDBOnly", &nssrv)) {
	    mod->base.flags |= NSSMODULE_FLAGS_MODULE_DB_ONLY;
	} else if (nssUTF8_Equal(last, "critical", &nssrv)) {
	    mod->base.flags |= NSSMODULE_FLAGS_CRITICAL;
	} else {
	    return PR_FAILURE;
	}
	if (done) break;
	last = ++mark;
    }
    return nssrv;
}

static PRStatus
parse_nss_parameters (
  NSSModule *mod,
  NSSUTF8 *nssParams,
  NSSArena *tmparena,
  NSSUTF8 **slotParams
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSUTF8 *current, *remainder;
    NSSUTF8 *attrib, *value;
    current = nssParams;
    while (nssrv == PR_SUCCESS) {
	nssrv = nssCryptokiArgs_ParseNextPair(current, 
	                                      &attrib, &value, 
	                                      &remainder, tmparena);
	if (nssrv != PR_SUCCESS) break;
	if (value) {
	    if (nssUTF8_Equal(attrib, "flags", &nssrv) ||
	        nssUTF8_Equal(attrib, "Flags", &nssrv)) {
		nssrv = parse_nss_flags(mod, value);
	    } else if (nssUTF8_Equal(attrib, "trustOrder", &nssrv)) {
		mod->order.trust = atoi(value);
	    } else if (nssUTF8_Equal(attrib, "cipherOrder", &nssrv)) {
		mod->order.cipher = atoi(value);
	    } else if (nssUTF8_Equal(attrib, "ciphers", &nssrv)) {
	    } else if (nssUTF8_Equal(attrib, "slotParams", &nssrv)) {
		/* slotParams doesn't get an arena, it is handled separately */
		*slotParams = nssUTF8_Duplicate(value, NULL);
	    }
	}
	if (*remainder == '\0') break;
	current = remainder;
    }
    return nssrv;
}

static PRStatus
parse_module_parameters (
  NSSModule *mod,
  NSSUTF8 *moduleParams,
  NSSUTF8 **slotParams
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSUTF8 *current, *remainder;
    NSSUTF8 *attrib, *value;
    NSSArena *arena = mod->base.arena;
    NSSArena *tmparena;
    current = moduleParams;
    tmparena = nssArena_Create();
    if (!tmparena) {
	return PR_FAILURE;
    }
    while (nssrv == PR_SUCCESS) {
	nssrv = nssCryptokiArgs_ParseNextPair(current, 
	                                      &attrib, &value, 
	                                      &remainder, tmparena);
	if (nssrv != PR_SUCCESS) break;
	if (value) {
	    if (nssUTF8_Equal(attrib, "name", &nssrv)) {
		mod->base.name = nssUTF8_Duplicate(value, arena);
	    } else if (nssUTF8_Equal(attrib, "library", &nssrv)) {
		mod->libraryName = nssUTF8_Duplicate(value, arena);
	    } else if (nssUTF8_Equal(attrib, "parameters", &nssrv)) {
		mod->libraryParams = nssUTF8_Duplicate(value, arena);
	    } else if (nssUTF8_Equal(attrib, "NSS", &nssrv)) {
		parse_nss_parameters(mod, value, tmparena, slotParams);
	    }
	}
	if (*remainder == '\0') break;
	current = remainder;
    }
    nssArena_Destroy(tmparena);
    return nssrv;
}

static NSSUTF8 **
get_module_specs (
  NSSModule *mod
)
{
    SECMODModuleDBFunc func = (SECMODModuleDBFunc)mod->moduleDBFunc;
    if (func) {
	return (*func)(SECMOD_MODULE_DB_FUNCTION_FIND,
	               mod->libraryParams,
	               NULL);
    }
    return NULL;
}

/* XXX continue working on */
NSS_IMPLEMENT NSSModule *
nssModule_CreateFromSpec (
  NSSUTF8 *moduleSpec,
  NSSModule *parent,
  PRBool loadSubModules
)
{
    PRStatus nssrv;
    NSSModule *thisModule;
    NSSArena *arena;
    NSSUTF8 *slotParams = NULL;
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    thisModule = nss_ZNEW(arena, NSSModule);
    if (!thisModule) {
	goto loser;
    }
    thisModule->base.lock = PZ_NewLock(nssILockOther);
    if (!thisModule->base.lock) {
	goto loser;
    }
    PR_AtomicIncrement(&thisModule->base.refCount);
    thisModule->base.arena = arena;
    thisModule->base.lock = PZ_NewLock(nssNSSILockOther);
    if (!thisModule->base.lock) {
	goto loser;
    }
    nssrv = parse_module_parameters(thisModule, moduleSpec, &slotParams);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    nssrv = nssModule_Load(thisModule);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    if (slotParams) {
	nssrv = parse_module_slot_parameters(thisModule, slotParams);
	nss_ZFreeIf(slotParams);
	if (nssrv != PR_SUCCESS) {
	    goto loser;
	}
    }
    if (loadSubModules && NSSMODULE_IS_MODULE_DB(thisModule)) {
	NSSUTF8 **moduleSpecs;
	NSSUTF8 **index;
	/* get the array of sub modules one level below this module */
	moduleSpecs = get_module_specs(thisModule);
	/* iterate over the array */
	for (index = moduleSpecs; index && *index; index++) {
	    NSSModule *child;
	    /* load the child recursively */
	    child = nssModule_CreateFromSpec(*index, thisModule, PR_TRUE);
	    if (!child) {
		/* when children fail, does the parent? */
		nssrv = PR_FAILURE;
		break;
	    }
	    if (NSSMODULE_IS_CRITICAL(child) && !child->isLoaded) {
		nssrv = PR_FAILURE;
		nssModule_Destroy(child);
		break;
	    }
	    nssModule_Destroy(child);
	    /*nss_ZFreeIf(*index);*/
	}
	/*nss_ZFreeIf(moduleSpecs);*/
    }
    /* The global list inherits the reference */
    nssrv = nssGlobalModuleList_Add(thisModule);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    return thisModule;
loser:
    if (thisModule->base.lock) {
	PZ_DestroyLock(thisModule->base.lock);
    }
    nssArena_Destroy(arena);
    return (NSSModule *)NULL;
}

NSS_IMPLEMENT PRStatus
nssModule_Destroy (
  NSSModule *mod
)
{
    PRUint32 i, numSlots;
    if (PR_AtomicDecrement(&mod->base.refCount) == 0) {
	if (mod->numSlots == 0) {
	    (void)nssModule_Unload(mod);
	    return nssArena_Destroy(mod->base.arena);
	} else {
	    numSlots = mod->numSlots;
	    for (i=0; i<numSlots; i++) {
		nssSlot_Destroy(mod->slots[i]);
	    }
	}
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssModule_DestroyFromSlot (
  NSSModule *mod,
  NSSSlot *slot
)
{
    PRUint32 i, numSlots = 0;
    PR_ASSERT(mod->base.refCount == 0);
    for (i=0; i<mod->numSlots; i++) {
	if (mod->slots[i] == slot) {
	    mod->slots[i] = NULL;
	} else if (mod->slots[i]) {
	    numSlots++;
	}
    }
    if (numSlots == 0) {
	(void)nssModule_Unload(mod);
	return nssArena_Destroy(mod->base.arena);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSModule *
nssModule_AddRef (
  NSSModule *mod
)
{
    PR_AtomicIncrement(&mod->base.refCount);
    return mod;
}

NSS_IMPLEMENT NSSUTF8 *
nssModule_GetName (
  NSSModule *mod
)
{
    return mod->base.name;
}

NSS_IMPLEMENT PRBool
nssModule_IsThreadSafe (
  NSSModule *module
)
{
    return NSSMODULE_IS_THREADSAFE(module);
}

NSS_IMPLEMENT PRBool
nssModule_IsInternal (
  NSSModule *mod
)
{
    return NSSMODULE_IS_INTERNAL(mod);
}

NSS_IMPLEMENT PRBool
nssModule_IsModuleDBOnly (
  NSSModule *mod
)
{
    return NSSMODULE_IS_MODULE_DB_ONLY(mod);
}

NSS_IMPLEMENT void *
nssModule_GetCryptokiEPV (
  NSSModule *mod
)
{
    return mod->epv;
}

NSS_IMPLEMENT NSSSlot **
nssModule_GetSlots (
  NSSModule *mod
)
{
    PRUint32 i;
    NSSSlot **rvSlots;
    rvSlots = nss_ZNEWARRAY(NULL, NSSSlot *, mod->numSlots + 1);
    if (rvSlots) {
	for (i=0; i<mod->numSlots; i++) {
	    rvSlots[i] = nssSlot_AddRef(mod->slots[i]);
	}
    }
    return rvSlots;
}

NSS_IMPLEMENT NSSSlot *
nssModule_FindSlotByName (
  NSSModule *mod,
  NSSUTF8 *slotName
)
{
    PRUint32 i;
    PRStatus nssrv;
    NSSSlot *slot;
    NSSUTF8 *name;
    for (i=0; i<mod->numSlots; i++) {
	slot = mod->slots[i];
	name = nssSlot_GetName(slot);
	if (nssUTF8_Equal(name, slotName, &nssrv)) {
	    return nssSlot_AddRef(slot);
	}
	if (nssrv != PR_SUCCESS) {
	    break;
	}
    }
    return (NSSSlot *)NULL;
}

NSS_IMPLEMENT NSSToken *
nssModule_FindTokenByName (
  NSSModule *mod,
  NSSUTF8 *tokenName
)
{
    PRUint32 i;
    PRStatus nssrv;
    NSSToken *tok;
    NSSUTF8 *name;
    for (i=0; i<mod->numSlots; i++) {
	tok = nssSlot_GetToken(mod->slots[i]);
	if (tok) {
	    name = nssToken_GetName(tok);
	    if (nssUTF8_Equal(name, tokenName, &nssrv)) {
		return tok;
	    }
	    if (nssrv != PR_SUCCESS) {
		break;
	    }
	}
    }
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT PRInt32
nssModule_GetCertOrder (
  NSSModule *module
)
{
    return 1; /* XXX */
}

#endif /* PURE_STAN_BUILD */

