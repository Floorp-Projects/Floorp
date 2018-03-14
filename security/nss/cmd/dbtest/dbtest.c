/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** dbtest.c
**
** QA test for cert and key databases, especially to open
** database readonly (NSS_INIT_READONLY) and force initializations
** even if the databases cannot be opened (NSS_INIT_FORCEOPEN)
**
*/
#include <stdio.h>
#include <string.h>

#if defined(WIN32)
#include "fcntl.h"
#include "io.h"
#endif

#include "secutil.h"
#include "pk11pub.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include "nspr.h"
#include "prtypes.h"
#include "certdb.h"
#include "nss.h"
#include "../modutil/modutil.h"

#include "plgetopt.h"

static char *progName;

char *dbDir = NULL;

static char *dbName[] = { "secmod.db", "cert8.db", "key3.db" };
static char *dbprefix = "";
static char *secmodName = "secmod.db";
static char *userPassword = "";
PRBool verbose;

static char *
getPassword(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    int *success = (int *)arg;

    if (retry) {
        *success = 0;
        return NULL;
    }

    *success = 1;
    return PORT_Strdup(userPassword);
}

static void
Usage(const char *progName)
{
    printf("Usage:  %s [-r] [-f] [-i] [-d dbdir ] \n",
           progName);
    printf("%-20s open database readonly (NSS_INIT_READONLY)\n", "-r");
    printf("%-20s Continue to force initializations even if the\n", "-f");
    printf("%-20s databases cannot be opened (NSS_INIT_FORCEOPEN)\n", " ");
    printf("%-20s Try to initialize the database\n", "-i");
    printf("%-20s Supply a password with which to initialize the db\n", "-p");
    printf("%-20s Directory with cert database (default is .\n",
           "-d certdir");
    exit(1);
}

int
main(int argc, char **argv)
{
    PLOptState *optstate;
    PLOptStatus optstatus;

    PRUint32 flags = 0;
    Error ret;
    SECStatus rv;
    char *dbString = NULL;
    PRBool doInitTest = PR_FALSE;
    int i;

    progName = strrchr(argv[0], '/');
    if (!progName)
        progName = strrchr(argv[0], '\\');
    progName = progName ? progName + 1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "rfip:d:h");

    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case 'h':
            default:
                Usage(progName);
                break;

            case 'r':
                flags |= NSS_INIT_READONLY;
                break;

            case 'f':
                flags |= NSS_INIT_FORCEOPEN;
                break;

            case 'i':
                doInitTest = PR_TRUE;
                break;

            case 'p':
                userPassword = PORT_Strdup(optstate->value);
                break;

            case 'd':
                dbDir = PORT_Strdup(optstate->value);
                break;
        }
    }
    PL_DestroyOptState(optstate);
    if (optstatus == PL_OPT_BAD)
        Usage(progName);

    if (dbDir) {
        char *tmp = dbDir;
        dbDir = SECU_ConfigDirectory(tmp);
        PORT_Free(tmp);
    } else {
        /* Look in $SSL_DIR */
        dbDir = SECU_ConfigDirectory(SECU_DefaultSSLDir());
    }
    PR_fprintf(PR_STDERR, "dbdir selected is %s\n\n", dbDir);

    if (dbDir[0] == '\0') {
        PR_fprintf(PR_STDERR, errStrings[DIR_DOESNT_EXIST_ERR], dbDir);
        ret = DIR_DOESNT_EXIST_ERR;
        goto loser;
    }

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    /* get the status of the directory and databases and output message */
    if (PR_Access(dbDir, PR_ACCESS_EXISTS) != PR_SUCCESS) {
        PR_fprintf(PR_STDERR, errStrings[DIR_DOESNT_EXIST_ERR], dbDir);
    } else if (PR_Access(dbDir, PR_ACCESS_READ_OK) != PR_SUCCESS) {
        PR_fprintf(PR_STDERR, errStrings[DIR_NOT_READABLE_ERR], dbDir);
    } else {
        if (!(flags & NSS_INIT_READONLY) &&
            PR_Access(dbDir, PR_ACCESS_WRITE_OK) != PR_SUCCESS) {
            PR_fprintf(PR_STDERR, errStrings[DIR_NOT_WRITEABLE_ERR], dbDir);
        }
        if (!doInitTest) {
            for (i = 0; i < 3; i++) {
                dbString = PR_smprintf("%s/%s", dbDir, dbName[i]);
                PR_fprintf(PR_STDOUT, "database checked is %s\n", dbString);
                if (PR_Access(dbString, PR_ACCESS_EXISTS) != PR_SUCCESS) {
                    PR_fprintf(PR_STDERR, errStrings[FILE_DOESNT_EXIST_ERR],
                               dbString);
                } else if (PR_Access(dbString, PR_ACCESS_READ_OK) != PR_SUCCESS) {
                    PR_fprintf(PR_STDERR, errStrings[FILE_NOT_READABLE_ERR],
                               dbString);
                } else if (!(flags & NSS_INIT_READONLY) &&
                           PR_Access(dbString, PR_ACCESS_WRITE_OK) != PR_SUCCESS) {
                    PR_fprintf(PR_STDERR, errStrings[FILE_NOT_WRITEABLE_ERR],
                               dbString);
                }
                PR_smprintf_free(dbString);
            }
        }
    }

    rv = NSS_Initialize(SECU_ConfigDirectory(dbDir), dbprefix, dbprefix,
                        secmodName, flags);
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        ret = NSS_INITIALIZE_FAILED_ERR;
    } else {
        ret = SUCCESS;
        if (doInitTest) {
            PK11SlotInfo *slot = PK11_GetInternalKeySlot();
            SECStatus rv;
            int passwordSuccess = 0;
            int type = CKM_DES3_CBC;
            SECItem keyid = { 0, NULL, 0 };
            unsigned char keyIdData[] = { 0xff, 0xfe };
            PK11SymKey *key = NULL;

            keyid.data = keyIdData;
            keyid.len = sizeof(keyIdData);

            PK11_SetPasswordFunc(getPassword);
            rv = PK11_InitPin(slot, (char *)NULL, userPassword);
            if (rv != SECSuccess) {
                PR_fprintf(PR_STDERR, "Failed to Init DB: %s\n",
                           SECU_Strerror(PORT_GetError()));
                ret = CHANGEPW_FAILED_ERR;
            }
            if (*userPassword && !PK11_IsLoggedIn(slot, &passwordSuccess)) {
                PR_fprintf(PR_STDERR, "New DB did not log in after init\n");
                ret = AUTHENTICATION_FAILED_ERR;
            }
            /* generate a symetric key */
            key = PK11_TokenKeyGen(slot, type, NULL, 0, &keyid,
                                   PR_TRUE, &passwordSuccess);

            if (!key) {
                PR_fprintf(PR_STDERR, "Could not generated symetric key: %s\n",
                           SECU_Strerror(PORT_GetError()));
                exit(UNSPECIFIED_ERR);
            }
            PK11_FreeSymKey(key);
            PK11_Logout(slot);

            PK11_Authenticate(slot, PR_TRUE, &passwordSuccess);

            if (*userPassword && !passwordSuccess) {
                PR_fprintf(PR_STDERR, "New DB Did not initalize\n");
                ret = AUTHENTICATION_FAILED_ERR;
            }
            key = PK11_FindFixedKey(slot, type, &keyid, &passwordSuccess);

            if (!key) {
                PR_fprintf(PR_STDERR, "Could not find generated key: %s\n",
                           SECU_Strerror(PORT_GetError()));
                ret = UNSPECIFIED_ERR;
            } else {
                PK11_FreeSymKey(key);
            }
            PK11_FreeSlot(slot);
        }

        if (NSS_Shutdown() != SECSuccess) {
            PR_fprintf(PR_STDERR, "Could not find generated key: %s\n",
                       SECU_Strerror(PORT_GetError()));
            exit(1);
        }
    }

loser:
    return ret;
}
