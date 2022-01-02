/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tool for converting builtin CA certs.
 */

#include "nssrenam.h"
#include "nss.h"
#include "cert.h"
#include "certdb.h"
#include "secutil.h"
#include "pk11func.h"

#if defined(WIN32)
#include <fcntl.h>
#include <io.h>
#endif

void
dumpbytes(unsigned char *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if ((i != 0) && ((i & 0xf) == 0)) {
            printf("\n");
        }
        printf("\\%03o", buf[i]);
    }
    printf("\n");
}

int
hasPositiveTrust(unsigned int trust)
{
    if (trust & CERTDB_TRUSTED) {
        if (trust & CERTDB_TRUSTED_CA) {
            return PR_TRUE;
        } else {
            return PR_FALSE;
        }
    } else {
        if (trust & CERTDB_TRUSTED_CA) {
            return PR_TRUE;
        } else if (trust & CERTDB_VALID_CA) {
            return PR_TRUE;
        } else if (trust & CERTDB_TERMINAL_RECORD) {
            return PR_FALSE;
        } else {
            return PR_FALSE;
        }
    }
    return PR_FALSE;
}

char *
getTrustString(unsigned int trust)
{
    if (trust & CERTDB_TRUSTED) {
        if (trust & CERTDB_TRUSTED_CA) {
            return "CKT_NSS_TRUSTED_DELEGATOR";
        } else {
            return "CKT_NSS_TRUSTED";
        }
    } else {
        if (trust & CERTDB_TRUSTED_CA) {
            return "CKT_NSS_TRUSTED_DELEGATOR";
        } else if (trust & CERTDB_VALID_CA) {
            return "CKT_NSS_VALID_DELEGATOR";
        } else if (trust & CERTDB_TERMINAL_RECORD) {
            return "CKT_NSS_NOT_TRUSTED";
        } else {
            return "CKT_NSS_MUST_VERIFY_TRUST";
        }
    }
    return "CKT_NSS_TRUST_UNKNOWN"; /* not reached */
}

static const SEC_ASN1Template serialTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(CERTCertificate, serialNumber) },
    { 0 }
};

void
print_crl_info(CERTName *name, SECItem *serial)
{
    PRBool saveWrapeState = SECU_GetWrapEnabled();
    SECU_EnableWrap(PR_FALSE);

    SECU_PrintNameQuotesOptional(stdout, name, "# Issuer", 0, PR_FALSE);
    printf("\n");

    SECU_PrintInteger(stdout, serial, "# Serial Number", 0);

    SECU_EnableWrap(saveWrapeState);
}

static SECStatus
ConvertCRLEntry(SECItem *sdder, PRInt32 crlentry, char *nickname)
{
    int rv;
    PLArenaPool *arena = NULL;
    CERTSignedCrl *newCrl = NULL;
    CERTCrlEntry *entry;

    CERTName *name = NULL;
    SECItem *derName = NULL;
    SECItem *serial = NULL;

    rv = SEC_ERROR_NO_MEMORY;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
        return rv;

    newCrl = CERT_DecodeDERCrlWithFlags(arena, sdder, SEC_CRL_TYPE,
                                        CRL_DECODE_DEFAULT_OPTIONS);
    if (!newCrl)
        return SECFailure;

    name = &newCrl->crl.name;
    derName = &newCrl->crl.derName;

    if (newCrl->crl.entries != NULL) {
        PRInt32 iv = 0;
        while ((entry = newCrl->crl.entries[iv++]) != NULL) {
            if (crlentry == iv) {
                serial = &entry->serialNumber;
                break;
            }
        }
    }

    if (!name || !derName || !serial)
        return SECFailure;

    printf("\n# Distrust \"%s\"\n", nickname);
    print_crl_info(name, serial);

    printf("CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST\n");
    printf("CKA_TOKEN CK_BBOOL CK_TRUE\n");
    printf("CKA_PRIVATE CK_BBOOL CK_FALSE\n");
    printf("CKA_MODIFIABLE CK_BBOOL CK_FALSE\n");
    printf("CKA_LABEL UTF8 \"%s\"\n", nickname);

    printf("CKA_ISSUER MULTILINE_OCTAL\n");
    dumpbytes(derName->data, derName->len);
    printf("END\n");
    printf("CKA_SERIAL_NUMBER MULTILINE_OCTAL\n");
    printf("\\002\\%03o", serial->len); /* 002: type integer; len >=3 digits */
    dumpbytes(serial->data, serial->len);
    printf("END\n");

    printf("CKA_TRUST_SERVER_AUTH CK_TRUST CKT_NSS_NOT_TRUSTED\n");
    printf("CKA_TRUST_EMAIL_PROTECTION CK_TRUST CKT_NSS_NOT_TRUSTED\n");
    printf("CKA_TRUST_CODE_SIGNING CK_TRUST CKT_NSS_NOT_TRUSTED\n");
    printf("CKA_TRUST_STEP_UP_APPROVED CK_BBOOL CK_FALSE\n");

    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

void
print_info(SECItem *sdder, CERTCertificate *c)
{
    PRBool saveWrapeState = SECU_GetWrapEnabled();
    SECU_EnableWrap(PR_FALSE);

    SECU_PrintNameQuotesOptional(stdout, &c->issuer, "# Issuer", 0, PR_FALSE);
    printf("\n");

    SECU_PrintInteger(stdout, &c->serialNumber, "# Serial Number", 0);

    SECU_PrintNameQuotesOptional(stdout, &c->subject, "# Subject", 0, PR_FALSE);
    printf("\n");

    SECU_PrintTimeChoice(stdout, &c->validity.notBefore, "# Not Valid Before", 0);
    SECU_PrintTimeChoice(stdout, &c->validity.notAfter, "# Not Valid After ", 0);

    SECU_PrintFingerprints(stdout, sdder, "# Fingerprint", 0);

    SECU_EnableWrap(saveWrapeState);
}

static SECStatus
ConvertCertificate(SECItem *sdder, char *nickname, CERTCertTrust *trust,
                   PRBool excludeCert, PRBool excludeHash)
{
    SECStatus rv = SECSuccess;
    CERTCertificate *cert;
    unsigned char sha1_hash[SHA1_LENGTH];
    unsigned char md5_hash[MD5_LENGTH];
    SECItem *serial = NULL;
    PRBool step_up = PR_FALSE;
    const char *trust_info;

    cert = CERT_DecodeDERCertificate(sdder, PR_FALSE, nickname);
    if (!cert) {
        return SECFailure;
    }
    serial = SEC_ASN1EncodeItem(NULL, NULL, cert, serialTemplate);
    if (!serial) {
        return SECFailure;
    }

    if (!excludeCert) {
        printf("\n#\n# Certificate \"%s\"\n#\n", nickname);
        print_info(sdder, cert);
        printf("CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE\n");
        printf("CKA_TOKEN CK_BBOOL CK_TRUE\n");
        printf("CKA_PRIVATE CK_BBOOL CK_FALSE\n");
        printf("CKA_MODIFIABLE CK_BBOOL CK_FALSE\n");
        printf("CKA_LABEL UTF8 \"%s\"\n", nickname);
        printf("CKA_CERTIFICATE_TYPE CK_CERTIFICATE_TYPE CKC_X_509\n");
        printf("CKA_SUBJECT MULTILINE_OCTAL\n");
        dumpbytes(cert->derSubject.data, cert->derSubject.len);
        printf("END\n");
        printf("CKA_ID UTF8 \"0\"\n");
        printf("CKA_ISSUER MULTILINE_OCTAL\n");
        dumpbytes(cert->derIssuer.data, cert->derIssuer.len);
        printf("END\n");
        printf("CKA_SERIAL_NUMBER MULTILINE_OCTAL\n");
        dumpbytes(serial->data, serial->len);
        printf("END\n");
        printf("CKA_VALUE MULTILINE_OCTAL\n");
        dumpbytes(sdder->data, sdder->len);
        printf("END\n");
        if (hasPositiveTrust(trust->sslFlags) ||
            hasPositiveTrust(trust->emailFlags) ||
            hasPositiveTrust(trust->objectSigningFlags)) {
            printf("CKA_NSS_MOZILLA_CA_POLICY CK_BBOOL CK_TRUE\n");
        }
        printf("CKA_NSS_SERVER_DISTRUST_AFTER CK_BBOOL CK_FALSE\n");
        printf("CKA_NSS_EMAIL_DISTRUST_AFTER CK_BBOOL CK_FALSE\n");
    }

    if ((trust->sslFlags | trust->emailFlags | trust->objectSigningFlags) ==
        CERTDB_TERMINAL_RECORD)
        trust_info = "Distrust";
    else
        trust_info = "Trust for";

    printf("\n# %s \"%s\"\n", trust_info, nickname);
    print_info(sdder, cert);

    printf("CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST\n");
    printf("CKA_TOKEN CK_BBOOL CK_TRUE\n");
    printf("CKA_PRIVATE CK_BBOOL CK_FALSE\n");
    printf("CKA_MODIFIABLE CK_BBOOL CK_FALSE\n");
    printf("CKA_LABEL UTF8 \"%s\"\n", nickname);

    if (!excludeHash) {
        PK11_HashBuf(SEC_OID_SHA1, sha1_hash, sdder->data, sdder->len);
        printf("CKA_CERT_SHA1_HASH MULTILINE_OCTAL\n");
        dumpbytes(sha1_hash, SHA1_LENGTH);
        printf("END\n");
        PK11_HashBuf(SEC_OID_MD5, md5_hash, sdder->data, sdder->len);
        printf("CKA_CERT_MD5_HASH MULTILINE_OCTAL\n");
        dumpbytes(md5_hash, MD5_LENGTH);
        printf("END\n");
    }

    printf("CKA_ISSUER MULTILINE_OCTAL\n");
    dumpbytes(cert->derIssuer.data, cert->derIssuer.len);
    printf("END\n");
    printf("CKA_SERIAL_NUMBER MULTILINE_OCTAL\n");
    dumpbytes(serial->data, serial->len);
    printf("END\n");

    printf("CKA_TRUST_SERVER_AUTH CK_TRUST %s\n",
           getTrustString(trust->sslFlags));
    printf("CKA_TRUST_EMAIL_PROTECTION CK_TRUST %s\n",
           getTrustString(trust->emailFlags));
    printf("CKA_TRUST_CODE_SIGNING CK_TRUST %s\n",
           getTrustString(trust->objectSigningFlags));
#ifdef notdef
    printf("CKA_TRUST_CLIENT_AUTH CK_TRUST CKT_NSS_TRUSTED\n");
    printf("CKA_TRUST_DIGITAL_SIGNATURE CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_NON_REPUDIATION CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_KEY_ENCIPHERMENT CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_DATA_ENCIPHERMENT CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_KEY_AGREEMENT CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_KEY_CERT_SIGN CK_TRUST CKT_NSS_TRUSTED_DELEGATOR\n");
#endif

    step_up = (trust->sslFlags & CERTDB_GOVT_APPROVED_CA);
    printf("CKA_TRUST_STEP_UP_APPROVED CK_BBOOL %s\n",
           step_up ? "CK_TRUE" : "CK_FALSE");

    PORT_Free(sdder->data);
    return (rv);
}

void
printheader()
{
    printf("# \n"
           "# This Source Code Form is subject to the terms of the Mozilla Public\n"
           "# License, v. 2.0. If a copy of the MPL was not distributed with this\n"
           "# file, You can obtain one at http://mozilla.org/MPL/2.0/.\n"
           "#\n"
           "# certdata.txt\n"
           "#\n"
           "# This file contains the object definitions for the certs and other\n"
           "# information \"built into\" NSS.\n"
           "#\n"
           "# Object definitions:\n"
           "#\n"
           "#    Certificates\n"
           "#\n"
           "#  -- Attribute --               -- type --          -- value --\n"
           "#  CKA_CLASS                     CK_OBJECT_CLASS     CKO_CERTIFICATE\n"
           "#  CKA_TOKEN                     CK_BBOOL            CK_TRUE\n"
           "#  CKA_PRIVATE                   CK_BBOOL            CK_FALSE\n"
           "#  CKA_MODIFIABLE                CK_BBOOL            CK_FALSE\n"
           "#  CKA_LABEL                     UTF8                (varies)\n"
           "#  CKA_CERTIFICATE_TYPE          CK_CERTIFICATE_TYPE CKC_X_509\n"
           "#  CKA_SUBJECT                   DER+base64          (varies)\n"
           "#  CKA_ID                        byte array          (varies)\n"
           "#  CKA_ISSUER                    DER+base64          (varies)\n"
           "#  CKA_SERIAL_NUMBER             DER+base64          (varies)\n"
           "#  CKA_VALUE                     DER+base64          (varies)\n"
           "#  CKA_NSS_EMAIL                 ASCII7              (unused here)\n"
           "#  CKA_NSS_SERVER_DISTRUST_AFTER DER+base64          (varies)\n"
           "#  CKA_NSS_EMAIL_DISTRUST_AFTER  DER+base64          (varies)\n"
           "#\n"
           "#    Trust\n"
           "#\n"
           "#  -- Attribute --              -- type --          -- value --\n"
           "#  CKA_CLASS                    CK_OBJECT_CLASS     CKO_TRUST\n"
           "#  CKA_TOKEN                    CK_BBOOL            CK_TRUE\n"
           "#  CKA_PRIVATE                  CK_BBOOL            CK_FALSE\n"
           "#  CKA_MODIFIABLE               CK_BBOOL            CK_FALSE\n"
           "#  CKA_LABEL                    UTF8                (varies)\n"
           "#  CKA_ISSUER                   DER+base64          (varies)\n"
           "#  CKA_SERIAL_NUMBER            DER+base64          (varies)\n"
           "#  CKA_CERT_HASH                binary+base64       (varies)\n"
           "#  CKA_EXPIRES                  CK_DATE             (not used here)\n"
           "#  CKA_TRUST_DIGITAL_SIGNATURE  CK_TRUST            (varies)\n"
           "#  CKA_TRUST_NON_REPUDIATION    CK_TRUST            (varies)\n"
           "#  CKA_TRUST_KEY_ENCIPHERMENT   CK_TRUST            (varies)\n"
           "#  CKA_TRUST_DATA_ENCIPHERMENT  CK_TRUST            (varies)\n"
           "#  CKA_TRUST_KEY_AGREEMENT      CK_TRUST            (varies)\n"
           "#  CKA_TRUST_KEY_CERT_SIGN      CK_TRUST            (varies)\n"
           "#  CKA_TRUST_CRL_SIGN           CK_TRUST            (varies)\n"
           "#  CKA_TRUST_SERVER_AUTH        CK_TRUST            (varies)\n"
           "#  CKA_TRUST_CLIENT_AUTH        CK_TRUST            (varies)\n"
           "#  CKA_TRUST_CODE_SIGNING       CK_TRUST            (varies)\n"
           "#  CKA_TRUST_EMAIL_PROTECTION   CK_TRUST            (varies)\n"
           "#  CKA_TRUST_IPSEC_END_SYSTEM   CK_TRUST            (varies)\n"
           "#  CKA_TRUST_IPSEC_TUNNEL       CK_TRUST            (varies)\n"
           "#  CKA_TRUST_IPSEC_USER         CK_TRUST            (varies)\n"
           "#  CKA_TRUST_TIME_STAMPING      CK_TRUST            (varies)\n"
           "#  (other trust attributes can be defined)\n"
           "#\n"
           "\n"
           "#\n"
           "# The object to tell NSS that this is a root list and we don't\n"
           "# have to go looking for others.\n"
           "#\n"
           "BEGINDATA\n"
           "CKA_CLASS CK_OBJECT_CLASS CKO_NSS_BUILTIN_ROOT_LIST\n"
           "CKA_TOKEN CK_BBOOL CK_TRUE\n"
           "CKA_PRIVATE CK_BBOOL CK_FALSE\n"
           "CKA_MODIFIABLE CK_BBOOL CK_FALSE\n"
           "CKA_LABEL UTF8 \"Mozilla Builtin Roots\"\n");
}

static void
Usage(char *progName)
{
    fprintf(stderr, "%s -t trust -n nickname [-i certfile] [-c] [-h]\n", progName);
    fprintf(stderr,
            "\tRead a der-encoded cert from certfile or stdin, and output\n"
            "\tit to stdout in a format suitable for the builtin root module.\n"
            "\tExample: %s -n MyCA -t \"C,C,C\" -i myca.der >> certdata.txt\n",
            progName);
    fprintf(stderr, "%s -D -n label [-i certfile]\n", progName);
    fprintf(stderr,
            "\tRead a der-encoded cert from certfile or stdin, and output\n"
            "\ta distrust record.\n"
            "\t(-D is equivalent to -t p,p,p -c -h)\n");
    fprintf(stderr, "%s -C -e crl-entry-number -n label [-i crlfile]\n", progName);
    fprintf(stderr,
            "\tRead a CRL from crlfile or stdin, and output\n"
            "\ta distrust record (issuer+serial).\n"
            "\t(-C implies -c -h)\n");
    fprintf(stderr, "%-15s trust flags (cCTpPuw).\n", "-t trust");
    fprintf(stderr, "%-15s nickname to assign to builtin cert, or\n",
            "-n nickname");
    fprintf(stderr, "%-15s a label for the distrust record.\n", "");
    fprintf(stderr, "%-15s exclude the certificate (only add a trust record)\n", "-c");
    fprintf(stderr, "%-15s exclude hash from trust record\n", "-h");
    fprintf(stderr, "%-15s     (useful to distrust any matching issuer/serial)\n", "");
    fprintf(stderr, "%-15s     (not allowed when adding positive trust)\n", "");
    fprintf(stderr, "%-15s a CRL entry number, as shown by \"crlutil -S\"\n", "-e");
    fprintf(stderr, "%-15s input file to read (default stdin)\n", "-i file");
    fprintf(stderr, "%-15s     (pipe through atob if the cert is b64-encoded)\n", "");
    fprintf(stderr, "%-15s convert a timestamp to DER, and output.\n", "-d timestamp");
    fprintf(stderr, "%-15s useful to fill server and email distrust fields\n", "");
    fprintf(stderr, "%-15s Example: %s -d 1561939200\n", "", progName);
    fprintf(stderr, "%-15s NOTE: The informed timestamp are interpreted as seconds\n", "");
    fprintf(stderr, "%-15s since unix epoch.\n", "");
    fprintf(stderr, "%-15s TIP: date -d \"2019-07-01 00:00:00 UTC\" +%%s\n", "");
    exit(-1);
}

enum {
    opt_Input = 0,
    opt_Nickname,
    opt_Trust,
    opt_Distrust,
    opt_ExcludeCert,
    opt_ExcludeHash,
    opt_DistrustCRL,
    opt_CRLEntry,
    opt_ConvertDate
};

static secuCommandFlag addbuiltin_options[] = {
    { /* opt_Input         */ 'i', PR_TRUE, 0, PR_FALSE },
    { /* opt_Nickname      */ 'n', PR_TRUE, 0, PR_FALSE },
    { /* opt_Trust         */ 't', PR_TRUE, 0, PR_FALSE },
    { /* opt_Distrust      */ 'D', PR_FALSE, 0, PR_FALSE },
    { /* opt_ExcludeCert   */ 'c', PR_FALSE, 0, PR_FALSE },
    { /* opt_ExcludeHash   */ 'h', PR_FALSE, 0, PR_FALSE },
    { /* opt_DistrustCRL   */ 'C', PR_FALSE, 0, PR_FALSE },
    { /* opt_CRLEntry      */ 'e', PR_TRUE, 0, PR_FALSE },
    { /* opt_ConvertDate   */ 'd', PR_TRUE, 0, PR_FALSE },
};

int
main(int argc, char **argv)
{
    SECStatus rv;
    char *nickname = NULL;
    char *trusts = NULL;
    char *progName;
    PRFileDesc *infile;
    CERTCertTrust trust = { 0 };
    SECItem derItem = { 0 };
    PRInt32 crlentry = 0;
    PRInt32 mutuallyExclusiveOpts = 0;
    PRBool decodeTrust = PR_FALSE;

    secuCommand addbuiltin = { 0 };
    addbuiltin.numOptions = sizeof(addbuiltin_options) / sizeof(secuCommandFlag);
    addbuiltin.options = addbuiltin_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &addbuiltin);

    if (rv != SECSuccess)
        Usage(progName);

    if (addbuiltin.options[opt_ConvertDate].activated) {
        char *endPtr;
        PRTime distrustTimestamp = strtol(addbuiltin.options[opt_ConvertDate].arg, &endPtr, 0) * PR_USEC_PER_SEC;
        if (*endPtr != '\0' && distrustTimestamp > 0) {
            Usage(progName);
            exit(1);
        }
        SECItem encTime;
        DER_EncodeTimeChoice(NULL, &encTime, distrustTimestamp);
        SECU_PrintTimeChoice(stdout, &encTime, "The timestamp represents this date", 0);
        printf("Locate the entry of the desired certificate in certdata.txt\n"
               "Erase the CKA_NSS_[SERVER|EMAIL]_DISTRUST_AFTER CK_BBOOL CK_FALSE\n"
               "And override with the following respective entry:\n\n");
        SECU_PrintTimeChoice(stdout, &encTime, "# For Server Distrust After", 0);
        printf("CKA_NSS_SERVER_DISTRUST_AFTER MULTILINE_OCTAL\n");
        dumpbytes(encTime.data, encTime.len);
        printf("END\n");
        SECU_PrintTimeChoice(stdout, &encTime, "# For Email Distrust After", 0);
        printf("CKA_NSS_EMAIL_DISTRUST_AFTER MULTILINE_OCTAL\n");
        dumpbytes(encTime.data, encTime.len);
        printf("END\n");
        exit(0);
    }

    if (addbuiltin.options[opt_Trust].activated)
        ++mutuallyExclusiveOpts;
    if (addbuiltin.options[opt_Distrust].activated)
        ++mutuallyExclusiveOpts;
    if (addbuiltin.options[opt_DistrustCRL].activated)
        ++mutuallyExclusiveOpts;

    if (mutuallyExclusiveOpts != 1) {
        fprintf(stderr, "%s: you must specify exactly one of -t or -D or -C\n",
                progName);
        Usage(progName);
    }

    if (addbuiltin.options[opt_DistrustCRL].activated) {
        if (!addbuiltin.options[opt_CRLEntry].activated) {
            fprintf(stderr, "%s: you must specify the CRL entry number.\n",
                    progName);
            Usage(progName);
        } else {
            crlentry = atoi(addbuiltin.options[opt_CRLEntry].arg);
            if (crlentry < 1) {
                fprintf(stderr, "%s: The CRL entry number must be > 0.\n",
                        progName);
                Usage(progName);
            }
        }
    }

    if (!addbuiltin.options[opt_Nickname].activated) {
        fprintf(stderr, "%s: you must specify parameter -n (a nickname or a label).\n",
                progName);
        Usage(progName);
    }

    if (addbuiltin.options[opt_Input].activated) {
        infile = PR_Open(addbuiltin.options[opt_Input].arg, PR_RDONLY, 00660);
        if (!infile) {
            fprintf(stderr, "%s: failed to open input file.\n", progName);
            exit(1);
        }
    } else {
#if defined(WIN32)
        /* If we're going to read binary data from stdin, we must put stdin
	** into O_BINARY mode or else incoming \r\n's will become \n's,
	** and latin-1 characters will be altered.
	*/

        int smrv = _setmode(_fileno(stdin), _O_BINARY);
        if (smrv == -1) {
            fprintf(stderr,
                    "%s: Cannot change stdin to binary mode. Use -i option instead.\n",
                    progName);
            exit(1);
        }
#endif
        infile = PR_STDIN;
    }

#if defined(WIN32)
    /* We must put stdout into O_BINARY mode or else the output will include
    ** carriage returns.
    */
    {
        int smrv = _setmode(_fileno(stdout), _O_BINARY);
        if (smrv == -1) {
            fprintf(stderr, "%s: Cannot change stdout to binary mode.\n", progName);
            exit(1);
        }
    }
#endif

    nickname = strdup(addbuiltin.options[opt_Nickname].arg);

    NSS_NoDB_Init(NULL);

    if (addbuiltin.options[opt_Distrust].activated ||
        addbuiltin.options[opt_DistrustCRL].activated) {
        addbuiltin.options[opt_ExcludeCert].activated = PR_TRUE;
        addbuiltin.options[opt_ExcludeHash].activated = PR_TRUE;
    }

    if (addbuiltin.options[opt_Distrust].activated) {
        trusts = strdup("p,p,p");
        decodeTrust = PR_TRUE;
    } else if (addbuiltin.options[opt_Trust].activated) {
        trusts = strdup(addbuiltin.options[opt_Trust].arg);
        decodeTrust = PR_TRUE;
    }

    if (decodeTrust) {
        rv = CERT_DecodeTrustString(&trust, trusts);
        if (rv) {
            fprintf(stderr, "%s: incorrectly formatted trust string.\n", progName);
            Usage(progName);
        }
    }

    if (addbuiltin.options[opt_Trust].activated &&
        addbuiltin.options[opt_ExcludeHash].activated) {
        if ((trust.sslFlags | trust.emailFlags | trust.objectSigningFlags) !=
            CERTDB_TERMINAL_RECORD) {
            fprintf(stderr, "%s: Excluding the hash only allowed with distrust.\n", progName);
            Usage(progName);
        }
    }

    SECU_FileToItem(&derItem, infile);

    /*printheader();*/

    if (addbuiltin.options[opt_DistrustCRL].activated) {
        rv = ConvertCRLEntry(&derItem, crlentry, nickname);
    } else {
        rv = ConvertCertificate(&derItem, nickname, &trust,
                                addbuiltin.options[opt_ExcludeCert].activated,
                                addbuiltin.options[opt_ExcludeHash].activated);
        if (rv) {
            fprintf(stderr, "%s: failed to convert certificate.\n", progName);
            exit(1);
        }
    }

    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    return (SECSuccess);
}
