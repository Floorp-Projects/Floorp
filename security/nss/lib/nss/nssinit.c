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
 # $Id: nssinit.c,v 1.27 2001/11/16 02:30:35 relyea%netscape.com Exp $
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
#include "secoid.h"
#include "nss.h"
#include "secrng.h"
#include "pk11func.h"

#include "pki3hack.h"

#define NSS_MAX_FLAG_SIZE  sizeof("readOnly")+sizeof("noCertDB")+ \
	sizeof("noModDB")+sizeof("forceOpen")+sizeof("passwordRequired")
#define NSS_DEFAULT_MOD_NAME "NSS Internal Module"
#define SECMOD_DB "secmod.db"

static char *
nss_makeFlags(PRBool readOnly, PRBool noCertDB, 
		PRBool noModDB, PRBool forceOpen, PRBool passwordRequired) 
{
    char *flags = (char *)PORT_Alloc(NSS_MAX_FLAG_SIZE);
    PRBool first = PR_TRUE;

    PORT_Memset(flags,0,NSS_MAX_FLAG_SIZE);
    if (readOnly) {
        PORT_Strcat(flags,"readOnly");
        first = PR_FALSE;
    }
    if (noCertDB) {
        if (!first) PORT_Strcat(flags,",");
        PORT_Strcat(flags,"noCertDB");
        first = PR_FALSE;
    }
    if (noModDB) {
        if (!first) PORT_Strcat(flags,",");
        PORT_Strcat(flags,"noModDB");
        first = PR_FALSE;
    }
    if (forceOpen) {
        if (!first) PORT_Strcat(flags,",");
        PORT_Strcat(flags,"forceOpen");
        first = PR_FALSE;
    }
    if (passwordRequired) {
        if (!first) PORT_Strcat(flags,",");
        PORT_Strcat(flags,"passwordRequired");
        first = PR_FALSE;
    }
    return flags;
}

/*
 * statics to remember the PKCS11_ConfigurePKCS11()
 * info.
 */
static char * pk11_config_strings = NULL;
static char * pk11_config_name = NULL;
static PRBool pk11_password_required = PR_FALSE;

/*
 * this is a legacy configuration function which used to be part of
 * the PKCS #11 internal token.
 */
void
PK11_ConfigurePKCS11(char *man, char *libdes, char *tokdes, char *ptokdes,
        char *slotdes, char *pslotdes, char *fslotdes, char *fpslotdes,
        int minPwd, int pwRequired)
{
   char *strings = NULL;
   char *newStrings;

   /* make sure the internationalization was done correctly... */
   strings = PR_smprintf("");
   if (strings == NULL) return;

    if (man) {
        newStrings = PR_smprintf("%s manufacturerID='%s'",strings,man);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    if (libdes) {
        newStrings = PR_smprintf("%s libraryDescription='%s'",strings,libdes);
	PR_smprintf_free(strings);
	strings = newStrings;
	if (pk11_config_name != NULL) {
	    PORT_Free(pk11_config_name);
	}
	pk11_config_name = PORT_Strdup(libdes);
    }
   if (strings == NULL) return;

    if (tokdes) {
        newStrings = PR_smprintf("%s cryptoTokenDescription='%s'",strings,
								tokdes);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    if (ptokdes) {
        newStrings = PR_smprintf("%s dbTokenDescription='%s'",strings,ptokdes);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    if (slotdes) {
        newStrings = PR_smprintf("%s cryptoSlotDescription='%s'",strings,
								slotdes);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    if (pslotdes) {
        newStrings = PR_smprintf("%s dbSlotDescription='%s'",strings,pslotdes);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    if (fslotdes) {
        newStrings = PR_smprintf("%s FIPSSlotDescription='%s'",
							strings,fslotdes);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    if (fpslotdes) {
        newStrings = PR_smprintf("%s FIPSTokenDescription='%s'",
							strings,fpslotdes);
	PR_smprintf_free(strings);
	strings = newStrings;
    }
   if (strings == NULL) return;

    newStrings = PR_smprintf("%s minPS=%d", strings, minPwd);
    PR_smprintf_free(strings);
    strings = newStrings;
   if (strings == NULL) return;

    if (pk11_config_strings != NULL) {
	PR_smprintf_free(pk11_config_strings);
    }
    pk11_config_strings = strings;
    pk11_password_required = pwRequired;

    return;
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

static SECStatus
nss_Init(const char *configdir, const char *certPrefix, const char *keyPrefix,
		 const char *secmodName, PRBool readOnly, PRBool noCertDB, 
					PRBool noModDB, PRBool forceOpen)
{
    char *moduleSpec = NULL;
    char *flags = NULL;
    SECStatus rv = SECFailure;

    flags = nss_makeFlags(readOnly,noCertDB,noModDB,forceOpen,
						pk11_password_required);
    if (flags == NULL) return rv;

    moduleSpec = PR_smprintf("name=\"%s\" parameters=\"configdir=%s certPrefix=%s keyPrefix=%s secmod=%s flags=%s %s\" NSS=\"flags=internal,moduleDB,moduleDBOnly,critical\"",
		pk11_config_name ? pk11_config_name : NSS_DEFAULT_MOD_NAME,
		configdir,certPrefix,keyPrefix,secmodName,flags,
		pk11_config_strings ? pk11_config_strings : "");
    PORT_Free(flags);

    if (moduleSpec) {
	SECMODModule *module = SECMOD_LoadModule(moduleSpec,NULL,PR_TRUE);
	PR_smprintf_free(moduleSpec);
	if (module) {
	    if (module->loaded) rv=SECSuccess;
	    SECMOD_DestroyModule(module);
	}
    }

    if (rv == SECSuccess) {
	/* can this function fail?? */
	STAN_LoadDefaultNSS3TrustDomain();
	CERT_SetDefaultCertDB((CERTCertDBHandle *)
				STAN_GetDefaultTrustDomain());
    }
    return rv;
}


SECStatus
NSS_Init(const char *configdir)
{
    return nss_Init(configdir, "", "", SECMOD_DB, PR_TRUE, 
		PR_FALSE, PR_FALSE, PR_FALSE);
}

SECStatus
NSS_InitReadWrite(const char *configdir)
{
    return nss_Init(configdir, "", "", SECMOD_DB, PR_FALSE, 
		PR_FALSE, PR_FALSE, PR_FALSE);
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
 * flags - change the open options of NSS_Initialize as follows:
 * 	NSS_INIT_READONLY - Open the databases read only.
 * 	NSS_INIT_NOCERTDB - Don't open the cert DB and key DB's, just 
 * 			initialize the volatile certdb.
 * 	NSS_INIT_NOMODDB  - Don't open the security module DB, just 
 *			initialize the 	PKCS #11 module.
 *      NSS_INIT_FORCEOPEN - Continue to force initializations even if the 
 * 			databases cannot be opened.
 */
SECStatus
NSS_Initialize(const char *configdir, const char *certPrefix, 
	const char *keyPrefix, const char *secmodName, PRUint32 flags)
{
    return nss_Init(configdir, certPrefix, keyPrefix, secmodName, 
	((flags & NSS_INIT_READONLY) == NSS_INIT_READONLY),
	((flags & NSS_INIT_NOCERTDB) == NSS_INIT_NOCERTDB),
	((flags & NSS_INIT_NOMODDB) == NSS_INIT_NOMODDB),
	((flags & NSS_INIT_FORCEOPEN) == NSS_INIT_FORCEOPEN));
}

/*
 * initialize NSS without a creating cert db's, key db's, or secmod db's.
 */
SECStatus
NSS_NoDB_Init(const char * configdir)
{
      return nss_Init(configdir?configdir:"","","",SECMOD_DB,
					PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
}

void
NSS_Shutdown(void)
{

#ifdef notdef
    SECOID_Shutdown();

    SECMOD_Shutdown();
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
#endif
}


extern const char __nss_base_rcsid[];
extern const char __nss_base_sccsid[];

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
    volatile char c; /* force a reference that won't get optimized away */

    c = __nss_base_rcsid[0] + __nss_base_sccsid[0]; 

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

