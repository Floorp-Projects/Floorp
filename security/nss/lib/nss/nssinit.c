/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <string.h>
#include "seccomon.h"
#include "prinit.h"
#include "prprf.h"
#include "prmem.h"
#include "cert.h"
#include "keyhi.h"
#include "secmod.h"
#include "secoid.h"
#include "nss.h"
#include "pk11func.h"
#include "secerr.h"
#include "nssbase.h"
#include "nssutil.h"

#ifndef NSS_DISABLE_LIBPKIX
#include "pkixt.h"
#include "pkix.h"
#include "pkix_tools.h"
#endif /* NSS_DISABLE_LIBPKIX */

#include "pki3hack.h"
#include "certi.h"
#include "secmodi.h"
#include "ocspti.h"
#include "ocspi.h"
#include "utilpars.h"

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

#define NSS_MAX_FLAG_SIZE sizeof("readOnly") + sizeof("noCertDB") +                                  \
                              sizeof("noModDB") + sizeof("forceOpen") + sizeof("passwordRequired") + \
                              sizeof("optimizeSpace") + sizeof("printPolicyFeedback")
#define NSS_DEFAULT_MOD_NAME "NSS Internal Module"

static char *
nss_makeFlags(PRBool readOnly, PRBool noCertDB,
              PRBool noModDB, PRBool forceOpen,
              PRBool passwordRequired, PRBool optimizeSpace)
{
    char *flags = (char *)PORT_Alloc(NSS_MAX_FLAG_SIZE);
    PRBool first = PR_TRUE;

    PORT_Memset(flags, 0, NSS_MAX_FLAG_SIZE);
    if (readOnly) {
        PORT_Strcat(flags, "readOnly");
        first = PR_FALSE;
    }
    if (noCertDB) {
        if (!first)
            PORT_Strcat(flags, ",");
        PORT_Strcat(flags, "noCertDB");
        first = PR_FALSE;
    }
    if (noModDB) {
        if (!first)
            PORT_Strcat(flags, ",");
        PORT_Strcat(flags, "noModDB");
        first = PR_FALSE;
    }
    if (forceOpen) {
        if (!first)
            PORT_Strcat(flags, ",");
        PORT_Strcat(flags, "forceOpen");
        first = PR_FALSE;
    }
    if (passwordRequired) {
        if (!first)
            PORT_Strcat(flags, ",");
        PORT_Strcat(flags, "passwordRequired");
        first = PR_FALSE;
    }
    if (optimizeSpace) {
        if (!first)
            PORT_Strcat(flags, ",");
        PORT_Strcat(flags, "optimizeSpace");
    }
    return flags;
}

/*
 * build config string from individual internationalized strings
 */
char *
nss_MkConfigString(const char *man, const char *libdesc, const char *tokdesc,
                   const char *ptokdesc, const char *slotdesc, const char *pslotdesc,
                   const char *fslotdesc, const char *fpslotdesc, int minPwd)
{
    char *strings = NULL;
    char *newStrings;

    /* make sure the internationalization was done correctly... */
    strings = PR_smprintf("");
    if (strings == NULL)
        return NULL;

    if (man) {
        newStrings = PR_smprintf("%s manufacturerID='%s'", strings, man);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (libdesc) {
        newStrings = PR_smprintf("%s libraryDescription='%s'", strings, libdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (tokdesc) {
        newStrings = PR_smprintf("%s cryptoTokenDescription='%s'", strings,
                                 tokdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (ptokdesc) {
        newStrings = PR_smprintf("%s dbTokenDescription='%s'", strings, ptokdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (slotdesc) {
        newStrings = PR_smprintf("%s cryptoSlotDescription='%s'", strings,
                                 slotdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (pslotdesc) {
        newStrings = PR_smprintf("%s dbSlotDescription='%s'", strings, pslotdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (fslotdesc) {
        newStrings = PR_smprintf("%s FIPSSlotDescription='%s'",
                                 strings, fslotdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    if (fpslotdesc) {
        newStrings = PR_smprintf("%s FIPSTokenDescription='%s'",
                                 strings, fpslotdesc);
        PR_smprintf_free(strings);
        strings = newStrings;
    }
    if (strings == NULL)
        return NULL;

    newStrings = PR_smprintf("%s minPS=%d", strings, minPwd);
    PR_smprintf_free(strings);
    strings = newStrings;

    return (strings);
}

/*
 * statics to remember the PK11_ConfigurePKCS11()
 * info.
 */
static char *pk11_config_strings = NULL;
static char *pk11_config_name = NULL;
static PRBool pk11_password_required = PR_FALSE;

/*
 * this is a legacy configuration function which used to be part of
 * the PKCS #11 internal token.
 */
void
PK11_ConfigurePKCS11(const char *man, const char *libdesc, const char *tokdesc,
                     const char *ptokdesc, const char *slotdesc, const char *pslotdesc,
                     const char *fslotdesc, const char *fpslotdesc, int minPwd,
                     int pwRequired)
{
    char *strings;

    strings = nss_MkConfigString(man, libdesc, tokdesc, ptokdesc, slotdesc,
                                 pslotdesc, fslotdesc, fpslotdesc, minPwd);
    if (strings == NULL) {
        return;
    }

    if (libdesc) {
        if (pk11_config_name != NULL) {
            PORT_Free(pk11_config_name);
        }
        pk11_config_name = PORT_Strdup(libdesc);
    }

    if (pk11_config_strings != NULL) {
        PR_smprintf_free(pk11_config_strings);
    }
    pk11_config_strings = strings;
    pk11_password_required = pwRequired;

    return;
}

void
PK11_UnconfigurePKCS11(void)
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

/*
 * The following code is an attempt to automagically find the external root
 * module.
 * Note: Keep the #if-defined chunks in order. HPUX must select before UNIX.
 */

static const char *dllname =
#if defined(XP_WIN32) || defined(XP_OS2)
    "nssckbi.dll";
#elif defined(HPUX) && !defined(__ia64) /* HP-UX PA-RISC */
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

static void
nss_FindExternalRootPaths(const char *dbpath,
                          const char *secmodprefix,
                          char **retoldpath, char **retnewpath)
{
    char *path, *oldpath = NULL, *lastsep;
    int len, path_len, secmod_len, dll_len;

    path_len = PORT_Strlen(dbpath);
    secmod_len = secmodprefix ? PORT_Strlen(secmodprefix) : 0;
    dll_len = PORT_Strlen(dllname);
    len = path_len + secmod_len + dll_len + 2; /* FILE_SEP + NULL */

    path = PORT_Alloc(len);
    if (path == NULL)
        return;

    /* back up to the top of the directory */
    PORT_Memcpy(path, dbpath, path_len);
    if (path[path_len - 1] != FILE_SEP) {
        path[path_len++] = FILE_SEP;
    }
    PORT_Strcpy(&path[path_len], dllname);
    if (secmod_len > 0) {
        lastsep = PORT_Strrchr(secmodprefix, FILE_SEP);
        if (lastsep) {
            int secmoddir_len = lastsep - secmodprefix + 1; /* FILE_SEP */
            oldpath = PORT_Alloc(len);
            if (oldpath == NULL) {
                PORT_Free(path);
                return;
            }
            PORT_Memcpy(oldpath, path, path_len);
            PORT_Memcpy(&oldpath[path_len], secmodprefix, secmoddir_len);
            PORT_Strcpy(&oldpath[path_len + secmoddir_len], dllname);
        }
    }
    *retoldpath = oldpath;
    *retnewpath = path;
    return;
}

static void
nss_FreeExternalRootPaths(char *oldpath, char *path)
{
    if (path) {
        PORT_Free(path);
    }
    if (oldpath) {
        PORT_Free(oldpath);
    }
}

static void
nss_FindExternalRoot(const char *dbpath, const char *secmodprefix)
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
        (void)SECMOD_AddNewModule("Root Certs", oldpath, 0, 0);
        hasrootcerts = SECMOD_HasRootCerts();
    }
    if (path && !hasrootcerts) {
        (void)SECMOD_AddNewModule("Root Certs", path, 0, 0);
    }
    nss_FreeExternalRootPaths(oldpath, path);
    return;
}

/*
 * see nss_Init for definitions of the various options.
 *
 * this function builds a moduleSpec string from the options and previously
 * set statics (from PKCS11_Configure, for instance), and uses it to kick off
 * the loading of the various PKCS #11 modules.
 */
static SECMODModule *
nss_InitModules(const char *configdir, const char *certPrefix,
                const char *keyPrefix, const char *secmodName,
                const char *updateDir, const char *updCertPrefix,
                const char *updKeyPrefix, const char *updateID,
                const char *updateName, char *configName, char *configStrings,
                PRBool pwRequired, PRBool readOnly, PRBool noCertDB,
                PRBool noModDB, PRBool forceOpen, PRBool optimizeSpace,
                PRBool isContextInit)
{
    SECMODModule *module = NULL;
    char *moduleSpec = NULL;
    char *flags = NULL;
    char *lconfigdir = NULL;
    char *lcertPrefix = NULL;
    char *lkeyPrefix = NULL;
    char *lsecmodName = NULL;
    char *lupdateDir = NULL;
    char *lupdCertPrefix = NULL;
    char *lupdKeyPrefix = NULL;
    char *lupdateID = NULL;
    char *lupdateName = NULL;

    if (NSS_InitializePRErrorTable() != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    flags = nss_makeFlags(readOnly, noCertDB, noModDB, forceOpen,
                          pwRequired, optimizeSpace);
    if (flags == NULL)
        return NULL;

    /*
     * configdir is double nested, and Windows uses the same character
     * for file seps as we use for escapes! (sigh).
     */
    lconfigdir = NSSUTIL_DoubleEscape(configdir, '\'', '\"');
    if (lconfigdir == NULL) {
        goto loser;
    }
    lcertPrefix = NSSUTIL_DoubleEscape(certPrefix, '\'', '\"');
    if (lcertPrefix == NULL) {
        goto loser;
    }
    lkeyPrefix = NSSUTIL_DoubleEscape(keyPrefix, '\'', '\"');
    if (lkeyPrefix == NULL) {
        goto loser;
    }
    lsecmodName = NSSUTIL_DoubleEscape(secmodName, '\'', '\"');
    if (lsecmodName == NULL) {
        goto loser;
    }
    lupdateDir = NSSUTIL_DoubleEscape(updateDir, '\'', '\"');
    if (lupdateDir == NULL) {
        goto loser;
    }
    lupdCertPrefix = NSSUTIL_DoubleEscape(updCertPrefix, '\'', '\"');
    if (lupdCertPrefix == NULL) {
        goto loser;
    }
    lupdKeyPrefix = NSSUTIL_DoubleEscape(updKeyPrefix, '\'', '\"');
    if (lupdKeyPrefix == NULL) {
        goto loser;
    }
    lupdateID = NSSUTIL_DoubleEscape(updateID, '\'', '\"');
    if (lupdateID == NULL) {
        goto loser;
    }
    lupdateName = NSSUTIL_DoubleEscape(updateName, '\'', '\"');
    if (lupdateName == NULL) {
        goto loser;
    }

    moduleSpec = PR_smprintf(
        "name=\"%s\" parameters=\"configdir='%s' certPrefix='%s' keyPrefix='%s' "
        "secmod='%s' flags=%s updatedir='%s' updateCertPrefix='%s' "
        "updateKeyPrefix='%s' updateid='%s' updateTokenDescription='%s' %s\" "
        "NSS=\"flags=internal,moduleDB,moduleDBOnly,critical%s\"",
        configName ? configName : NSS_DEFAULT_MOD_NAME,
        lconfigdir, lcertPrefix, lkeyPrefix, lsecmodName, flags,
        lupdateDir, lupdCertPrefix, lupdKeyPrefix, lupdateID,
        lupdateName, configStrings ? configStrings : "",
        isContextInit ? "" : ",defaultModDB,internalKeySlot");

loser:
    PORT_Free(flags);
    if (lconfigdir)
        PORT_Free(lconfigdir);
    if (lcertPrefix)
        PORT_Free(lcertPrefix);
    if (lkeyPrefix)
        PORT_Free(lkeyPrefix);
    if (lsecmodName)
        PORT_Free(lsecmodName);
    if (lupdateDir)
        PORT_Free(lupdateDir);
    if (lupdCertPrefix)
        PORT_Free(lupdCertPrefix);
    if (lupdKeyPrefix)
        PORT_Free(lupdKeyPrefix);
    if (lupdateID)
        PORT_Free(lupdateID);
    if (lupdateName)
        PORT_Free(lupdateName);

    if (moduleSpec) {
        module = SECMOD_LoadModule(moduleSpec, NULL, PR_TRUE);
        PR_smprintf_free(moduleSpec);
        if (module && !module->loaded) {
            SECMOD_DestroyModule(module);
            return NULL;
        }
    }
    return module;
}

/*
 * OK there are now lots of options here, lets go through them all:
 *
 * configdir - base directory where all the cert, key, and module datbases live.
 * certPrefix - prefix added to the beginning of the cert database example: "
 *             "https-server1-"
 * keyPrefix - prefix added to the beginning of the key database example: "
 *             "https-server1-"
 * secmodName - name of the security module database (usually "secmod.db").
 * updateDir - used in initMerge, old directory to update from.
 * updateID - used in initMerge, unique ID to represent the updated directory.
 * updateName - used in initMerge, token name when updating.
 * initContextPtr -  used in initContext, pointer to return a unique context
 *            value.
 * readOnly - Boolean: true if the databases are to be opened read only.
 * nocertdb - Don't open the cert DB and key DB's, just initialize the
 *             Volatile certdb.
 * nomoddb - Don't open the security module DB, just initialize the
 *             PKCS #11 module.
 * forceOpen - Continue to force initializations even if the databases cannot
 *             be opened.
 * noRootInit - don't try to automatically load the root cert store if one is
 *           not found.
 * optimizeSpace - tell NSS to use fewer hash table buckets.
 *
 * The next three options are used in an attempt to share PKCS #11 modules
 * with other loaded, running libraries. PKCS #11 was not designed with this
 * sort of sharing in mind, so use of these options may lead to questionable
 * results. These options are may be incompatible with NSS_LoadContext() calls.
 *
 * noSingleThreadedModules - don't load modules that are not thread safe (many
 *           smart card tokens will not work).
 * allowAlreadyInitializedModules - if a module has already been loaded and
 *           initialize try to use it.
 * don'tFinalizeModules -  dont shutdown modules we may have loaded.
 */

static PRBool nssIsInitted = PR_FALSE;
static NSSInitContext *nssInitContextList = NULL;

#ifndef NSS_DISABLE_LIBPKIX
static void *plContext = NULL;
#endif /* NSS_DISABLE_LIBPKIX */

struct NSSInitContextStr {
    NSSInitContext *next;
    PRUint32 magic;
};

#define NSS_INIT_MAGIC 0x1413A91C
static SECStatus nss_InitShutdownList(void);

/* All initialized to zero in BSS */
static PRCallOnceType nssInitOnce;
static PZLock *nssInitLock;
static PZCondVar *nssInitCondition;
static int nssIsInInit;

static PRStatus
nss_doLockInit(void)
{
    nssInitLock = PZ_NewLock(nssILockOther);
    if (nssInitLock == NULL) {
        return PR_FAILURE;
    }
    nssInitCondition = PZ_NewCondVar(nssInitLock);
    if (nssInitCondition == NULL) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static SECStatus
nss_Init(const char *configdir, const char *certPrefix, const char *keyPrefix,
         const char *secmodName, const char *updateDir,
         const char *updCertPrefix, const char *updKeyPrefix,
         const char *updateID, const char *updateName,
         NSSInitContext **initContextPtr,
         NSSInitParameters *initParams,
         PRBool readOnly, PRBool noCertDB,
         PRBool noModDB, PRBool forceOpen, PRBool noRootInit,
         PRBool optimizeSpace, PRBool noSingleThreadedModules,
         PRBool allowAlreadyInitializedModules,
         PRBool dontFinalizeModules)
{
    SECMODModule *parent = NULL;
#ifndef NSS_DISABLE_LIBPKIX
    PKIX_UInt32 actualMinorVersion = 0;
    PKIX_Error *pkixError = NULL;
#endif /* NSS_DISABLE_LIBPKIX */
    PRBool isReallyInitted;
    char *configStrings = NULL;
    char *configName = NULL;
    PRBool passwordRequired = PR_FALSE;
#ifdef POLICY_FILE
    char *ignoreVar;
#endif

    /* if we are trying to init with a traditional NSS_Init call, maintain
     * the traditional idempotent behavior. */
    if (!initContextPtr && nssIsInitted) {
        return SECSuccess;
    }

    /* make sure our lock and condition variable are initialized one and only
     * one time */
    if (PR_CallOnce(&nssInitOnce, nss_doLockInit) != PR_SUCCESS) {
        return SECFailure;
    }

    /*
     * if we haven't done basic initialization, single thread the
     * initializations.
     */
    PZ_Lock(nssInitLock);
    isReallyInitted = NSS_IsInitialized();
    if (!isReallyInitted) {
        while (!isReallyInitted && nssIsInInit) {
            PZ_WaitCondVar(nssInitCondition, PR_INTERVAL_NO_TIMEOUT);
            isReallyInitted = NSS_IsInitialized();
        }
        /* once we've completed basic initialization, we can allow more than
         * one process initialize NSS at a time. */
    }
    nssIsInInit++;
    PZ_Unlock(nssInitLock);

    /* this tells us whether or not some library has already initialized us.
     * if so, we don't want to double call some of the basic initialization
     * functions */

    if (!isReallyInitted) {
#ifdef DEBUG
        CERTCertificate dummyCert;
        /* New option bits must not change the size of CERTCertificate. */
        PORT_Assert(sizeof(dummyCert.options) == sizeof(void *));
#endif

        if (SECSuccess != cert_InitLocks()) {
            goto loser;
        }

        if (SECSuccess != InitCRLCache()) {
            goto loser;
        }

        if (SECSuccess != OCSP_InitGlobal()) {
            goto loser;
        }
    }

    if (noSingleThreadedModules || allowAlreadyInitializedModules ||
        dontFinalizeModules) {
        pk11_setGlobalOptions(noSingleThreadedModules,
                              allowAlreadyInitializedModules,
                              dontFinalizeModules);
    }

    if (initContextPtr) {
        *initContextPtr = PORT_ZNew(NSSInitContext);
        if (*initContextPtr == NULL) {
            goto loser;
        }
        /*
         * For traditional NSS_Init, we used the PK11_Configure() call to set
         * globals. with InitContext, we pass those strings in as parameters.
         *
         * This allows old NSS_Init calls to work as before, while at the same
         * time new calls and old calls will not interfere with each other.
         */
        if (initParams) {
            if (initParams->length < sizeof(NSSInitParameters)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                goto loser;
            }
            configStrings = nss_MkConfigString(initParams->manufactureID,
                                               initParams->libraryDescription,
                                               initParams->cryptoTokenDescription,
                                               initParams->dbTokenDescription,
                                               initParams->cryptoSlotDescription,
                                               initParams->dbSlotDescription,
                                               initParams->FIPSSlotDescription,
                                               initParams->FIPSTokenDescription,
                                               initParams->minPWLen);
            if (configStrings == NULL) {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                goto loser;
            }
            configName = initParams->libraryDescription;
            passwordRequired = initParams->passwordRequired;
        }

        /* If we're NSS_ContextInit, we're probably a library. It could be
         * possible that the application initialized NSS then forked(). The
         * library would have no knowledge of that. If we call 
         * SECMOD_RestartModules() here, we will be able to continue on with
         * NSS as normal. SECMOD_RestartModules() does have the side affect
         * of losing all our PKCS #11 objects in the new process, but only if
         * the module needs to be reinited. If it needs to be reinit those
         * objects are inaccessible anyway, it's always save to call
         * SECMOD_RestartModules(PR_FALSE).
         */
        /* NOTE: We could call SECMOD_Init() here, but if we aren't already
         * inited, then there's no modules to restart, so SECMOD_RestartModules
         * will return immediately */
        SECMOD_RestartModules(PR_FALSE);
    } else {
        configStrings = pk11_config_strings;
        configName = pk11_config_name;
        passwordRequired = pk11_password_required;
    }

    /* Skip the module init if we are already initted and we are trying
     * to init with noCertDB and noModDB */
    if (!(isReallyInitted && noCertDB && noModDB)) {
        parent = nss_InitModules(configdir, certPrefix, keyPrefix, secmodName,
                                 updateDir, updCertPrefix, updKeyPrefix, updateID,
                                 updateName, configName, configStrings, passwordRequired,
                                 readOnly, noCertDB, noModDB, forceOpen, optimizeSpace,
                                 (initContextPtr != NULL));

        if (parent == NULL) {
            goto loser;
        }
    }

    /* finish up initialization */
    if (!isReallyInitted) {
        if (SECOID_Init() != SECSuccess) {
            goto loser;
        }
#ifdef POLICY_FILE
        /* Load the system crypto policy file if it exists,
         * unless the NSS_IGNORE_SYSTEM_POLICY environment
         * variable has been set to 1. */
        ignoreVar = PR_GetEnvSecure("NSS_IGNORE_SYSTEM_POLICY");
        if (ignoreVar == NULL || strncmp(ignoreVar, "1", sizeof("1")) != 0) {
            if (PR_Access(POLICY_PATH "/" POLICY_FILE, PR_ACCESS_READ_OK) == PR_SUCCESS) {
                SECMODModule *module = SECMOD_LoadModule(
                    "name=\"Policy File\" "
                    "parameters=\"configdir='sql:" POLICY_PATH "' "
                    "secmod='" POLICY_FILE "' "
                    "flags=readOnly,noCertDB,forceSecmodChoice,forceOpen\" "
                    "NSS=\"flags=internal,moduleDB,skipFirst,moduleDBOnly,critical\"",
                    parent, PR_TRUE);
                if (module) {
                    PRBool isLoaded = module->loaded;
                    SECMOD_DestroyModule(module);
                    if (!isLoaded) {
                        goto loser;
                    }
                }
            }
        }
#endif
        if (STAN_LoadDefaultNSS3TrustDomain() != PR_SUCCESS) {
            goto loser;
        }
        if (nss_InitShutdownList() != SECSuccess) {
            goto loser;
        }
        CERT_SetDefaultCertDB((CERTCertDBHandle *)
                                  STAN_GetDefaultTrustDomain());
        if ((!noModDB) && (!noCertDB) && (!noRootInit)) {
            if (!SECMOD_HasRootCerts()) {
                const char *dbpath = configdir;
                /* handle supported database modifiers */
                if (strncmp(dbpath, "sql:", 4) == 0) {
                    dbpath += 4;
                } else if (strncmp(dbpath, "dbm:", 4) == 0) {
                    dbpath += 4;
                } else if (strncmp(dbpath, "extern:", 7) == 0) {
                    dbpath += 7;
                } else if (strncmp(dbpath, "rdb:", 4) == 0) {
                    /* if rdb: is specified, the configdir isn't really a
               * path. Skip it */
                    dbpath = NULL;
                }
                if (dbpath) {
                    nss_FindExternalRoot(dbpath, secmodName);
                }
            }
        }
        pk11sdr_Init();
        cert_CreateSubjectKeyIDHashTable();

#ifndef NSS_DISABLE_LIBPKIX
        pkixError = PKIX_Initialize(PKIX_FALSE, PKIX_MAJOR_VERSION, PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION, &actualMinorVersion, &plContext);

        if (pkixError != NULL) {
            goto loser;
        } else {
            char *ev = PR_GetEnvSecure("NSS_ENABLE_PKIX_VERIFY");
            if (ev && ev[0]) {
                CERT_SetUsePKIXForValidation(PR_TRUE);
            }
        }
#endif /* NSS_DISABLE_LIBPKIX */
    }

    /*
     * Now mark the appropriate init state. If initContextPtr was passed
     * in, then return the new context pointer and add it to the
     * nssInitContextList. Otherwise set the global nss_isInitted flag
     */
    PZ_Lock(nssInitLock);
    if (!initContextPtr) {
        nssIsInitted = PR_TRUE;
    } else {
        (*initContextPtr)->magic = NSS_INIT_MAGIC;
        (*initContextPtr)->next = nssInitContextList;
        nssInitContextList = (*initContextPtr);
    }
    nssIsInInit--;
    /* now that we are inited, all waiters can move forward */
    PZ_NotifyAllCondVar(nssInitCondition);
    PZ_Unlock(nssInitLock);

    if (initContextPtr && configStrings) {
        PR_smprintf_free(configStrings);
    }
    if (parent) {
        SECMOD_DestroyModule(parent);
    }

    return SECSuccess;

loser:
    if (initContextPtr && *initContextPtr) {
        PORT_Free(*initContextPtr);
        *initContextPtr = NULL;
        if (configStrings) {
            PR_smprintf_free(configStrings);
        }
    }
    PZ_Lock(nssInitLock);
    nssIsInInit--;
    /* We failed to init, allow one to move forward */
    PZ_NotifyCondVar(nssInitCondition);
    PZ_Unlock(nssInitLock);
    if (parent) {
        SECMOD_DestroyModule(parent);
    }
    return SECFailure;
}

SECStatus
NSS_Init(const char *configdir)
{
    return nss_Init(configdir, "", "", SECMOD_DB, "", "", "", "", "", NULL,
                    NULL, PR_TRUE, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE,
                    PR_TRUE, PR_FALSE, PR_FALSE, PR_FALSE);
}

SECStatus
NSS_InitReadWrite(const char *configdir)
{
    return nss_Init(configdir, "", "", SECMOD_DB, "", "", "", "", "", NULL,
                    NULL, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE,
                    PR_TRUE, PR_FALSE, PR_FALSE, PR_FALSE);
}

/*
 * OK there are now lots of options here, lets go through them all:
 *
 * configdir - base directory where all the cert, key, and module datbases live.
 * certPrefix - prefix added to the beginning of the cert database example: "
 *             "https-server1-"
 * keyPrefix - prefix added to the beginning of the key database example: "
 *             "https-server1-"
 * secmodName - name of the security module database (usually "secmod.db").
 * flags - change the open options of NSS_Initialize as follows:
 *   NSS_INIT_READONLY - Open the databases read only.
 *   NSS_INIT_NOCERTDB - Don't open the cert DB and key DB's, just
 *             initialize the volatile certdb.
 *   NSS_INIT_NOMODDB  - Don't open the security module DB, just
 *             initialize the      PKCS #11 module.
 *      NSS_INIT_FORCEOPEN - Continue to force initializations even if the
 *             databases cannot be opened.
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
                    "", "", "", "", "", NULL, NULL,
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

NSSInitContext *
NSS_InitContext(const char *configdir, const char *certPrefix,
                const char *keyPrefix, const char *secmodName,
                NSSInitParameters *initParams, PRUint32 flags)
{
    SECStatus rv;
    NSSInitContext *context;

    rv = nss_Init(configdir, certPrefix, keyPrefix, secmodName,
                  "", "", "", "", "", &context, initParams,
                  ((flags & NSS_INIT_READONLY) == NSS_INIT_READONLY),
                  ((flags & NSS_INIT_NOCERTDB) == NSS_INIT_NOCERTDB),
                  ((flags & NSS_INIT_NOMODDB) == NSS_INIT_NOMODDB),
                  ((flags & NSS_INIT_FORCEOPEN) == NSS_INIT_FORCEOPEN), PR_TRUE,
                  ((flags & NSS_INIT_OPTIMIZESPACE) == NSS_INIT_OPTIMIZESPACE),
                  ((flags & NSS_INIT_PK11THREADSAFE) == NSS_INIT_PK11THREADSAFE),
                  ((flags & NSS_INIT_PK11RELOAD) == NSS_INIT_PK11RELOAD),
                  ((flags & NSS_INIT_NOPK11FINALIZE) == NSS_INIT_NOPK11FINALIZE));
    return (rv == SECSuccess) ? context : NULL;
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
                    NULL, NULL,
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
NSS_NoDB_Init(const char *configdir)
{
    return nss_Init("", "", "", "", "", "", "", "", "", NULL, NULL,
                    PR_TRUE, PR_TRUE, PR_TRUE, PR_TRUE, PR_TRUE, PR_TRUE,
                    PR_FALSE, PR_FALSE, PR_FALSE);
}

#define NSS_SHUTDOWN_STEP 10

struct NSSShutdownFuncPair {
    NSS_ShutdownFunc func;
    void *appData;
};

static struct NSSShutdownListStr {
    PZLock *lock;
    int allocatedFuncs;
    int peakFuncs;
    struct NSSShutdownFuncPair *funcs;
} nssShutdownList = { 0 };

/*
 * find and existing shutdown function
 */
static int
nss_GetShutdownEntry(NSS_ShutdownFunc sFunc, void *appData)
{
    int count, i;
    count = nssShutdownList.peakFuncs;

    for (i = 0; i < count; i++) {
        if ((nssShutdownList.funcs[i].func == sFunc) &&
            (nssShutdownList.funcs[i].appData == appData)) {
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

    /* make sure our lock and condition variable are initialized one and only
     * one time */
    if (PR_CallOnce(&nssInitOnce, nss_doLockInit) != PR_SUCCESS) {
        return SECFailure;
    }

    PZ_Lock(nssInitLock);
    if (!NSS_IsInitialized()) {
        PZ_Unlock(nssInitLock);
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }
    PZ_Unlock(nssInitLock);
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
            (struct NSSShutdownFuncPair *)PORT_Realloc(nssShutdownList.funcs,
                                                       (nssShutdownList.allocatedFuncs + NSS_SHUTDOWN_STEP) * sizeof(struct NSSShutdownFuncPair));
        if (!funcs) {
            PZ_Unlock(nssShutdownList.lock);
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

    /* make sure our lock and condition variable are initialized one and only
     * one time */
    if (PR_CallOnce(&nssInitOnce, nss_doLockInit) != PR_SUCCESS) {
        return SECFailure;
    }
    PZ_Lock(nssInitLock);
    if (!NSS_IsInitialized()) {
        PZ_Unlock(nssInitLock);
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }
    PZ_Unlock(nssInitLock);

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
    if (nssShutdownList.lock != NULL) {
        return SECSuccess;
    }
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
    for (i = 0; i < nssShutdownList.peakFuncs; i++) {
        struct NSSShutdownFuncPair *funcPair = &nssShutdownList.funcs[i];
        if (funcPair->func) {
            if ((*funcPair->func)(funcPair->appData, NULL) != SECSuccess) {
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
nss_Shutdown(void)
{
    SECStatus shutdownRV = SECSuccess;
    SECStatus rv;
    PRStatus status;
    NSSInitContext *temp;

    rv = nss_ShutdownShutdownList();
    if (rv != SECSuccess) {
        shutdownRV = SECFailure;
    }
    cert_DestroyLocks();
    ShutdownCRLCache();
    OCSP_ShutdownGlobal();
#ifndef NSS_DISABLE_LIBPKIX
    PKIX_Shutdown(plContext);
#endif /* NSS_DISABLE_LIBPKIX */
    SECOID_Shutdown();
    status = STAN_Shutdown();
    cert_DestroySubjectKeyIDHashTable();
    pk11_SetInternalKeySlot(NULL);
    rv = SECMOD_Shutdown();
    if (rv != SECSuccess) {
        shutdownRV = SECFailure;
    }
    pk11sdr_Shutdown();
    nssArena_Shutdown();
    if (status == PR_FAILURE) {
        if (NSS_GetError() == NSS_ERROR_BUSY) {
            PORT_SetError(SEC_ERROR_BUSY);
        }
        shutdownRV = SECFailure;
    }
    /*
     * A thread's error stack is automatically destroyed when the thread
     * terminates, except for the primordial thread, whose error stack is
     * destroyed by PR_Cleanup.  Since NSS is usually shut down by the
     * primordial thread and many NSS-based apps don't call PR_Cleanup,
     * we destroy the calling thread's error stack here. This must be
     * done after any NSS_GetError call, otherwise NSS_GetError will
     * create the error stack again.
     */
    nss_DestroyErrorStack();
    nssIsInitted = PR_FALSE;
    temp = nssInitContextList;
    nssInitContextList = NULL;
    /* free the old list. This is necessary when we are called from
     * NSS_Shutdown(). */
    while (temp) {
        NSSInitContext *next = temp->next;
        temp->magic = 0;
        PORT_Free(temp);
        temp = next;
    }
    return shutdownRV;
}

SECStatus
NSS_Shutdown(void)
{
    SECStatus rv;
    /* make sure our lock and condition variable are initialized one and only
     * one time */
    if (PR_CallOnce(&nssInitOnce, nss_doLockInit) != PR_SUCCESS) {
        return SECFailure;
    }
    PZ_Lock(nssInitLock);

    if (!nssIsInitted) {
        PZ_Unlock(nssInitLock);
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }

    /* If one or more threads are in the middle of init, wait for them
     * to complete */
    while (nssIsInInit) {
        PZ_WaitCondVar(nssInitCondition, PR_INTERVAL_NO_TIMEOUT);
    }
    rv = nss_Shutdown();
    PZ_Unlock(nssInitLock);
    return rv;
}

/*
 * remove the context from a list. return true if found, false if not
 */
PRBool
nss_RemoveList(NSSInitContext *context)
{
    NSSInitContext *this = nssInitContextList;
    NSSInitContext **last = &nssInitContextList;

    while (this) {
        if (this == context) {
            *last = this->next;
            this->magic = 0;
            PORT_Free(this);
            return PR_TRUE;
        }
        last = &this->next;
        this = this->next;
    }
    return PR_FALSE;
}

/*
 * This form of shutdown is safe in the case where we may have multiple
 * entities using NSS in a single process. Each entity calls shutdown with
 * it's own context. The application (which doesn't get a context), calls
 * shutdown with NULL. Once all users have 'checked in' NSS will shutdown.
 * This is different than NSS_Shutdown, where calling it will shutdown NSS
 * irreguardless of who else may have NSS open.
 */
SECStatus
NSS_ShutdownContext(NSSInitContext *context)
{
    SECStatus rv = SECSuccess;

    /* make sure our lock and condition variable are initialized one and only
     * one time */
    if (PR_CallOnce(&nssInitOnce, nss_doLockInit) != PR_SUCCESS) {
        return SECFailure;
    }
    PZ_Lock(nssInitLock);
    /* If one or more threads are in the middle of init, wait for them
     * to complete */
    while (nssIsInInit) {
        PZ_WaitCondVar(nssInitCondition, PR_INTERVAL_NO_TIMEOUT);
    }

    /* OK, we are the only thread now either initializing or shutting down */

    if (!context) {
        if (!nssIsInitted) {
            PZ_Unlock(nssInitLock);
            PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
            return SECFailure;
        }
        nssIsInitted = 0;
    } else if (!nss_RemoveList(context)) {
        PZ_Unlock(nssInitLock);
        /* context was already freed or wasn't valid */
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }
    if ((nssIsInitted == 0) && (nssInitContextList == NULL)) {
        rv = nss_Shutdown();
    }

    /* NOTE: we don't try to free the nssInitLocks to prevent races against
     * the locks. There may be a thread, right now, waiting in NSS_Init for us
     * to free the lock below. If we delete the locks, bad things would happen
     * to that thread */
    PZ_Unlock(nssInitLock);

    return rv;
}

PRBool
NSS_IsInitialized(void)
{
    return (nssIsInitted) || (nssInitContextList != NULL);
}

extern const char __nss_base_version[];

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
     * patch releases or builds.
     */
    int vmajor = 0, vminor = 0, vpatch = 0, vbuild = 0;
    const char *ptr = importedVersion;
#define NSS_VERSION_VARIABLE __nss_base_version
#include "verref.h"

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
            if (*ptr == '.') {
                ptr++;
                while (isdigit(*ptr)) {
                    vbuild = 10 * vbuild + *ptr - '0';
                    ptr++;
                }
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
    if (vmajor == NSS_VMAJOR && vminor == NSS_VMINOR &&
        vpatch == NSS_VPATCH && vbuild > NSS_VBUILD) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

const char *
NSS_GetVersion(void)
{
    return NSS_VERSION;
}
