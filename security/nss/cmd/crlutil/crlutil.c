/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** certutil.c
**
** utility for managing certificates and the cert database
**
*/
/* test only */

#include "nspr.h"
#include "plgetopt.h"
#include "secutil.h"
#include "cert.h"
#include "certi.h"
#include "certdb.h"
#include "nss.h"
#include "pk11func.h"
#include "crlgen.h"

#define SEC_CERT_DB_EXISTS 0
#define SEC_CREATE_CERT_DB 1

static char *progName;

static CERTSignedCrl *
FindCRL(CERTCertDBHandle *certHandle, char *name, int type)
{
    CERTSignedCrl *crl = NULL;
    CERTCertificate *cert = NULL;
    SECItem derName;

    derName.data = NULL;
    derName.len = 0;

    cert = CERT_FindCertByNicknameOrEmailAddr(certHandle, name);
    if (!cert) {
        CERTName *certName = NULL;
        PLArenaPool *arena = NULL;
        SECStatus rv = SECSuccess;

        certName = CERT_AsciiToName(name);
        if (certName) {
            arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
            if (arena) {
                SECItem *nameItem =
                    SEC_ASN1EncodeItem(arena, NULL, (void *)certName,
                                       SEC_ASN1_GET(CERT_NameTemplate));
                if (nameItem) {
                    rv = SECITEM_CopyItem(NULL, &derName, nameItem);
                }
                PORT_FreeArena(arena, PR_FALSE);
            }
            CERT_DestroyName(certName);
        }

        if (rv != SECSuccess) {
            SECU_PrintError(progName, "SECITEM_CopyItem failed, out of memory");
            return ((CERTSignedCrl *)NULL);
        }

        if (!derName.len || !derName.data) {
            SECU_PrintError(progName, "could not find certificate named '%s'", name);
            return ((CERTSignedCrl *)NULL);
        }
    } else {
        SECITEM_CopyItem(NULL, &derName, &cert->derSubject);
        CERT_DestroyCertificate(cert);
    }

    crl = SEC_FindCrlByName(certHandle, &derName, type);
    if (crl == NULL)
        SECU_PrintError(progName, "could not find %s's CRL", name);
    if (derName.data) {
        SECITEM_FreeItem(&derName, PR_FALSE);
    }
    return (crl);
}

static SECStatus
DisplayCRL(CERTCertDBHandle *certHandle, char *nickName, int crlType)
{
    CERTSignedCrl *crl = NULL;

    crl = FindCRL(certHandle, nickName, crlType);

    if (crl) {
        SECU_PrintCRLInfo(stdout, &crl->crl, "CRL Info:\n", 0);
        SEC_DestroyCrl(crl);
        return SECSuccess;
    }
    return SECFailure;
}

static void
ListCRLNames(CERTCertDBHandle *certHandle, int crlType, PRBool deletecrls)
{
    CERTCrlHeadNode *crlList = NULL;
    CERTCrlNode *crlNode = NULL;
    CERTName *name = NULL;
    PLArenaPool *arena = NULL;
    SECStatus rv;

    do {
        arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
        if (arena == NULL) {
            fprintf(stderr, "%s: fail to allocate memory\n", progName);
            break;
        }

        name = PORT_ArenaZAlloc(arena, sizeof(*name));
        if (name == NULL) {
            fprintf(stderr, "%s: fail to allocate memory\n", progName);
            break;
        }
        name->arena = arena;

        rv = SEC_LookupCrls(certHandle, &crlList, crlType);
        if (rv != SECSuccess) {
            fprintf(stderr, "%s: fail to look up CRLs (%s)\n", progName,
                    SECU_Strerror(PORT_GetError()));
            break;
        }

        /* just in case */
        if (!crlList)
            break;

        crlNode = crlList->first;

        fprintf(stdout, "\n");
        fprintf(stdout, "\n%-40s %-5s\n\n", "CRL names", "CRL Type");
        while (crlNode) {
            char *asciiname = NULL;
            CERTCertificate *cert = NULL;
            if (crlNode->crl && crlNode->crl->crl.derName.data != NULL) {
                cert = CERT_FindCertByName(certHandle,
                                           &crlNode->crl->crl.derName);
                if (!cert) {
                    SECU_PrintError(progName, "could not find signing "
                                              "certificate in database");
                }
            }
            if (cert) {
                char *certName = NULL;
                if (cert->nickname && PORT_Strlen(cert->nickname) > 0) {
                    certName = cert->nickname;
                } else if (cert->emailAddr && PORT_Strlen(cert->emailAddr) > 0) {
                    certName = cert->emailAddr;
                }
                if (certName) {
                    asciiname = PORT_Strdup(certName);
                }
                CERT_DestroyCertificate(cert);
            }

            if (!asciiname) {
                name = &crlNode->crl->crl.name;
                if (!name) {
                    SECU_PrintError(progName, "fail to get the CRL "
                                              "issuer name");
                    continue;
                }
                asciiname = CERT_NameToAscii(name);
            }
            fprintf(stdout, "%-40s %-5s\n", asciiname, "CRL");
            if (asciiname) {
                PORT_Free(asciiname);
            }
            if (PR_TRUE == deletecrls) {
                CERTSignedCrl *acrl = NULL;
                SECItem *issuer = &crlNode->crl->crl.derName;
                acrl = SEC_FindCrlByName(certHandle, issuer, crlType);
                if (acrl) {
                    SEC_DeletePermCRL(acrl);
                    SEC_DestroyCrl(acrl);
                }
            }
            crlNode = crlNode->next;
        }

    } while (0);
    if (crlList)
        PORT_FreeArena(crlList->arena, PR_FALSE);
    PORT_FreeArena(arena, PR_FALSE);
}

static SECStatus
ListCRL(CERTCertDBHandle *certHandle, char *nickName, int crlType)
{
    if (nickName == NULL) {
        ListCRLNames(certHandle, crlType, PR_FALSE);
        return SECSuccess;
    }

    return DisplayCRL(certHandle, nickName, crlType);
}

static SECStatus
DeleteCRL(CERTCertDBHandle *certHandle, char *name, int type)
{
    CERTSignedCrl *crl = NULL;
    SECStatus rv = SECFailure;

    crl = FindCRL(certHandle, name, type);
    if (!crl) {
        SECU_PrintError(progName, "could not find the issuer %s's CRL", name);
        return SECFailure;
    }
    rv = SEC_DeletePermCRL(crl);
    SEC_DestroyCrl(crl);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "fail to delete the issuer %s's CRL "
                                  "from the perm database (reason: %s)",
                        name, SECU_Strerror(PORT_GetError()));
        return SECFailure;
    }
    return (rv);
}

SECStatus
ImportCRL(CERTCertDBHandle *certHandle, char *url, int type,
          PRFileDesc *inFile, PRInt32 importOptions, PRInt32 decodeOptions,
          secuPWData *pwdata)
{
    CERTSignedCrl *crl = NULL;
    SECItem crlDER;
    PK11SlotInfo *slot = NULL;
    int rv;
#if defined(DEBUG_jp96085)
    PRIntervalTime starttime, endtime, elapsed;
    PRUint32 mins, secs, msecs;
#endif

    crlDER.data = NULL;

    /* Read in the entire file specified with the -f argument */
    rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE, PR_FALSE);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "unable to read input file");
        return (SECFailure);
    }

    decodeOptions |= CRL_DECODE_DONT_COPY_DER;

    slot = PK11_GetInternalKeySlot();

    if (PK11_NeedLogin(slot)) {
        rv = PK11_Authenticate(slot, PR_TRUE, pwdata);
        if (rv != SECSuccess)
            goto loser;
    }

#if defined(DEBUG_jp96085)
    starttime = PR_IntervalNow();
#endif
    crl = PK11_ImportCRL(slot, &crlDER, url, type,
                         NULL, importOptions, NULL, decodeOptions);
#if defined(DEBUG_jp96085)
    endtime = PR_IntervalNow();
    elapsed = endtime - starttime;
    mins = PR_IntervalToSeconds(elapsed) / 60;
    secs = PR_IntervalToSeconds(elapsed) % 60;
    msecs = PR_IntervalToMilliseconds(elapsed) % 1000;
    printf("Elapsed : %2d:%2d.%3d\n", mins, secs, msecs);
#endif
    if (!crl) {
        const char *errString;

        rv = SECFailure;
        errString = SECU_Strerror(PORT_GetError());
        if (errString && PORT_Strlen(errString) == 0)
            SECU_PrintError(progName,
                            "CRL is not imported (error: input CRL is not up to date.)");
        else
            SECU_PrintError(progName, "unable to import CRL");
    } else {
        SEC_DestroyCrl(crl);
    }
loser:
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return (rv);
}

SECStatus
DumpCRL(PRFileDesc *inFile)
{
    int rv;
    PLArenaPool *arena = NULL;
    CERTSignedCrl *newCrl = NULL;

    SECItem crlDER;
    crlDER.data = NULL;

    /* Read in the entire file specified with the -f argument */
    rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE, PR_FALSE);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "unable to read input file");
        return (SECFailure);
    }

    rv = SEC_ERROR_NO_MEMORY;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
        return rv;

    newCrl = CERT_DecodeDERCrlWithFlags(arena, &crlDER, SEC_CRL_TYPE,
                                        CRL_DECODE_DEFAULT_OPTIONS);
    if (!newCrl)
        return SECFailure;

    SECU_PrintCRLInfo(stdout, &newCrl->crl, "CRL file contents", 0);

    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

static CERTCertificate *
FindSigningCert(CERTCertDBHandle *certHandle, CERTSignedCrl *signCrl,
                char *certNickName)
{
    CERTCertificate *cert = NULL, *certTemp = NULL;
    SECStatus rv = SECFailure;
    CERTAuthKeyID *authorityKeyID = NULL;
    SECItem *subject = NULL;

    PORT_Assert(certHandle != NULL);
    if (!certHandle || (!signCrl && !certNickName)) {
        SECU_PrintError(progName, "invalid args for function "
                                  "FindSigningCert \n");
        return NULL;
    }

    if (signCrl) {
#if 0
        authorityKeyID = SECU_FindCRLAuthKeyIDExten(tmpArena, scrl);
#endif
        subject = &signCrl->crl.derName;
    } else {
        certTemp = CERT_FindCertByNickname(certHandle, certNickName);
        if (!certTemp) {
            SECU_PrintError(progName, "could not find certificate \"%s\" "
                                      "in database",
                            certNickName);
            goto loser;
        }
        subject = &certTemp->derSubject;
    }

    cert = SECU_FindCrlIssuer(certHandle, subject, authorityKeyID, PR_Now());
    if (!cert) {
        SECU_PrintError(progName, "could not find signing certificate "
                                  "in database");
        goto loser;
    } else {
        rv = SECSuccess;
    }

loser:
    if (certTemp)
        CERT_DestroyCertificate(certTemp);
    if (cert && rv != SECSuccess)
        CERT_DestroyCertificate(cert);
    return cert;
}

static CERTSignedCrl *
CreateModifiedCRLCopy(PLArenaPool *arena, CERTCertDBHandle *certHandle,
                      CERTCertificate **cert, char *certNickName,
                      PRFileDesc *inFile, PRInt32 decodeOptions,
                      PRInt32 importOptions)
{
    SECItem crlDER = { 0, NULL, 0 };
    CERTSignedCrl *signCrl = NULL;
    CERTSignedCrl *modCrl = NULL;
    PLArenaPool *modArena = NULL;
    SECStatus rv = SECSuccess;

    if (!arena || !certHandle || !certNickName) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        SECU_PrintError(progName, "CreateModifiedCRLCopy: invalid args\n");
        return NULL;
    }

    modArena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (!modArena) {
        SECU_PrintError(progName, "fail to allocate memory\n");
        return NULL;
    }

    if (inFile != NULL) {
        rv = SECU_ReadDERFromFile(&crlDER, inFile, PR_FALSE, PR_FALSE);
        if (rv != SECSuccess) {
            SECU_PrintError(progName, "unable to read input file");
            PORT_FreeArena(modArena, PR_FALSE);
            goto loser;
        }

        decodeOptions |= CRL_DECODE_DONT_COPY_DER;

        modCrl = CERT_DecodeDERCrlWithFlags(modArena, &crlDER, SEC_CRL_TYPE,
                                            decodeOptions);
        if (!modCrl) {
            SECU_PrintError(progName, "fail to decode CRL");
            goto loser;
        }

        if (0 == (importOptions & CRL_IMPORT_BYPASS_CHECKS)) {
            /* If caCert is a v2 certificate, make sure that it
             * can be used for crl signing purpose */
            *cert = FindSigningCert(certHandle, modCrl, NULL);
            if (!*cert) {
                goto loser;
            }

            rv = CERT_VerifySignedData(&modCrl->signatureWrap, *cert,
                                       PR_Now(), NULL);
            if (rv != SECSuccess) {
                SECU_PrintError(progName, "fail to verify signed data\n");
                goto loser;
            }
        }
    } else {
        modCrl = FindCRL(certHandle, certNickName, SEC_CRL_TYPE);
        if (!modCrl) {
            SECU_PrintError(progName, "fail to find crl %s in database\n",
                            certNickName);
            goto loser;
        }
    }

    signCrl = PORT_ArenaZNew(arena, CERTSignedCrl);
    if (signCrl == NULL) {
        SECU_PrintError(progName, "fail to allocate memory\n");
        goto loser;
    }

    rv = SECU_CopyCRL(arena, &signCrl->crl, &modCrl->crl);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "unable to dublicate crl for "
                                  "modification.");
        goto loser;
    }

    /* Make sure the update time is current. It can be modified later
     * by "update <time>" command from crl generation script */
    rv = DER_EncodeTimeChoice(arena, &signCrl->crl.lastUpdate, PR_Now());
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "fail to encode current time\n");
        goto loser;
    }

    signCrl->arena = arena;

loser:
    if (crlDER.data) {
        SECITEM_FreeItem(&crlDER, PR_FALSE);
    }
    if (modCrl)
        SEC_DestroyCrl(modCrl);
    if (rv != SECSuccess && signCrl) {
        SEC_DestroyCrl(signCrl);
        signCrl = NULL;
    }
    return signCrl;
}

static CERTSignedCrl *
CreateNewCrl(PLArenaPool *arena, CERTCertDBHandle *certHandle,
             CERTCertificate *cert)
{
    CERTSignedCrl *signCrl = NULL;
    void *dummy = NULL;
    SECStatus rv;
    void *mark = NULL;

    /* if the CERTSignedCrl structure changes, this function will need to be
       updated as well */
    if (!cert || !arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        SECU_PrintError(progName, "invalid args for function "
                                  "CreateNewCrl\n");
        return NULL;
    }

    mark = PORT_ArenaMark(arena);

    signCrl = PORT_ArenaZNew(arena, CERTSignedCrl);
    if (signCrl == NULL) {
        SECU_PrintError(progName, "fail to allocate memory\n");
        return NULL;
    }

    dummy = SEC_ASN1EncodeInteger(arena, &signCrl->crl.version,
                                  SEC_CRL_VERSION_2);
    /* set crl->version */
    if (!dummy) {
        SECU_PrintError(progName, "fail to create crl version data "
                                  "container\n");
        goto loser;
    }

    /* copy SECItem name from cert */
    rv = SECITEM_CopyItem(arena, &signCrl->crl.derName, &cert->derSubject);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "fail to duplicate der name from "
                                  "certificate.\n");
        goto loser;
    }

    /* copy CERTName name structure from cert issuer */
    rv = CERT_CopyName(arena, &signCrl->crl.name, &cert->subject);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "fail to duplicate RD name from "
                                  "certificate.\n");
        goto loser;
    }

    rv = DER_EncodeTimeChoice(arena, &signCrl->crl.lastUpdate, PR_Now());
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "fail to encode current time\n");
        goto loser;
    }

    /* set fields */
    signCrl->arena = arena;
    signCrl->dbhandle = certHandle;
    signCrl->crl.arena = arena;

    return signCrl;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}

static SECStatus
UpdateCrl(CERTSignedCrl *signCrl, PRFileDesc *inCrlInitFile)
{
    CRLGENGeneratorData *crlGenData = NULL;
    SECStatus rv;

    if (!signCrl || !inCrlInitFile) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        SECU_PrintError(progName, "invalid args for function "
                                  "CreateNewCrl\n");
        return SECFailure;
    }

    crlGenData = CRLGEN_InitCrlGeneration(signCrl, inCrlInitFile);
    if (!crlGenData) {
        SECU_PrintError(progName, "can not initialize parser structure.\n");
        return SECFailure;
    }

    rv = CRLGEN_ExtHandleInit(crlGenData);
    if (rv == SECFailure) {
        SECU_PrintError(progName, "can not initialize entries handle.\n");
        goto loser;
    }

    rv = CRLGEN_StartCrlGen(crlGenData);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "crl generation failed");
        goto loser;
    }

loser:
    /* CommitExtensionsAndEntries is partially responsible for freeing
     * up memory that was used for CRL generation. Should be called regardless
     * of previouse call status, but only after initialization of
     * crlGenData was done. It will commit all changes that was done before
     * an error has occurred.
     */
    if (SECSuccess != CRLGEN_CommitExtensionsAndEntries(crlGenData)) {
        SECU_PrintError(progName, "crl generation failed");
        rv = SECFailure;
    }
    CRLGEN_FinalizeCrlGeneration(crlGenData);
    return rv;
}

static SECStatus
SignAndStoreCrl(CERTSignedCrl *signCrl, CERTCertificate *cert,
                char *outFileName, SECOidTag hashAlgTag, int ascii,
                char *slotName, char *url, secuPWData *pwdata)
{
    PK11SlotInfo *slot = NULL;
    PRFileDesc *outFile = NULL;
    SECStatus rv;
    SignAndEncodeFuncExitStat errCode;

    PORT_Assert(signCrl && (!ascii || outFileName));
    if (!signCrl || (ascii && !outFileName)) {
        SECU_PrintError(progName, "invalid args for function "
                                  "SignAndStoreCrl\n");
        return SECFailure;
    }

    if (!slotName || !PL_strcmp(slotName, "internal"))
        slot = PK11_GetInternalKeySlot();
    else
        slot = PK11_FindSlotByName(slotName);
    if (!slot) {
        SECU_PrintError(progName, "can not find requested slot");
        return SECFailure;
    }

    if (PK11_NeedLogin(slot)) {
        rv = PK11_Authenticate(slot, PR_TRUE, pwdata);
        if (rv != SECSuccess)
            goto loser;
    }

    rv = SECU_SignAndEncodeCRL(cert, signCrl, hashAlgTag, &errCode);
    if (rv != SECSuccess) {
        char *errMsg = NULL;
        switch (errCode) {
            case noKeyFound:
                errMsg = "No private key found of signing cert";
                break;

            case noSignatureMatch:
                errMsg = "Key and Algorithm OId are do not match";
                break;

            default:
            case failToEncode:
                errMsg = "Failed to encode crl structure";
                break;

            case failToSign:
                errMsg = "Failed to sign crl structure";
                break;

            case noMem:
                errMsg = "Can not allocate memory";
                break;
        }
        SECU_PrintError(progName, "%s\n", errMsg);
        goto loser;
    }

    if (outFileName) {
        outFile = PR_Open(outFileName, PR_WRONLY | PR_CREATE_FILE, PR_IRUSR | PR_IWUSR);
        if (!outFile) {
            SECU_PrintError(progName, "unable to open \"%s\" for writing\n",
                            outFileName);
            goto loser;
        }
    }

    rv = SECU_StoreCRL(slot, signCrl->derCrl, outFile, ascii, url);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "fail to save CRL\n");
    }

loser:
    if (outFile)
        PR_Close(outFile);
    if (slot)
        PK11_FreeSlot(slot);
    return rv;
}

static SECStatus
GenerateCRL(CERTCertDBHandle *certHandle, char *certNickName,
            PRFileDesc *inCrlInitFile, PRFileDesc *inFile,
            char *outFileName, int ascii, char *slotName,
            PRInt32 importOptions, char *alg, PRBool quiet,
            PRInt32 decodeOptions, char *url, secuPWData *pwdata,
            int modifyFlag)
{
    CERTCertificate *cert = NULL;
    CERTSignedCrl *signCrl = NULL;
    PLArenaPool *arena = NULL;
    SECStatus rv;
    SECOidTag hashAlgTag = SEC_OID_UNKNOWN;

    if (alg) {
        hashAlgTag = SECU_StringToSignatureAlgTag(alg);
        if (hashAlgTag == SEC_OID_UNKNOWN) {
            SECU_PrintError(progName, "%s -Z:  %s is not a recognized type.\n",
                            progName, alg);
            return SECFailure;
        }
    } else {
        hashAlgTag = SEC_OID_UNKNOWN;
    }

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (!arena) {
        SECU_PrintError(progName, "fail to allocate memory\n");
        return SECFailure;
    }

    if (modifyFlag == PR_TRUE) {
        signCrl = CreateModifiedCRLCopy(arena, certHandle, &cert, certNickName,
                                        inFile, decodeOptions, importOptions);
        if (signCrl == NULL) {
            rv = SECFailure;
            goto loser;
        }
    }

    if (!cert) {
        cert = FindSigningCert(certHandle, signCrl, certNickName);
        if (cert == NULL) {
            rv = SECFailure;
            goto loser;
        }
    }

    if (!signCrl) {
        if (modifyFlag == PR_TRUE) {
            if (!outFileName) {
                int len = strlen(certNickName) + 5;
                outFileName = PORT_ArenaAlloc(arena, len);
                PR_snprintf(outFileName, len, "%s.crl", certNickName);
            }
            SECU_PrintError(progName, "Will try to generate crl. "
                                      "It will be saved in file: %s",
                            outFileName);
        }
        signCrl = CreateNewCrl(arena, certHandle, cert);
        if (!signCrl) {
            rv = SECFailure;
            goto loser;
        }
    }

    rv = UpdateCrl(signCrl, inCrlInitFile);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SignAndStoreCrl(signCrl, cert, outFileName, hashAlgTag, ascii,
                         slotName, url, pwdata);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (signCrl && !quiet) {
        SECU_PrintCRLInfo(stdout, &signCrl->crl, "CRL Info:\n", 0);
    }

loser:
    if (arena && (!signCrl || !signCrl->arena))
        PORT_FreeArena(arena, PR_FALSE);
    if (signCrl)
        SEC_DestroyCrl(signCrl);
    if (cert)
        CERT_DestroyCertificate(cert);
    return (rv);
}

static void
Usage(char *progName)
{
    fprintf(stderr,
            "Usage:  %s -L [-n nickname] [-d keydir] [-P dbprefix] [-t crlType]\n"
            "        %s -D -n nickname [-d keydir] [-P dbprefix]\n"
            "        %s -S -i crl\n"
            "        %s -I -i crl -t crlType [-u url] [-d keydir] [-P dbprefix] [-B] "
            "[-p pwd-file] -w [pwd-string]\n"
            "        %s -E -t crlType [-d keydir] [-P dbprefix]\n"
            "        %s -T\n"
            "        %s -G|-M -c crl-init-file -n nickname [-i crl] [-u url] "
            "[-d keydir] [-P dbprefix] [-Z alg] ] [-p pwd-file] -w [pwd-string] "
            "[-a] [-B]\n",
            progName, progName, progName, progName, progName, progName, progName);

    fprintf(stderr, "%-15s List CRL\n", "-L");
    fprintf(stderr, "%-20s Specify the nickname of the CA certificate\n",
            "-n nickname");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
            "-P dbprefix");

    fprintf(stderr, "%-15s Delete a CRL from the cert database\n", "-D");
    fprintf(stderr, "%-20s Specify the nickname for the CA certificate\n",
            "-n nickname");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
            "-P dbprefix");

    fprintf(stderr, "%-15s Erase all CRLs of specified type from hte cert database\n", "-E");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
            "-P dbprefix");

    fprintf(stderr, "%-15s Show contents of a CRL file (without database)\n", "-S");
    fprintf(stderr, "%-20s Specify the file which contains the CRL to show\n",
            "-i crl");

    fprintf(stderr, "%-15s Import a CRL to the cert database\n", "-I");
    fprintf(stderr, "%-20s Specify the file which contains the CRL to import\n",
            "-i crl");
    fprintf(stderr, "%-20s Specify the url.\n", "-u url");
    fprintf(stderr, "%-20s Specify the crl type.\n", "-t crlType");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
            "-P dbprefix");
#ifdef DEBUG
    fprintf(stderr, "%-15s Test . Only for debugging purposes. See source code\n", "-T");
#endif
    fprintf(stderr, "%-20s CRL Types (default is SEC_CRL_TYPE):\n", " ");
    fprintf(stderr, "%-20s \t 0 - SEC_KRL_TYPE\n", " ");
    fprintf(stderr, "%-20s \t 1 - SEC_CRL_TYPE\n", " ");
    fprintf(stderr, "\n%-20s Bypass CA certificate checks.\n", "-B");
    fprintf(stderr, "\n%-20s Partial decode for faster operation.\n", "-p");
    fprintf(stderr, "%-20s Repeat the operation.\n", "-r <iterations>");
    fprintf(stderr, "\n%-15s Create CRL\n", "-G");
    fprintf(stderr, "%-15s Modify CRL\n", "-M");
    fprintf(stderr, "%-20s Specify crl initialization file\n",
            "-c crl-conf-file");
    fprintf(stderr, "%-20s Specify the nickname of the CA certificate\n",
            "-n nickname");
    fprintf(stderr, "%-20s Specify the file which contains the CRL to import\n",
            "-i crl");
    fprintf(stderr, "%-20s Specify a CRL output file\n",
            "-o crl-output-file");
    fprintf(stderr, "%-20s Specify to use base64 encoded CRL output format\n",
            "-a");
    fprintf(stderr, "%-20s Key database directory (default is ~/.netscape)\n",
            "-d keydir");
    fprintf(stderr, "%-20s Provide path to a default pwd file\n",
            "-f pwd-file");
    fprintf(stderr, "%-20s Provide db password in command line\n",
            "-w pwd-string");
    fprintf(stderr, "%-20s Cert & Key database prefix (default is \"\")\n",
            "-P dbprefix");
    fprintf(stderr, "%-20s Specify the url.\n", "-u url");
    fprintf(stderr, "\n%-20s Bypass CA certificate checks.\n", "-B");

    exit(-1);
}

int
main(int argc, char **argv)
{
    CERTCertDBHandle *certHandle;
    PRFileDesc *inFile;
    PRFileDesc *inCrlInitFile = NULL;
    int generateCRL;
    int modifyCRL;
    int listCRL;
    int importCRL;
    int showFileCRL;
    int deleteCRL;
    int rv;
    char *nickName;
    char *url;
    char *dbPrefix = "";
    char *alg = NULL;
    char *outFile = NULL;
    char *slotName = NULL;
    int ascii = 0;
    int crlType;
    PLOptState *optstate;
    PLOptStatus status;
    SECStatus secstatus;
    PRInt32 decodeOptions = CRL_DECODE_DEFAULT_OPTIONS;
    PRInt32 importOptions = CRL_IMPORT_DEFAULT_OPTIONS;
    PRBool quiet = PR_FALSE;
    PRBool test = PR_FALSE;
    PRBool erase = PR_FALSE;
    PRInt32 i = 0;
    PRInt32 iterations = 1;
    PRBool readonly = PR_FALSE;

    secuPWData pwdata = { PW_NONE, 0 };

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    rv = 0;
    deleteCRL = importCRL = listCRL = generateCRL = modifyCRL = showFileCRL = 0;
    inFile = NULL;
    nickName = url = NULL;
    certHandle = NULL;
    crlType = SEC_CRL_TYPE;
    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "sqBCDGILMSTEP:f:d:i:h:n:p:t:u:r:aZ:o:c:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
                Usage(progName);
                break;

            case 'T':
                test = PR_TRUE;
                break;

            case 'E':
                erase = PR_TRUE;
                break;

            case 'B':
                importOptions |= CRL_IMPORT_BYPASS_CHECKS;
                break;

            case 'G':
                generateCRL = 1;
                break;

            case 'M':
                modifyCRL = 1;
                break;

            case 'D':
                deleteCRL = 1;
                break;

            case 'I':
                importCRL = 1;
                break;

            case 'S':
                showFileCRL = 1;
                break;

            case 'C':
            case 'L':
                listCRL = 1;
                break;

            case 'P':
                dbPrefix = strdup(optstate->value);
                break;

            case 'Z':
                alg = strdup(optstate->value);
                break;

            case 'a':
                ascii = 1;
                break;

            case 'c':
                inCrlInitFile = PR_Open(optstate->value, PR_RDONLY, 0);
                if (!inCrlInitFile) {
                    PR_fprintf(PR_STDERR, "%s: unable to open \"%s\" for reading\n",
                               progName, optstate->value);
                    PL_DestroyOptState(optstate);
                    return -1;
                }
                break;

            case 'd':
                SECU_ConfigDirectory(optstate->value);
                break;

            case 'f':
                pwdata.source = PW_FROMFILE;
                pwdata.data = strdup(optstate->value);
                break;

            case 'h':
                slotName = strdup(optstate->value);
                break;

            case 'i':
                inFile = PR_Open(optstate->value, PR_RDONLY, 0);
                if (!inFile) {
                    PR_fprintf(PR_STDERR, "%s: unable to open \"%s\" for reading\n",
                               progName, optstate->value);
                    PL_DestroyOptState(optstate);
                    return -1;
                }
                break;

            case 'n':
                nickName = strdup(optstate->value);
                break;

            case 'o':
                outFile = strdup(optstate->value);
                break;

            case 'p':
                decodeOptions |= CRL_DECODE_SKIP_ENTRIES;
                break;

            case 'r': {
                const char *str = optstate->value;
                if (str && atoi(str) > 0)
                    iterations = atoi(str);
            } break;

            case 't': {
                crlType = atoi(optstate->value);
                if (crlType != SEC_CRL_TYPE && crlType != SEC_KRL_TYPE) {
                    PR_fprintf(PR_STDERR, "%s: invalid crl type\n", progName);
                    PL_DestroyOptState(optstate);
                    return -1;
                }
                break;

                case 'q':
                    quiet = PR_TRUE;
                    break;

                case 'w':
                    pwdata.source = PW_PLAINTEXT;
                    pwdata.data = strdup(optstate->value);
                    break;

                case 'u':
                    url = strdup(optstate->value);
                    break;
            }
        }
    }
    PL_DestroyOptState(optstate);

    if (deleteCRL && !nickName)
        Usage(progName);
    if (importCRL && !inFile)
        Usage(progName);
    if (showFileCRL && !inFile)
        Usage(progName);
    if ((generateCRL && !nickName) ||
        (modifyCRL && !inFile && !nickName))
        Usage(progName);
    if (!(listCRL || deleteCRL || importCRL || showFileCRL || generateCRL ||
          modifyCRL || test || erase))
        Usage(progName);

    if (listCRL || showFileCRL) {
        readonly = PR_TRUE;
    }

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    if (showFileCRL) {
        NSS_NoDB_Init(NULL);
    } else {
        secstatus = NSS_Initialize(SECU_ConfigDirectory(NULL), dbPrefix, dbPrefix,
                                   "secmod.db", readonly ? NSS_INIT_READONLY : 0);
        if (secstatus != SECSuccess) {
            SECU_PrintPRandOSError(progName);
            return -1;
        }
    }

    SECU_RegisterDynamicOids();

    certHandle = CERT_GetDefaultCertDB();
    if (certHandle == NULL) {
        SECU_PrintError(progName, "unable to open the cert db");
        /*ignoring return value of NSS_Shutdown() as code returns -1*/
        (void)NSS_Shutdown();
        return (-1);
    }

    CRLGEN_InitCrlGenParserLock();

    for (i = 0; i < iterations; i++) {
        /* Read in the private key info */
        if (deleteCRL)
            DeleteCRL(certHandle, nickName, crlType);
        else if (listCRL) {
            rv = ListCRL(certHandle, nickName, crlType);
        } else if (importCRL) {
            rv = ImportCRL(certHandle, url, crlType, inFile, importOptions,
                           decodeOptions, &pwdata);
        } else if (showFileCRL) {
            rv = DumpCRL(inFile);
        } else if (generateCRL || modifyCRL) {
            if (!inCrlInitFile)
                inCrlInitFile = PR_STDIN;
            rv = GenerateCRL(certHandle, nickName, inCrlInitFile,
                             inFile, outFile, ascii, slotName,
                             importOptions, alg, quiet,
                             decodeOptions, url, &pwdata,
                             modifyCRL);
        } else if (erase) {
            /* list and delete all CRLs */
            ListCRLNames(certHandle, crlType, PR_TRUE);
        }
#ifdef DEBUG
        else if (test) {
            /* list and delete all CRLs */
            ListCRLNames(certHandle, crlType, PR_TRUE);
            /* list CRLs */
            ListCRLNames(certHandle, crlType, PR_FALSE);
            /* import CRL as a blob */
            rv = ImportCRL(certHandle, url, crlType, inFile, importOptions,
                           decodeOptions, &pwdata);
            /* list CRLs */
            ListCRLNames(certHandle, crlType, PR_FALSE);
        }
#endif
    }

    CRLGEN_DestroyCrlGenParserLock();

    if (NSS_Shutdown() != SECSuccess) {
        rv = SECFailure;
    }

    return (rv != SECSuccess);
}
