/*
 * NSS utility functions
 *
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
 *
 # $Id: dbinit.c,v 1.4 2001/09/20 21:05:48 relyea%netscape.com Exp $
 */

#include <ctype.h>
#include "seccomon.h"
#include "prinit.h"
#include "prprf.h"
#include "prmem.h"
#include "cert.h"
#include "keylow.h"
#include "keydbt.h"
#include "ssl.h"
#include "sslproto.h"
#include "secmod.h"
#include "secmodi.h"
#include "secoid.h"
#include "nss.h"
#include "secrng.h"
#include "cdbhdl.h"
#include "pk11func.h"
#include "pkcs11i.h"

static char *secmodname = NULL;  

static char *
pk11_certdb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char *)arg;
    const char *dbver;

    switch (dbVersion) {
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

    return PR_smprintf(CERT_DB_FMT, configdir, dbver);
}
    
static char *
pk11_keydb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char *)arg;
    const char *dbver;
    
    switch (dbVersion) {
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

    return PR_smprintf(KEY_DB_FMT, configdir, dbver);
}

/* for now... we need to define vendor specific codes here.
 */
#define CKR_CERTDB_FAILED	CKR_DEVICE_ERROR
#define CKR_KEYDB_FAILED	CKR_DEVICE_ERROR

static CK_RV
pk11_OpenCertDB(const char * configdir,  const char *prefix, PRBool readOnly)
{
    CERTCertDBHandle *certdb;
    CK_RV        crv = CKR_OK;
    SECStatus    rv;
    char * name = NULL;

    certdb = CERT_GetDefaultCertDB();
    if (certdb)
    	return CKR_OK;	/* idempotency */

    name = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,prefix);
    if (name == NULL) goto loser;

    certdb = (CERTCertDBHandle*)PORT_ZAlloc(sizeof(CERTCertDBHandle));
    if (certdb == NULL) 
    	goto loser;

/* fix when we get the DB in */
    rv = CERT_OpenCertDB(certdb, readOnly, pk11_certdb_name_cb, (void *)name);
    if (rv == SECSuccess)
	CERT_SetDefaultCertDB(certdb);
    else {
	PR_Free(certdb);
loser: 
	crv = CKR_CERTDB_FAILED;
    }
    if (name) PORT_Free(name);
    return crv;
}

static CK_RV
pk11_OpenKeyDB(const char * configdir, const char *prefix, PRBool readOnly)
{
    SECKEYKeyDBHandle *keydb;
    char * name = NULL;

    keydb = SECKEY_GetDefaultKeyDB();
    if (keydb)
    	return SECSuccess;
    name = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,prefix);	
    if (name == NULL) 
	return SECFailure;
    keydb = SECKEY_OpenKeyDB(readOnly, pk11_keydb_name_cb, (void *)name);
    if (keydb == NULL)
	return CKR_KEYDB_FAILED;
    SECKEY_SetDefaultKeyDB(keydb);
    PORT_Free(name);

    return CKR_OK;
}

static CERTCertDBHandle certhandle = { 0 };

static PRBool isInitialized = PR_FALSE;

static CK_RV 
pk11_OpenVolatileCertDB() {
      SECStatus rv = SECSuccess;
      /* now we want to verify the signature */
      /*  Initialize the cert code */
      rv = CERT_OpenVolatileCertDB(&certhandle);
      if (rv != SECSuccess) {
	   return CKR_DEVICE_ERROR;
      }
      CERT_SetDefaultCertDB(&certhandle);
      return CKR_OK;
}

/* forward declare so that a failure in the init case can shutdown */
void pk11_Shutdown(void);

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
pk11_DBInit(const char *configdir, const char *certPrefix, 
		const char *keyPrefix,
		 const char *secmodName, PRBool readOnly, PRBool noCertDB, 
					PRBool noModDB, PRBool forceOpen)
{
    SECStatus rv      = SECFailure;
    CK_RV crv = CKR_OK;

    if( isInitialized ) {
	return CKR_OK;
    }

    rv = RNG_RNGInit();     	/* initialize random number generator */
    if (rv != SECSuccess) {
	crv = CKR_DEVICE_ERROR;
	goto loser;
    }
    RNG_SystemInfoForRNG();

    if (noCertDB) {
	crv = pk11_OpenVolatileCertDB();
	if (crv != CKR_OK) {
	    goto loser;
	}
    } else {
	crv = pk11_OpenCertDB(configdir, certPrefix, readOnly);
	if (crv != CKR_OK) {
	    if (!forceOpen) goto loser;
	    crv = pk11_OpenVolatileCertDB();
	    if (crv != CKR_OK) {
		goto loser;
	    }
	}

	crv = pk11_OpenKeyDB(configdir, keyPrefix, readOnly);
	if (crv != CKR_OK) {
	    if (!forceOpen) goto loser;
	}
    }

    isInitialized = PR_TRUE;

loser:
    if (crv != CKR_OK) {
	pk11_Shutdown();
    }
    return crv;
}


void
pk11_Shutdown(void)
{
    CERTCertDBHandle *certHandle;
    SECKEYKeyDBHandle *keyHandle;

    PR_FREEIF(secmodname);
    certHandle = CERT_GetDefaultCertDB();
    if (certHandle)
    	CERT_ClosePermCertDB(certHandle);
    CERT_SetDefaultCertDB(NULL); 

    keyHandle = SECKEY_GetDefaultKeyDB();
    if (keyHandle)
    	SECKEY_CloseKeyDB(keyHandle);
    SECKEY_SetDefaultKeyDB(NULL); 

    isInitialized = PR_FALSE;
}
