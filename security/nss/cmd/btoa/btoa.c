/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plgetopt.h"
#include "secutil.h"
#include "nssb64.h"
#include <errno.h>

#if defined(XP_WIN) || (defined(__sun) && !defined(SVR4))
#if !defined(WIN32)
extern int fread(char *, size_t, size_t, FILE*);
extern int fwrite(char *, size_t, size_t, FILE*);
extern int fprintf(FILE *, char *, ...);
#endif
#endif

#if defined(WIN32)
#include "fcntl.h"
#include "io.h"
#endif

static PRInt32 
output_ascii (void *arg, const char *obuf, PRInt32 size)
{
    FILE *outFile = arg;
    int nb;

    nb = fwrite(obuf, 1, size, outFile);
    if (nb != size) {
	PORT_SetError(SEC_ERROR_IO);
	return -1;
    }

    return nb;
}

static SECStatus
encode_file(FILE *outFile, FILE *inFile)
{
    NSSBase64Encoder *cx;
    int nb;
    SECStatus status = SECFailure;
    unsigned char ibuf[4096];

    cx = NSSBase64Encoder_Create(output_ascii, outFile);
    if (!cx) {
	return -1;
    }

    for (;;) {
	if (feof(inFile)) break;
	nb = fread(ibuf, 1, sizeof(ibuf), inFile);
	if (nb != sizeof(ibuf)) {
	    if (nb == 0) {
		if (ferror(inFile)) {
		    PORT_SetError(SEC_ERROR_IO);
		    goto loser;
		}
		/* eof */
		break;
	    }
	}

	status = NSSBase64Encoder_Update(cx, ibuf, nb);
	if (status != SECSuccess) goto loser;
    }

    status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
    if (status != SECSuccess)
	return status;

    /*
     * Add a trailing CRLF.  Note this must be done *after* the call
     * to Destroy above (because only then are we sure all data has
     * been written out).
     */
    fwrite("\r\n", 1, 2, outFile);
    return SECSuccess;

  loser:
    (void) NSSBase64Encoder_Destroy(cx, PR_TRUE);
    return status;
}

static void Usage(char *progName)
{
    fprintf(stderr,
	    "Usage: %s [-i input] [-o output]\n",
	    progName);
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
	    "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    exit(-1);
}

int main(int argc, char **argv)
{
    char *progName;
    SECStatus rv;
    FILE *inFile, *outFile;
    PLOptState *optstate;
    PLOptStatus status;

    inFile = 0;
    outFile = 0;
    progName = strrchr(argv[0], '/');
    if (!progName)
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    /* Parse command line arguments */
    optstate = PL_CreateOptState(argc, argv, "i:o:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  default:
	    Usage(progName);
	    break;

	  case 'i':
	    inFile = fopen(optstate->value, "rb");
	    if (!inFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		return -1;
	    }
	    break;

	  case 'o':
	    outFile = fopen(optstate->value, "wb");
	    if (!outFile) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		return -1;
	    }
	    break;
	}
    }
    if (status == PL_OPT_BAD)
	Usage(progName);
    if (!inFile) {
#if defined(WIN32)
	/* If we're going to read binary data from stdin, we must put stdin
	** into O_BINARY mode or else incoming \r\n's will become \n's.
	*/

	int smrv = _setmode(_fileno(stdin), _O_BINARY);
	if (smrv == -1) {
	    fprintf(stderr,
	    "%s: Cannot change stdin to binary mode. Use -i option instead.\n",
	            progName);
	    return smrv;
	}
#endif
    	inFile = stdin;
    }
    if (!outFile) {
#if defined(WIN32)
	/* We're going to write binary data to stdout. We must put stdout
	** into O_BINARY mode or else outgoing \r\n's will become \r\r\n's.
	*/

	int smrv = _setmode(_fileno(stdout), _O_BINARY);
	if (smrv == -1) {
	    fprintf(stderr,
	    "%s: Cannot change stdout to binary mode. Use -o option instead.\n",
	            progName);
	    return smrv;
	}
#endif
    	outFile = stdout;
    }
    rv = encode_file(outFile, inFile);
    if (rv != SECSuccess) {
	fprintf(stderr, "%s: lossage: error=%d errno=%d\n",
		progName, PORT_GetError(), errno);
	return -1;
    }
    return 0;
}
