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

#include "pk11pars.h"
#include "pkcs11i.h"

#define FREE_CLEAR(p) if (p) { PORT_Free(p); p = NULL; }

static void
secmod_parseFlags(char *tmp, pk11_parameters *parsed) { 
    parsed->readOnly = pk11_argHasFlag("flags","readOnly",tmp);
    parsed->noCertDB = pk11_argHasFlag("flags","noCertDB",tmp);
    parsed->noModDB = pk11_argHasFlag("flags","noModDB",tmp);
    parsed->forceOpen = pk11_argHasFlag("flags","forceOpen",tmp);
    parsed->pwRequired = pk11_argHasFlag("flags","passwordRequired",tmp);
    return;
}



CK_RV
secmod_parseParameters(char *param, pk11_parameters *parsed) 
{
    int next;
    char *tmp;
    char *index;
    index = pk11_argStrip(param);

    PORT_Memset(parsed, 0, sizeof(pk11_parameters));

    while (*index) {
	PK11_HANDLE_STRING_ARG(index,parsed->configdir,"configdir=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->certPrefix,"certprefix=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->keyPrefix,"keyprefix=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->secmodName,"secmod=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->man,"manufactureID=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->libdes,"libraryDescription=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->tokdes,"cryptoTokenDescription=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->ptokdes,"dbTokenDescription=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->slotdes,"cryptoSlotDescription=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->pslotdes,"dbSlotDescription=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->fslotdes,"FIPSSlotDescription=",;)
	PK11_HANDLE_STRING_ARG(index,parsed->fpslotdes,"FIPSTokenDescription=",;)
	PK11_HANDLE_STRING_ARG(index,tmp,"minPWLen=", 
			if(tmp) { parsed->minPW=atoi(tmp); PORT_Free(tmp); })
	PK11_HANDLE_STRING_ARG(index,tmp,"flags=", 
		if(tmp) { secmod_parseFlags(param,parsed); PORT_Free(tmp); })
	PK11_HANDLE_FINAL_ARG(index)
   }
   return CKR_OK;
}

void
secmod_freeParams(pk11_parameters *params)
{
    FREE_CLEAR(params->configdir);
    FREE_CLEAR(params->certPrefix);
    FREE_CLEAR(params->keyPrefix);
    FREE_CLEAR(params->secmodName);
    FREE_CLEAR(params->man);
    FREE_CLEAR(params->libdes); 
    FREE_CLEAR(params->tokdes);
    FREE_CLEAR(params->ptokdes);
    FREE_CLEAR(params->slotdes);
    FREE_CLEAR(params->pslotdes);
    FREE_CLEAR(params->fslotdes);
    FREE_CLEAR(params->fpslotdes);
}


char *
secmod_getSecmodName(char *param, PRBool *rw)
{
    int next;
    char *configdir = NULL;
    char *secmodName = NULL;
    char *value = NULL;
    char *save_params = param;
    param = pk11_argStrip(param);
	

    while (*param) {
	PK11_HANDLE_STRING_ARG(param,configdir,"configdir=",;)
	PK11_HANDLE_STRING_ARG(param,secmodName,"secmod=",;)
	PK11_HANDLE_FINAL_ARG(param)
   }

   *rw = PR_TRUE;
   if (pk11_argHasFlag("flags","readOnly",save_params) ||
	pk11_argHasFlag("flags","noModDB",save_params)) *rw = PR_FALSE;

   if (!secmodName || *secmodName == '\0') secmodName = PORT_Strdup(SECMOD_DB);

   if (configdir) {
	value = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,secmodName);
   } else {
	value = PORT_Strdup(secmodName);
   }
   PORT_Free(secmodName);
   if (configdir) PORT_Free(configdir);
   return value;
}

/* Construct a database key for a given module */
static SECStatus secmod_MakeKey(DBT *key, char * module) {
    int len = 0;
    char *commonName;

    commonName = pk11_argGetParamValue("name",module);
    if (commonName == NULL) {
	commonName = pk11_argGetParamValue("library",module);
    }
    if (commonName == NULL) return SECFailure;
    len = PORT_Strlen(commonName);
    key->data = commonName;
    key->size = len;
    return SECSuccess;
}

/* free out constructed database key */
static void secmod_FreeKey(DBT *key) {
    if (key->data) {
	PORT_Free(key->data);
    }
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
    unsigned char trustOrder[4];
    unsigned char cipherOrder[4];
    unsigned char reserved1;
    unsigned char isModuleDB;
    unsigned char isModuleDBOnly;
    unsigned char isCritical;
    unsigned char reserved[4];
    unsigned char names[6];	/* enough space for the length fields */
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
#define SECMOD_DB_VERSION_MINOR 5
#define SECMOD_DB_EXT1_VERSION_MAJOR 0
#define SECMOD_DB_EXT1_VERSION_MINOR 5
#define SECMOD_DB_NOUI_VERSION_MAJOR 0
#define SECMOD_DB_NOUI_VERSION_MINOR 4

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
static SECStatus secmod_EncodeData(DBT *data, char * module) {
    secmodData *encoded = NULL;
    secmodSlotData *slot;
    unsigned char *dataPtr;
    unsigned short len, len2 = 0, len3 = 0;
    int count = 0;
    unsigned short offset;
    int dataLen, i;
    unsigned long order;
    unsigned long  ssl[2];
    char *commonName = NULL , *dllName = NULL, *param = NULL, *nss = NULL;
    char *slotParams, *ciphers;
    PK11PreSlotInfo *slotInfo = NULL;
    SECStatus rv = SECFailure;

    rv = pk11_argParseModuleSpec(module,&dllName,&commonName,&param,&nss);
    if (rv != SECSuccess) return rv;
    rv = SECFailure;

    if (commonName == NULL) {
	/* set error */
	goto loser;
    }

    len = PORT_Strlen(commonName);
    if (dllName) {
    	len2 = PORT_Strlen(dllName);
    }
    if (param) {
	len3 = PORT_Strlen(param);
    }

    slotParams = pk11_argGetParamValue("slotParams",nss); 
    slotInfo = pk11_argParseSlotInfo(NULL,slotParams,&count);
    if (slotParams) PORT_Free(slotParams);

    if (count && slotInfo == NULL) {
	/* set error */
	goto loser;
    }

    dataLen = sizeof(secmodData) + len + len2 + len3 + sizeof(unsigned short) +
				 count*sizeof(secmodSlotData);

    data->data = (unsigned char *) PORT_ZAlloc(dataLen);
    encoded = (secmodData *)data->data;
    dataPtr = (unsigned char *) data->data;
    data->size = dataLen;

    if (encoded == NULL) {
	/* set error */
	goto loser;
    }

    encoded->major = SECMOD_DB_VERSION_MAJOR;
    encoded->minor = SECMOD_DB_VERSION_MINOR;
    encoded->internal = (unsigned char) 
			(pk11_argHasFlag("flags","internal",nss) ? 1 : 0);
    encoded->fips = (unsigned char) 
			(pk11_argHasFlag("flags","FIPS",nss) ? 1 : 0);
    encoded->isModuleDB = (unsigned char) 
			(pk11_argHasFlag("flags","isModuleDB",nss) ? 1 : 0);
    encoded->isModuleDBOnly = (unsigned char) 
			(pk11_argHasFlag("flags","isModuleDBOnly",nss) ? 1 : 0);
    encoded->isCritical = (unsigned char) 
			(pk11_argHasFlag("flags","critical",nss) ? 1 : 0);

    order = pk11_argReadLong("trustOrder",nss);
    SECMOD_PUTLONG(encoded->trustOrder,order);
    order = pk11_argReadLong("cipherOrder",nss);
    SECMOD_PUTLONG(encoded->cipherOrder,order);

   
    ciphers = pk11_argGetParamValue("ciphers",nss); 
    pk11_argSetNewCipherFlags(&ssl[0], ciphers);
    SECMOD_PUTLONG(encoded->ssl,ssl[0]);
    SECMOD_PUTLONG(&encoded->ssl[4],ssl[1]);

    offset = (unsigned long) &(((secmodData *)0)->names[0]);
    SECMOD_PUTSHORT(encoded->nameStart,offset);
    offset = offset + len + len2 + len3 + 3*sizeof(unsigned short);
    SECMOD_PUTSHORT(encoded->slotOffset,offset);


    SECMOD_PUTSHORT(&dataPtr[offset],((unsigned short)count));
    slot = (secmodSlotData *)(dataPtr+offset+sizeof(unsigned short));

    offset = 0;
    SECMOD_PUTSHORT(encoded->names,len);
    offset += sizeof(unsigned short);
    PORT_Memcpy(&encoded->names[offset],commonName,len);
    offset += len;


    SECMOD_PUTSHORT(&encoded->names[offset],len2);
    offset += sizeof(unsigned short);
    if (len2) PORT_Memcpy(&encoded->names[offset],dllName,len2);
    offset += len2;

    SECMOD_PUTSHORT(&encoded->names[offset],len3);
    offset += sizeof(unsigned short);
    if (len3) PORT_Memcpy(&encoded->names[offset],param,len3);
    offset += len3;

    if (count) {
	for (i=0; i < count; i++) {
	    SECMOD_PUTLONG(slot[i].slotID, slotInfo[i].slotID);
	    SECMOD_PUTLONG(slot[i].defaultFlags,
					slotInfo[i].defaultFlags);
	    SECMOD_PUTLONG(slot[i].timeout,slotInfo[i].timeout);
	    slot[i].askpw = slotInfo[i].askpw;
	    slot[i].hasRootCerts = slotInfo[i].hasRootCerts;
	    PORT_Memset(slot[i].reserved, 0, sizeof(slot[i].reserved));
	}
    }
    rv = SECSuccess;

loser:
    if (commonName) PORT_Free(commonName);
    if (dllName) PORT_Free(dllName);
    if (param) PORT_Free(param);
    if (slotInfo) PORT_Free(slotInfo);
    return rv;

}

static void 
secmod_FreeData(DBT *data)
{
    if (data->data) {
	PORT_Free(data->data);
    }
}

/*
 * build a module from the data base entry.
 */
static char *
secmod_DecodeData(char *defParams, DBT *data, PRBool *retInternal)
{
    secmodData *encoded;
    secmodSlotData *slots;
    char *commonName = NULL,*dllName = NULL,*parameters = NULL;
    unsigned char *names;
    unsigned short len;
    unsigned long slotCount;
    unsigned short offset;
    PRBool isOldVersion  = PR_FALSE;
    PRBool internal, isFIPS, isModuleDB=PR_FALSE, isModuleDBOnly=PR_FALSE;
    PRBool extended=PR_FALSE;
    PRBool hasRootCerts=PR_FALSE,hasRootTrust=PR_FALSE;
    unsigned long trustOrder=0, cipherOrder=0;
    unsigned long ssl0=0, ssl1=0;
    char **slotStrings = NULL;
    unsigned long slotID,defaultFlags,timeout;
    char *nss,*moduleSpec;
    int i;

    PLArenaPool *arena;

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL) return NULL;

    encoded = (secmodData *)data->data;
    names = (unsigned char *)data->data;
    offset = SECMOD_GETSHORT(encoded->slotOffset);
    slots = (secmodSlotData *) (names + offset + 2);
    slotCount = SECMOD_GETSHORT(names + offset);
    names += SECMOD_GETSHORT(encoded->nameStart);

    * retInternal = internal = (encoded->internal != 0) ? PR_TRUE: PR_FALSE;
    isFIPS = (encoded->fips != 0) ? PR_TRUE: PR_FALSE;
    len = SECMOD_GETSHORT(names);

    if (internal && (encoded->major == SECMOD_DB_NOUI_VERSION_MAJOR) &&
 	(encoded->minor <= SECMOD_DB_NOUI_VERSION_MINOR)) {
	isOldVersion = PR_TRUE;
    }

    if ((encoded->major == SECMOD_DB_EXT1_VERSION_MAJOR) &&
	(encoded->minor >= SECMOD_DB_EXT1_VERSION_MINOR)) {
	trustOrder = SECMOD_GETLONG(encoded->trustOrder);
	cipherOrder = SECMOD_GETLONG(encoded->cipherOrder);
	isModuleDB = (encoded->isModuleDB != 0) ? PR_TRUE: PR_FALSE;
	isModuleDBOnly = (encoded->isModuleDBOnly != 0) ? PR_TRUE: PR_FALSE;
	extended = PR_TRUE;
    } 

    /* decode the common name */
    commonName = (char*)PORT_ArenaAlloc(arena,len+1);
    if (commonName == NULL) {
	PORT_FreeArena(arena,PR_TRUE);
	return NULL;
    }
    PORT_Memcpy(commonName,&names[2],len);
    commonName[len] = 0;

    /* decode the DLL name */
    names += len+2;
    len = SECMOD_GETSHORT(names);
    if (len) {
	dllName = (char*)PORT_ArenaAlloc(arena,len + 1);
	if (dllName == NULL) {
	    PORT_FreeArena(arena,PR_TRUE);
	    return NULL;
	}
	PORT_Memcpy(dllName,&names[2],len);
	dllName[len] = 0;
    }
    if (!internal && extended) {
	names += len+2;
	len = SECMOD_GETSHORT(names);
	if (len) {
	    parameters = (char*)PORT_ArenaAlloc(arena,len + 1);
	    if (parameters == NULL) {
		PORT_FreeArena(arena,PR_TRUE);
		return NULL;
	    }
	    PORT_Memcpy(parameters,&names[2],len);
	    parameters[len] = 0;
	}
    }
    if (internal) {
	parameters = PORT_ArenaStrdup(arena,defParams);
    }

    /* decode SSL cipher enable flags */
    ssl0 = SECMOD_GETLONG(encoded->ssl);
    ssl1 = SECMOD_GETLONG(&encoded->ssl[4]);

    /*  slotCount; */
    slotStrings = (char **)PORT_ArenaAlloc(arena, slotCount * sizeof(char *));
    for (i=0; i < (int) slotCount; i++) {
	slotID = SECMOD_GETLONG(slots[i].slotID);
	defaultFlags = SECMOD_GETLONG(slots[i].defaultFlags);
	if (isOldVersion && internal && (slotID != 2)) {
		unsigned long internalFlags=
			pk11_argSlotFlags("slotFlags",SECMOD_SLOT_FLAGS);
		defaultFlags |= internalFlags;
	}
	timeout = SECMOD_GETLONG(slots[i].timeout);
	hasRootCerts = slots[i].hasRootCerts;
	if (hasRootCerts && !extended) {
	    trustOrder = 20;
	}

        slotStrings[i] = pk11_mkSlotString(slotID,defaultFlags,
			timeout,slots[i].askpw,hasRootCerts,hasRootTrust);
    }

    nss = pk11_mkNSS(slotStrings, slotCount, internal, isFIPS, isModuleDB, 
	isModuleDBOnly, internal, trustOrder, cipherOrder, ssl0, ssl1);
    moduleSpec = pk11_mkNewModuleSpec(dllName,commonName,parameters,nss);
    PR_smprintf_free(nss);
    PORT_FreeArena(arena,PR_TRUE);

    return (moduleSpec);
}



static DB *secmod_OpenDB(char *dbName, PRBool readOnly) {
    DB *pkcs11db = NULL;
   
    /* I'm sure we should do more checks here sometime... */
    pkcs11db = dbopen(dbName, readOnly ? O_RDONLY : O_RDWR, 0600, DB_HASH, 0);

    /* didn't exist? create it */
    if (pkcs11db == NULL) {
	 if (readOnly) return NULL;

	 pkcs11db = dbopen( dbName,
                             O_RDWR | O_CREAT | O_TRUNC, 0600, DB_HASH, 0 );
	 if (pkcs11db) (* pkcs11db->sync)(pkcs11db, 0);
    }
    return pkcs11db;
}

static void secmod_CloseDB(DB *pkcs11db) {
     (*pkcs11db->close)(pkcs11db);
}


#define SECMOD_STEP 10
#define PK11_DEFAULT_INTERNAL_INIT "library= name=\"NSS Internal PKCS #11 Module\" parameters=\"%s\" NSS=\"Flags=internal,critical slotParams=(1={%s askpw=any timeout=30})\""
/*
 * Read all the existing modules in
 */
char **
secmod_ReadPermDB(char *dbname, char *params, PRBool rw) {
    DBT key,data;
    int ret;
    DB *pkcs11db = NULL;
    char **moduleList = NULL;
    int moduleCount = 1;
    int useCount = SECMOD_STEP;

    moduleList = (char **) PORT_ZAlloc(useCount*sizeof(char **));
    if (moduleList == NULL) return NULL;

    pkcs11db = secmod_OpenDB(dbname,PR_TRUE);
    if (pkcs11db == NULL) goto done;

    /* read and parse the file or data base */
    ret = (*pkcs11db->seq)(pkcs11db, &key, &data, R_FIRST);
    if (ret)  goto done;


    do {
	char *moduleString;
	PRBool internal = PR_FALSE;
	if ((moduleCount+1) >= useCount) {
	    useCount += SECMOD_STEP;
	    moduleList = 
		(char **)PORT_Realloc(moduleList,useCount*sizeof(char *));
	    if (moduleList == NULL) goto done;
	    PORT_Memset(&moduleList[moduleCount+1],0,
						sizeof(char *)*SECMOD_STEP);
	}
	moduleString = secmod_DecodeData(params,&data,&internal);
	if (internal) {
	    moduleList[0] = moduleString;
	} else {
	    moduleList[moduleCount] = moduleString;
	    moduleCount++;
	}
    } while ( (*pkcs11db->seq)(pkcs11db, &key, &data, R_NEXT) == 0);

done:
    if (!moduleList[0]) {
	moduleList[0] = PR_smprintf(PK11_DEFAULT_INTERNAL_INIT,params,
						SECMOD_SLOT_FLAGS);
    }
    /* deal with trust cert db here */

    if (pkcs11db) {
	secmod_CloseDB(pkcs11db);
    } else {
	secmod_AddPermDB(dbname,moduleList[0], rw) ;
    }
    return moduleList;
}

/*
 * Delete a module from the Data Base
 */
SECStatus
secmod_DeletePermDB(char *dbname, char *args, PRBool rw) {
    DBT key;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;

    if (!rw) return SECFailure;

    /* make sure we have a db handle */
    pkcs11db = secmod_OpenDB(dbname,PR_FALSE);
    if (pkcs11db == NULL) {
	return SECFailure;
    }

    rv = secmod_MakeKey(&key,args);
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
secmod_AddPermDB(char *dbname, char *module, PRBool rw) {
    DBT key,data;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;


    if (!rw) return SECFailure;

    /* make sure we have a db handle */
    pkcs11db = secmod_OpenDB(dbname,PR_FALSE);
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
