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
#include "prlock.h"
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
    newMod->refLock = (void *)PR_NewLock();
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
	SECMOD_TLS_FLAG;

/* create a Internal  module */
SECMODModule *SECMOD_NewInternal(void) {
    SECMODModule *intern;
    static PK11PreSlotInfo internSlotInfo =
	{ 1, SECMOD_RSA_FLAG|SECMOD_DSA_FLAG|SECMOD_RC2_FLAG|
	SECMOD_RC4_FLAG|SECMOD_DES_FLAG|SECMOD_RANDOM_FLAG|
	SECMOD_SHA1_FLAG|SECMOD_MD5_FLAG|SECMOD_MD2_FLAG|
	SECMOD_SSL_FLAG|SECMOD_TLS_FLAG, -1, 30, 0 };

    intern = SECMOD_NewModule();
    if (intern == NULL) {
	return NULL;
    }

    /*
     * make this module an internal module
     */
    intern->commonName = "Netscape Internal PKCS #11 Module";
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
    intern->commonName = "Netscape Internal FIPS PKCS #11 Module";
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
    PK11_USE_THREADS(PR_Lock((PRLock *)module->refLock);)
    PORT_Assert(module->refCount > 0);

    module->refCount++;
    PK11_USE_THREADS(PR_Unlock((PRLock*)module->refLock);)
    return module;
}


/* destroy an existing module */
void
SECMOD_DestroyModule(SECMODModule *module) {
    PRBool willfree = PR_FALSE;
    int slotCount;
    int i;

    PK11_USE_THREADS(PR_Lock((PRLock *)module->refLock);)
    if (module->refCount-- == 1) {
	willfree = PR_TRUE;
    }
    PORT_Assert(willfree || (module->refCount > 0));
    PK11_USE_THREADS(PR_Unlock((PRLock *)module->refLock);)

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
	PK11_USE_THREADS(PR_Lock((PRLock *)module->refLock);)
	if (module->slotCount-- == 1) {
	    willfree = PR_TRUE;
	}
	PORT_Assert(willfree || (module->slotCount > 0));
	PK11_USE_THREADS(PR_Unlock((PRLock *)module->refLock);)
        if (!willfree) return;
    }
    if (module->loaded) {
	SECMOD_UnloadModule(module);
    }
    PK11_USE_THREADS(PR_DestroyLock((PRLock *)module->refLock);)
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


/* Construct a database key for a given module */
static SECStatus secmod_MakeKey(DBT *key, SECMODModule * module) {
    int len = 0;

    len = PORT_Strlen(module->commonName);
    key->data = module->commonName;
    key->size = len;
    return SECSuccess;
}

/* free out constructed database key */
static void secmod_FreeKey(DBT *key) {
    key->data = NULL;
    key->size = 0;
}

typedef struct secmodDataStr secmodData;
typedef struct secmodSlotDataStr secmodSlotData;
struct secmodDataStr {
    unsigned char major;
    unsigned char minor;
    unsigned char nameStart[2];
    unsigned char slotOffset[2];
    unsigned char internal;
    unsigned char fips;
    unsigned char ssl[8];
    unsigned char names[4];	/* enough space for the length fields */
};

struct secmodSlotDataStr {
    unsigned char slotID[4];
    unsigned char defaultFlags[4];
    unsigned char timeout[4];
    unsigned char askpw;
    unsigned char hasRootCerts;
    unsigned char reserved[18]; /* this makes it a round 32 bytes */
};

#define SECMOD_DB_VERSION_MAJOR 0
#define SECMOD_DB_VERSION_MINOR 4
#define SECMOD_DB_NOUI_VERSION_MAJOR 0
#define SECMOD_DB_NOUI_VERSION_MINOR 3

#define SECMOD_PUTSHORT(dest,src) \
	(dest)[1] = (unsigned char) ((src)&0xff); \
	(dest)[0] = (unsigned char) (((src) >> 8) & 0xff);
#define SECMOD_PUTLONG(dest,src) \
	(dest)[3] = (unsigned char) ((src)&0xff); \
	(dest)[2] = (unsigned char) (((src) >> 8) & 0xff); \
	(dest)[1] = (unsigned char) (((src) >> 16) & 0xff); \
	(dest)[0] = (unsigned char) (((src) >> 24) & 0xff);
#define SECMOD_GETSHORT(src) \
	((unsigned short) (((src)[0] << 8) | (src)[1]))
#define SECMOD_GETLONG(src) \
	((unsigned long) (( (unsigned long) (src)[0] << 24) | \
			( (unsigned long) (src)[1] << 16)  | \
			( (unsigned long) (src)[2] << 8) | \
			(unsigned long) (src)[3]))
/*
 * build a data base entry from a module 
 */
static SECStatus secmod_EncodeData(DBT *data, SECMODModule * module) {
    secmodData *encoded;
    secmodSlotData *slot;
    unsigned char *dataPtr;
    unsigned short len, len2 = 0,count = 0;
    unsigned short offset;
    int dataLen, i, si;

    len = PORT_Strlen(module->commonName);
    if (module->dllName) {
    	len2 = PORT_Strlen(module->dllName);
    }
    if (module->slotCount != 0) {
	for (i=0; i < module->slotCount; i++) {
	    if (module->slots[i]->defaultFlags != 0) {
		count++;
	    }
	}
    } else {
	count = module->slotInfoCount;
    }
    dataLen = sizeof(secmodData) + len + len2 + 2 +
				 count*sizeof(secmodSlotData);

    data->data = (unsigned char *)
			PORT_Alloc(dataLen);
    encoded = (secmodData *)data->data;
    dataPtr = (unsigned char *) data->data;
    data->size = dataLen;

    if (encoded == NULL) return SECFailure;

    encoded->major = SECMOD_DB_VERSION_MAJOR;
    encoded->minor = SECMOD_DB_VERSION_MINOR;
    encoded->internal = (unsigned char) (module->internal ? 1 : 0);
    encoded->fips = (unsigned char) (module->isFIPS ? 1 : 0);
    SECMOD_PUTLONG(encoded->ssl,module->ssl[0]);
    SECMOD_PUTLONG(&encoded->ssl[4],module->ssl[1]);

    offset = (unsigned long) &(((secmodData *)0)->names[0]);
    SECMOD_PUTSHORT(encoded->nameStart,offset);
    offset = offset +len + len2 + 4;
    SECMOD_PUTSHORT(encoded->slotOffset,offset);


    SECMOD_PUTSHORT(&dataPtr[offset],count);
    slot = (secmodSlotData *)(dataPtr+offset+2);

    SECMOD_PUTSHORT(encoded->names,len);
    PORT_Memcpy(&encoded->names[2],module->commonName,len);


    SECMOD_PUTSHORT(&encoded->names[len+2],len2);
    if (len2) PORT_Memcpy(&encoded->names[len+4],module->dllName,len2);

    if (module->slotCount) {
      for (i=0,si=0; i < module->slotCount; i++) {
	if (module->slots[i]->defaultFlags) {
	    SECMOD_PUTLONG(slot[si].slotID, module->slots[i]->slotID);
	    SECMOD_PUTLONG(slot[si].defaultFlags,
					     module->slots[i]->defaultFlags);
	    SECMOD_PUTLONG(slot[si].timeout,module->slots[i]->timeout);
	    slot[si].askpw = module->slots[i]->askpw;
	    slot[si].hasRootCerts = module->slots[i]->hasRootCerts;
	    PORT_Memset(slot[si].reserved, 0, sizeof(slot[si].reserved));
	    si++;
	}
      }
    } else {
	for (i=0; i < module->slotInfoCount; i++) {
	    SECMOD_PUTLONG(slot[i].slotID, module->slotInfo[i].slotID);
	    SECMOD_PUTLONG(slot[i].defaultFlags,
					module->slotInfo[i].defaultFlags);
	    SECMOD_PUTLONG(slot[i].timeout,module->slotInfo[i].timeout);
	    slot[i].askpw = module->slotInfo[i].askpw;
	    slot[i].hasRootCerts = module->slotInfo[i].hasRootCerts;
	    PORT_Memset(slot[i].reserved, 0, sizeof(slot[i].reserved));
	}
    }

    return SECSuccess;

}

static void secmod_FreeData(DBT *data) {
    if (data->data) {
	PORT_Free(data->data);
    }
}


/*
 * build a module from the data base entry.
 */
static SECMODModule *secmod_DecodeData(DBT *data) {
    SECMODModule * module;
    secmodData *encoded;
    secmodSlotData *slots;
    unsigned char *names;
    unsigned short len,len1;
    unsigned long slotCount;
    unsigned short offset;
    PRBool isOldVersion  = PR_FALSE;
    int i;

    encoded = (secmodData *)data->data;
    names = (unsigned char *)data->data;
    offset = SECMOD_GETSHORT(encoded->slotOffset);
    slots = (secmodSlotData *) (names + offset + 2);
    slotCount = SECMOD_GETSHORT(names + offset);
    names += SECMOD_GETSHORT(encoded->nameStart);

    module = SECMOD_NewModule();
    if (module == NULL) return NULL;

    module->internal = (encoded->internal != 0) ? PR_TRUE: PR_FALSE;
    module->isFIPS = (encoded->fips != 0) ? PR_TRUE: PR_FALSE;
    len = SECMOD_GETSHORT(names);

    if (module->internal && (encoded->major == SECMOD_DB_NOUI_VERSION_MAJOR) &&
 	(encoded->minor <= SECMOD_DB_NOUI_VERSION_MINOR)) {
	isOldVersion = PR_TRUE;
    }

    /* decode the common name */
    module->commonName = (char*)PORT_ArenaAlloc(module->arena,len+1);
    if (module->commonName == NULL) {
	SECMOD_DestroyModule(module);
	return NULL;
    }
    PORT_Memcpy(module->commonName,&names[2],len);
    module->commonName[len] = 0;

    /* decode the DLL name */
    len1 = (names[len+2] << 8) | names[len+3];
    if (len1) {
	module->dllName = (char*)PORT_ArenaAlloc(module->arena,len1 + 1);
	if (module->dllName == NULL) {
	    SECMOD_DestroyModule(module);
	    return NULL;
	}
	PORT_Memcpy(module->dllName,&names[len+4],len1);
	module->dllName[len1] = 0;
    }

    module->slotInfoCount = slotCount;
    module->slotInfo = (PK11PreSlotInfo *) PORT_ArenaAlloc(module->arena,
				slotCount * sizeof(PK11PreSlotInfo));
    for (i=0; i < (int) slotCount; i++) {
	module->slotInfo[i].slotID = SECMOD_GETLONG(slots[i].slotID);
	module->slotInfo[i].defaultFlags = 
				SECMOD_GETLONG(slots[i].defaultFlags);
	if (isOldVersion && module->internal && 
				(module->slotInfo[i].slotID != 2)) {
		module->slotInfo[i].defaultFlags |= internalFlags;
	}
	module->slotInfo[i].timeout = SECMOD_GETLONG(slots[i].timeout);
	module->slotInfo[i].askpw = slots[i].askpw;
	module->slotInfo[i].hasRootCerts = slots[i].hasRootCerts;
	if (module->slotInfo[i].askpw == 0xff) {
	   module->slotInfo[i].askpw = -1;
	}
    }

    /* decode SSL cipher enable flags */
    module->ssl[0] = SECMOD_GETLONG(encoded->ssl);
    module->ssl[1] = SECMOD_GETLONG(&encoded->ssl[4]);

    return (module);
}

/*
 * open the PKCS #11 data base.
 */
static char *pkcs11dbName = NULL;
void SECMOD_InitDB(char *dbname) {
    pkcs11dbName = PORT_Strdup(dbname);
}


static DB *secmod_OpenDB(PRBool readOnly) {
    DB *pkcs11db = NULL;
    char *dbname;

    if (pkcs11dbName == NULL) return NULL;
    dbname = pkcs11dbName;
   
    /* I'm sure we should do more checks here sometime... */
    pkcs11db = dbopen(dbname, readOnly ? O_RDONLY : O_RDWR, 0600, DB_HASH, 0);

    /* didn't exist? create it */
    if (pkcs11db == NULL) {
	 if (readOnly) return NULL;

	 pkcs11db = dbopen( dbname,
                             O_RDWR | O_CREAT | O_TRUNC, 0600, DB_HASH, 0 );
	 if (pkcs11db) (* pkcs11db->sync)(pkcs11db, 0);
    }
    return pkcs11db;
}

static void secmod_CloseDB(DB *pkcs11db) {
     (*pkcs11db->close)(pkcs11db);
}

/*
 * Read all the existing modules in
 */
SECMODModuleList *
SECMOD_ReadPermDB(void) {
    DBT key,data;
    int ret;
    DB *pkcs11db = NULL;
    SECMODModuleList *newmod = NULL,*mod = NULL;

    pkcs11db = secmod_OpenDB(PR_TRUE);
    if (pkcs11db == NULL) {
	return NULL;
    }

    /* read and parse the file or data base */
    ret = (*pkcs11db->seq)(pkcs11db, &key, &data, R_FIRST);
    if (ret)  goto done;

    do {
        /* allocate space for modules */
	newmod = SECMOD_NewModuleListElement();
	if (newmod == NULL) break;
	newmod->module = secmod_DecodeData(&data);
	if (newmod->module == NULL) {
	    SECMOD_DestroyModuleListElement(newmod);
	    break;
	}
	newmod->next = mod;
	mod = newmod;
    } while ( (*pkcs11db->seq)(pkcs11db, &key, &data, R_NEXT) == 0);

done:
    secmod_CloseDB(pkcs11db);
    return mod;
}

/*
 * Delete a module from the Data Base
 */
SECStatus
SECMOD_DeletePermDB(SECMODModule * module) {
    DBT key;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;

    /* make sure we have a db handle */
    pkcs11db = secmod_OpenDB(PR_FALSE);
    if (pkcs11db == NULL) {
	return SECFailure;
    }

    rv = secmod_MakeKey(&key,module);
    if (rv != SECSuccess) goto done;
    rv = SECFailure;
    ret = (*pkcs11db->del)(pkcs11db, &key, 0);
    secmod_FreeKey(&key);
    if (ret != 0) goto done;


    ret = (*pkcs11db->sync)(pkcs11db, 0);
    if (ret == 0) rv = SECSuccess;

done:
    secmod_CloseDB(pkcs11db);
    return rv;
}

/*
 * Add a module to the Data base 
 */
SECStatus
SECMOD_AddPermDB(SECMODModule *module) {
    DBT key,data;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;

    /* make sure we have a db handle */
    pkcs11db = secmod_OpenDB(PR_FALSE);
    if (pkcs11db == NULL) {
	return SECFailure;
    }

    rv = secmod_MakeKey(&key,module);
    if (rv != SECSuccess) goto done;
    rv = secmod_EncodeData(&data,module);
    if (rv != SECSuccess) {
	secmod_FreeKey(&key);
	goto done;
    }
    rv = SECFailure;
    ret = (*pkcs11db->put)(pkcs11db, &key, &data, 0);
    secmod_FreeKey(&key);
    secmod_FreeData(&data);
    if (ret != 0) goto done;

    ret = (*pkcs11db->sync)(pkcs11db, 0);
    if (ret == 0) rv = SECSuccess;

done:
    secmod_CloseDB(pkcs11db);
    return rv;
}
