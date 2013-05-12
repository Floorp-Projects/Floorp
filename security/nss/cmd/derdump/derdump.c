/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secutil.h"
#include "nss.h"
#include <errno.h>

#if defined(XP_WIN) || (defined(__sun) && !defined(SVR4))
#if !defined(WIN32)
extern int fprintf(FILE *, char *, ...);
#endif
#endif
#include "plgetopt.h"

static void Usage(char *progName)
{
    fprintf(stderr,
	    "Usage: %s [-r] [-i input] [-o output]\n",
	    progName);
    fprintf(stderr, "%-20s For formatted items, dump raw bytes as well\n",
	    "-r");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
	    "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    exit(-1);
}

int main(int argc, char **argv)
{
    char *progName;
    FILE *outFile;
    PRFileDesc *inFile;
    SECItem der;
    SECStatus rv;
    PRInt16 xp_error;
    PRBool raw = PR_FALSE;
    PLOptState *optstate;
    PLOptStatus status;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    /* Parse command line arguments */
    inFile = 0;
    outFile = 0;
    optstate = PL_CreateOptState(argc, argv, "i:o:r");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case 'i':
	    inFile = PR_Open(optstate->value, PR_RDONLY, 0);
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "w");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'r':
	    raw = PR_TRUE;
	    break;

	  default:
	    Usage(progName);
	    break;
	}
    }
	if (status == PL_OPT_BAD)
		Usage(progName);

    if (!inFile) inFile = PR_STDIN;
    if (!outFile) outFile = stdout;

    rv = NSS_NoDB_Init(NULL);	/* XXX */
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }

	rv = SECU_ReadDERFromFile(&der, inFile, PR_FALSE);
    if (rv == SECSuccess) {
	rv = DER_PrettyPrint(outFile, &der, raw);
	if (rv == SECSuccess)
	    return 0;
    }

    xp_error = PORT_GetError();
    if (xp_error) {
	SECU_PrintError(progName, "error %d", xp_error);
    }
    if (errno) {
	SECU_PrintSystemError(progName, "errno=%d", errno);
    }
    return 1;
}
