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
 *  The following code handles the storage of PKCS 11 modules used by the
 * NSS. This file is written to abstract away how the modules are
 * stored so we can deside that later.
 */

#include "pk11pars.h"
#include "lgdb.h"
#include "mcom_db.h"
#include "secerr.h"

#define FREE_CLEAR(p) if (p) { PORT_Free(p); p = NULL; }

/* Construct a database key for a given module */
static SECStatus secmod_MakeKey(DBT *key, char * module) {
    int len = 0;
    char *commonName;

    commonName = secmod_argGetParamValue("name",module);
    if (commonName == NULL) {
	commonName = secmod_argGetParamValue("library",module);
    }
    if (commonName == NULL) return SECFailure;
    len = PORT_Strlen(commonName);
    key->data = commonName;
    key->size = len;
    return SECSuccess;
}

/* free out constructed database key */
static void 
secmod_FreeKey(DBT *key) 
{
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
#define SECMOD_DB_VERSION_MINOR 6
#define SECMOD_DB_EXT1_VERSION_MAJOR 0
#define SECMOD_DB_EXT1_VERSION_MINOR 6
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
static SECStatus 
secmod_EncodeData(DBT *data, char * module) 
{
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

    rv = secmod_argParseModuleSpec(module,&dllName,&commonName,&param,&nss);
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

    slotParams = secmod_argGetParamValue("slotParams",nss); 
    slotInfo = secmod_argParseSlotInfo(NULL,slotParams,&count);
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
			(secmod_argHasFlag("flags","internal",nss) ? 1 : 0);
    encoded->fips = (unsigned char) 
			(secmod_argHasFlag("flags","FIPS",nss) ? 1 : 0);
    encoded->isModuleDB = (unsigned char) 
			(secmod_argHasFlag("flags","isModuleDB",nss) ? 1 : 0);
    encoded->isModuleDBOnly = (unsigned char) 
		    (secmod_argHasFlag("flags","isModuleDBOnly",nss) ? 1 : 0);
    encoded->isCritical = (unsigned char) 
			(secmod_argHasFlag("flags","critical",nss) ? 1 : 0);

    order = secmod_argReadLong("trustOrder", nss, SECMOD_DEFAULT_TRUST_ORDER, 
                               NULL);
    SECMOD_PUTLONG(encoded->trustOrder,order);
    order = secmod_argReadLong("cipherOrder", nss, SECMOD_DEFAULT_CIPHER_ORDER, 
                               NULL);
    SECMOD_PUTLONG(encoded->cipherOrder,order);

   
    ciphers = secmod_argGetParamValue("ciphers",nss); 
    secmod_argSetNewCipherFlags(&ssl[0], ciphers);
    SECMOD_PUTLONG(encoded->ssl,ssl[0]);
    SECMOD_PUTLONG(&encoded->ssl[4],ssl[1]);
    if (ciphers) PORT_Free(ciphers);

    offset = (unsigned short) offsetof(secmodData, names);
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
    if (nss) PORT_Free(nss);
    return rv;

}

static void 
secmod_FreeData(DBT *data)
{
    if (data->data) {
	PORT_Free(data->data);
    }
}

static void
secmod_FreeSlotStrings(char **slotStrings, int count)
{
    int i;

    for (i=0; i < count; i++) {
	if (slotStrings[i]) {
	    PR_smprintf_free(slotStrings[i]);
	    slotStrings[i] = NULL;
	}
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
    PLArenaPool *arena;
    char *commonName 		= NULL;
    char *dllName    		= NULL;
    char *parameters 		= NULL;
    char *nss;
    char *moduleSpec;
    char **slotStrings 		= NULL;
    unsigned char *names;
    unsigned long slotCount;
    unsigned long ssl0		=0;
    unsigned long ssl1		=0;
    unsigned long slotID;
    unsigned long defaultFlags;
    unsigned long timeout;
    unsigned long trustOrder	=SECMOD_DEFAULT_TRUST_ORDER;
    unsigned long cipherOrder	=SECMOD_DEFAULT_CIPHER_ORDER;
    unsigned short len;
    unsigned short namesOffset  = 0;	/* start of the names block */
    unsigned long namesRunningOffset;	/* offset to name we are 
					 * currently processing */
    unsigned short slotOffset;
    PRBool isOldVersion  	= PR_FALSE;
    PRBool internal;
    PRBool isFIPS;
    PRBool isModuleDB    	=PR_FALSE;
    PRBool isModuleDBOnly	=PR_FALSE;
    PRBool extended      	=PR_FALSE;
    int i;


    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL) 
    	return NULL;

#define CHECK_SIZE(x) \
    if ((unsigned int) data->size < (unsigned int)(x)) goto db_loser

    /* -------------------------------------------------------------
    ** Process the buffer header, which is the secmodData struct. 
    ** It may be an old or new version.  Check the length for each. 
    */

    CHECK_SIZE( offsetof(secmodData, trustOrder[0]) );

    encoded = (secmodData *)data->data;

    internal = (encoded->internal != 0) ? PR_TRUE: PR_FALSE;
    isFIPS   = (encoded->fips     != 0) ? PR_TRUE: PR_FALSE;

    if (retInternal)
	*retInternal = internal;
    if (internal) {
	parameters = PORT_ArenaStrdup(arena,defParams);
	if (parameters == NULL) 
	    goto loser;
    }
    if (internal && (encoded->major == SECMOD_DB_NOUI_VERSION_MAJOR) &&
 	(encoded->minor <= SECMOD_DB_NOUI_VERSION_MINOR)) {
	isOldVersion = PR_TRUE;
    }
    if ((encoded->major == SECMOD_DB_EXT1_VERSION_MAJOR) &&
	(encoded->minor >= SECMOD_DB_EXT1_VERSION_MINOR)) {
	CHECK_SIZE( sizeof(secmodData));
	trustOrder     = SECMOD_GETLONG(encoded->trustOrder);
	cipherOrder    = SECMOD_GETLONG(encoded->cipherOrder);
	isModuleDB     = (encoded->isModuleDB != 0) ? PR_TRUE: PR_FALSE;
	isModuleDBOnly = (encoded->isModuleDBOnly != 0) ? PR_TRUE: PR_FALSE;
	extended       = PR_TRUE;
    } 
    if (internal && !extended) {
	trustOrder = 0;
	cipherOrder = 100;
    }
    /* decode SSL cipher enable flags */
    ssl0 = SECMOD_GETLONG(encoded->ssl);
    ssl1 = SECMOD_GETLONG(encoded->ssl + 4);

    slotOffset  = SECMOD_GETSHORT(encoded->slotOffset);
    namesOffset = SECMOD_GETSHORT(encoded->nameStart);


    /*--------------------------------------------------------------
    ** Now process the variable length set of names.                
    ** The names have this structure:
    ** struct {
    **     BYTE  commonNameLen[ 2 ];
    **     BYTE  commonName   [ commonNameLen ];
    **     BTTE  libNameLen   [ 2 ];
    **     BYTE  libName      [ libNameLen ];
    ** If it is "extended" it also has these members:
    **     BYTE  initStringLen[ 2 ];
    **     BYTE  initString   [ initStringLen ];
    ** }
    */

    namesRunningOffset = namesOffset;
    /* copy the module's common name */
    CHECK_SIZE( namesRunningOffset + 2);
    names = (unsigned char *)data->data;
    len   = SECMOD_GETSHORT(names+namesRunningOffset);

    CHECK_SIZE( namesRunningOffset + 2 + len);
    commonName = (char*)PORT_ArenaAlloc(arena,len+1);
    if (commonName == NULL) 
	goto loser;
    PORT_Memcpy(commonName, names + namesRunningOffset + 2, len);
    commonName[len] = 0;
    namesRunningOffset += len + 2;

    /* copy the module's shared library file name. */
    CHECK_SIZE( namesRunningOffset + 2);
    len = SECMOD_GETSHORT(names + namesRunningOffset);
    if (len) {
	CHECK_SIZE( namesRunningOffset + 2 + len);
	dllName = (char*)PORT_ArenaAlloc(arena,len + 1);
	if (dllName == NULL) 
	    goto loser;
	PORT_Memcpy(dllName, names + namesRunningOffset + 2, len);
	dllName[len] = 0;
    }
    namesRunningOffset += len + 2;

    /* copy the module's initialization string, if present. */
    if (!internal && extended) {
	CHECK_SIZE( namesRunningOffset + 2);
	len = SECMOD_GETSHORT(names+namesRunningOffset);
	if (len) {
	    CHECK_SIZE( namesRunningOffset + 2 + len );
	    parameters = (char*)PORT_ArenaAlloc(arena,len + 1);
	    if (parameters == NULL) 
		goto loser;
	    PORT_Memcpy(parameters,names + namesRunningOffset + 2, len);
	    parameters[len] = 0;
	}
	namesRunningOffset += len + 2;
    }

    /* 
     * Consistency check: Make sure the slot and names blocks don't
     * overlap. These blocks can occur in any order, so this check is made 
     * in 2 parts. First we check the case where the slot block starts 
     * after the name block. Later, when we have the slot block length,
     * we check the case where slot block starts before the name block.
     * NOTE: in most cases any overlap will likely be detected by invalid 
     * data read from the blocks, but it's better to find out sooner 
     * than later.
     */
    if (slotOffset >= namesOffset) { /* slot block starts after name block */
	if (slotOffset < namesRunningOffset) {
	    goto db_loser;
	}
    }

    /* ------------------------------------------------------------------
    ** Part 3, process the slot table.
    ** This part has this structure:
    ** struct {
    **     BYTE slotCount [ 2 ];
    **     secmodSlotData [ slotCount ];
    ** {
    */

    CHECK_SIZE( slotOffset + 2 );
    slotCount = SECMOD_GETSHORT((unsigned char *)data->data + slotOffset);

    /* 
     * Consistency check: Part 2. We now have the slot block length, we can 
     * check the case where the slotblock procedes the name block.
     */
    if (slotOffset < namesOffset) { /* slot block starts before name block */
	if (namesOffset < slotOffset + 2 + slotCount*sizeof(secmodSlotData)) {
	    goto db_loser;
	}
    }

    CHECK_SIZE( (slotOffset + 2 + slotCount * sizeof(secmodSlotData)));
    slots = (secmodSlotData *) ((unsigned char *)data->data + slotOffset + 2);

    /*  slotCount; */
    slotStrings = (char **)PORT_ArenaZAlloc(arena, slotCount * sizeof(char *));
    if (slotStrings == NULL)
	goto loser;
    for (i=0; i < (int) slotCount; i++, slots++) {
	PRBool hasRootCerts	=PR_FALSE;
	PRBool hasRootTrust	=PR_FALSE;
	slotID       = SECMOD_GETLONG(slots->slotID);
	defaultFlags = SECMOD_GETLONG(slots->defaultFlags);
	timeout      = SECMOD_GETLONG(slots->timeout);
	hasRootCerts = slots->hasRootCerts;
	if (isOldVersion && internal && (slotID != 2)) {
		unsigned long internalFlags=
			secmod_argSlotFlags("slotFlags",SECMOD_SLOT_FLAGS);
		defaultFlags |= internalFlags;
	}
	if (hasRootCerts && !extended) {
	    trustOrder = 100;
	}

	slotStrings[i] = secmod_mkSlotString(slotID, defaultFlags, timeout, 
	                                   (unsigned char)slots->askpw, 
	                                   hasRootCerts, hasRootTrust);
	if (slotStrings[i] == NULL) {
	    secmod_FreeSlotStrings(slotStrings,i);
	    goto loser;
	}
    }

    nss = secmod_mkNSS(slotStrings, slotCount, internal, isFIPS, isModuleDB, 
		     isModuleDBOnly, internal, trustOrder, cipherOrder, 
		     ssl0, ssl1);
    secmod_FreeSlotStrings(slotStrings,slotCount);
    /* it's permissible (and normal) for nss to be NULL. it simply means
     * there are no NSS specific parameters in the database */
    moduleSpec = secmod_mkNewModuleSpec(dllName,commonName,parameters,nss);
    PR_smprintf_free(nss);
    PORT_FreeArena(arena,PR_TRUE);
    return moduleSpec;

db_loser:
    PORT_SetError(SEC_ERROR_BAD_DATABASE);
loser:
    PORT_FreeArena(arena,PR_TRUE);
    return NULL;
}

static DB *
secmod_OpenDB(const char *appName, const char *filename, const char *dbName, 
				PRBool readOnly, PRBool update)
{
    DB *pkcs11db = NULL;


    if (appName) {
	char *secname = PORT_Strdup(filename);
	int len = strlen(secname);
	int status = RDB_FAIL;

	if (len >= 3 && PORT_Strcmp(&secname[len-3],".db") == 0) {
	   secname[len-3] = 0;
	}
    	pkcs11db=
	   rdbopen(appName, "", secname, readOnly ? NO_RDONLY:NO_RDWR, NULL);
	if (update && !pkcs11db) {
	    DB *updatedb;

    	    pkcs11db = rdbopen(appName, "", secname, NO_CREATE, &status);
	    if (!pkcs11db) {
		if (status == RDB_RETRY) {
 		    pkcs11db= rdbopen(appName, "", secname, 
					readOnly ? NO_RDONLY:NO_RDWR, NULL);
		}
		PORT_Free(secname);
		return pkcs11db;
	    }
	    updatedb = dbopen(dbName, NO_RDONLY, 0600, DB_HASH, 0);
	    if (updatedb) {
		db_Copy(pkcs11db,updatedb);
		(*updatedb->close)(updatedb);
	    } else {
		(*pkcs11db->close)(pkcs11db);
		PORT_Free(secname);
		return NULL;
	   }
	}
	PORT_Free(secname);
	return pkcs11db;
    }
  
    /* I'm sure we should do more checks here sometime... */
    pkcs11db = dbopen(dbName, readOnly ? NO_RDONLY : NO_RDWR, 0600, DB_HASH, 0);

    /* didn't exist? create it */
    if (pkcs11db == NULL) {
	 if (readOnly) 
	     return NULL;

	 pkcs11db = dbopen( dbName, NO_CREATE, 0600, DB_HASH, 0 );
	 if (pkcs11db) 
	     (* pkcs11db->sync)(pkcs11db, 0);
    }
    return pkcs11db;
}

static void 
secmod_CloseDB(DB *pkcs11db) 
{
     (*pkcs11db->close)(pkcs11db);
}

static char *
secmod_addEscape(const char *string, char quote)
{
    char *newString = 0;
    int escapes = 0, size = 0;
    const char *src;
    char *dest;

    for (src=string; *src ; src++) {
	if ((*src == quote) || (*src == '\\')) escapes++;
	size++;
    }

    newString = PORT_ZAlloc(escapes+size+1); 
    if (newString == NULL) {
	return NULL;
    }

    for (src=string, dest=newString; *src; src++,dest++) {
	if ((*src == '\\') || (*src == quote)) {
	    *dest++ = '\\';
	}
	*dest = *src;
    }

    return newString;
}

SECStatus legacy_AddSecmodDB(const char *appName, const char *filename, 
			const char *dbname, char *module, PRBool rw);

#define SECMOD_STEP 10
#define SFTK_DEFAULT_INTERNAL_INIT "library= name=\"NSS Internal PKCS #11 Module\" parameters=\"%s\" NSS=\"Flags=internal,critical trustOrder=75 cipherOrder=100 slotParams=(1={%s askpw=any timeout=30})\""
/*
 * Read all the existing modules in
 */
char **
legacy_ReadSecmodDB(const char *appName, const char *filename,
				const char *dbname, char *params, PRBool rw)
{
    DBT key,data;
    int ret;
    DB *pkcs11db = NULL;
    char **moduleList = NULL, **newModuleList = NULL;
    int moduleCount = 1;
    int useCount = SECMOD_STEP;

    moduleList = (char **) PORT_ZAlloc(useCount*sizeof(char **));
    if (moduleList == NULL) return NULL;

    pkcs11db = secmod_OpenDB(appName,filename,dbname,PR_TRUE,rw);
    if (pkcs11db == NULL) goto done;

    /* read and parse the file or data base */
    ret = (*pkcs11db->seq)(pkcs11db, &key, &data, R_FIRST);
    if (ret)  goto done;


    do {
	char *moduleString;
	PRBool internal = PR_FALSE;
	if ((moduleCount+1) >= useCount) {
	    useCount += SECMOD_STEP;
	    newModuleList =
		(char **)PORT_Realloc(moduleList,useCount*sizeof(char *));
	    if (newModuleList == NULL) goto done;
	    moduleList = newModuleList;
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
	char * newparams = secmod_addEscape(params,'"');
	if (newparams) {
	    moduleList[0] = PR_smprintf(SFTK_DEFAULT_INTERNAL_INIT,newparams,
						SECMOD_SLOT_FLAGS);
	    PORT_Free(newparams);
	}
    }
    /* deal with trust cert db here */

    if (pkcs11db) {
	secmod_CloseDB(pkcs11db);
    } else if (moduleList[0] && rw) {
	legacy_AddSecmodDB(appName,filename,dbname,moduleList[0], rw) ;
    }
    if (!moduleList[0]) {
	PORT_Free(moduleList);
	moduleList = NULL;
    }
    return moduleList;
}

SECStatus
legacy_ReleaseSecmodDBData(const char *appName, const char *filename, 
			const char *dbname, char **moduleSpecList, PRBool rw)
{
    if (moduleSpecList) {
	char **index;
	for(index = moduleSpecList; *index; index++) {
	    PR_smprintf_free(*index);
	}
	PORT_Free(moduleSpecList);
    }
    return SECSuccess;
}

/*
 * Delete a module from the Data Base
 */
SECStatus
legacy_DeleteSecmodDB(const char *appName, const char *filename, 
			const char *dbname, char *args, PRBool rw)
{
    DBT key;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;

    if (!rw) return SECFailure;

    /* make sure we have a db handle */
    pkcs11db = secmod_OpenDB(appName,filename,dbname,PR_FALSE,PR_FALSE);
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
legacy_AddSecmodDB(const char *appName, const char *filename, 
			const char *dbname, char *module, PRBool rw)
{
    DBT key,data;
    SECStatus rv = SECFailure;
    DB *pkcs11db = NULL;
    int ret;


    if (!rw) return SECFailure;

    /* make sure we have a db handle */
    pkcs11db = secmod_OpenDB(appName,filename,dbname,PR_FALSE,PR_FALSE);
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
