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
 # $Id: nssinit.c,v 1.8 2001/01/18 20:29:00 wtc%netscape.com Exp $
 */

#include <ctype.h>
#include "seccomon.h"
#include "prinit.h"
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
#include "pk11func.h"



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

    return PR_smprintf("%scert%s.db", configdir, dbver);
}
    
static char *
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

    return PR_smprintf("%skey%s.db", configdir, dbver);
}

static SECStatus 
nss_OpenCertDB(const char * configdir,  const char *prefix, PRBool readOnly)
{
    CERTCertDBHandle *certdb;
    SECStatus         status;
    char * name = NULL;

    certdb = CERT_GetDefaultCertDB();
    if (certdb)
    	return SECSuccess;	/* idempotency */

    name = PR_smprintf("%s/%s",configdir,prefix);
    if (name == NULL) goto loser;

    certdb = (CERTCertDBHandle*)PORT_ZAlloc(sizeof(CERTCertDBHandle));
    if (certdb == NULL) 
    	goto loser;

    status = CERT_OpenCertDB(certdb, readOnly, nss_certdb_name_cb, (void *)name);
    if (status == SECSuccess)
	CERT_SetDefaultCertDB(certdb);
    else {
	PR_Free(certdb);
loser: 
	status = SECFailure;
    }
    if (name) PORT_Free(name);
    return status;
}

static SECStatus
nss_OpenKeyDB(const char * configdir, const char *prefix, PRBool readOnly)
{
    SECKEYKeyDBHandle *keydb;
    char * name = NULL;

    keydb = SECKEY_GetDefaultKeyDB();
    if (keydb)
    	return SECSuccess;
    name = PR_smprintf("%s/%s",configdir,prefix);
    if (name == NULL) 
	return SECFailure;
    keydb = SECKEY_OpenKeyDB(readOnly, nss_keydb_name_cb, (void *)name);
    if (keydb == NULL)
	return SECFailure;
    SECKEY_SetDefaultKeyDB(keydb);
    return SECSuccess;
}

static SECStatus
nss_OpenSecModDB(const char * configdir,const char *dbname)
{
    static char *secmodname;

    /* XXX
     * For idempotency, this should check to see if the secmodDB is alredy open
     * but no function exists to make that determination.
     */
    if (secmodname)
    	return SECSuccess;
    secmodname = PR_smprintf("%s/%s", configdir,dbname);
    if (secmodname == NULL)
	return SECFailure;
    SECMOD_init(secmodname);
    return SECSuccess;
}

static SECStatus
nss_Init(const char *configdir, const char *certPrefix, const char *keyPrefix, const char *secmodName, PRBool readOnly)
{
    SECStatus status;
    SECStatus rv      = SECFailure;

    RNG_RNGInit();     		/* initialize random number generator */
    RNG_SystemInfoForRNG();

    status = nss_OpenCertDB(configdir, certPrefix, readOnly);
    if (status != SECSuccess)
	goto loser;

    status = nss_OpenKeyDB(configdir, keyPrefix, readOnly);
    if (status != SECSuccess)
	goto loser;

    status = nss_OpenSecModDB(configdir, secmodName);
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
    return nss_Init(configdir, "", "", "secmod.db", PR_TRUE);
}

SECStatus
NSS_InitReadWrite(const char *configdir)
{
    return nss_Init(configdir, "", "", "secmod.db", PR_FALSE);
}

SECStatus
NSS_Initialize(const char *configdir, const char *certPrefix, const char *keyPrefix, const char *secmodName, PRBool readonly)
{
    return nss_Init(configdir, certPrefix, keyPrefix, secmodName, PR_TRUE);
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

PRBool
NSS_VersionCheck(const char *importedVersion)
{
    /*
     * This is the secret handshake algorithm.
     *
     * This release has a simple version compatibility
     * check algorithm.  This release is not backward
     * compatible with previous major releases.  It is
     * not compatible with future major, minor, or
     * patch releases.
     */
    int vmajor = 0, vminor = 0, vpatch = 0;
    const char *ptr = importedVersion;

    while (isdigit(*ptr)) {
        vmajor = 10 * vmajor + *ptr - '0';
        ptr++;
    }
    if (*ptr == '.') {
        ptr++;
        while (isdigit(*ptr)) {
            vminor = 10 * vminor + *ptr - '0';
            ptr++;
        }
        if (*ptr == '.') {
            ptr++;
            while (isdigit(*ptr)) {
                vpatch = 10 * vpatch + *ptr - '0';
                ptr++;
            }
        }
    }

    if (vmajor != NSS_VMAJOR) {
        return PR_FALSE;
    }
    if (vmajor == NSS_VMAJOR && vminor > NSS_VMINOR) {
        return PR_FALSE;
    }
    if (vmajor == NSS_VMAJOR && vminor == NSS_VMINOR && vpatch > NSS_VPATCH) {
        return PR_FALSE;
    }
    /* Check dependent libraries */
    if (PR_VersionCheck(PR_VERSION) == PR_FALSE) {
        return PR_FALSE;
    }
    return PR_TRUE;
}
