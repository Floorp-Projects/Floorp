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

#ifdef macintosh
#define PATH_SEPARATOR ":"
#define SECMOD_DB "Security Modules"
#else
#define PATH_SEPARATOR "/"
#define SECMOD_DB "secmod.db"
#endif


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
    param = pk11_argStrip(param);
	

    while (*param) {
	PK11_HANDLE_STRING_ARG(param,configdir,"configdir",;)
	PK11_HANDLE_STRING_ARG(param,secmodName,"secmod",;)
	PK11_HANDLE_FINAL_ARG(param)
   }

   *rw = PR_TRUE;
   if (pk11_argHasFlag("flags","readOnly",param) ||
	pk11_argHasFlag("flags","noModDB",param)) *rw = PR_FALSE;

   if (secmodName == NULL) secmodName = PORT_Strdup(SECMOD_DB);

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
    unsigned char hasParameters;
    unsigned char isModuleDB;
    unsigned char isModuleDBOnly;
    unsigned char reserved;
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
#define SECMOD_DB_VERSION_MINOR 5
#define SECMOD_DB_EXT1_VERSION_MAJOR 0
#define SECMOD_DB_EXT1_VERSION_MINOR 5
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

#ifdef notdef
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
#endif

static void 
secmod_FreeData(DBT *data)
{
    if (data->data) {
	PORT_Free(data->data);
    }
}

static unsigned long internalFlags = SECMOD_RSA_FLAG|SECMOD_DSA_FLAG|
	SECMOD_RC2_FLAG| SECMOD_RC4_FLAG|SECMOD_DES_FLAG|SECMOD_RANDOM_FLAG|
	SECMOD_SHA1_FLAG|SECMOD_MD5_FLAG|SECMOD_MD2_FLAG|SECMOD_SSL_FLAG|
	SECMOD_TLS_FLAG|SECMOD_AES_FLAG;

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
    PRBool hasParameters=PR_FALSE,extended=PR_FALSE;
    PRBool hasRootCerts=PR_FALSE,hasRootTrust=PR_FALSE;
    unsigned long trustOrder=0, cipherOrder=0;
    unsigned long ssl0=0, ssl1=0;
    char **slotInfo = NULL;
    unsigned long slotID,defaultFlags,timeout;
    char *askpw,*flags,*rootFlags,*slotStrings,*nssFlags,*ciphers,*nss,*params;
    int i,slotLen;

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
	hasParameters = (encoded->hasParameters != 0) ? PR_TRUE: PR_FALSE;
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
    if (!internal && hasParameters) {
	names += len+2;
	len = SECMOD_GETSHORT(names);
	parameters = (char*)PORT_ArenaAlloc(arena,len + 1);
	if (parameters == NULL) {
	    PORT_FreeArena(arena,PR_TRUE);
	    return NULL;
	}
	PORT_Memcpy(parameters,&names[2],len);
	parameters[len] = 0;
    }
    if (internal) {
	parameters = PORT_ArenaStrdup(arena,defParams);
    }

    /* decode SSL cipher enable flags */
    ssl0 = SECMOD_GETLONG(encoded->ssl);
    ssl1 = SECMOD_GETLONG(&encoded->ssl[4]);

    /*  slotCount; */
    slotInfo = (char **)PORT_ArenaAlloc(arena, slotCount * sizeof(char *));
    for (i=0; i < (int) slotCount; i++) {
	slotID = SECMOD_GETLONG(slots[i].slotID);
	defaultFlags = SECMOD_GETLONG(slots[i].defaultFlags);
	if (isOldVersion && internal && (slotID != 2)) {
		defaultFlags |= internalFlags;
	}
	timeout = SECMOD_GETLONG(slots[i].timeout);
	switch (slots[i].askpw) {
	case 0xff:
		askpw = "every";
		break;
	case 1:
		askpw = "timeout";
		break;
	default:
		askpw = "any";
		break;
	}
	hasRootCerts = slots[i].hasRootCerts;
	if (hasRootCerts && !extended) {
	    trustOrder = 20;
	}
	flags = pk11_makeSlotFlags(arena,defaultFlags);
	rootFlags = pk11_makeRootFlags(arena,hasRootCerts,hasRootTrust);
	slotInfo[i] = PR_smprintf("0x%08x=[slotFlags=%s askpw=%s timeout=%d rootFlags=%s]",slotID,flags,askpw,timeout,rootFlags);
    }

    /* now let's build up the string
     * first the slot infos
     */
    slotLen=0;
    for (i=0; i < (int)slotCount; i++) {
	slotLen += strlen(slotInfo[i])+1;
    }

    slotStrings = (char *)PORT_ArenaAlloc(arena,slotLen);
    PORT_Memset(slotStrings,0,slotLen);
    for (i=0; i < (int)slotCount; i++) {
	PORT_Strcat(slotStrings,slotInfo[i]);
	PORT_Strcat(slotStrings," ");
	PR_smprintf_free(slotInfo[i]);
	slotInfo[i]=NULL;
    }
    
    /*
     * now the NSS structure
     */
    nssFlags = pk11_makeNSSFlags(arena,internal,isFIPS,isModuleDB,
	isModuleDBOnly,internal); 
	/* for now only the internal module is critical */
    ciphers = pk11_makeCipherFlags(arena, ssl0, ssl1);
    nss = PR_smprintf("NSS=\"trustOrder=%d cipherOrder=%d Flags='%s' slotParams={%s} ciphers='%s'\"",trustOrder,cipherOrder,nssFlags,slotStrings,ciphers);

    /*
     * now the final spec
     */
    if (hasParameters) {
	params = PR_smprintf("library=\"%s\" name=\"%s\" parameters=\"%s\" NSS=\"%s\"",dllName,commonName,parameters,nss);
    } else {
	params = PR_smprintf("library=\"%s\" name=\"%s\" NSS=\"%s\"",dllName,commonName,nss);
    }
    PR_smprintf_free(nss);
    PORT_FreeArena(arena,PR_TRUE);

    return (params);
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
#define PK11_DEFAULT_INTERNAL_INIT "library= name=\"NSS Internal PKCS #11 Module\" parameters=\"%s\" NSS=\"Flags=internal,critical slotParams=(1={slotFlags=all askpw=any timeout=30})\""
/*
 * Read all the existing modules in
 */
char **
SECMOD_ReadPermDB(char *dbname, char *params, PRBool rw) {
    DBT key,data;
    int ret;
    DB *pkcs11db = NULL;
    char **moduleList = NULL;
    int moduleCount = 1;
    int useCount = SECMOD_STEP;

    moduleList = (char **) PORT_Alloc(useCount*sizeof(char **));
    if (moduleList == NULL) return NULL;

    pkcs11db = secmod_OpenDB(dbname,PR_TRUE);
    if (pkcs11db == NULL) goto done;

    /* read and parse the file or data base */
    ret = (*pkcs11db->seq)(pkcs11db, &key, &data, R_FIRST);
    if (ret)  goto done;


    do {
	char *moduleString;
	PRBool internal = PR_FALSE;
	if (moduleCount >= useCount) {
	    useCount += SECMOD_STEP;
	    moduleList = 
		(char **)PORT_Realloc(moduleList,useCount*sizeof(char **));
	    if (moduleList == NULL) goto done;
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
	moduleList[0] = PR_smprintf(PK11_DEFAULT_INTERNAL_INIT,params);
    }
    /* deal with trust cert db here */

    if (pkcs11db) {
	secmod_CloseDB(pkcs11db);
    } else {
	SECMOD_AddPermDB(dbname,moduleList[0], rw) ;
    }
    return moduleList;
}

/*
 * Delete a module from the Data Base
 */
SECStatus
SECMOD_DeletePermDB(char *dbname, char *args, PRBool rw) {
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
SECMOD_AddPermDB(char *dbname, char *module, PRBool rw) {
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
#ifdef notdef
    rv = secmod_EncodeData(&data,module);
    if (rv != SECSuccess) {
	secmod_FreeKey(&key);
	goto done;
    }
#endif
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
