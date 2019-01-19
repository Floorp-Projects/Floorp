/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "seccomon.h"
#include "prio.h"
#include "prprf.h"
#include "plhash.h"
#include "prenv.h"

/*
 * The following provides a default example for operating systems to set up
 * and manage applications loading NSS on their OS globally.
 *
 * This code hooks in to the system pkcs11.txt, which controls all the loading
 * of pkcs11 modules common to all applications.
 */

/*
 * OS Specific function to get where the NSS user database should reside.
 */

#ifdef XP_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int
testdir(char *dir)
{
    struct stat buf;
    memset(&buf, 0, sizeof(buf));

    if (stat(dir, &buf) < 0) {
        return 0;
    }

    return S_ISDIR(buf.st_mode);
}

/**
 * Append given @dir to @path and creates the directory with mode @mode.
 * Returns 0 if successful, -1 otherwise.
 * Assumes that the allocation for @path has sufficient space for @dir
 * to be added.
 */
static int
appendDirAndCreate(char *path, char *dir, mode_t mode)
{
    PORT_Strcat(path, dir);
    if (!testdir(path)) {
        if (mkdir(path, mode)) {
            return -1;
        }
    }
    return 0;
}

#define XDG_NSS_USER_PATH1 "/.local"
#define XDG_NSS_USER_PATH2 "/share"
#define XDG_NSS_USER_PATH3 "/pki"

#define NSS_USER_PATH1 "/.pki"
#define NSS_USER_PATH2 "/nssdb"

/**
 * Return the path to user's NSS database.
 * We search in the following dirs in order:
 * (1) $HOME/.pki/nssdb;
 * (2) $XDG_DATA_HOME/pki/nssdb if XDG_DATA_HOME is set;
 * (3) $HOME/.local/share/pki/nssdb (default XDG_DATA_HOME value).
 * If (1) does not exist, then the returned dir will be set to either
 * (2) or (3), depending if XDG_DATA_HOME is set.
 */
char *
getUserDB(void)
{
    char *userdir = PR_GetEnvSecure("HOME");
    char *nssdir = NULL;

    if (userdir == NULL) {
        return NULL;
    }

    nssdir = PORT_Alloc(strlen(userdir) + sizeof(NSS_USER_PATH1) + sizeof(NSS_USER_PATH2));
    PORT_Strcpy(nssdir, userdir);
    PORT_Strcat(nssdir, NSS_USER_PATH1 NSS_USER_PATH2);
    if (testdir(nssdir)) {
        /* $HOME/.pki/nssdb exists */
        return nssdir;
    } else {
        /* either $HOME/.pki or $HOME/.pki/nssdb does not exist */
        PORT_Free(nssdir);
    }
    int size = 0;
    char *xdguserdatadir = PR_GetEnvSecure("XDG_DATA_HOME");
    if (xdguserdatadir) {
        size = strlen(xdguserdatadir);
    } else {
        size = strlen(userdir) + sizeof(XDG_NSS_USER_PATH1) + sizeof(XDG_NSS_USER_PATH2);
    }
    size += sizeof(XDG_NSS_USER_PATH3) + sizeof(NSS_USER_PATH2);

    nssdir = PORT_Alloc(size);
    if (nssdir == NULL) {
        return NULL;
    }

    if (xdguserdatadir) {
        PORT_Strcpy(nssdir, xdguserdatadir);
        if (!testdir(nssdir)) {
            PORT_Free(nssdir);
            return NULL;
        }

    } else {
        PORT_Strcpy(nssdir, userdir);
        if (appendDirAndCreate(nssdir, XDG_NSS_USER_PATH1, 0755) ||
            appendDirAndCreate(nssdir, XDG_NSS_USER_PATH2, 0755)) {
            PORT_Free(nssdir);
            return NULL;
        }
    }
    /* ${XDG_DATA_HOME:-$HOME/.local/share}/pki/nssdb */
    if (appendDirAndCreate(nssdir, XDG_NSS_USER_PATH3, 0760) ||
        appendDirAndCreate(nssdir, NSS_USER_PATH2, 0760)) {
        PORT_Free(nssdir);
        return NULL;
    }
    return nssdir;
}

#define NSS_DEFAULT_SYSTEM "/etc/pki/nssdb"
static char *
getSystemDB(void)
{
    return PORT_Strdup(NSS_DEFAULT_SYSTEM);
}

static PRBool
userIsRoot()
{
    /* this works for linux and all unixes that we know off
       though it isn't stated as such in POSIX documentation */
    return getuid() == 0;
}

static PRBool
userCanModifySystemDB()
{
    return (access(NSS_DEFAULT_SYSTEM, W_OK) == 0);
}

#else
#ifdef XP_WIN
static char *
getUserDB(void)
{
    /* use the registry to find the user's NSS_DIR. if no entry exists, create
     * one in the users Appdir location */
    return NULL;
}

static char *
getSystemDB(void)
{
    /* use the registry to find the system's NSS_DIR. if no entry exists, create
     * one based on the windows system data area */
    return NULL;
}

static PRBool
userIsRoot()
{
    /* use the registry to find if the user is the system administrator. */
    return PR_FALSE;
}

static PRBool
userCanModifySystemDB()
{
    /* use the registry to find if the user has administrative privilege
    * to modify the system's nss database. */
    return PR_FALSE;
}

#else
#error "Need to write getUserDB, SystemDB, userIsRoot, and userCanModifySystemDB functions"
#endif
#endif

static PRBool
getFIPSEnv(void)
{
    char *fipsEnv = PR_GetEnvSecure("NSS_FIPS");
    if (!fipsEnv) {
        return PR_FALSE;
    }
    if ((strcasecmp(fipsEnv, "fips") == 0) ||
        (strcasecmp(fipsEnv, "true") == 0) ||
        (strcasecmp(fipsEnv, "on") == 0) ||
        (strcasecmp(fipsEnv, "1") == 0)) {
        return PR_TRUE;
    }
    return PR_FALSE;
}
#ifdef XP_LINUX

static PRBool
getFIPSMode(void)
{
    FILE *f;
    char d;
    size_t size;

    f = fopen("/proc/sys/crypto/fips_enabled", "r");
    if (!f) {
        /* if we don't have a proc flag, fall back to the
     * environment variable */
        return getFIPSEnv();
    }

    size = fread(&d, 1, 1, f);
    fclose(f);
    if (size != 1)
        return PR_FALSE;
    if (d != '1')
        return PR_FALSE;
    return PR_TRUE;
}

#else
static PRBool
getFIPSMode(void)
{
    return getFIPSEnv();
}
#endif

#define NSS_DEFAULT_FLAGS "flags=readonly"

/* configuration flags according to
 * https://developer.mozilla.org/en/PKCS11_Module_Specs
 * As stated there the slotParams start with a slot name which is a slotID
 * Slots 1 through 3 are reserved for the nss internal modules as follows:
 * 1 for crypto operations slot non-fips,
 * 2 for the key slot, and
 * 3 for the crypto operations slot fips
 */
#define CIPHER_ORDER_FLAGS "cipherOrder=100"
#define SLOT_FLAGS                                                  \
    "[slotFlags=RSA,RC4,RC2,DES,DH,SHA1,MD5,MD2,SSL,TLS,AES,RANDOM" \
    " askpw=any timeout=30 ]"

static const char *nssDefaultFlags =
    CIPHER_ORDER_FLAGS " slotParams={0x00000001=" SLOT_FLAGS " }  ";

static const char *nssDefaultFIPSFlags =
    CIPHER_ORDER_FLAGS " slotParams={0x00000003=" SLOT_FLAGS " }  ";

/*
 * This function builds the list of databases and modules to load, and sets
 * their configuration. For the sample we have a fixed set.
 *  1. We load the user's home nss database.
 *  2. We load the user's custom PKCS #11 modules.
 *  3. We load the system nss database readonly.
 *
 * Any space allocated in get_list must be freed in release_list.
 * This function can use whatever information is available to the application.
 * it is running in the process of the application for which it is making
 * decisions, so it's possible to acquire the application name as part of
 * the decision making process.
 *
 */
static char **
get_list(char *filename, char *stripped_parameters)
{
    char **module_list = PORT_ZNewArray(char *, 5);
    char *userdb, *sysdb;
    int isFIPS = getFIPSMode();
    const char *nssflags = isFIPS ? nssDefaultFIPSFlags : nssDefaultFlags;
    int next = 0;

    /* can't get any space */
    if (module_list == NULL) {
        return NULL;
    }

    sysdb = getSystemDB();
    userdb = getUserDB();

    /* Don't open root's user DB */
    if (userdb != NULL && !userIsRoot()) {
        /* return a list of databases to open. First the user Database */
        module_list[next++] = PR_smprintf(
            "library= "
            "module=\"NSS User database\" "
            "parameters=\"configdir='sql:%s' %s tokenDescription='NSS user database'\" "
            "NSS=\"trustOrder=75 %sflags=internal%s\"",
            userdb, stripped_parameters, nssflags,
            isFIPS ? ",FIPS" : "");

        /* now open the user's defined PKCS #11 modules */
        /* skip the local user DB entry */
        module_list[next++] = PR_smprintf(
            "library= "
            "module=\"NSS User database\" "
            "parameters=\"configdir='sql:%s' %s\" "
            "NSS=\"flags=internal,moduleDBOnly,defaultModDB,skipFirst\"",
            userdb, stripped_parameters);
    }

    /* now the system database (always read only unless it's root) */
    if (sysdb) {
        const char *readonly = userCanModifySystemDB() ? "" : "flags=readonly";
        module_list[next++] = PR_smprintf(
            "library= "
            "module=\"NSS system database\" "
            "parameters=\"configdir='sql:%s' tokenDescription='NSS system database' %s\" "
            "NSS=\"trustOrder=80 %sflags=internal,critical\"",
            sysdb, readonly, nssflags);
    }

    /* that was the last module */
    module_list[next] = 0;

    PORT_Free(userdb);
    PORT_Free(sysdb);

    return module_list;
}

static char **
release_list(char **arg)
{
    static char *success = "Success";
    int next;

    for (next = 0; arg[next]; next++) {
        free(arg[next]);
    }
    PORT_Free(arg);
    return &success;
}

#include "utilpars.h"

#define TARGET_SPEC_COPY(new, start, end) \
    if (end > start) {                    \
        int _cnt = end - start;           \
        PORT_Memcpy(new, start, _cnt);    \
        new += _cnt;                      \
    }

/*
 * According the strcpy man page:
 *
 * The strings  may  not overlap, and the destination string dest must be
 * large enough to receive the copy.
 *
 * This implementation allows target to overlap with src.
 * It does not allow the src to overlap the target.
 *  example: overlapstrcpy(string, string+4) is fine
 *           overlapstrcpy(string+4, string) is not.
 */
static void
overlapstrcpy(char *target, char *src)
{
    while (*src) {
        *target++ = *src++;
    }
    *target = 0;
}

/* determine what options the user was trying to open this database with */
/* filename is the directory pointed to by configdir= */
/* stripped is the rest of the parameters with configdir= stripped out */
static SECStatus
parse_parameters(const char *parameters, char **filename, char **stripped)
{
    const char *sourcePrev;
    const char *sourceCurr;
    char *targetCurr;
    char *newStripped;
    *filename = NULL;
    *stripped = NULL;

    newStripped = PORT_Alloc(PORT_Strlen(parameters) + 2);
    targetCurr = newStripped;
    sourcePrev = parameters;
    sourceCurr = NSSUTIL_ArgStrip(parameters);
    TARGET_SPEC_COPY(targetCurr, sourcePrev, sourceCurr);

    while (*sourceCurr) {
        int next;
        sourcePrev = sourceCurr;
        NSSUTIL_HANDLE_STRING_ARG(sourceCurr, *filename, "configdir=",
                                  sourcePrev = sourceCurr;)
        NSSUTIL_HANDLE_FINAL_ARG(sourceCurr);
        TARGET_SPEC_COPY(targetCurr, sourcePrev, sourceCurr);
    }
    *targetCurr = 0;
    if (*filename == NULL) {
        PORT_Free(newStripped);
        return SECFailure;
    }
    /* strip off any directives from the filename */
    if (strncmp("sql:", *filename, 4) == 0) {
        overlapstrcpy(*filename, (*filename) + 4);
    } else if (strncmp("dbm:", *filename, 4) == 0) {
        overlapstrcpy(*filename, (*filename) + 4);
    } else if (strncmp("extern:", *filename, 7) == 0) {
        overlapstrcpy(*filename, (*filename) + 7);
    }
    *stripped = newStripped;
    return SECSuccess;
}

/* entry point */
char **
NSS_ReturnModuleSpecData(unsigned long function, char *parameters, void *args)
{
    char *filename = NULL;
    char *stripped = NULL;
    char **retString = NULL;
    SECStatus rv;

    rv = parse_parameters(parameters, &filename, &stripped);
    if (rv != SECSuccess) {
        /* use defaults */
        filename = getSystemDB();
        if (!filename) {
            return NULL;
        }
        stripped = PORT_Strdup(NSS_DEFAULT_FLAGS);
        if (!stripped) {
            free(filename);
            return NULL;
        }
    }
    switch (function) {
        case SECMOD_MODULE_DB_FUNCTION_FIND:
            retString = get_list(filename, stripped);
            break;
        case SECMOD_MODULE_DB_FUNCTION_RELEASE:
            retString = release_list((char **)args);
            break;
        /* can't add or delete from this module DB */
        case SECMOD_MODULE_DB_FUNCTION_ADD:
        case SECMOD_MODULE_DB_FUNCTION_DEL:
            retString = NULL;
            break;
        default:
            retString = NULL;
            break;
    }

    PORT_Free(filename);
    PORT_Free(stripped);
    return retString;
}
