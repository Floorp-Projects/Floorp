/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plgetopt.h"
#include "secutil.h"
#include "nssb64.h"
#include <errno.h>

#if defined(XP_WIN) || (defined(__sun) && !defined(SVR4))
#if !defined(WIN32)
extern int fread(char *, size_t, size_t, FILE *);
extern int fwrite(char *, size_t, size_t, FILE *);
extern int fprintf(FILE *, char *, ...);
#endif
#endif

#if defined(WIN32)
#include "fcntl.h"
#include "io.h"
#endif

static PRInt32
output_binary(void *arg, const unsigned char *obuf, PRInt32 size)
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

static PRBool
isBase64Char(char c)
{
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '+' || c == '/' ||
            c == '=');
}

static SECStatus
decode_file(FILE *outFile, FILE *inFile)
{
    NSSBase64Decoder *cx;
    SECStatus status = SECFailure;
    char ibuf[4096];
    const char *ptr;

    cx = NSSBase64Decoder_Create(output_binary, outFile);
    if (!cx) {
        return -1;
    }

    for (;;) {
        if (feof(inFile))
            break;
        if (!fgets(ibuf, sizeof(ibuf), inFile)) {
            if (ferror(inFile)) {
                PORT_SetError(SEC_ERROR_IO);
                goto loser;
            }
            /* eof */
            break;
        }
        for (ptr = ibuf; *ptr; ++ptr) {
            char c = *ptr;
            if (c == '\n' || c == '\r') {
                break; /* found end of line */
            }
            if (!isBase64Char(c)) {
                ptr = ibuf; /* ignore line */
                break;
            }
        }
        if (ibuf == ptr) {
            continue; /* skip empty or non-base64 line */
        }

        status = NSSBase64Decoder_Update(cx, ibuf, ptr - ibuf);
        if (status != SECSuccess)
            goto loser;
    }

    return NSSBase64Decoder_Destroy(cx, PR_FALSE);

loser:
    (void)NSSBase64Decoder_Destroy(cx, PR_TRUE);
    return status;
}

static void
Usage(char *progName)
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

int
main(int argc, char **argv)
{
    char *progName;
    SECStatus rv;
    FILE *inFile, *outFile;
    PLOptState *optstate;
    PLOptStatus status;

    inFile = 0;
    outFile = 0;
    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    /* Parse command line arguments */
    optstate = PL_CreateOptState(argc, argv, "?hi:o:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
            case 'h':
                Usage(progName);
                break;

            case 'i':
                inFile = fopen(optstate->value, "r");
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
    if (!inFile)
        inFile = stdin;
    if (!outFile) {
#if defined(WIN32)
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
    rv = decode_file(outFile, inFile);
    if (rv != SECSuccess) {
        fprintf(stderr, "%s: lossage: error=%d errno=%d\n",
                progName, PORT_GetError(), errno);
        return -1;
    }
    return 0;
}
