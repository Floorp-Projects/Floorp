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
    int option;
    FILE *outFile;
    PRFileDesc *inFile;
    SECItem der;
    SECStatus rv;
    int16 xp_error;
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
