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
static const char CVS_ID[] = "@(#) $RCSfile: devmod.c,v $ $Revision: 1.1 $ $Date: 2001/11/08 00:14:52 $ $Name:  $";
#endif /* DEBUG */

#include "nspr.h"

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

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

/* Alloc memory for a module.  Copy in the module name and library path
 *  if provided.  XXX use the opaque arg also, right? 
 */
NSS_IMPLEMENT NSSModule *
nssModule_Create
(
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
	rvMod->name = nssUTF8_Duplicate(moduleOpt, arena);
	if (!rvMod->name) {
	    goto loser;
	}
    }
    if (uriOpt) {
	/* Load the module from a URI. */
	/* XXX at this time - only file URI (even worse, no file:// for now) */
	rvMod->libraryPath = nssUTF8_Duplicate(uriOpt, arena);
	if (!rvMod->libraryPath) {
	    goto loser;
	}
    }
    rvMod->arena = arena;
    rvMod->refCount = 1;
    /* everything else is 0/NULL at this point. */
    return rvMod;
loser:
    nssArena_Destroy(arena);
    return (NSSModule *)NULL;
}

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
    ckrv = CKAPI(mod)->C_GetSlotList(CK_FALSE, NULL, &ulNumSlots);
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
    ckrv = CKAPI(mod)->C_GetSlotList(CK_FALSE, slotIDs, &ulNumSlots);
    if (ckrv != CKR_OK) {
	/* what is the error? */
	goto loser;
    }
    /* Alloc memory for the array of slots, in the module's arena */
    mark = nssArena_Mark(mod->arena);
    if (!mark) {
	return PR_FAILURE;
    }
    slots = nss_ZNEWARRAY(mod->arena, NSSSlot *, ulNumSlots);
    if (!slots) {
	goto loser;
    }
    /* Initialize each slot */
    for (i=0; i<ulNumSlots; i++) {
	slots[i] = nssSlot_Create(mod->arena, slotIDs[i], mod);
    }
    nss_ZFreeIf(slotIDs);
    nssrv = nssArena_Unmark(mod->arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    mod->slots = slots;
    mod->numSlots = ulNumSlots;
    return PR_SUCCESS;
loser:
    if (mark) {
	nssArena_Release(mod->arena, mark);
    }
    nss_ZFreeIf(slotIDs);
    return PR_FAILURE;
}

/* This is going to take much more thought.  The module has a list of slots,
 * each of which points to a token.  Presumably, all are ref counted.  Some
 * kind of consistency check is needed here, or perhaps at a higher level.
 */
NSS_IMPLEMENT PRStatus
nssModule_Destroy
(
  NSSModule *mod
)
{
    PRUint32 i;
    if (--mod->refCount == 0) {
	/* Ignore any failure here.  */
	for (i=0; i<mod->numSlots; i++) {
	    nssSlot_Destroy(mod->slots[i]);
	}
	(void)nssModule_Unload(mod);
	return NSSArena_Destroy(mod->arena);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSModule *
nssModule_AddRef
(
  NSSModule *mod
)
{
    ++mod->refCount;
    return mod;
}

NSS_IMPLEMENT PRStatus
nssModule_Load
(
  NSSModule *mod
)
{
    PRLibrary *library = NULL;
    CK_C_GetFunctionList ep;
    CK_RV ckrv;
    /* Use NSPR to load the library */
    library = PR_LoadLibrary((char *)mod->libraryPath);
    if (!library) {
	/* what's the error to set? */
	return PR_FAILURE;
    }
    mod->library = library;
    /* TODO: semantics here in SECMOD_LoadModule about module db */
    /* Load the cryptoki entry point function */
    ep = (CK_C_GetFunctionList)PR_FindSymbol(library, "C_GetFunctionList");
    if (ep == NULL) {
	goto loser;
    }
    /* Load the cryptoki entry point vector (function list) */
    ckrv = (*ep)((CK_FUNCTION_LIST_PTR *)&mod->epv);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    /* Initialize the module */
    /* XXX This is where some additional parameters may need to come in,
     *     SECMOD_LoadModule has LibraryParameters
     */
    ckrv = CKAPI(mod)->C_Initialize(&s_ck_initialize_args);
    if (ckrv != CKR_OK) {
	/* Apparently the token is not thread safe.  Retry without 
	 * threading parameters. 
	 */
        mod->flags |= NSSMODULE_FLAGS_NOT_THREADSAFE;
	ckrv = CKAPI(mod)->C_Initialize((CK_VOID_PTR)NULL);
	if (ckrv != CKR_OK) {
	    goto loser;
	}
    }
    /* TODO: check the version # using C_GetInfo */
    /* TODO: if the name is not set, get it from info.libraryDescription */
    /* Now load the slots */
    if (module_load_slots(mod) != PR_SUCCESS) {
	goto loser;
    }
    /* Module has successfully loaded */
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
nssModule_Unload
(
  NSSModule *mod
)
{
    PRStatus nssrv = PR_SUCCESS;
    if (mod->library) {
	(void)CKAPI(mod)->C_Finalize(NULL);
	nssrv = PR_UnloadLibrary(mod->library);
    }
    /* Free the slots, yes? */
    mod->library = NULL;
    mod->epv = NULL;
    return nssrv;
}

NSS_IMPLEMENT NSSSlot *
nssModule_FindSlotByName
(
  NSSModule *mod,
  NSSUTF8 *slotName
)
{
    PRUint32 i;
    PRStatus nssrv;
    for (i=0; i<mod->numSlots; i++) {
	if (nssUTF8_Equal(mod->slots[i]->name, slotName, &nssrv)) {
	    return nssSlot_AddRef(mod->slots[i]);
	}
	if (nssrv != PR_SUCCESS) {
	    break;
	}
    }
    return (NSSSlot *)NULL;
}

NSS_EXTERN NSSToken *
nssModule_FindTokenByName
(
  NSSModule *mod,
  NSSUTF8 *tokenName
)
{
    PRUint32 i;
    PRStatus nssrv;
    NSSToken *tok;
    for (i=0; i<mod->numSlots; i++) {
	tok = mod->slots[i]->token;
	if (nssUTF8_Equal(tok->name, tokenName, &nssrv)) {
	    return nssToken_AddRef(tok);
	}
	if (nssrv != PR_SUCCESS) {
	    break;
	}
    }
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT PRStatus *
nssModule_TraverseCertificates
(
  NSSModule *mod,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRUint32 i;
    for (i=0; i<mod->numSlots; i++) {
	/* might as well skip straight to token, right? or is this slot? */
	nssToken_TraverseCertificates(mod->slots[i]->token, 
	                              NULL, callback, arg);
    }
    return NULL;
}

#ifdef DEBUG
void
nssModule_Debug(NSSModule *m)
{
    PRUint32 i;
    printf("\n");
    printf("Module Name: %s\n", m->name);
    printf("Module Path: %s\n", m->libraryPath);
    printf("Number of Slots in Module: %d\n\n", m->numSlots);
    for (i=0; i<m->numSlots; i++) {
	printf("\tSlot #%d\n", i);
	if (m->slots[i]->name) {
	    printf("\tSlot Name: %s\n", m->slots[i]->name);
	} else {
	    printf("\tSlot Name: <NULL>\n");
	}
	if (m->slots[i]->token) {
	    printf("\tToken Name: %s\n", m->slots[i]->token->name);
	} else {
	    printf("\tToken: <NULL>\n");
	}
    }
}
#endif
