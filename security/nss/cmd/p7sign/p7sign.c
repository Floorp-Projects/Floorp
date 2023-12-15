/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * p7sign -- A command to create a *detached* pkcs7 signature (over a given
 * input file).
 */

#include "nspr.h"
#include "plgetopt.h"
#include "secutil.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "sechash.h" /* for HASH_GetHashObject() */
#include "nss.h"
#include "pk11func.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

#if (defined(XP_WIN) && !defined(WIN32)) || (defined(__sun) && !defined(SVR4))
extern int fread(char *, size_t, size_t, FILE *);
extern int fwrite(char *, size_t, size_t, FILE *);
extern int fprintf(FILE *, char *, ...);
#endif

static secuPWData pwdata = { PW_NONE, 0 };

static void
Usage(char *progName)
{
    HASH_HashType hashAlg;

    fprintf(stderr,
            "Usage:  %s -k keyname [-d keydir] [-i input] [-o output] [-e]\n",
            progName);
    fprintf(stderr,
            "        %*s [-p password|-f password file] [-a hash] [-u certusage]\n",
            (int)strlen(progName), "");
    fprintf(stderr, "%-20s Nickname of key to use for signature\n",
            "-k keyname");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
            "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
            "-o output");
    fprintf(stderr, "%-20s Encapsulate content in signature message\n",
            "-e");
    fprintf(stderr, "%-20s Password to the key databse\n", "-p password");
    fprintf(stderr, "%-20s File to read password from\n", "-f password file");
    fprintf(stderr, "%-20s Use case-insensitive hash algorithm (default: SHA-1)\n",
            "-a hash");
    fprintf(stderr, "%-25s  ", "");
    for (hashAlg = HASH_AlgNULL + 1; hashAlg != HASH_AlgTOTAL; ++hashAlg)
        fprintf(stderr, "%s%s", hashAlg == HASH_AlgNULL + 1 ? "" : ", ",
                SECOID_FindOIDByTag(HASH_GetHashOidTagByHashType(hashAlg))->desc);
    fputc('\n', stderr);
    fprintf(stderr, "%-20s Sign for usage (default: certUsageEmailSigner)\n",
            "-u certusage");
    fprintf(stderr, "%-25s  0 - certUsageSSLClient\n", "");
    fprintf(stderr, "%-25s  1 - certUsageSSLServer\n", "");
    fprintf(stderr, "%-25s  2 - certUsageSSLServerWithStepUp\n", "");
    fprintf(stderr, "%-25s  3 - certUsageSSLCA\n", "");
    fprintf(stderr, "%-25s  4 - certUsageEmailSigner\n", "");
    fprintf(stderr, "%-25s  5 - certUsageEmailRecipient\n", "");
    fprintf(stderr, "%-25s  6 - certUsageObjectSigner\n", "");
    fprintf(stderr, "%-25s  7 - certUsageUserCertImport\n", "");
    fprintf(stderr, "%-25s  8 - certUsageVerifyCA\n", "");
    fprintf(stderr, "%-25s  9 - certUsageProtectedObjectSigner\n", "");
    fprintf(stderr, "%-25s 10 - certUsageStatusResponder\n", "");
    fprintf(stderr, "%-25s 11 - certUsageAnyCA\n", "");
    fprintf(stderr, "%-25s 12 - certUsageIPsec\n", "");
    exit(-1);
}

static void
SignOut(void *arg, const char *buf, unsigned long len)
{
    FILE *out;

    out = (FILE *)arg;
    fwrite(buf, len, 1, out);
}

static int
CreateDigest(SECItem *data, char *digestdata, unsigned int *len,
             unsigned int maxlen, HASH_HashType hashAlg)
{
    const SECHashObject *hashObj;
    void *hashcx;

    hashObj = HASH_GetHashObject(hashAlg);

    hashcx = (*hashObj->create)();
    if (hashcx == NULL)
        return -1;

    (*hashObj->begin)(hashcx);
    (*hashObj->update)(hashcx, data->data, data->len);
    (*hashObj->end)(hashcx, (unsigned char *)digestdata, len, maxlen);
    (*hashObj->destroy)(hashcx, PR_TRUE);
    return 0;
}

static int
SignFile(FILE *outFile, PRFileDesc *inFile, CERTCertificate *cert,
         PRBool encapsulated, HASH_HashType hashAlg, SECOidTag hashAlgOid,
         SECCertUsage usage)
{
    char digestdata[HASH_LENGTH_MAX];
    unsigned int len;
    SECItem digest, data2sign;
    SEC_PKCS7ContentInfo *cinfo;
    SECStatus rv;

    if (outFile == NULL || inFile == NULL || cert == NULL)
        return -1;

    /* suck the file in */
    if (SECU_ReadDERFromFile(&data2sign, inFile, PR_FALSE,
                             PR_FALSE) != SECSuccess)
        return -1;

    if (!encapsulated) {
        /* unfortunately, we must create the digest ourselves */
        /* SEC_PKCS7CreateSignedData should have a flag to not include */
        /* the content for non-encapsulated content at encode time, but */
        /* should always compute the hash itself */
        if (CreateDigest(&data2sign, digestdata, &len,
                         sizeof(digestdata), hashAlg) < 0) {
            SECITEM_FreeItem(&data2sign, PR_FALSE);
            return -1;
        }
        digest.data = (unsigned char *)digestdata;
        digest.len = len;
    }

    cinfo = SEC_PKCS7CreateSignedData(cert, usage, NULL,
                                      hashAlgOid,
                                      encapsulated ? NULL : &digest,
                                      NULL, NULL);
    if (cinfo == NULL) {
        SECITEM_FreeItem(&data2sign, PR_FALSE);
        return -1;
    }

    if (encapsulated) {
        SEC_PKCS7SetContent(cinfo, (char *)data2sign.data, data2sign.len);
    }

    rv = SEC_PKCS7IncludeCertChain(cinfo, NULL);
    if (rv != SECSuccess) {
        SEC_PKCS7DestroyContentInfo(cinfo);
        SECITEM_FreeItem(&data2sign, PR_FALSE);
        return -1;
    }

    rv = SEC_PKCS7Encode(cinfo, SignOut, outFile, NULL,
                         NULL, &pwdata);

    SECITEM_FreeItem(&data2sign, PR_FALSE);
    SEC_PKCS7DestroyContentInfo(cinfo);

    if (rv != SECSuccess)
        return -1;

    return 0;
}

int
main(int argc, char **argv)
{
    char *progName;
    FILE *outFile;
    PRFileDesc *inFile;
    char *keyName = NULL;
    CERTCertDBHandle *certHandle;
    CERTCertificate *cert = NULL;
    PRBool encapsulated = PR_FALSE;
    HASH_HashType hashAlg = HASH_AlgSHA1;
    SECOidTag hashAlgOid = SEC_OID_SHA1;
    SECCertUsage usage = certUsageEmailSigner;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus rv;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    inFile = NULL;
    outFile = NULL;
    keyName = NULL;

    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "ed:k:i:o:p:f:a:u:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
                Usage(progName);
                break;

            case 'e':
                /* create a message with the signed content encapsulated */
                encapsulated = PR_TRUE;
                break;

            case 'd':
                SECU_ConfigDirectory(optstate->value);
                break;

            case 'i':
                inFile = PR_Open(optstate->value, PR_RDONLY, 0);
                if (!inFile) {
                    fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
                            progName, optstate->value);
                    return -1;
                }
                break;

            case 'k':
                keyName = strdup(optstate->value);
                break;

            case 'o':
                outFile = fopen(optstate->value, "wb");
                if (!outFile) {
                    fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
                            progName, optstate->value);
                    return -1;
                }
                break;
            case 'p':
                pwdata.source = PW_PLAINTEXT;
                pwdata.data = strdup(optstate->value);
                break;

            case 'f':
                pwdata.source = PW_FROMFILE;
                pwdata.data = PORT_Strdup(optstate->value);
                break;

            case 'a':
                for (hashAlg = HASH_AlgNULL + 1; hashAlg != HASH_AlgTOTAL;
                     ++hashAlg) {
                    hashAlgOid = HASH_GetHashOidTagByHashType(hashAlg);
                    if (!PORT_Strcasecmp(optstate->value,
                                         SECOID_FindOIDByTag(hashAlgOid)->desc))
                        break;
                }
                if (hashAlg == HASH_AlgTOTAL)
                    Usage(progName);
                break;

            case 'u':
                usage = atoi(optstate->value);
                if (usage < certUsageSSLClient || usage > certUsageIPsec)
                    Usage(progName);
                break;
        }
    }
    PL_DestroyOptState(optstate);

    if (!keyName)
        Usage(progName);

    if (!inFile)
        inFile = PR_STDIN;
    if (!outFile)
        outFile = stdout;

    /* Call the initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_Init(SECU_ConfigDirectory(NULL));
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError(progName);
        goto loser;
    }

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    /* open cert database */
    certHandle = CERT_GetDefaultCertDB();
    if (certHandle == NULL) {
        rv = SECFailure;
        goto loser;
    }

    /* find cert */
    cert = SECU_FindCertByNicknameOrFilename(certHandle, keyName, PR_FALSE, NULL);
    if (cert == NULL) {
        SECU_PrintError(progName,
                        "the corresponding cert for key \"%s\" does not exist",
                        keyName);
        rv = SECFailure;
        goto loser;
    }

    if (SignFile(outFile, inFile, cert, encapsulated,
                 hashAlg, hashAlgOid, usage)) {
        SECU_PrintError(progName, "problem signing data");
        rv = SECFailure;
        goto loser;
    }

loser:
    if (pwdata.data) {
        PORT_Free(pwdata.data);
    }
    if (keyName) {
        PORT_Free(keyName);
    }
    if (cert) {
        CERT_DestroyCertificate(cert);
    }
    if (inFile && inFile != PR_STDIN) {
        PR_Close(inFile);
    }
    if (outFile && outFile != stdout) {
        fclose(outFile);
    }
    if (NSS_Shutdown() != SECSuccess) {
        SECU_PrintError(progName, "NSS shutdown:");
        exit(1);
    }

    return (rv != SECSuccess);
}
