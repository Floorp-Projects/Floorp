/*
 * NSS utility functions
 *
 * ***** BEGIN LICENSE BLOCK *****
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
/* $Id: dbinit.c,v 1.29 2006/06/26 23:42:59 wtchang%redhat.com Exp $ */

#include <ctype.h>
#include "seccomon.h"
#include "prinit.h"
#include "prprf.h"
#include "prmem.h"
#include "pratom.h"
#include "pcertt.h"
#include "lowkeyi.h"
#include "pcert.h"
#include "cdbhdl.h"
#include "keydbi.h"
#include "pkcs11i.h"

static char *
sftk_certdb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char *)arg;
    const char *dbver;
    char *smpname = NULL;
    char *dbname = NULL;

    switch (dbVersion) {
      case 8:
	dbver = "8";
	break;
      case 7:
	dbver = "7";
	break;
      case 6:
	dbver = "6";
	break;
      case 5:
	dbver = "5";
	break;
      case 4:
      default:
	dbver = "";
	break;
    }

    /* make sure we return something allocated with PORT_ so we have properly
     * matched frees at the end */
    smpname = PR_smprintf(CERT_DB_FMT, configdir, dbver);
    if (smpname) {
	dbname = PORT_Strdup(smpname);
	PR_smprintf_free(smpname);
    }
    return dbname;
}
    
static char *
sftk_keydb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char *)arg;
    const char *dbver;
    char *smpname = NULL;
    char *dbname = NULL;
    
    switch (dbVersion) {
      case 4:
	dbver = "4";
	break;
      case 3:
	dbver = "3";
	break;
      case 1:
	dbver = "1";
	break;
      case 2:
      default:
	dbver = "";
	break;
    }

    smpname = PR_smprintf(KEY_DB_FMT, configdir, dbver);
    if (smpname) {
	dbname = PORT_Strdup(smpname);
	PR_smprintf_free(smpname);
    }
    return dbname;
}

const char *
sftk_EvaluateConfigDir(const char *configdir,char **appName)
{
    if (PORT_Strncmp(configdir, MULTIACCESS, sizeof(MULTIACCESS)-1) == 0) {
	char *cdir;

	*appName = PORT_Strdup(configdir+sizeof(MULTIACCESS)-1);
	if (*appName == NULL) {
	    return configdir;
	}
	cdir = *appName;
	while (*cdir && *cdir != ':') {
	    cdir++;
	}
	if (*cdir == ':') {
	   *cdir = 0;
	   cdir++;
	}
	configdir = cdir;
    }
    return configdir;
}

static CK_RV
sftk_OpenCertDB(const char * configdir, const char *prefix, PRBool readOnly,
    					    NSSLOWCERTCertDBHandle **certdbPtr)
{
    NSSLOWCERTCertDBHandle *certdb = NULL;
    CK_RV        crv = CKR_NETSCAPE_CERTDB_FAILED;
    SECStatus    rv;
    char * name = NULL;
    char * appName = NULL;

    if (prefix == NULL) {
	prefix = "";
    }

    configdir = sftk_EvaluateConfigDir(configdir, &appName);

    name = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,prefix);
    if (name == NULL) goto loser;

    certdb = (NSSLOWCERTCertDBHandle*)PORT_ZAlloc(sizeof(NSSLOWCERTCertDBHandle));
    if (certdb == NULL) 
    	goto loser;

    certdb->ref = 1;
/* fix when we get the DB in */
    rv = nsslowcert_OpenCertDB(certdb, readOnly, appName, prefix,
				sftk_certdb_name_cb, (void *)name, PR_FALSE);
    if (rv == SECSuccess) {
	crv = CKR_OK;
	*certdbPtr = certdb;
	certdb = NULL;
    }
loser: 
    if (certdb) PR_Free(certdb);
    if (name) PR_smprintf_free(name);
    if (appName) PORT_Free(appName);
    return crv;
}

static CK_RV
sftk_OpenKeyDB(const char * configdir, const char *prefix, PRBool readOnly,
    						NSSLOWKEYDBHandle **keydbPtr)
{
    NSSLOWKEYDBHandle *keydb;
    char * name = NULL;
    char * appName = NULL;

    if (prefix == NULL) {
	prefix = "";
    }
    configdir = sftk_EvaluateConfigDir(configdir, &appName);

    name = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,prefix);	
    if (name == NULL) 
	return CKR_HOST_MEMORY;
    keydb = nsslowkey_OpenKeyDB(readOnly, appName, prefix, 
					sftk_keydb_name_cb, (void *)name);
    PR_smprintf_free(name);
    if (appName) PORT_Free(appName);
    if (keydb == NULL)
	return CKR_NETSCAPE_KEYDB_FAILED;
    *keydbPtr = keydb;

    return CKR_OK;
}


/*
 * OK there are now lots of options here, lets go through them all:
 *
 * configdir - base directory where all the cert, key, and module datbases live.
 * certPrefix - prefix added to the beginning of the cert database example: "
 * 			"https-server1-"
 * keyPrefix - prefix added to the beginning of the key database example: "
 * 			"https-server1-"
 * secmodName - name of the security module database (usually "secmod.db").
 * readOnly - Boolean: true if the databases are to be openned read only.
 * nocertdb - Don't open the cert DB and key DB's, just initialize the 
 *			Volatile certdb.
 * nomoddb - Don't open the security module DB, just initialize the 
 *			PKCS #11 module.
 * forceOpen - Continue to force initializations even if the databases cannot
 * 			be opened.
 */
CK_RV
sftk_DBInit(const char *configdir, const char *certPrefix, 
	    const char *keyPrefix, PRBool readOnly, 
	    PRBool noCertDB, PRBool noKeyDB, PRBool forceOpen,
	    NSSLOWCERTCertDBHandle **certdbPtr, NSSLOWKEYDBHandle **keydbPtr)
{
    CK_RV crv = CKR_OK;


    if (!noCertDB) {
	crv = sftk_OpenCertDB(configdir, certPrefix, readOnly, certdbPtr);
	if (crv != CKR_OK) {
	    if (!forceOpen) goto loser;
	    crv = CKR_OK;
	}
    }
    if (!noKeyDB) {

	crv = sftk_OpenKeyDB(configdir, keyPrefix, readOnly, keydbPtr);
	if (crv != CKR_OK) {
	    if (!forceOpen) goto loser;
	    crv = CKR_OK;
	}
    }


loser:
    return crv;
}

NSSLOWCERTCertDBHandle *
sftk_getCertDB(SFTKSlot *slot)
{
    NSSLOWCERTCertDBHandle *certHandle;

    PZ_Lock(slot->slotLock);
    certHandle = slot->certDB;
    if (certHandle) {
	PR_AtomicIncrement(&certHandle->ref);
    }
    PZ_Unlock(slot->slotLock);
    return certHandle;
}

NSSLOWKEYDBHandle *
sftk_getKeyDB(SFTKSlot *slot)
{
    NSSLOWKEYDBHandle *keyHandle;

    PZ_Lock(slot->slotLock);
    keyHandle = slot->keyDB;
    if (keyHandle) {
	PR_AtomicIncrement(&keyHandle->ref);
    }
    PZ_Unlock(slot->slotLock);
    return keyHandle;
}

void
sftk_freeCertDB(NSSLOWCERTCertDBHandle *certHandle)
{
   PRInt32 ref = PR_AtomicDecrement(&certHandle->ref);
   if (ref == 0) {
	nsslowcert_ClosePermCertDB(certHandle);
   }
}

void
sftk_freeKeyDB(NSSLOWKEYDBHandle *keyHandle)
{
   PRInt32 ref = PR_AtomicDecrement(&keyHandle->ref);
   if (ref == 0) {
	nsslowkey_CloseKeyDB(keyHandle);
   }
}
   

static int rdbmapflags(int flags);
static rdbfunc sftk_rdbfunc = NULL;
static rdbstatusfunc sftk_rdbstatusfunc = NULL;

/* NOTE: SHLIB_SUFFIX is defined on the command line */
#define RDBLIB SHLIB_PREFIX"rdb."SHLIB_SUFFIX

DB * rdbopen(const char *appName, const char *prefix, 
			const char *type, int flags, int *status)
{
    PRLibrary *lib;
    DB *db;

    if (sftk_rdbfunc) {
	db = (*sftk_rdbfunc)(appName,prefix,type,rdbmapflags(flags));
	if (!db && status && sftk_rdbstatusfunc) {
	    *status = (*sftk_rdbstatusfunc)();
	}
	return db;
    }

    /*
     * try to open the library.
     */
    lib = PR_LoadLibrary(RDBLIB);

    if (!lib) {
	return NULL;
    }

    /* get the entry points */
    sftk_rdbstatusfunc = (rdbstatusfunc) PR_FindFunctionSymbol(lib,"rdbstatus");
    sftk_rdbfunc = (rdbfunc) PR_FindFunctionSymbol(lib,"rdbopen");
    if (sftk_rdbfunc) {
	db = (*sftk_rdbfunc)(appName,prefix,type,rdbmapflags(flags));
	if (!db && status && sftk_rdbstatusfunc) {
	    *status = (*sftk_rdbstatusfunc)();
	}
	return db;
    }

    /* couldn't find the entry point, unload the library and fail */
    PR_UnloadLibrary(lib);
    return NULL;
}

/*
 * the following data structures are from rdb.h.
 */
struct RDBStr {
    DB	db;
    int (*xactstart)(DB *db);
    int (*xactdone)(DB *db, PRBool abort);
    int version;
    int (*dbinitcomplete)(DB *db);
};

#define DB_RDB ((DBTYPE) 0xff)
#define RDB_RDONLY	1
#define RDB_RDWR 	2
#define RDB_CREATE      4

static int
rdbmapflags(int flags) {
   switch (flags) {
   case NO_RDONLY:
	return RDB_RDONLY;
   case NO_RDWR:
	return RDB_RDWR;
   case NO_CREATE:
	return RDB_CREATE;
   default:
	break;
   }
   return 0;
}


PRBool
db_IsRDB(DB *db)
{
    return (PRBool) db->type == DB_RDB;
}

int
db_BeginTransaction(DB *db)
{
    struct RDBStr *rdb = (struct RDBStr *)db;
    if (db->type != DB_RDB) {
	return 0;
    }

    return rdb->xactstart(db);
}

int
db_FinishTransaction(DB *db, PRBool abort)
{
    struct RDBStr *rdb = (struct RDBStr *)db;
    if (db->type != DB_RDB) {
	return 0;
    }

    return rdb->xactdone(db, abort);
}

int
db_InitComplete(DB *db)
{
    struct RDBStr *rdb = (struct RDBStr *)db;
    if (db->type != DB_RDB) {
	return 0;
    }
    /* we should have addes a version number to the RDBS structure. Since we
     * didn't, we detect that we have and 'extended' structure if the rdbstatus
     * func exists */
    if (!sftk_rdbstatusfunc) {
	return 0;
    }

    return rdb->dbinitcomplete(db);
}



SECStatus
db_Copy(DB *dest,DB *src)
{
    int ret;
    DBT key,data;
    ret = (*src->seq)(src, &key, &data, R_FIRST);
    if (ret)  {
	return SECSuccess;
    }

    do {
	(void)(*dest->put)(dest,&key,&data, R_NOOVERWRITE);
    } while ( (*src->seq)(src, &key, &data, R_NEXT) == 0);
    (void)(*dest->sync)(dest,0);

    return SECSuccess;
}

