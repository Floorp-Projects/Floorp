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

#include <stdio.h>
#include <string.h>
#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#include <sys/time.h>
#include <termios.h>
#endif

#include "secopt.h"

#if defined(XP_WIN)
#include <time.h>
#include <conio.h>
#endif

#if defined(__sun) && !defined(SVR4)
extern int fclose(FILE*);
extern int fprintf(FILE *, char *, ...);
extern int getopt(int, char**, char*);
extern int isatty(int);
extern char *optarg;
extern char *sys_errlist[];
#define strerror(errno) sys_errlist[errno]
#endif

#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

static char *progName;

static SECStatus
ListKeys(SECKEYKeyDBHandle *handle, FILE *out)
{
    int rt;

    rt = SECU_PrintKeyNames(handle, out);
    if (rt) {
	SECU_PrintError(progName, "unable to list nicknames");
	return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
DumpPublicKey(SECKEYKeyDBHandle *handle, char *nickname, FILE *out)
{
    SECKEYLowPrivateKey *privKey;
    SECKEYLowPublicKey *publicKey;

    /* check if key actually exists */
    if (SECU_CheckKeyNameExists(handle, nickname) == PR_FALSE) {
	SECU_PrintError(progName, "the key \"%s\" does not exist", nickname);
	return SECFailure;
    }

    /* Read in key */
    privKey = SECU_GetPrivateKey(handle, nickname);
    if (!privKey) {
	return SECFailure;
    }

    publicKey = SECKEY_LowConvertToPublicKey(privKey);

    /* Output public key (in the clear) */
    switch(publicKey->keyType) {
      case rsaKey:
	fprintf(out, "RSA Public-Key:\n");
	SECU_PrintInteger(out, &publicKey->u.rsa.modulus, "modulus", 1);
	SECU_PrintInteger(out, &publicKey->u.rsa.publicExponent,
			  "publicExponent", 1);
	break;
      case dsaKey:
	fprintf(out, "DSA Public-Key:\n");
	SECU_PrintInteger(out, &publicKey->u.dsa.params.prime, "prime", 1);
	SECU_PrintInteger(out, &publicKey->u.dsa.params.subPrime,
			  "subPrime", 1);
	SECU_PrintInteger(out, &publicKey->u.dsa.params.base, "base", 1);
	SECU_PrintInteger(out, &publicKey->u.dsa.publicValue, "publicValue", 1);
	break;
      default:
	fprintf(out, "unknown key type\n");
	break;
    }
    return SECSuccess;
}

static SECStatus
DumpPrivateKey(SECKEYKeyDBHandle *handle, char *nickname, FILE *out)
{
    SECKEYLowPrivateKey *key;

    /* check if key actually exists */
    if (SECU_CheckKeyNameExists(handle, nickname) == PR_FALSE) {
	SECU_PrintError(progName, "the key \"%s\" does not exist", nickname);
	return SECFailure;
    }

    /* Read in key */
    key = SECU_GetPrivateKey(handle, nickname);
    if (!key) {
	SECU_PrintError(progName, "error retrieving key");
	return SECFailure;
    }

    switch(key->keyType) {
      case rsaKey:
	fprintf(out, "RSA Private-Key:\n");
	SECU_PrintInteger(out, &key->u.rsa.modulus, "modulus", 1);
	SECU_PrintInteger(out, &key->u.rsa.publicExponent, "publicExponent", 1);
	SECU_PrintInteger(out, &key->u.rsa.privateExponent,
			  "privateExponent", 1);
	SECU_PrintInteger(out, &key->u.rsa.prime1, "prime1", 1);
	SECU_PrintInteger(out, &key->u.rsa.prime2, "prime2", 1);
	SECU_PrintInteger(out, &key->u.rsa.exponent1, "exponent1", 1);
	SECU_PrintInteger(out, &key->u.rsa.exponent2, "exponent2", 1);
	SECU_PrintInteger(out, &key->u.rsa.coefficient, "coefficient", 1);
	break;
      case dsaKey:
	fprintf(out, "DSA Private-Key:\n");
	SECU_PrintInteger(out, &key->u.dsa.params.prime, "prime", 1);
	SECU_PrintInteger(out, &key->u.dsa.params.subPrime, "subPrime", 1);
	SECU_PrintInteger(out, &key->u.dsa.params.base, "base", 1);
	SECU_PrintInteger(out, &key->u.dsa.publicValue, "publicValue", 1);
	SECU_PrintInteger(out, &key->u.dsa.privateValue, "privateValue", 1);
	break;
      default:
	fprintf(out, "unknown key type\n");
	break;
    }
    return SECSuccess;
}

static SECStatus
ChangePassword(SECKEYKeyDBHandle *handle)
{
    SECStatus rv;

    /* Write out database with a new password */
    rv = SECU_ChangeKeyDBPassword(handle, NULL);
    if (rv) {
	SECU_PrintError(progName, "unable to change key password");
    }
    return rv;
}

static SECStatus
DeletePrivateKey (SECKEYKeyDBHandle *keyHandle, char *nickName)
{
    SECStatus rv;

    rv = SECU_DeleteKeyByName (keyHandle, nickName);
    if (rv != SECSuccess)
	fprintf(stderr, "%s: problem deleting private key (%s)\n",
		progName, SECU_Strerror(PR_GetError()));
    return (rv);

}


static void
Usage(const char *progName)
{
    fprintf(stderr,
	    "Usage:  %s -p name [-d keydir]\n", progName);
    fprintf(stderr,
	    "        %s -P name [-d keydir]\n", progName);
    fprintf(stderr,
	    "        %s -D name [-d keydir]\n", progName);
    fprintf(stderr,
	    "        %s -l [-d keydir]\n", progName);
    fprintf(stderr,
	    "        %s -c [-d keydir]\n", progName);

    fprintf(stderr, "%-20s Pretty print public key info for named key\n",
	    "-p nickname");
    fprintf(stderr, "%-20s Pretty print private key info for named key\n",
	    "-P nickname");
    fprintf(stderr, "%-20s Delete named private key from the key database\n",
	    "-D nickname");
    fprintf(stderr, "%-20s List the nicknames for the keys in a database\n",
	    "-l");
    fprintf(stderr, "%-20s Change the key database password\n",
	    "-c");
    fprintf(stderr, "\n");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
	    "-d keydir");

    exit(-1);
}

int main(int argc, char **argv)
{
    int o, changePassword, deleteKey, dumpPublicKey, dumpPrivateKey, list;
    char *nickname;
    SECStatus rv;
    SECKEYKeyDBHandle *keyHandle;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    /* Parse command line arguments */
    changePassword = deleteKey = dumpPublicKey = dumpPrivateKey = list = 0;
    nickname = NULL;

    while ((o = getopt(argc, argv, "ADP:cd:glp:")) != -1) {
	switch (o) {
	  case '?':
	    Usage(progName);
	    break;

	  case 'A':
	    fprintf(stderr, "%s: Can no longer add a key.", progName);
	    fprintf(stderr, " Use pkcs12 to import a key.\n\n");
	    Usage(progName);
	    break;

	  case 'D':
	    deleteKey = 1;
	    nickname = optarg;
	    break;

	  case 'P':
	    dumpPrivateKey = 1;
	    nickname = optarg;
	    break;

	  case 'c':
	    changePassword = 1;
	    break;

	  case 'd':
	    SECU_ConfigDirectory(optarg);
	    break;

	  case 'g':
	    fprintf(stderr, "%s: Can no longer generate a key.", progName);
	    fprintf(stderr, " Use certutil to generate a cert request.\n\n");
	    Usage(progName);
	    break;

	  case 'l':
	    list = 1;
	    break;

	  case 'p':
	    dumpPublicKey = 1;
	    nickname = optarg;
	    break;
	}
    }

    if (dumpPublicKey+changePassword+dumpPrivateKey+list+deleteKey != 1)
	Usage(progName);

    if ((list || changePassword) && nickname)
	Usage(progName);

    if ((dumpPublicKey || dumpPrivateKey || deleteKey) && !nickname)
	Usage(progName);


    /* Call the libsec initialization routines */
    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    SEC_Init();

    /*
     * XXX Note that the following opens the key database writable.
     * If dumpPublicKey or dumpPrivateKey or list, though, we only want
     * to open it read-only.  There needs to be a better interface
     * to the initialization routines so that we can specify which way
     * to open it.
     */
    rv = SECU_PKCS11Init();
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "SECU_PKCS11Init failed");
	return -1;
    }

    keyHandle = SECKEY_GetDefaultKeyDB();
    if (keyHandle == NULL) {
        SECU_PrintError(progName, "could not open key database");
	return -1;
    }

    SECU_RegisterDynamicOids();
    if (dumpPublicKey) {
	rv = DumpPublicKey(keyHandle, nickname, stdout);
    } else
    if (changePassword) {
	rv = ChangePassword(keyHandle);
    } else
    if (dumpPrivateKey) {
	rv = DumpPrivateKey(keyHandle, nickname, stdout);
    } else
    if (list) {
	rv = ListKeys(keyHandle, stdout);
    } else
    if (deleteKey) {
	rv = DeletePrivateKey(keyHandle, nickname);
    }

    
    return rv ? -1 : 0;
}
