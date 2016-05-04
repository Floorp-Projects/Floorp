/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Regression test for bug 588269
 *
 * SECMOD_CloseUserDB should properly close the user DB, and it should
 * be possible to later re-add that same user DB as a new slot
 */

#include "secutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nspr.h"
#include "nss.h"
#include "secerr.h"
#include "pk11pub.h"
#include "plgetopt.h"

void
Usage(char *progName)
{
    fprintf(stderr, "Usage: %s -d dbDir\n", progName);
    exit(1);
}

SECStatus
TestOpenCloseUserDB(char *progName, char *configDir, char *tokenName)
{
    char *modspec = NULL;
    SECStatus rv = SECSuccess;
    PK11SlotInfo *userDbSlot = NULL;

    printf("Loading database <%s> - %s\n", configDir, tokenName);
    modspec = PR_smprintf("configDir='%s' tokenDescription='%s'",
                          configDir, tokenName);
    if (!modspec) {
        rv = SECFailure;
        goto loser;
    }

    userDbSlot = SECMOD_OpenUserDB(modspec);
    PR_smprintf_free(modspec);
    if (!userDbSlot) {
        SECU_PrintError(progName, "couldn't open database");
        rv = SECFailure;
        goto loser;
    }

    printf("Closing database\n");
    rv = SECMOD_CloseUserDB(userDbSlot);

    if (rv != SECSuccess) {
        SECU_PrintError(progName, "couldn't close database");
    }

    PK11_FreeSlot(userDbSlot);

loser:
    return rv;
}

int
main(int argc, char **argv)
{
    PLOptState *optstate;
    PLOptStatus optstatus;
    SECStatus rv = SECFailure;
    char *progName;
    char *dbDir = NULL;

    progName = strrchr(argv[0], '/');
    if (!progName) {
        progName = strrchr(argv[0], '\\');
    }
    progName = progName ? progName + 1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "d:");
    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case 'd':
                dbDir = strdup(optstate->value);
                break;
        }
    }
    if (optstatus == PL_OPT_BAD || dbDir == NULL) {
        Usage(progName);
    }

    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "unable to initialize NSS");
        goto loser;
    }

    printf("Open and Close Test 1\n");
    rv = TestOpenCloseUserDB(progName, dbDir, "Test Slot 1");
    if (rv != SECSuccess) {
        goto loser;
    }

    printf("Open and Close Test 2\n");
    rv = TestOpenCloseUserDB(progName, dbDir, "Test Slot 2");
    if (rv != SECSuccess) {
        goto loser;
    }

loser:
    if (dbDir)
        free(dbDir);

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }
    PR_Cleanup();

    if (rv != SECSuccess) {
        exit(1);
    }

    return 0;
}
