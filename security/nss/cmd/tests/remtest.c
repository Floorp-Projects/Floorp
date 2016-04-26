/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
**
** Sample client side test program that uses SSL and NSS
**
*/

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#else
#include "ctype.h"	/* for isalpha() */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "nspr.h"
#include "prio.h"
#include "prnetdb.h"
#include "nss.h"
#include "pk11func.h"
#include "plgetopt.h"

void
Usage(char *progName) 
{
    fprintf(stderr,"usage: %s [-d profiledir] -t tokenName [-r]\n", progName);
    exit(1);
}

int main(int argc, char **argv)
{
    char *             certDir  =  NULL;
    PLOptState *optstate;
    PLOptStatus optstatus;
    SECStatus rv;
    char * tokenName = NULL;
    PRBool cont=PR_TRUE;
    PK11TokenEvent event = PK11TokenPresentEvent;
    PK11TokenStatus status;
    char *progName;
    PK11SlotInfo *slot;

    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "rd:t:");
    while ((optstatus = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {

	  case 'd':
	    certDir = strdup(optstate->value);
	    certDir = SECU_ConfigDirectory(certDir);
	    break;
	  case 't':
	    tokenName = strdup(optstate->value);
	    break;
	  case 'r':
	    event = PK11TokenRemovedOrChangedEvent;
	    break;
	}
    }
    if (optstatus == PL_OPT_BAD)
	Usage(progName);

    if (tokenName == NULL) {
	Usage(progName);
    }

    if (!certDir) {
	certDir = SECU_DefaultSSLDir();	/* Look in $SSL_DIR */
	certDir = SECU_ConfigDirectory(certDir); /* call even if it's NULL */
    }

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    /* open the cert DB, the key DB, and the secmod DB. */
    rv = NSS_Init(certDir);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "unable to open cert database");
	return 1;
    }

    printf("Looking up tokenNamed: <%s>\n",tokenName);
    slot = PK11_FindSlotByName(tokenName);
    if (slot == NULL) {
	SECU_PrintError(progName, "unable to find token");
	return 1;
    }

    do {
	status = 
	   PK11_WaitForTokenEvent(slot,event,PR_INTERVAL_NO_TIMEOUT, 0, 0);

	switch (status) {
	case PK11TokenNotRemovable:
	    cont = PR_FALSE;
	    printf("%s Token Not Removable\n",tokenName);
	    break;
	case PK11TokenChanged:
	    event = PK11TokenRemovedOrChangedEvent;
	    printf("%s Token Changed\n", tokenName);
	    break;
	case PK11TokenRemoved:
	    event = PK11TokenPresentEvent;
	    printf("%s Token Removed\n", tokenName);
	    break;
	case PK11TokenPresent:
	    event = PK11TokenRemovedOrChangedEvent;
	    printf("%s Token Present\n", tokenName);
	    break;
	}
    } while (cont);

    PK11_FreeSlot(slot);

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }
    PR_Cleanup();
    return 0;
}
