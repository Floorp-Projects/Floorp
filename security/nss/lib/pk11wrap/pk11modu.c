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
 *  The following code handles the storage of PKCS 11 modules used by the
 * NSS. This file is written to abstract away how the modules are
 * stored so we can deside that later.
 */
#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "pkcs11.h"
#include "secmodi.h"
#include "pk11func.h"
#include "mcom_db.h"

/* create a new module */
SECMODModule *SECMOD_NewModule(void) {
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

static unsigned long internalFlags = SECMOD_RSA_FLAG|SECMOD_DSA_FLAG|
	SECMOD_RC2_FLAG| SECMOD_RC4_FLAG|SECMOD_DES_FLAG|SECMOD_RANDOM_FLAG|
	SECMOD_SHA1_FLAG|SECMOD_MD5_FLAG|SECMOD_MD2_FLAG|SECMOD_SSL_FLAG|
	SECMOD_TLS_FLAG|SECMOD_AES_FLAG;

/* create a Internal  module */
SECMODModule *SECMOD_NewInternal(void) {
    SECMODModule *intern;
    static PK11PreSlotInfo internSlotInfo =
	{ 1, SECMOD_RSA_FLAG|SECMOD_DSA_FLAG|SECMOD_RC2_FLAG|
	SECMOD_RC4_FLAG|SECMOD_DES_FLAG|SECMOD_RANDOM_FLAG|
	SECMOD_SHA1_FLAG|SECMOD_MD5_FLAG|SECMOD_MD2_FLAG|
	SECMOD_SSL_FLAG|SECMOD_TLS_FLAG|SECMOD_AES_FLAG, -1, 30, 0 };

    intern = SECMOD_NewModule();
    if (intern == NULL) {
	return NULL;
    }

    /*
     * make this module an internal module
     */
    intern->commonName = "NSS Internal PKCS #11 Module";
    intern->internal = PR_TRUE;
    intern->slotInfoCount = 1;
    intern->slotInfo = &internSlotInfo;

    return (intern);
}

/* create a FIPS Internal  module */
SECMODModule *SECMOD_GetFIPSInternal(void) {
    SECMODModule *intern;

    intern = SECMOD_NewInternal();
    if (intern == NULL) {
	return NULL;
    }

    /*
     * make this module a FIPS internal module
     */
    intern->slotInfo[0].slotID = 3; /* FIPS slot */
    intern->commonName = "NSS Internal FIPS PKCS #11 Module";
    intern->isFIPS = PR_TRUE;

    return (intern);
}

SECMODModule *SECMOD_DupModule(SECMODModule *old) {
    SECMODModule *newMod;

    newMod = SECMOD_NewModule();
    if (newMod == NULL) {
	return NULL;
    }

    /*
     * initialize of the fields of the module
     */
    newMod->dllName = PORT_ArenaStrdup(newMod->arena,old->dllName);
    newMod->commonName = PORT_ArenaStrdup(newMod->arena,old->commonName);;

    return newMod;
    
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
    if (module->loaded) {
	SECMOD_UnloadModule(module);
    }
    PK11_USE_THREADS(PZ_DestroyLock((PZLock *)module->refLock);)
    PORT_FreeArena(module->arena,PR_FALSE);
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

