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
 # $Id: dbinit.c,v 1.6 2001/11/15 23:04:39 relyea%netscape.com Exp $
 */

#include <ctype.h>
#include "seccomon.h"
#include "prinit.h"
#include "prprf.h"
#include "prmem.h"
#include "pcertt.h"
#include "lowkeyi.h"
#include "pcert.h"
/*#include "secmodi.h" */
#include "secrng.h"
#include "cdbhdl.h"
/*#include "pk11func.h" */
#include "pkcs11i.h"

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

    return PR_smprintf(KEY_DB_FMT, configdir, dbver);
}

/* for now... we need to define vendor specific codes here.
 */
#define CKR_CERTDB_FAILED	CKR_DEVICE_ERROR
#define CKR_KEYDB_FAILED	CKR_DEVICE_ERROR

static CK_RV
pk11_OpenCertDB(const char * configdir, const char *prefix, PRBool readOnly,
    					    NSSLOWCERTCertDBHandle **certdbPtr)
{
    NSSLOWCERTCertDBHandle *certdb;
    CK_RV        crv = CKR_CERTDB_FAILED;
    SECStatus    rv;
    char * name = NULL;

    if (prefix == NULL) {
	prefix = "";
    }

    name = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,prefix);
    if (name == NULL) goto loser;

    certdb = (NSSLOWCERTCertDBHandle*)PORT_ZAlloc(sizeof(NSSLOWCERTCertDBHandle));
    if (certdb == NULL) 
    	goto loser;

/* fix when we get the DB in */
    rv = nsslowcert_OpenCertDB(certdb, readOnly, 
				pk11_certdb_name_cb, (void *)name, PR_FALSE);
    if (rv == SECSuccess) {
	crv = CKR_OK;
	*certdbPtr = certdb;
	certdb = NULL;
    }
loser: 
    if (certdb) PR_Free(certdb);
    if (name) PORT_Free(name);
    return crv;
}

static CK_RV
pk11_OpenKeyDB(const char * configdir, const char *prefix, PRBool readOnly,
    						NSSLOWKEYDBHandle **keydbPtr)
{
    NSSLOWKEYDBHandle *keydb;
    char * name = NULL;

    if (prefix == NULL) {
	prefix = "";
    }
    name = PR_smprintf("%s" PATH_SEPARATOR "%s",configdir,prefix);	
    if (name == NULL) 
	return SECFailure;
    keydb = nsslowkey_OpenKeyDB(readOnly, pk11_keydb_name_cb, (void *)name);
    PORT_Free(name);
    if (keydb == NULL)
	return CKR_KEYDB_FAILED;
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
pk11_DBInit(const char *configdir, const char *certPrefix, 
	    const char *keyPrefix, PRBool readOnly, 
	    PRBool noCertDB, PRBool noKeyDB, PRBool forceOpen,
	    NSSLOWCERTCertDBHandle **certdbPtr, NSSLOWKEYDBHandle **keydbPtr)
{
    CK_RV crv = CKR_OK;


    if (!noCertDB) {
	crv = pk11_OpenCertDB(configdir, certPrefix, readOnly, certdbPtr);
	if (crv != CKR_OK) {
	    if (!forceOpen) goto loser;
	    crv = CKR_OK;
	}
    }
    if (!noKeyDB) {

	crv = pk11_OpenKeyDB(configdir, keyPrefix, readOnly, keydbPtr);
	if (crv != CKR_OK) {
	    if (!forceOpen) goto loser;
	    crv = CKR_OK;
	}
    }


loser:
    return crv;
}


#ifdef notdef
void
pk11_Shutdown(void)
{
    NSSLOWCERTCertDBHandle *certHandle;
    NSSLOWKEYDBHandle *keyHandle;

    PR_FREEIF(secmodname);
    certHandle = nsslowcert_GetDefaultCertDB();
    if (certHandle)
    	nsslowcert_ClosePermCertDB(certHandle);
    nsslowcert_SetDefaultCertDB(NULL); 

    keyHandle = nsslowkey_GetDefaultKeyDB();
    if (keyHandle)
    	nsslowkey_CloseKeyDB(keyHandle);
    nsslowkey_SetDefaultKeyDB(NULL); 

    isInitialized = PR_FALSE;
}
#endif
