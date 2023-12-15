/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secutil.h"
#include "pk11func.h"
#include "sechash.h"
#include "secoid.h"

#if defined(XP_WIN) || (defined(__sun) && !defined(SVR4))
#if !defined(WIN32)
extern int fread(char *, size_t, size_t, FILE *);
extern int fwrite(char *, size_t, size_t, FILE *);
extern int fprintf(FILE *, char *, ...);
#endif
#endif

#include "plgetopt.h"

static SECOidData *
HashNameToOID(const char *hashName)
{
    HASH_HashType htype;
    SECOidData *hashOID;

    for (htype = HASH_AlgNULL + 1; htype < HASH_AlgTOTAL; htype++) {
        hashOID = SECOID_FindOIDByTag(HASH_GetHashOidTagByHashType(htype));
        if (PORT_Strcasecmp(hashName, hashOID->desc) == 0)
            break;
    }

    if (htype == HASH_AlgTOTAL)
        return NULL;

    return hashOID;
}

static void
Usage(char *progName)
{
    HASH_HashType htype;

    fprintf(stderr,
            "Usage:  %s -t type [-i input] [-o output]\n",
            progName);
    fprintf(stderr, "%-20s Specify the digest method (must be one of\n",
            "-t type");
    fprintf(stderr, "%-20s ", "");
    for (htype = HASH_AlgNULL + 1; htype < HASH_AlgTOTAL; htype++) {
        fputs(SECOID_FindOIDByTag(HASH_GetHashOidTagByHashType(htype))->desc,
              stderr);
        if (htype == (HASH_AlgTOTAL - 2))
            fprintf(stderr, " or ");
        else if (htype != (HASH_AlgTOTAL - 1))
            fprintf(stderr, ", ");
    }
    fprintf(stderr, " (case ignored))\n");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
            "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
            "-o output");
    exit(-1);
}

static int
DigestFile(FILE *outFile, FILE *inFile, SECOidData *hashOID)
{
    int nb;
    unsigned char ibuf[4096], digest[HASH_LENGTH_MAX];
    PK11Context *hashcx;
    unsigned int len;
    SECStatus rv;

    hashcx = PK11_CreateDigestContext(hashOID->offset);
    if (hashcx == NULL) {
        return -1;
    }
    PK11_DigestBegin(hashcx);

    for (;;) {
        if (feof(inFile))
            break;
        nb = fread(ibuf, 1, sizeof(ibuf), inFile);
        if (nb != sizeof(ibuf)) {
            if (nb == 0) {
                if (ferror(inFile)) {
                    PORT_SetError(SEC_ERROR_IO);
                    PK11_DestroyContext(hashcx, PR_TRUE);
                    return -1;
                }
                /* eof */
                break;
            }
        }
        rv = PK11_DigestOp(hashcx, ibuf, nb);
        if (rv != SECSuccess) {
            PK11_DestroyContext(hashcx, PR_TRUE);
            return -1;
        }
    }

    rv = PK11_DigestFinal(hashcx, digest, &len, HASH_LENGTH_MAX);
    PK11_DestroyContext(hashcx, PR_TRUE);

    if (rv != SECSuccess)
        return -1;

    nb = fwrite(digest, 1, len, outFile);
    if (nb != len) {
        PORT_SetError(SEC_ERROR_IO);
        return -1;
    }

    return 0;
}

#include "nss.h"

int
main(int argc, char **argv)
{
    char *progName;
    FILE *inFile, *outFile;
    char *hashName;
    SECOidData *hashOID;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus rv;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    inFile = NULL;
    outFile = NULL;
    hashName = NULL;

    rv = NSS_Init("/tmp");
    if (rv != SECSuccess) {
        fprintf(stderr, "%s: NSS_Init failed in directory %s\n",
                progName, "/tmp");
        return -1;
    }

    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "t:i:o:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
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
                outFile = fopen(optstate->value, "w");
                if (!outFile) {
                    fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
                            progName, optstate->value);
                    return -1;
                }
                break;

            case 't':
                hashName = strdup(optstate->value);
                break;
        }
    }

    if (!hashName)
        Usage(progName);

    if (!inFile)
        inFile = stdin;
    if (!outFile)
        outFile = stdout;

    hashOID = HashNameToOID(hashName);
    free(hashName);
    if (hashOID == NULL) {
        fprintf(stderr, "%s: invalid digest type\n", progName);
        Usage(progName);
    }

    if (DigestFile(outFile, inFile, hashOID)) {
        fprintf(stderr, "%s: problem digesting data (%s)\n",
                progName, SECU_Strerror(PORT_GetError()));
        return -1;
    }

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    return 0;
}
