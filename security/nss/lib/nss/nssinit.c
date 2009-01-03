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
/* $Id: nssinit.c,v 1.97 2008/08/22 01:33:03 wtc%google.com Exp $ */

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
#include "pk11func.h"
#include "secerr.h"
#include "nssbase.h"
#include "pkixt.h"
#include "pkix.h"
#include "pkix_tools.h"

#include "pki3hack.h"
#include "certi.h"
#include "secmodi.h"
#include "ocspti.h"
#include "ocspi.h"

/*
 * On Windows nss3.dll needs to export the symbol 'mktemp' to be
 * fully backward compatible with the nss3.dll in NSS 3.2.x and
 * 3.3.x.  This symbol was unintentionally exported and its
 * definition (in DBM) was moved from nss3.dll to softokn3.dll
 * in NSS 3.4.  See bug 142575.
 */
#ifdef WIN32_NSS3_DLL_COMPAT
#include <io.h>

/* exported as 'mktemp' */
char *
nss_mktemp(char *path)
{
    return _mktemp(path);
}
#endif

#define NSS_MAX_FLAG_SIZE  sizeof("readOnly")+sizeof("noCertDB")+ \
	sizeof("noModDB")+sizeof("forceOpen")+sizeof("passwordRequired")+ \
	sizeof ("optimizeSpace")
#define NSS_DEFAULT_MOD_NAME "NSS Internal Module"

static char *
nss_makeFlags(PRBool readOnly, PRBool noCertDB, 
				PRBool noModDB, PRBool forceOpen, 
				PRBool passwordRequired, PRBool optimizeSpace) 
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
    if (optimizeSpace) {
        if (!first) PORT_Strcat(flags,",");
        PORT_Strcat(flags,"optimizeSpace");
        first = PR_FALSE;
    }
    return flags;
}

/*
 * statics to remember the PK11_ConfigurePKCS11()
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
PK11_ConfigurePKCS11(const char *man, const char *libdes, const char *tokdes,
	const char *ptokdes, const char *slotdes, const char *pslotdes, 
	const char *fslotdes, const char *fpslotdes, int minPwd, int pwRequired)
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

void PK11_UnconfigurePKCS11(void)
{
    if (pk11_config_strings != NULL) {
	PR_smprintf_free(pk11_config_strings);
        pk11_config_strings = NULL;
    }
    if (pk11_config_name) {
        PORT_Free(pk11_config_name);
        pk11_config_name = NULL;
    }
}

static char *
nss_addEscape(const char *string, char quote)
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

static char *
nss_doubleEscape(const char *string)
{
    char *round1 = NULL;
    char *retValue = NULL;
    if (string == NULL) {
	goto done;
    }
    round1 = nss_addEscape(string,'\'');
    if (round1) {
	retValue = nss_addEscape(round1,'"');
	PORT_Free(round1);
    }

done:
    if (retValue == NULL) {
	retValue = PORT_Strdup("");
    }
    return retValue;
}


/*
 * The following code is an attempt to automagically find the external root
 * module.
 * Note: Keep the #if-defined chunks in order. HPUX must select before UNIX.
 */

static const char *dllname =
#if defined(XP_WIN32) || defined(XP_OS2)
	"nssckbi.dll";
#elif defined(HPUX) && !defined(__ia64)  /* HP-UX PA-RISC */
	"libnssckbi.sl";
#elif defined(DARWIN)
	"libnssckbi.dylib";
#elif defined(XP_UNIX) || defined(XP_BEOS)
	"libnssckbi.so";
#else
	#error "Uh! Oh! I don't know about this platform."
#endif

/* Should we have platform ifdefs here??? */
#define FILE_SEP '/'

static void nss_FindExternalRootPaths(const char *dbpath, 
                                      const char* secmodprefix,
                              char** retoldpath, char** retnewpath)
{
    char *path, *oldpath = NULL, *lastsep;
    int len, path_len, secmod_len, dll_len;

    path_len = PORT_Strlen(dbpath);
    secmod_len = secmodprefix ? PORT_Strlen(secmodprefix) : 0;
    dll_len = PORT_Strlen(dllname);
    len = path_len + secmod_len + dll_len + 2; /* FILE_SEP + NULL */

    path = PORT_Alloc(len);
    if (path == NULL) return;

    /* back up to the top of the directory */
    PORT_Memcpy(path,dbpath,path_len);
    if (path[path_len-1] != FILE_SEP) {
        path[path_len++] = FILE_SEP;
    }
    PORT_Strcpy(&path[path_len],dllname);
    if (secmod_len > 0) {
        lastsep = PORT_Strrchr(secmodprefix, FILE_SEP);
        if (lastsep) {
            int secmoddir_len = lastsep-secmodprefix+1; /* FILE_SEP */
            oldpath = PORT_Alloc(len);
            if (oldpath == NULL) {
                PORT_Free(path);
                return;
            }
            PORT_Memcpy(oldpath,path,path_len);
            PORT_Memcpy(&oldpath[path_len],secmodprefix,secmoddir_len);
            PORT_Strcpy(&oldpath[path_len+secmoddir_len],dllname);
        }
    }
    *retoldpath = oldpath;
    *retnewpath = path;
    return;
}

static void nss_FreeExternalRootPaths(char* oldpath, char* path)
{
    if (path) {
        PORT_Free(path);
    }
    if (oldpath) {
        PORT_Free(oldpath);
    }
}

static void
nss_FindExternalRoot(const char *dbpath, const char* secmodprefix)
{
	char *path = NULL;
        char *oldpath = NULL;
        PRBool hasrootcerts = PR_FALSE;

        /*
         * 'oldpath' is the external root path in NSS 3.3.x or older.
         * For backward compatibility we try to load the root certs
         * module with the old path first.
         */
        nss_FindExternalRootPaths(dbpath, secmodprefix, &oldpath, &path);
        if (oldpath) {
            (void) SECMOD_AddNewModule("Root Certs",oldpath, 0, 0);
            hasrootcerts = SECMOD_HasRootCerts();
        }
        if (path && !hasrootcerts) {
	    (void) SECMOD_AddNewModule("Root Certs",path, 0, 0);
        }
        nss_FreeExternalRootPaths(oldpath, path);
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
 * readOnly - Boolean: true if the databases are to be opened read only.
 * nocertdb - Don't open the cert DB and key DB's, just initialize the 
 *			Volatile certdb.
 * nomoddb - Don't open the security module DB, just initialize the 
 *			PKCS #11 module.
 * forceOpen - Continue to force initializations even if the databases cannot
 * 			be opened.
 */

static PRBool nss_IsInitted = PR_FALSE;
static void* plContext = NULL;

static SECStatus nss_InitShutdownList(void);

#ifdef DEBUG
static CERTCertificate dummyCert;
#endif

static SECStatus
nss_Init(const char *configdir, const char *certPrefix, const char *keyPrefix,
		 const char *secmodName, const char *updateDir, 
		 const char *updCertPrefix, const char *updKeyPrefix,
		 const char *updateID, const char *updateName,
			PRBool readOnly, PRBool noCertDB, 
			PRBool noModDB, PRBool forceOpen, PRBool noRootInit,
			PRBool optimizeSpace, PRBool noSingleThreadedModules,
			PRBool allowAlreadyInitializedModules,
			PRBool dontFinalizeModules)
{
    char *moduleSpec = NULL;
    char *flags = NULL;
    SECStatus rv = SECFailure;
    char *lconfigdir = NULL;
    char *lcertPrefix = NULL;
    char *lkeyPrefix = NULL;
    char *lsecmodName = NULL;
    char *lupdateDir = NULL;
    char *lupdCertPrefix = NULL;
    char *lupdKeyPrefix = NULL;
    char *lupdateID = NULL;
    char *lupdateName = NULL;
    PKIX_UInt32 actualMinorVersion = 0;
    PKIX_Error *pkixError = NULL;;

    if (nss_IsInitted) {
	return SECSuccess;
    }

    /* New option bits must not change the size of CERTCertificate. */
    PORT_Assert(sizeof(dummyCert.options) == sizeof(void *));

    if (SECSuccess != cert_InitLocks()) {
        return SECFailure;
    }

    if (SECSuccess != InitCRLCache()) {
        return SECFailure;
    }
    
    if (SECSuccess != OCSP_InitGlobal()) {
        return SECFailure;
    }

    flags = nss_makeFlags(readOnly,noCertDB,noModDB,forceOpen,
					pk11_password_required, optimizeSpace);
    if (flags == NULL) return rv;

    /*
     * configdir is double nested, and Windows uses the same character
     * for file seps as we use for escapes! (sigh).
     */
    lconfigdir = nss_doubleEscape(configdir);
    if (lconfigdir == NULL) {
	goto loser;
    }
    lcertPrefix = nss_doubleEscape(certPrefix);
    if (lcertPrefix == NULL) {
	goto loser;
    }
    lkeyPrefix = nss_doubleEscape(keyPrefix);
    if (lkeyPrefix == NULL) {
	goto loser;
    }
    lsecmodName = nss_doubleEscape(secmodName);
    if (lsecmodName == NULL) {
	goto loser;
    }
    lupdateDir = nss_doubleEscape(updateDir);
    if (lupdateDir == NULL) {
	goto loser;
    }
    lupdCertPrefix = nss_doubleEscape(updCertPrefix);
    if (lupdCertPrefix == NULL) {
	goto loser;
    }
    lupdKeyPrefix = nss_doubleEscape(updKeyPrefix);
    if (lupdKeyPrefix == NULL) {
	goto loser;
    }
    lupdateID = nss_doubleEscape(updateID);
    if (lupdateID == NULL) {
	goto loser;
    }
    lupdateName = nss_doubleEscape(updateName);
    if (lupdateName == NULL) {
	goto loser;
    }
    if (noSingleThreadedModules || allowAlreadyInitializedModules ||
        dontFinalizeModules) {
        pk11_setGlobalOptions(noSingleThreadedModules,
                              allowAlreadyInitializedModules,
                              dontFinalizeModules);
    }

    moduleSpec = PR_smprintf(
     "name=\"%s\" parameters=\"configdir='%s' certPrefix='%s' keyPrefix='%s' "
     "secmod='%s' flags=%s updatedir='%s' updateCertPrefix='%s' "
     "updateKeyPrefix='%s' updateid='%s' updateTokenDescription='%s' %s\" "
     "NSS=\"flags=internal,moduleDB,moduleDBOnly,critical\"",
		pk11_config_name ? pk11_config_name : NSS_DEFAULT_MOD_NAME,
		lconfigdir,lcertPrefix,lkeyPrefix,lsecmodName,flags,
		lupdateDir, lupdCertPrefix, lupdKeyPrefix, lupdateID, 
		lupdateName, pk11_config_strings ? pk11_config_strings : "");

loser:
    PORT_Free(flags);
    if (lconfigdir) PORT_Free(lconfigdir);
    if (lcertPrefix) PORT_Free(lcertPrefix);
    if (lkeyPrefix) PORT_Free(lkeyPrefix);
    if (lsecmodName) PORT_Free(lsecmodName);
    if (lupdateDir) PORT_Free(lupdateDir);
    if (lupdCertPrefix) PORT_Free(lupdCertPrefix);
    if (lupdKeyPrefix) PORT_Free(lupdKeyPrefix);
    if (lupdateID) PORT_Free(lupdateID);
    if (lupdateName) PORT_Free(lupdateName);

    if (moduleSpec) {
	SECMODModule *module = SECMOD_LoadModule(moduleSpec,NULL,PR_TRUE);
	PR_smprintf_free(moduleSpec);
	if (module) {
	    if (module->loaded) rv=SECSuccess;
	    SECMOD_DestroyModule(module);
	}
    }

    if (rv == SECSuccess) {
	if (SECOID_Init() != SECSuccess) {
	    return SECFailure;
	}
	if (STAN_LoadDefaultNSS3TrustDomain() != PR_SUCCESS) {
	    return SECFailure;
	}
	if (nss_InitShutdownList() != SECSuccess) {
	    return SECFailure;
	}
	CERT_SetDefaultCertDB((CERTCertDBHandle *)
				STAN_GetDefaultTrustDomain());
	if ((!noModDB) && (!noCertDB) && (!noRootInit)) {
	    if (!SECMOD_HasRootCerts()) {
		nss_FindExternalRoot(configdir, secmodName);
	    }
	}
	pk11sdr_Init();
	cert_CreateSubjectKeyIDHashTable();
	nss_IsInitted = PR_TRUE;
    }

    if (SECSuccess == rv) {
	pkixError = PKIX_Initialize
	    (PKIX_FALSE, PKIX_MAJOR_VERSION, PKIX_MINOR_VERSION,
	    PKIX_MINOR_VERSION, &actualMinorVersion, &plContext);

	if (pkixError != NULL) {
	    rv = SECFailure;
	} else {
            char *ev = getenv("NSS_ENABLE_PKIX_VERIFY");
            if (ev && ev[0]) {
                CERT_SetUsePKIXForValidation(PR_TRUE);
            }
        }
    }

    return rv;
}


SECStatus
NSS_Init(const char *configdir)
{
    return nss_Init(configdir, "", "", SECMOD_DB, "", "", "", "", "",
		PR_TRUE, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE, 
		PR_TRUE, PR_FALSE, PR_FALSE, PR_FALSE);
}

SECStatus
NSS_InitReadWrite(const char *configdir)
{
    return nss_Init(configdir, "", "", SECMOD_DB, "", "", "", "", "",
		PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE, 
		PR_TRUE, PR_FALSE, PR_FALSE, PR_FALSE);
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
 *      NSS_INIT_PK11THREADSAFE - only load PKCS#11 modules that are
 *                      thread-safe, ie. that support locking - either OS
 *                      locking or NSS-provided locks . If a PKCS#11
 *                      module isn't thread-safe, don't serialize its
 *                      calls; just don't load it instead. This is necessary
 *                      if another piece of code is using the same PKCS#11
 *                      modules that NSS is accessing without going through
 *                      NSS, for example the Java SunPKCS11 provider.
 *      NSS_INIT_PK11RELOAD - ignore the CKR_CRYPTOKI_ALREADY_INITIALIZED
 *                      error when loading PKCS#11 modules. This is necessary
 *                      if another piece of code is using the same PKCS#11
 *                      modules that NSS is accessing without going through
 *                      NSS, for example Java SunPKCS11 provider.
 *      NSS_INIT_NOPK11FINALIZE - never call C_Finalize on any
 *                      PKCS#11 module. This may be necessary in order to
 *                      ensure continuous operation and proper shutdown
 *                      sequence if another piece of code is using the same
 *                      PKCS#11 modules that NSS is accessing without going
 *                      through NSS, for example Java SunPKCS11 provider.
 *                      The following limitation applies when this is set :
 *                      SECMOD_WaitForAnyTokenEvent will not use
 *                      C_WaitForSlotEvent, in order to prevent the need for
 *                      C_Finalize. This call will be emulated instead.
 *      NSS_INIT_RESERVED - Currently has no effect, but may be used in the
 *                      future to trigger better cooperation between PKCS#11
 *                      modules used by both NSS and the Java SunPKCS11
 *                      provider. This should occur after a new flag is defined
 *                      for C_Initialize by the PKCS#11 working group.
 *      NSS_INIT_COOPERATE - Sets 4 recommended options for applications that
 *                      use both NSS and the Java SunPKCS11 provider. 
 */
SECStatus
NSS_Initialize(const char *configdir, const char *certPrefix, 
	const char *keyPrefix, const char *secmodName, PRUint32 flags)
{
    return nss_Init(configdir, certPrefix, keyPrefix, secmodName,
	"", "", "", "", "",
	((flags & NSS_INIT_READONLY) == NSS_INIT_READONLY),
	((flags & NSS_INIT_NOCERTDB) == NSS_INIT_NOCERTDB),
	((flags & NSS_INIT_NOMODDB) == NSS_INIT_NOMODDB),
	((flags & NSS_INIT_FORCEOPEN) == NSS_INIT_FORCEOPEN),
	((flags & NSS_INIT_NOROOTINIT) == NSS_INIT_NOROOTINIT),
	((flags & NSS_INIT_OPTIMIZESPACE) == NSS_INIT_OPTIMIZESPACE),
        ((flags & NSS_INIT_PK11THREADSAFE) == NSS_INIT_PK11THREADSAFE),
        ((flags & NSS_INIT_PK11RELOAD) == NSS_INIT_PK11RELOAD),
        ((flags & NSS_INIT_NOPK11FINALIZE) == NSS_INIT_NOPK11FINALIZE));
}

SECStatus
NSS_InitWithMerge(const char *configdir, const char *certPrefix, 
	const char *keyPrefix, const char *secmodName, 
	const char *updateDir, const char *updCertPrefix,
	const char *updKeyPrefix, const char *updateID, 
	const char *updateName, PRUint32 flags)
{
    return nss_Init(configdir, certPrefix, keyPrefix, secmodName,
	updateDir, updCertPrefix, updKeyPrefix, updateID, updateName,
	((flags & NSS_INIT_READONLY) == NSS_INIT_READONLY),
	((flags & NSS_INIT_NOCERTDB) == NSS_INIT_NOCERTDB),
	((flags & NSS_INIT_NOMODDB) == NSS_INIT_NOMODDB),
	((flags & NSS_INIT_FORCEOPEN) == NSS_INIT_FORCEOPEN),
	((flags & NSS_INIT_NOROOTINIT) == NSS_INIT_NOROOTINIT),
	((flags & NSS_INIT_OPTIMIZESPACE) == NSS_INIT_OPTIMIZESPACE),
        ((flags & NSS_INIT_PK11THREADSAFE) == NSS_INIT_PK11THREADSAFE),
        ((flags & NSS_INIT_PK11RELOAD) == NSS_INIT_PK11RELOAD),
        ((flags & NSS_INIT_NOPK11FINALIZE) == NSS_INIT_NOPK11FINALIZE));
}

/*
 * initialize NSS without a creating cert db's, key db's, or secmod db's.
 */
SECStatus
NSS_NoDB_Init(const char * configdir)
{
      return nss_Init("","","","", "", "", "", "", "",
			PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE,
			PR_FALSE,PR_FALSE,PR_FALSE);
}


#define NSS_SHUTDOWN_STEP 10

struct NSSShutdownFuncPair {
    NSS_ShutdownFunc	func;
    void		*appData;
};

static struct NSSShutdownListStr {
    PZLock		*lock;
    int			allocatedFuncs;
    int			peakFuncs;
    struct NSSShutdownFuncPair	*funcs;
} nssShutdownList = { 0 };

/*
 * find and existing shutdown function
 */
static int 
nss_GetShutdownEntry(NSS_ShutdownFunc sFunc, void *appData)
{
    int count, i;
    count = nssShutdownList.peakFuncs;
    /* expect the list to be short, just do a linear search */
    for (i=0; i < count; i++) {
	if ((nssShutdownList.funcs[i].func == sFunc) &&
	    (nssShutdownList.funcs[i].appData == appData)){
	    return i;
	}
    }
    return -1;
}
    
/*
 * register a callback to be called when NSS shuts down
 */
SECStatus
NSS_RegisterShutdown(NSS_ShutdownFunc sFunc, void *appData)
{
    int i;

    if (!nss_IsInitted) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    if (sFunc == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    PORT_Assert(nssShutdownList.lock);
    PZ_Lock(nssShutdownList.lock);

    /* make sure we don't have a duplicate */
    i = nss_GetShutdownEntry(sFunc, appData);
    if (i >= 0) {
	PZ_Unlock(nssShutdownList.lock);
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    /* find an empty slot */
    i = nss_GetShutdownEntry(NULL, NULL);
    if (i >= 0) {
	nssShutdownList.funcs[i].func = sFunc;
	nssShutdownList.funcs[i].appData = appData;
	PZ_Unlock(nssShutdownList.lock);
	return SECSuccess;
    }
    if (nssShutdownList.allocatedFuncs == nssShutdownList.peakFuncs) {
	struct NSSShutdownFuncPair *funcs = 
		(struct NSSShutdownFuncPair *)PORT_Realloc
		(nssShutdownList.funcs, 
		(nssShutdownList.allocatedFuncs + NSS_SHUTDOWN_STEP) 
		*sizeof(struct NSSShutdownFuncPair));
	if (!funcs) {
	    return SECFailure;
	}
	nssShutdownList.funcs = funcs;
	nssShutdownList.allocatedFuncs += NSS_SHUTDOWN_STEP;
    }
    nssShutdownList.funcs[nssShutdownList.peakFuncs].func = sFunc;
    nssShutdownList.funcs[nssShutdownList.peakFuncs].appData = appData;
    nssShutdownList.peakFuncs++;
    PZ_Unlock(nssShutdownList.lock);
    return SECSuccess;
}

/*
 * unregister a callback so it won't get called on shutdown.
 */
SECStatus
NSS_UnregisterShutdown(NSS_ShutdownFunc sFunc, void *appData)
{
    int i;
    if (!nss_IsInitted) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }

    PORT_Assert(nssShutdownList.lock);
    PZ_Lock(nssShutdownList.lock);
    i = nss_GetShutdownEntry(sFunc, appData);
    if (i >= 0) {
	nssShutdownList.funcs[i].func = NULL;
	nssShutdownList.funcs[i].appData = NULL;
    }
    PZ_Unlock(nssShutdownList.lock);

    if (i < 0) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * bring up and shutdown the shutdown list
 */
static SECStatus
nss_InitShutdownList(void)
{
    nssShutdownList.lock = PZ_NewLock(nssILockOther);
    if (nssShutdownList.lock == NULL) {
	return SECFailure;
    }
    nssShutdownList.funcs = PORT_ZNewArray(struct NSSShutdownFuncPair, 
				           NSS_SHUTDOWN_STEP);
    if (nssShutdownList.funcs == NULL) {
	PZ_DestroyLock(nssShutdownList.lock);
    	nssShutdownList.lock = NULL;
	return SECFailure;
    }
    nssShutdownList.allocatedFuncs = NSS_SHUTDOWN_STEP;
    nssShutdownList.peakFuncs = 0;

    return SECSuccess;
}

static SECStatus
nss_ShutdownShutdownList(void)
{
    SECStatus rv = SECSuccess;
    int i;

    /* call all the registerd functions first */
    for (i=0; i < nssShutdownList.peakFuncs; i++) {
	struct NSSShutdownFuncPair *funcPair = &nssShutdownList.funcs[i];
	if (funcPair->func) {
	    if ((*funcPair->func)(funcPair->appData,NULL) != SECSuccess) {
		rv = SECFailure;
	    }
	}
    }

    nssShutdownList.peakFuncs = 0;
    nssShutdownList.allocatedFuncs = 0;
    PORT_Free(nssShutdownList.funcs);
    nssShutdownList.funcs = NULL;
    if (nssShutdownList.lock) {
	PZ_DestroyLock(nssShutdownList.lock);
    }
    nssShutdownList.lock = NULL;
    return rv;
}


extern const NSSError NSS_ERROR_BUSY;

SECStatus
NSS_Shutdown(void)
{
    SECStatus shutdownRV = SECSuccess;
    SECStatus rv;
    PRStatus status;

    if (!nss_IsInitted) {
	PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
	return SECFailure;
    }

    rv = nss_ShutdownShutdownList();
    if (rv != SECSuccess) {
	shutdownRV = SECFailure;
    }
    cert_DestroyLocks();
    ShutdownCRLCache();
    OCSP_ShutdownGlobal();
    PKIX_Shutdown(plContext);
    SECOID_Shutdown();
    status = STAN_Shutdown();
    cert_DestroySubjectKeyIDHashTable();
    rv = SECMOD_Shutdown();
    if (rv != SECSuccess) {
	shutdownRV = SECFailure;
    }
    pk11sdr_Shutdown();
    /*
     * A thread's error stack is automatically destroyed when the thread
     * terminates, except for the primordial thread, whose error stack is
     * destroyed by PR_Cleanup.  Since NSS is usually shut down by the
     * primordial thread and many NSS-based apps don't call PR_Cleanup,
     * we destroy the calling thread's error stack here.
     */
    nss_DestroyErrorStack();
    nssArena_Shutdown();
    if (status == PR_FAILURE) {
	if (NSS_GetError() == NSS_ERROR_BUSY) {
	    PORT_SetError(SEC_ERROR_BUSY);
	}
	shutdownRV = SECFailure;
    }
    nss_IsInitted = PR_FALSE;
    return shutdownRV;
}

PRBool
NSS_IsInitialized(void)
{
    return nss_IsInitted;
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
