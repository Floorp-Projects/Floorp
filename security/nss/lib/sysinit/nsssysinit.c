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
 * Red Hat, Inc
 * Portions created by the Initial Developer are Copyright (C) 2009
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
#include "seccomon.h"
#include "prio.h"
#include "prprf.h"
#include "plhash.h"

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

   if (stat(dir,&buf) < 0) {
	return 0;
   }

   return S_ISDIR(buf.st_mode);
}

#define NSS_USER_PATH1 "/.pki"
#define NSS_USER_PATH2 "/nssdb"
static char *
getUserDB(void)
{
   char *userdir = getenv("HOME");
   char *nssdir = NULL;

   if (userdir == NULL) {
	return NULL;
   }

   nssdir = PORT_Alloc(strlen(userdir)
		+sizeof(NSS_USER_PATH1)+sizeof(NSS_USER_PATH2));
   if (nssdir == NULL) {
	return NULL;
   }
   PORT_Strcpy(nssdir, userdir);
   /* verify it exists */
   if (!testdir(nssdir)) {
	PORT_Free(nssdir);
	return NULL;
   }
   PORT_Strcat(nssdir, NSS_USER_PATH1);
   if (!testdir(nssdir) && mkdir(nssdir, 0760)) {
	PORT_Free(nssdir);
	return NULL;
   }
   PORT_Strcat(nssdir, NSS_USER_PATH2);
   if (!testdir(nssdir) && mkdir(nssdir, 0760)) {
	PORT_Free(nssdir);
	return NULL;
   }
   return nssdir;
}

#define NSS_DEFAULT_SYSTEM "/etc/pki/nssdb"
static char *
getSystemDB(void) {
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
    char *fipsEnv = getenv("NSS_FIPS");
    if (!fipsEnv) {
	return PR_FALSE;
    }
    if ((strcasecmp(fipsEnv,"fips") == 0) ||
	(strcasecmp(fipsEnv,"true") == 0) ||
	(strcasecmp(fipsEnv,"on") == 0) ||
	(strcasecmp(fipsEnv,"1") == 0)) {
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
#define ORDER_FLAGS "trustOrder=75 cipherOrder=100"
#define SLOT_FLAGS \
	"[slotFlags=RSA,RC4,RC2,DES,DH,SHA1,MD5,MD2,SSL,TLS,AES,RANDOM" \
	" askpw=any timeout=30 ]"
 
static const char *nssDefaultFlags =
	ORDER_FLAGS " slotParams={0x00000001=" SLOT_FLAGS " }  ";

static const char *nssDefaultFIPSFlags =
	ORDER_FLAGS " slotParams={0x00000003=" SLOT_FLAGS " }  ";

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
        "NSS=\"%sflags=internal%s\"",
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

#if 0
	/* This doesn't actually work. If we register
		both this and the sysdb (in either order)
		then only one of them actually shows up */

    /* Using a NULL filename as a Boolean flag to
     * prevent registering both an application-defined
     * db and the system db. rhbz #546211.
     */
    PORT_Assert(filename);
    if (sysdb && PL_CompareStrings(filename, sysdb))
	    filename = NULL;
    else if (userdb && PL_CompareStrings(filename, userdb))
	    filename = NULL;

    if (filename && !userIsRoot()) {
	    module_list[next++] = PR_smprintf(
	      "library= "
	      "module=\"NSS database\" "
	      "parameters=\"configdir='sql:%s' tokenDescription='NSS database sql:%s'\" "
	      "NSS=\"%sflags=internal\"",filename, filename, nssflags);
    }
#endif

    /* now the system database (always read only unless it's root) */
    if (sysdb) {
	    const char *readonly = userCanModifySystemDB() ? "" : "flags=readonly";
	    module_list[next++] = PR_smprintf(
	      "library= "
	      "module=\"NSS system database\" "
	      "parameters=\"configdir='sql:%s' tokenDescription='NSS system database' %s\" "
	      "NSS=\"%sflags=internal,critical\"",sysdb, readonly, nssflags);
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


#include "pk11pars.h"

#define TARGET_SPEC_COPY(new, start, end)    \
  if (end > start) {                         \
        int _cnt = end - start;              \
        PORT_Memcpy(new, start, _cnt);       \
        new += _cnt;                         \
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
/* stripped is the rest of the paramters with configdir= stripped out */
static SECStatus
parse_paramters(char *parameters, char **filename, char **stripped)
{
    char *sourcePrev;
    char *sourceCurr;
    char *targetCurr;
    char *newStripped;
    *filename = NULL;
    *stripped = NULL;

    newStripped = PORT_Alloc(PORT_Strlen(parameters)+2);
    targetCurr = newStripped;
    sourcePrev = parameters;
    sourceCurr = secmod_argStrip(parameters);
    TARGET_SPEC_COPY(targetCurr, sourcePrev, sourceCurr);

    while (*sourceCurr) {
	int next;
	sourcePrev = sourceCurr;
	SECMOD_HANDLE_STRING_ARG(sourceCurr, *filename, "configdir=",
		sourcePrev = sourceCurr; )
	SECMOD_HANDLE_FINAL_ARG(sourceCurr);
	TARGET_SPEC_COPY(targetCurr, sourcePrev, sourceCurr);
    }
    *targetCurr = 0;
    if (*filename == NULL) {
	PORT_Free(newStripped);
	return SECFailure;
    }
    /* strip off any directives from the filename */
    if (strncmp("sql:", *filename, 4) == 0) {
	overlapstrcpy(*filename, (*filename)+4);
    } else if (strncmp("dbm:", *filename, 4) == 0) {
	overlapstrcpy(*filename, (*filename)+4);
    } else if (strncmp("extern:", *filename, 7) == 0) {
	overlapstrcpy(*filename, (*filename)+7);
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

    rv = parse_paramters(parameters, &filename, &stripped);
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
