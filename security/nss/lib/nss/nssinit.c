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
 # $Id: nssinit.c,v 1.4 2000/11/09 22:59:38 mcgreer%netscape.com Exp $
 */

#include "seccomon.h"
#include "prprf.h"
#include "prmem.h"
#include "cert.h"
#include "key.h"
#include "ssl.h"
#include "sslproto.h"
#include "secmod.h"
#include "secmodi.h"
#include "nss.h"
#include "secrng.h"
#include "cdbhdl.h"	/* ??? */



static char *
nss_certdb_name_cb(void *arg, int dbVersion)
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

    return PR_smprintf("%s/cert%s.db", configdir, dbver);
}
    
char *
nss_keydb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char *)arg;
    const char *dbver;
    
    switch (dbVersion) {
      case 3:
	dbver = "3";
	break;
      case 2:
      default:
	dbver = "";
	break;
    }

    return PR_smprintf("%s/key%s.db", configdir, dbver);
}

SECStatus 
nss_OpenCertDB(const char * configdir, PRBool readOnly)
{
    CERTCertDBHandle *certdb;
    SECStatus         status;

    certdb = CERT_GetDefaultCertDB();
    if (certdb)
    	return SECSuccess;	/* idempotency */

    certdb = (CERTCertDBHandle*)PORT_ZAlloc(sizeof(CERTCertDBHandle));
    if (certdb == NULL) 
    	goto loser;

    status = CERT_OpenCertDB(certdb, readOnly, nss_certdb_name_cb, (void *)configdir);
    if (status == SECSuccess)
	CERT_SetDefaultCertDB(certdb);
    else {
	PR_Free(certdb);
loser: 
	status = SECFailure;
    }
    return status;
}

SECStatus
nss_OpenKeyDB(const char * configdir, PRBool readOnly)
{
    SECKEYKeyDBHandle *keydb;

    keydb = SECKEY_GetDefaultKeyDB();
    if (keydb)
    	return SECSuccess;
    keydb = SECKEY_OpenKeyDB(readOnly, nss_keydb_name_cb, (void *)configdir);
    if (keydb == NULL)
	return SECFailure;
    SECKEY_SetDefaultKeyDB(keydb);
    return SECSuccess;
}

SECStatus
nss_OpenSecModDB(const char * configdir)
{
    static char *secmodname;

    /* XXX
     * For idempotency, this should check to see if the secmodDB is alredy open
     * but no function exists to make that determination.
     */
    if (secmodname)
    	return SECSuccess;
    secmodname = PR_smprintf("%s/secmod.db", configdir);
    if (secmodname == NULL)
	return SECFailure;
    SECMOD_init(secmodname);
    return SECSuccess;
}

SECStatus
nss_Init(const char *configdir, PRBool readOnly)
{
    SECStatus status;
    SECStatus rv      = SECFailure;

    RNG_RNGInit();     		/* initialize random number generator */
    RNG_SystemInfoForRNG();

    status = nss_OpenCertDB(configdir, readOnly);
    if (status != SECSuccess)
	goto loser;

    status = nss_OpenKeyDB(configdir, readOnly);
    if (status != SECSuccess)
	goto loser;

    status = nss_OpenSecModDB(configdir);
    if (status != SECSuccess)
	goto loser;

    rv = SECSuccess;

loser:
    if (rv != SECSuccess) 
	NSS_Shutdown();
    return rv;
}

SECStatus
NSS_Init(const char *configdir)
{
    return nss_Init(configdir, PR_TRUE);
}

SECStatus
NSS_InitReadWrite(const char *configdir)
{
    return nss_Init(configdir, PR_FALSE);
}

/*
 * initialize NSS without a creating cert db's, key db's, or secmod db's.
 */
SECStatus
NSS_NoDB_Init(const char * configdir)
{
          
      CERTCertDBHandle certhandle = { 0 };
      SECStatus rv = SECSuccess;
      SECMODModule *module;

      /* now we want to verify the signature */
      /*  Initialize the cert code */
      rv = CERT_OpenVolatileCertDB(&certhandle);
      if (rv != SECSuccess) {
	   return rv;
      }
      CERT_SetDefaultCertDB(&certhandle);

      RNG_RNGInit();
      RNG_SystemInfoForRNG();
      PK11_InitSlotLists();

      module = SECMOD_NewInternal();
      if (module == NULL) {
	   return SECFailure;
      }
      rv = SECMOD_LoadModule(module);
      if (rv != SECSuccess) {
	   return rv;
      }

      SECMOD_SetInternalModule(module);
      return rv;
}

void
NSS_Shutdown(void)
{
    CERTCertDBHandle *certHandle;
    SECKEYKeyDBHandle *keyHandle;

    certHandle = CERT_GetDefaultCertDB();
    if (certHandle)
    	CERT_ClosePermCertDB(certHandle);

    keyHandle = SECKEY_GetDefaultKeyDB();
    if (keyHandle)
    	SECKEY_CloseKeyDB(keyHandle);

    /* XXX
     * This should also close the secmod DB, 
     * but there's no secmod function to close the DB.
     */
}

