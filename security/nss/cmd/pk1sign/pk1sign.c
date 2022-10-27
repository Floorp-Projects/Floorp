/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * pk1sign
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
#include "cryptohi.h"
#include "plbase64.h"

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

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

SEC_ASN1Template CERTSignatureDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(CERTSignedData) },
    { SEC_ASN1_INLINE,
      offsetof(CERTSignedData, signatureAlgorithm),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_BIT_STRING,
      offsetof(CERTSignedData, signature) },
    { 0 }
};

static void
Usage(char *progName)
{
    fprintf(stderr,
            "Usage:  %s -k keyname [-d keydir] [-i input] [-o output]\n",
            progName);
    fprintf(stderr, "%-20s Nickname of key to use for signature\n",
            "-k keyname");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Define an input file to use (default is stdin)\n",
            "-i input");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
            "-o output");
    fprintf(stderr, "%-20s Password to the key databse\n", "-p");
    fprintf(stderr, "%-20s password file\n", "-f");
    exit(-1);
}

static int
ExportPublicKey(FILE *outFile, CERTCertificate *cert)
{
    char *data;
    SECKEYPublicKey *publicKey;
    SECItem *item;

    if (!cert)
        return -1;

    publicKey = CERT_ExtractPublicKey(cert);
    if (!publicKey)
        return -1;

    item = SECKEY_EncodeDERSubjectPublicKeyInfo(publicKey);
    SECKEY_DestroyPublicKey(publicKey);
    if (!item)
        return -1;

    data = PL_Base64Encode((const char *)item->data, item->len, NULL);
    SECITEM_FreeItem(item, PR_TRUE);
    if (!data)
        return -1;

    fputs("pubkey:\n", outFile);
    fputs(data, outFile);
    fputs("\n", outFile);
    PR_Free(data);

    return 0;
}

static int
SignFile(FILE *outFile, PRFileDesc *inFile, CERTCertificate *cert)
{
    SECItem data2sign;
    SECStatus rv;
    SECOidTag algID;
    CERTSignedData sd;
    SECKEYPrivateKey *privKey = NULL;
    char *data = NULL;
    PLArenaPool *arena = NULL;
    SECItem *result = NULL;
    int returnValue = 0;

    if (outFile == NULL || inFile == NULL || cert == NULL) {
        return -1;
    }

    /* suck the file in */
    if (SECU_ReadDERFromFile(&data2sign, inFile, PR_FALSE,
                             PR_FALSE) != SECSuccess) {
        return -1;
    }

    privKey = NULL;
    privKey = PK11_FindKeyByAnyCert(cert, NULL);
    if (!privKey) {
        returnValue = -1;
        goto loser;
    }

    algID = SEC_GetSignatureAlgorithmOidTag(privKey->keyType, SEC_OID_SHA1);
    if (algID == SEC_OID_UNKNOWN) {
        returnValue = -1;
        goto loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    PORT_Memset(&sd, 0, sizeof(CERTSignedData));

    rv = SEC_SignData(&(sd.signature), data2sign.data, data2sign.len, privKey, algID);
    if (rv != SECSuccess) {
        fprintf(stderr, "Could not sign.\n");
        returnValue = -1;
        goto loser;
    }
    sd.signature.len = sd.signature.len << 3;

    rv = SECOID_SetAlgorithmID(arena, &sd.signatureAlgorithm, algID, 0);
    if (rv != SECSuccess) {
        fprintf(stderr, "Could not set alg id.\n");
        returnValue = -1;
        goto loser;
    }

    result = SEC_ASN1EncodeItem(arena, NULL, &sd, CERTSignatureDataTemplate);
    SECITEM_FreeItem(&(sd.signature), PR_FALSE);

    if (!result) {
        fprintf(stderr, "Could not encode.\n");
        returnValue = -1;
        goto loser;
    }

    data = PL_Base64Encode((const char *)result->data, result->len, NULL);
    if (!data) {
        returnValue = -1;
        goto loser;
    }

    fputs("signature:\n", outFile);
    fputs(data, outFile);
    fputs("\n", outFile);
    ExportPublicKey(outFile, cert);

loser:
    if (privKey) {
        SECKEY_DestroyPrivateKey(privKey);
    }
    if (data) {
        PR_Free(data);
    }
    PORT_FreeArena(arena, PR_FALSE);

    return returnValue;
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
    optstate = PL_CreateOptState(argc, argv, "ed:k:i:o:p:f:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
                Usage(progName);
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
        }
    }

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
    cert = CERT_FindCertByNickname(certHandle, keyName);
    if (cert == NULL) {
        SECU_PrintError(progName,
                        "the corresponding cert for key \"%s\" does not exist",
                        keyName);
        rv = SECFailure;
        goto loser;
    }

    if (SignFile(outFile, inFile, cert)) {
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
