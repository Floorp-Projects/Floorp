/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * cmsutil -- A command to work with CMS data
 */

#include "nspr.h"
#include "secutil.h"
#include "plgetopt.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "secoid.h"
#include "cms.h"
#include "nss.h"
#include "smime.h"
#include "pk11func.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "fcntl.h"
#include "io.h"
#endif

#include <stdio.h>
#include <string.h>

char *progName = NULL;
static int cms_verbose = 0;
static secuPWData pwdata = { PW_NONE, 0 };
static PK11PasswordFunc pwcb = NULL;
static void *pwcb_arg = NULL;

/* XXX stolen from cmsarray.c
 * nss_CMSArray_Count - count number of elements in array
 */
int
nss_CMSArray_Count(void **array)
{
    int n = 0;
    if (array == NULL)
        return 0;
    while (*array++ != NULL)
        n++;
    return n;
}

static SECStatus
DigestFile(PLArenaPool *poolp, SECItem ***digests, SECItem *input,
           SECAlgorithmID **algids)
{
    NSSCMSDigestContext *digcx;
    SECStatus rv;

    digcx = NSS_CMSDigestContext_StartMultiple(algids);
    if (digcx == NULL)
        return SECFailure;

    NSS_CMSDigestContext_Update(digcx, input->data, input->len);

    rv = NSS_CMSDigestContext_FinishMultiple(digcx, poolp, digests);
    return rv;
}

static void
Usage(void)
{
    fprintf(stderr,
            "Usage:  %s [-C|-D|-E|-O|-S] [<options>] [-d dbdir] [-u certusage]\n"
            " -C            create a CMS encrypted data message\n"
            " -D            decode a CMS message\n"
            "  -b           decode a batch of files named in infile\n"
            "  -c content   use this detached content\n"
            "  -n           suppress output of content\n"
            "  -h num       display num levels of CMS message info as email headers\n"
            "  -k           keep decoded encryption certs in perm cert db\n"
            " -E            create a CMS enveloped data message\n"
            "  -r id,...    create envelope for these recipients,\n"
            "               where id can be a certificate nickname or email address\n"
            " -S            create a CMS signed data message\n"
            "  -G           include a signing time attribute\n"
            "  -H hash      use hash (default:SHA256)\n"
            "  -N nick      use certificate named \"nick\" for signing\n"
            "  -P           include a SMIMECapabilities attribute\n"
            "  -T           do not include content in CMS message\n"
            "  -Y nick      include a EncryptionKeyPreference attribute with cert\n"
            "                 (use \"NONE\" to omit)\n"
            " -O            create a CMS signed message containing only certificates\n"
            " General Options:\n"
            " -d dbdir      key/cert database directory (default: ~/.netscape)\n"
            " -e envelope   enveloped data message in this file is used for bulk key\n"
            " -i infile     use infile as source of data (default: stdin)\n"
            " -o outfile    use outfile as destination of data (default: stdout)\n"
            " -p password   use password as key db password (default: prompt)\n"
            " -f pwfile     use password file to set password on all PKCS#11 tokens)\n"
            " -u certusage  set type of certificate usage (default: certUsageEmailSigner)\n"
            " -v            print debugging information\n"
            "\n"
            "Cert usage codes:\n",
            progName);
    fprintf(stderr, "%-25s  0 - certUsageSSLClient\n", " ");
    fprintf(stderr, "%-25s  1 - certUsageSSLServer\n", " ");
    fprintf(stderr, "%-25s  2 - certUsageSSLServerWithStepUp\n", " ");
    fprintf(stderr, "%-25s  3 - certUsageSSLCA\n", " ");
    fprintf(stderr, "%-25s  4 - certUsageEmailSigner\n", " ");
    fprintf(stderr, "%-25s  5 - certUsageEmailRecipient\n", " ");
    fprintf(stderr, "%-25s  6 - certUsageObjectSigner\n", " ");
    fprintf(stderr, "%-25s  7 - certUsageUserCertImport\n", " ");
    fprintf(stderr, "%-25s  8 - certUsageVerifyCA\n", " ");
    fprintf(stderr, "%-25s  9 - certUsageProtectedObjectSigner\n", " ");
    fprintf(stderr, "%-25s 10 - certUsageStatusResponder\n", " ");
    fprintf(stderr, "%-25s 11 - certUsageAnyCA\n", " ");
    fprintf(stderr, "%-25s 12 - certUsageIPsec\n", " ");

    exit(-1);
}

struct optionsStr {
    char *pwfile;
    char *password;
    SECCertUsage certUsage;
    CERTCertDBHandle *certHandle;
};

struct decodeOptionsStr {
    struct optionsStr *options;
    SECItem content;
    int headerLevel;
    PRBool suppressContent;
    NSSCMSGetDecryptKeyCallback dkcb;
    PK11SymKey *bulkkey;
    PRBool keepCerts;
};

struct signOptionsStr {
    struct optionsStr *options;
    char *nickname;
    char *encryptionKeyPreferenceNick;
    PRBool signingTime;
    PRBool smimeProfile;
    PRBool detached;
    SECOidTag hashAlgTag;
};

struct envelopeOptionsStr {
    struct optionsStr *options;
    char **recipients;
};

struct certsonlyOptionsStr {
    struct optionsStr *options;
    char **recipients;
};

struct encryptOptionsStr {
    struct optionsStr *options;
    char **recipients;
    NSSCMSMessage *envmsg;
    SECItem *input;
    FILE *outfile;
    PRFileDesc *envFile;
    PK11SymKey *bulkkey;
    SECOidTag bulkalgtag;
    int keysize;
};

static NSSCMSMessage *
decode(FILE *out, SECItem *input, const struct decodeOptionsStr *decodeOptions)
{
    NSSCMSDecoderContext *dcx;
    SECStatus rv;
    NSSCMSMessage *cmsg;
    int nlevels, i;
    SECItem sitem = { 0, 0, 0 };

    PORT_SetError(0);
    dcx = NSS_CMSDecoder_Start(NULL,
                               NULL, NULL,          /* content callback     */
                               pwcb, pwcb_arg,      /* password callback    */
                               decodeOptions->dkcb, /* decrypt key callback */
                               decodeOptions->bulkkey);
    if (dcx == NULL) {
        fprintf(stderr, "%s: failed to set up message decoder.\n", progName);
        return NULL;
    }
    rv = NSS_CMSDecoder_Update(dcx, (char *)input->data, input->len);
    if (rv != SECSuccess) {
        fprintf(stderr, "%s: failed to decode message.\n", progName);
        NSS_CMSDecoder_Cancel(dcx);
        return NULL;
    }
    cmsg = NSS_CMSDecoder_Finish(dcx);
    if (cmsg == NULL) {
        fprintf(stderr, "%s: failed to decode message.\n", progName);
        return NULL;
    }

    if (decodeOptions->headerLevel >= 0) {
        /*fprintf(out, "SMIME: ", decodeOptions->headerLevel, i);*/
        fprintf(out, "SMIME: ");
    }

    nlevels = NSS_CMSMessage_ContentLevelCount(cmsg);
    for (i = 0; i < nlevels; i++) {
        NSSCMSContentInfo *cinfo;
        SECOidTag typetag;

        cinfo = NSS_CMSMessage_ContentLevel(cmsg, i);
        typetag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);

        if (decodeOptions->headerLevel >= 0)
            fprintf(out, "\tlevel=%d.%d; ", decodeOptions->headerLevel, nlevels - i);

        switch (typetag) {
            case SEC_OID_PKCS7_SIGNED_DATA: {
                NSSCMSSignedData *sigd = NULL;
                SECItem **digests;
                int nsigners;
                int j;

                if (decodeOptions->headerLevel >= 0)
                    fprintf(out, "type=signedData; ");
                sigd = (NSSCMSSignedData *)NSS_CMSContentInfo_GetContent(cinfo);
                if (sigd == NULL) {
                    SECU_PrintError(progName, "signedData component missing");
                    goto loser;
                }

                /* if we have a content file, but no digests for this signedData */
                if (decodeOptions->content.data != NULL &&
                    !NSS_CMSSignedData_HasDigests(sigd)) {
                    PLArenaPool *poolp;
                    SECAlgorithmID **digestalgs;

                    /* detached content: grab content file */
                    sitem = decodeOptions->content;

                    if ((poolp = PORT_NewArena(1024)) == NULL) {
                        fprintf(stderr, "cmsutil: Out of memory.\n");
                        goto loser;
                    }
                    digestalgs = NSS_CMSSignedData_GetDigestAlgs(sigd);
                    if (DigestFile(poolp, &digests, &sitem, digestalgs) !=
                        SECSuccess) {
                        SECU_PrintError(progName,
                                        "problem computing message digest");
                        PORT_FreeArena(poolp, PR_FALSE);
                        goto loser;
                    }
                    if (NSS_CMSSignedData_SetDigests(sigd, digestalgs, digests) !=
                        SECSuccess) {
                        SECU_PrintError(progName,
                                        "problem setting message digests");
                        PORT_FreeArena(poolp, PR_FALSE);
                        goto loser;
                    }
                    PORT_FreeArena(poolp, PR_FALSE);
                }

                /* import the certificates */
                if (NSS_CMSSignedData_ImportCerts(sigd,
                                                  decodeOptions->options->certHandle,
                                                  decodeOptions->options->certUsage,
                                                  decodeOptions->keepCerts) !=
                    SECSuccess) {
                    SECU_PrintError(progName, "cert import failed");
                    goto loser;
                }

                /* find out about signers */
                nsigners = NSS_CMSSignedData_SignerInfoCount(sigd);
                if (decodeOptions->headerLevel >= 0)
                    fprintf(out, "nsigners=%d; ", nsigners);
                if (nsigners == 0) {
                    /* Might be a cert transport message
                    ** or might be an invalid message, such as a QA test message
                    ** or a message from an attacker.
                    */
                    rv = NSS_CMSSignedData_VerifyCertsOnly(sigd,
                                                           decodeOptions->options->certHandle,
                                                           decodeOptions->options->certUsage);
                    if (rv != SECSuccess) {
                        fprintf(stderr, "cmsutil: Verify certs-only failed!\n");
                        goto loser;
                    }
                    return cmsg;
                }

                /* still no digests? */
                if (!NSS_CMSSignedData_HasDigests(sigd)) {
                    SECU_PrintError(progName, "no message digests");
                    goto loser;
                }

                for (j = 0; j < nsigners; j++) {
                    const char *svs;
                    NSSCMSSignerInfo *si;
                    NSSCMSVerificationStatus vs;
                    SECStatus bad;

                    si = NSS_CMSSignedData_GetSignerInfo(sigd, j);
                    if (decodeOptions->headerLevel >= 0) {
                        char *signercn;
                        static char empty[] = { "" };

                        signercn = NSS_CMSSignerInfo_GetSignerCommonName(si);
                        if (signercn == NULL)
                            signercn = empty;
                        fprintf(out, "\n\t\tsigner%d.id=\"%s\"; ", j, signercn);
                        if (signercn != empty)
                            PORT_Free(signercn);
                    }
                    bad = NSS_CMSSignedData_VerifySignerInfo(sigd, j,
                                                             decodeOptions->options->certHandle,
                                                             decodeOptions->options->certUsage);
                    vs = NSS_CMSSignerInfo_GetVerificationStatus(si);
                    svs = NSS_CMSUtil_VerificationStatusToString(vs);
                    if (decodeOptions->headerLevel >= 0) {
                        fprintf(out, "signer%d.status=%s; ", j, svs);
                        /* goto loser ? */
                    } else if (bad && out) {
                        fprintf(stderr, "signer %d status = %s\n", j, svs);
                        goto loser;
                    }
                }
            } break;
            case SEC_OID_PKCS7_ENVELOPED_DATA: {
                NSSCMSEnvelopedData *envd;
                if (decodeOptions->headerLevel >= 0)
                    fprintf(out, "type=envelopedData; ");
                envd = (NSSCMSEnvelopedData *)NSS_CMSContentInfo_GetContent(cinfo);
                if (envd == NULL) {
                    SECU_PrintError(progName, "envelopedData component missing");
                    goto loser;
                }
            } break;
            case SEC_OID_PKCS7_ENCRYPTED_DATA: {
                NSSCMSEncryptedData *encd;
                if (decodeOptions->headerLevel >= 0)
                    fprintf(out, "type=encryptedData; ");
                encd = (NSSCMSEncryptedData *)NSS_CMSContentInfo_GetContent(cinfo);
                if (encd == NULL) {
                    SECU_PrintError(progName, "encryptedData component missing");
                    goto loser;
                }
            } break;
            case SEC_OID_PKCS7_DATA:
                if (decodeOptions->headerLevel >= 0)
                    fprintf(out, "type=data; ");
                break;
            default:
                break;
        }
        if (decodeOptions->headerLevel >= 0)
            fprintf(out, "\n");
    }

    if (!decodeOptions->suppressContent && out) {
        SECItem *item = (sitem.data ? &sitem
                                    : NSS_CMSMessage_GetContent(cmsg));
        if (item && item->data && item->len) {
            fwrite(item->data, item->len, 1, out);
        }
    }
    return cmsg;

loser:
    if (cmsg)
        NSS_CMSMessage_Destroy(cmsg);
    return NULL;
}

/* example of a callback function to use with encoder */
/*
static void
writeout(void *arg, const char *buf, unsigned long len)
{
    FILE *f = (FILE *)arg;

    if (f != NULL && buf != NULL)
        (void)fwrite(buf, len, 1, f);
}
*/

static NSSCMSMessage *
signed_data(struct signOptionsStr *signOptions)
{
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd;
    NSSCMSSignerInfo *signerinfo;
    CERTCertificate *cert = NULL, *ekpcert = NULL;

    if (cms_verbose) {
        fprintf(stderr, "Input to signed_data:\n");
        if (signOptions->options->password)
            fprintf(stderr, "password [%s]\n", signOptions->options->password);
        else if (signOptions->options->pwfile)
            fprintf(stderr, "password file [%s]\n", signOptions->options->pwfile);
        else
            fprintf(stderr, "password [NULL]\n");
        fprintf(stderr, "certUsage [%d]\n", signOptions->options->certUsage);
        if (signOptions->options->certHandle)
            fprintf(stderr, "certdb [%p]\n", signOptions->options->certHandle);
        else
            fprintf(stderr, "certdb [NULL]\n");
        if (signOptions->nickname)
            fprintf(stderr, "nickname [%s]\n", signOptions->nickname);
        else
            fprintf(stderr, "nickname [NULL]\n");
    }
    if (signOptions->nickname == NULL) {
        fprintf(stderr,
                "ERROR: please indicate the nickname of a certificate to sign with.\n");
        return NULL;
    }
    if ((cert = CERT_FindUserCertByUsage(signOptions->options->certHandle,
                                         signOptions->nickname,
                                         signOptions->options->certUsage,
                                         PR_FALSE,
                                         &pwdata)) == NULL) {
        SECU_PrintError(progName,
                        "the corresponding cert for key \"%s\" does not exist",
                        signOptions->nickname);
        return NULL;
    }
    if (cms_verbose) {
        fprintf(stderr, "Found certificate for %s\n", signOptions->nickname);
    }
    /*
     * create the message object
     */
    cmsg = NSS_CMSMessage_Create(NULL); /* create a message on its own pool */
    if (cmsg == NULL) {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        return NULL;
    }
    /*
     * build chain of objects: message->signedData->data
     */
    if ((sigd = NSS_CMSSignedData_Create(cmsg)) == NULL) {
        fprintf(stderr, "ERROR: cannot create CMS signedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSSignedData_GetContentInfo(sigd);
    /* we're always passing data in and detaching optionally */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL,
                                           signOptions->detached) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
        goto loser;
    }
    /*
     * create & attach signer information
     */
    signerinfo = NSS_CMSSignerInfo_Create(cmsg, cert, signOptions->hashAlgTag);
    if (signerinfo == NULL) {
        fprintf(stderr, "ERROR: cannot create CMS signerInfo object.\n");
        goto loser;
    }
    if (cms_verbose) {
        fprintf(stderr,
                "Created CMS message, added signed data w/ signerinfo\n");
    }
    signerinfo->cmsg->pwfn_arg = pwcb_arg;
    /* we want the cert chain included for this one */
    if (NSS_CMSSignerInfo_IncludeCerts(signerinfo, NSSCMSCM_CertChain,
                                       signOptions->options->certUsage) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot find cert chain.\n");
        goto loser;
    }
    if (cms_verbose) {
        fprintf(stderr, "imported certificate\n");
    }
    if (signOptions->signingTime) {
        if (NSS_CMSSignerInfo_AddSigningTime(signerinfo, PR_Now()) !=
            SECSuccess) {
            fprintf(stderr, "ERROR: cannot add signingTime attribute.\n");
            goto loser;
        }
    }
    if (signOptions->smimeProfile) {
        if (NSS_CMSSignerInfo_AddSMIMECaps(signerinfo) != SECSuccess) {
            fprintf(stderr, "ERROR: cannot add SMIMECaps attribute.\n");
            goto loser;
        }
    }

    if (!signOptions->encryptionKeyPreferenceNick) {
        /* check signing cert for fitness as encryption cert */
        SECStatus FitForEncrypt = CERT_CheckCertUsage(cert,
                                                      certUsageEmailRecipient);

        if (SECSuccess == FitForEncrypt) {
            /* if yes, add signing cert as EncryptionKeyPreference */
            if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, cert,
                                                      signOptions->options->certHandle) !=
                SECSuccess) {
                fprintf(stderr,
                        "ERROR: cannot add default SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
            if (NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs(signerinfo, cert,
                                                        signOptions->options->certHandle) !=
                SECSuccess) {
                fprintf(stderr,
                        "ERROR: cannot add default MS SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
        } else {
            /* this is a dual-key cert case, we need to look for the encryption
               certificate under the same nickname as the signing cert */
            /* get the cert, add it to the message */
            if ((ekpcert = CERT_FindUserCertByUsage(
                     signOptions->options->certHandle,
                     signOptions->nickname,
                     certUsageEmailRecipient,
                     PR_FALSE,
                     &pwdata)) == NULL) {
                SECU_PrintError(progName,
                                "the corresponding cert for key \"%s\" does not exist",
                                signOptions->encryptionKeyPreferenceNick);
                goto loser;
            }
            if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, ekpcert,
                                                      signOptions->options->certHandle) !=
                SECSuccess) {
                fprintf(stderr,
                        "ERROR: cannot add SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
            if (NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs(signerinfo, ekpcert,
                                                        signOptions->options->certHandle) !=
                SECSuccess) {
                fprintf(stderr,
                        "ERROR: cannot add MS SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
            if (NSS_CMSSignedData_AddCertificate(sigd, ekpcert) != SECSuccess) {
                fprintf(stderr, "ERROR: cannot add encryption certificate.\n");
                goto loser;
            }
        }
    } else if (PL_strcmp(signOptions->encryptionKeyPreferenceNick, "NONE") == 0) {
        /* No action */
    } else {
        /* get the cert, add it to the message */
        if ((ekpcert = CERT_FindUserCertByUsage(
                 signOptions->options->certHandle,
                 signOptions->encryptionKeyPreferenceNick,
                 certUsageEmailRecipient, PR_FALSE, &pwdata)) ==
            NULL) {
            SECU_PrintError(progName,
                            "the corresponding cert for key \"%s\" does not exist",
                            signOptions->encryptionKeyPreferenceNick);
            goto loser;
        }
        if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, ekpcert,
                                                  signOptions->options->certHandle) !=
            SECSuccess) {
            fprintf(stderr, "ERROR: cannot add SMIMEEncKeyPrefs attribute.\n");
            goto loser;
        }
        if (NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs(signerinfo, ekpcert,
                                                    signOptions->options->certHandle) !=
            SECSuccess) {
            fprintf(stderr, "ERROR: cannot add MS SMIMEEncKeyPrefs attribute.\n");
            goto loser;
        }
        if (NSS_CMSSignedData_AddCertificate(sigd, ekpcert) != SECSuccess) {
            fprintf(stderr, "ERROR: cannot add encryption certificate.\n");
            goto loser;
        }
    }

    if (NSS_CMSSignedData_AddSignerInfo(sigd, signerinfo) != SECSuccess) {
        fprintf(stderr, "ERROR: cannot add CMS signerInfo object.\n");
        goto loser;
    }
    if (cms_verbose) {
        fprintf(stderr, "created signed-data message\n");
    }
    if (ekpcert) {
        CERT_DestroyCertificate(ekpcert);
    }
    if (cert) {
        CERT_DestroyCertificate(cert);
    }
    return cmsg;
loser:
    if (ekpcert) {
        CERT_DestroyCertificate(ekpcert);
    }
    if (cert) {
        CERT_DestroyCertificate(cert);
    }
    NSS_CMSMessage_Destroy(cmsg);
    return NULL;
}

static NSSCMSMessage *
enveloped_data(struct envelopeOptionsStr *envelopeOptions)
{
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSEnvelopedData *envd;
    NSSCMSRecipientInfo *recipientinfo;
    CERTCertificate **recipientcerts = NULL;
    CERTCertDBHandle *dbhandle;
    PLArenaPool *tmppoolp = NULL;
    SECOidTag bulkalgtag;
    int keysize, i = 0;
    int cnt;
    dbhandle = envelopeOptions->options->certHandle;
    /* count the recipients */
    if ((cnt = nss_CMSArray_Count((void **)envelopeOptions->recipients)) == 0) {
        fprintf(stderr, "ERROR: please name at least one recipient.\n");
        goto loser;
    }
    if ((tmppoolp = PORT_NewArena(1024)) == NULL) {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    /* XXX find the recipient's certs by email address or nickname */
    if ((recipientcerts =
             (CERTCertificate **)PORT_ArenaZAlloc(tmppoolp,
                                                  (cnt + 1) * sizeof(CERTCertificate *))) ==
        NULL) {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    for (i = 0; envelopeOptions->recipients[i] != NULL; i++) {
        if ((recipientcerts[i] =
                 CERT_FindCertByNicknameOrEmailAddr(dbhandle,
                                                    envelopeOptions->recipients[i])) ==
            NULL) {
            SECU_PrintError(progName, "cannot find certificate for \"%s\"",
                            envelopeOptions->recipients[i]);
            i = 0;
            goto loser;
        }
    }
    recipientcerts[i] = NULL;
    i = 0;
    /* find a nice bulk algorithm */
    if (NSS_SMIMEUtil_FindBulkAlgForRecipients(recipientcerts, &bulkalgtag,
                                               &keysize) != SECSuccess) {
        fprintf(stderr, "ERROR: cannot find common bulk algorithm.\n");
        goto loser;
    }
    /*
     * create the message object
     */
    cmsg = NSS_CMSMessage_Create(NULL); /* create a message on its own pool */
    if (cmsg == NULL) {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        goto loser;
    }
    /*
     * build chain of objects: message->envelopedData->data
     */
    if ((envd = NSS_CMSEnvelopedData_Create(cmsg, bulkalgtag, keysize)) ==
        NULL) {
        fprintf(stderr, "ERROR: cannot create CMS envelopedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_EnvelopedData(cmsg, cinfo, envd) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS envelopedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSEnvelopedData_GetContentInfo(envd);
    /* we're always passing data in, so the content is NULL */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
        goto loser;
    }
    /*
     * create & attach recipient information
     */
    for (i = 0; recipientcerts[i] != NULL; i++) {
        if ((recipientinfo = NSS_CMSRecipientInfo_Create(cmsg,
                                                         recipientcerts[i])) ==
            NULL) {
            fprintf(stderr, "ERROR: cannot create CMS recipientInfo object.\n");
            goto loser;
        }
        if (NSS_CMSEnvelopedData_AddRecipient(envd, recipientinfo) !=
            SECSuccess) {
            fprintf(stderr, "ERROR: cannot add CMS recipientInfo object.\n");
            goto loser;
        }
        CERT_DestroyCertificate(recipientcerts[i]);
    }
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, PR_FALSE);
    return cmsg;
loser:
    if (recipientcerts) {
        for (; recipientcerts[i] != NULL; i++) {
            CERT_DestroyCertificate(recipientcerts[i]);
        }
    }
    if (cmsg)
        NSS_CMSMessage_Destroy(cmsg);
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, PR_FALSE);
    return NULL;
}

PK11SymKey *
dkcb(void *arg, SECAlgorithmID *algid)
{
    return (PK11SymKey *)arg;
}

static SECStatus
get_enc_params(struct encryptOptionsStr *encryptOptions)
{
    struct envelopeOptionsStr envelopeOptions;
    SECStatus rv = SECFailure;
    NSSCMSMessage *env_cmsg;
    NSSCMSContentInfo *cinfo;
    int i, nlevels;
    /*
     * construct an enveloped data message to obtain bulk keys
     */
    if (encryptOptions->envmsg) {
        env_cmsg = encryptOptions->envmsg; /* get it from an old message */
    } else {
        SECItem dummyOut = { 0, 0, 0 };
        SECItem dummyIn = { 0, 0, 0 };
        char str[] = "Hello!";
        PLArenaPool *tmparena = PORT_NewArena(1024);
        dummyIn.data = (unsigned char *)str;
        dummyIn.len = strlen(str);
        envelopeOptions.options = encryptOptions->options;
        envelopeOptions.recipients = encryptOptions->recipients;
        env_cmsg = enveloped_data(&envelopeOptions);
        NSS_CMSDEREncode(env_cmsg, &dummyIn, &dummyOut, tmparena);
        PR_Write(encryptOptions->envFile, dummyOut.data, dummyOut.len);
        PORT_FreeArena(tmparena, PR_FALSE);
    }
    /*
     * get the content info for the enveloped data
     */
    nlevels = NSS_CMSMessage_ContentLevelCount(env_cmsg);
    for (i = 0; i < nlevels; i++) {
        SECOidTag typetag;
        cinfo = NSS_CMSMessage_ContentLevel(env_cmsg, i);
        typetag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);
        if (typetag == SEC_OID_PKCS7_DATA) {
            /*
             * get the symmetric key
             */
            encryptOptions->bulkalgtag = NSS_CMSContentInfo_GetContentEncAlgTag(cinfo);
            encryptOptions->keysize = NSS_CMSContentInfo_GetBulkKeySize(cinfo);
            encryptOptions->bulkkey = NSS_CMSContentInfo_GetBulkKey(cinfo);
            rv = SECSuccess;
            break;
        }
    }
    if (i == nlevels) {
        fprintf(stderr, "%s: could not retrieve enveloped data.", progName);
    }
    if (env_cmsg)
        NSS_CMSMessage_Destroy(env_cmsg);
    return rv;
}

static NSSCMSMessage *
encrypted_data(struct encryptOptionsStr *encryptOptions)
{
    SECStatus rv = SECFailure;
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSEncryptedData *encd;
    NSSCMSEncoderContext *ecx = NULL;
    PLArenaPool *tmppoolp = NULL;
    SECItem derOut = { 0, 0, 0 };
    /* arena for output */
    tmppoolp = PORT_NewArena(1024);
    if (!tmppoolp) {
        fprintf(stderr, "%s: out of memory.\n", progName);
        return NULL;
    }
    /*
     * create the message object
     */
    cmsg = NSS_CMSMessage_Create(NULL);
    if (cmsg == NULL) {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        goto loser;
    }
    /*
     * build chain of objects: message->encryptedData->data
     */
    if ((encd = NSS_CMSEncryptedData_Create(cmsg, encryptOptions->bulkalgtag,
                                            encryptOptions->keysize)) ==
        NULL) {
        fprintf(stderr, "ERROR: cannot create CMS encryptedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_EncryptedData(cmsg, cinfo, encd) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS encryptedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSEncryptedData_GetContentInfo(encd);
    /* we're always passing data in, so the content is NULL */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
        goto loser;
    }
    ecx = NSS_CMSEncoder_Start(cmsg, NULL, NULL, &derOut, tmppoolp, NULL, NULL,
                               dkcb, encryptOptions->bulkkey, NULL, NULL);
    if (!ecx) {
        fprintf(stderr, "%s: cannot create encoder context.\n", progName);
        goto loser;
    }
    rv = NSS_CMSEncoder_Update(ecx, (char *)encryptOptions->input->data,
                               encryptOptions->input->len);
    if (rv) {
        fprintf(stderr, "%s: failed to add data to encoder.\n", progName);
        goto loser;
    }
    rv = NSS_CMSEncoder_Finish(ecx);
    if (rv) {
        fprintf(stderr, "%s: failed to encrypt data.\n", progName);
        goto loser;
    }
    fwrite(derOut.data, derOut.len, 1, encryptOptions->outfile);
    /*
    if (bulkkey)
        PK11_FreeSymKey(bulkkey);
        */
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, PR_FALSE);
    return cmsg;
loser:
    /*
    if (bulkkey)
        PK11_FreeSymKey(bulkkey);
        */
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, PR_FALSE);
    if (cmsg)
        NSS_CMSMessage_Destroy(cmsg);
    return NULL;
}

static NSSCMSMessage *
signed_data_certsonly(struct certsonlyOptionsStr *certsonlyOptions)
{
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd;
    CERTCertificate **certs = NULL;
    CERTCertDBHandle *dbhandle;
    PLArenaPool *tmppoolp = NULL;
    int i = 0, cnt;
    dbhandle = certsonlyOptions->options->certHandle;
    if ((cnt = nss_CMSArray_Count((void **)certsonlyOptions->recipients)) == 0) {
        fprintf(stderr,
                "ERROR: please indicate the nickname of a certificate to sign with.\n");
        goto loser;
    }
    if (!(tmppoolp = PORT_NewArena(1024))) {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    if (!(certs = PORT_ArenaZNewArray(tmppoolp, CERTCertificate *, cnt + 1))) {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    for (i = 0; certsonlyOptions->recipients[i] != NULL; i++) {
        if ((certs[i] =
                 CERT_FindCertByNicknameOrEmailAddr(dbhandle,
                                                    certsonlyOptions->recipients[i])) ==
            NULL) {
            SECU_PrintError(progName, "cannot find certificate for \"%s\"",
                            certsonlyOptions->recipients[i]);
            i = 0;
            goto loser;
        }
    }
    certs[i] = NULL;
    i = 0;
    /*
     * create the message object
     */
    cmsg = NSS_CMSMessage_Create(NULL);
    if (cmsg == NULL) {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        goto loser;
    }
    /*
     * build chain of objects: message->signedData->data
     */
    if ((sigd = NSS_CMSSignedData_CreateCertsOnly(cmsg, certs[0], PR_TRUE)) ==
        NULL) {
        fprintf(stderr, "ERROR: cannot create CMS signedData object.\n");
        goto loser;
    }
    CERT_DestroyCertificate(certs[0]);
    for (i = 1; i < cnt; i++) {
        if (NSS_CMSSignedData_AddCertChain(sigd, certs[i])) {
            fprintf(stderr, "ERROR: cannot add cert chain for \"%s\".\n",
                    certsonlyOptions->recipients[i]);
            goto loser;
        }
        CERT_DestroyCertificate(certs[i]);
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
        goto loser;
    }
    cinfo = NSS_CMSSignedData_GetContentInfo(sigd);
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) !=
        SECSuccess) {
        fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
        goto loser;
    }
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, PR_FALSE);
    return cmsg;
loser:
    if (certs) {
        for (; i < cnt; i++) {
            CERT_DestroyCertificate(certs[i]);
        }
    }
    if (cmsg)
        NSS_CMSMessage_Destroy(cmsg);
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, PR_FALSE);
    return NULL;
}

static char *
pl_fgets(char *buf, int size, PRFileDesc *fd)
{
    char *bp = buf;
    int nb = 0;
    ;

    while (size > 1) {
        nb = PR_Read(fd, bp, 1);
        if (nb < 0) {
            /* deal with error */
            return NULL;
        } else if (nb == 0) {
            /* deal with EOF */
            return NULL;
        } else if (*bp == '\n') {
            /* deal with EOL */
            ++bp; /* keep EOL character */
            break;
        } else {
            /* ordinary character */
            ++bp;
            --size;
        }
    }
    *bp = '\0';
    return buf;
}

typedef enum { UNKNOWN,
               DECODE,
               SIGN,
               ENCRYPT,
               ENVELOPE,
               CERTSONLY } Mode;

static int
doBatchDecode(FILE *outFile, PRFileDesc *batchFile,
              const struct decodeOptionsStr *decodeOptions)
{
    char *str;
    int exitStatus = 0;
    char batchLine[512];

    while (NULL != (str = pl_fgets(batchLine, sizeof batchLine, batchFile))) {
        NSSCMSMessage *cmsg = NULL;
        PRFileDesc *inFile;
        int len = strlen(str);
        SECStatus rv;
        SECItem input = { 0, 0, 0 };
        char cc;

        while (len > 0 &&
               ((cc = str[len - 1]) == '\n' || cc == '\r')) {
            str[--len] = '\0';
        }
        if (!len) /* skip empty line */
            continue;
        if (str[0] == '#')
            continue; /* skip comment line */
        fprintf(outFile, "========== %s ==========\n", str);
        inFile = PR_Open(str, PR_RDONLY, 00660);
        if (inFile == NULL) {
            fprintf(outFile, "%s: unable to open \"%s\" for reading\n",
                    progName, str);
            exitStatus = 1;
            continue;
        }
        rv = SECU_FileToItem(&input, inFile);
        PR_Close(inFile);
        if (rv != SECSuccess) {
            SECU_PrintError(progName, "unable to read infile");
            exitStatus = 1;
            continue;
        }
        cmsg = decode(outFile, &input, decodeOptions);
        SECITEM_FreeItem(&input, PR_FALSE);
        if (cmsg)
            NSS_CMSMessage_Destroy(cmsg);
        else {
            SECU_PrintError(progName, "problem decoding");
            exitStatus = 1;
        }
    }
    return exitStatus;
}

int
main(int argc, char **argv)
{
    FILE *outFile;
    NSSCMSMessage *cmsg = NULL;
    PRFileDesc *inFile;
    PLOptState *optstate;
    PLOptStatus status;
    Mode mode = UNKNOWN;
    struct decodeOptionsStr decodeOptions = { 0 };
    struct signOptionsStr signOptions = { 0 };
    struct envelopeOptionsStr envelopeOptions = { 0 };
    struct certsonlyOptionsStr certsonlyOptions = { 0 };
    struct encryptOptionsStr encryptOptions = { 0 };
    struct optionsStr options = { 0 };
    int exitstatus;
    static char *ptrarray[128] = { 0 };
    int nrecipients = 0;
    char *str, *tok;
    char *envFileName;
    SECItem input = { 0, 0, 0 };
    SECItem envmsg = { 0, 0, 0 };
    SECStatus rv;
    PRFileDesc *contentFile = NULL;
    PRBool batch = PR_FALSE;

#ifdef NISCC_TEST
    const char *ev = PR_GetEnvSecure("NSS_DISABLE_ARENA_FREE_LIST");
    PORT_Assert(ev);
    ev = PR_GetEnvSecure("NSS_STRICT_SHUTDOWN");
    PORT_Assert(ev);
#endif

    progName = strrchr(argv[0], '/');
    if (!progName)
        progName = strrchr(argv[0], '\\');
    progName = progName ? progName + 1 : argv[0];

    inFile = PR_STDIN;
    outFile = stdout;
    envFileName = NULL;
    mode = UNKNOWN;
    decodeOptions.content.data = NULL;
    decodeOptions.content.len = 0;
    decodeOptions.suppressContent = PR_FALSE;
    decodeOptions.headerLevel = -1;
    decodeOptions.keepCerts = PR_FALSE;
    options.certUsage = certUsageEmailSigner;
    options.password = NULL;
    options.pwfile = NULL;
    signOptions.nickname = NULL;
    signOptions.detached = PR_FALSE;
    signOptions.signingTime = PR_FALSE;
    signOptions.smimeProfile = PR_FALSE;
    signOptions.encryptionKeyPreferenceNick = NULL;
    signOptions.hashAlgTag = SEC_OID_SHA256;
    envelopeOptions.recipients = NULL;
    encryptOptions.recipients = NULL;
    encryptOptions.envmsg = NULL;
    encryptOptions.envFile = NULL;
    encryptOptions.bulkalgtag = SEC_OID_UNKNOWN;
    encryptOptions.bulkkey = NULL;
    encryptOptions.keysize = -1;

    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv,
                                 "CDEGH:N:OPSTY:bc:d:e:f:h:i:kno:p:r:s:u:v");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case 'C':
                mode = ENCRYPT;
                break;
            case 'D':
                mode = DECODE;
                break;
            case 'E':
                mode = ENVELOPE;
                break;
            case 'G':
                if (mode != SIGN) {
                    fprintf(stderr,
                            "%s: option -G only supported with option -S.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                signOptions.signingTime = PR_TRUE;
                break;
            case 'H':
                if (mode != SIGN) {
                    fprintf(stderr,
                            "%s: option -H only supported with option -S.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                decodeOptions.suppressContent = PR_TRUE;
                if (!strcmp(optstate->value, "MD2"))
                    signOptions.hashAlgTag = SEC_OID_MD2;
                else if (!strcmp(optstate->value, "MD4"))
                    signOptions.hashAlgTag = SEC_OID_MD4;
                else if (!strcmp(optstate->value, "MD5"))
                    signOptions.hashAlgTag = SEC_OID_MD5;
                else if (!strcmp(optstate->value, "SHA1"))
                    signOptions.hashAlgTag = SEC_OID_SHA1;
                else if (!strcmp(optstate->value, "SHA256"))
                    signOptions.hashAlgTag = SEC_OID_SHA256;
                else if (!strcmp(optstate->value, "SHA384"))
                    signOptions.hashAlgTag = SEC_OID_SHA384;
                else if (!strcmp(optstate->value, "SHA512"))
                    signOptions.hashAlgTag = SEC_OID_SHA512;
                else {
                    fprintf(stderr,
                            "%s: -H requires one of MD2,MD4,MD5,SHA1,SHA256,SHA384,SHA512\n",
                            progName);
                    exit(1);
                }
                break;
            case 'N':
                if (mode != SIGN) {
                    fprintf(stderr,
                            "%s: option -N only supported with option -S.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                signOptions.nickname = PORT_Strdup(optstate->value);
                break;
            case 'O':
                mode = CERTSONLY;
                break;
            case 'P':
                if (mode != SIGN) {
                    fprintf(stderr,
                            "%s: option -P only supported with option -S.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                signOptions.smimeProfile = PR_TRUE;
                break;
            case 'S':
                mode = SIGN;
                break;
            case 'T':
                if (mode != SIGN) {
                    fprintf(stderr,
                            "%s: option -T only supported with option -S.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                signOptions.detached = PR_TRUE;
                break;
            case 'Y':
                if (mode != SIGN) {
                    fprintf(stderr,
                            "%s: option -Y only supported with option -S.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                signOptions.encryptionKeyPreferenceNick = strdup(optstate->value);
                break;

            case 'b':
                if (mode != DECODE) {
                    fprintf(stderr,
                            "%s: option -b only supported with option -D.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                batch = PR_TRUE;
                break;

            case 'c':
                if (mode != DECODE) {
                    fprintf(stderr,
                            "%s: option -c only supported with option -D.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                contentFile = PR_Open(optstate->value, PR_RDONLY, 006600);
                if (contentFile == NULL) {
                    fprintf(stderr, "%s: unable to open \"%s\" for reading.\n",
                            progName, optstate->value);
                    exit(1);
                }

                rv = SECU_FileToItem(&decodeOptions.content, contentFile);
                PR_Close(contentFile);
                if (rv != SECSuccess) {
                    SECU_PrintError(progName, "problem reading content file");
                    exit(1);
                }
                if (!decodeOptions.content.data) {
                    /* file was zero length */
                    decodeOptions.content.data = (unsigned char *)PORT_Strdup("");
                    decodeOptions.content.len = 0;
                }

                break;
            case 'd':
                SECU_ConfigDirectory(optstate->value);
                break;
            case 'e':
                envFileName = PORT_Strdup(optstate->value);
                encryptOptions.envFile = PR_Open(envFileName, PR_RDONLY, 00660);
                break;

            case 'h':
                if (mode != DECODE) {
                    fprintf(stderr,
                            "%s: option -h only supported with option -D.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                decodeOptions.headerLevel = atoi(optstate->value);
                if (decodeOptions.headerLevel < 0) {
                    fprintf(stderr, "option -h cannot have a negative value.\n");
                    exit(1);
                }
                break;
            case 'i':
                if (!optstate->value) {
                    fprintf(stderr, "-i option requires filename argument\n");
                    exit(1);
                }
                inFile = PR_Open(optstate->value, PR_RDONLY, 00660);
                if (inFile == NULL) {
                    fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
                            progName, optstate->value);
                    exit(1);
                }
                break;

            case 'k':
                if (mode != DECODE) {
                    fprintf(stderr,
                            "%s: option -k only supported with option -D.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                decodeOptions.keepCerts = PR_TRUE;
                break;

            case 'n':
                if (mode != DECODE) {
                    fprintf(stderr,
                            "%s: option -n only supported with option -D.\n",
                            progName);
                    Usage();
                    exit(1);
                }
                decodeOptions.suppressContent = PR_TRUE;
                break;
            case 'o':
                outFile = fopen(optstate->value, "wb");
                if (outFile == NULL) {
                    fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
                            progName, optstate->value);
                    exit(1);
                }
                break;
            case 'p':
                if (!optstate->value) {
                    fprintf(stderr, "%s: option -p must have a value.\n", progName);
                    Usage();
                    exit(1);
                }

                options.password = strdup(optstate->value);
                break;

            case 'f':
                if (!optstate->value) {
                    fprintf(stderr, "%s: option -f must have a value.\n", progName);
                    Usage();
                    exit(1);
                }

                options.pwfile = strdup(optstate->value);
                break;

            case 'r':
                if (!optstate->value) {
                    fprintf(stderr, "%s: option -r must have a value.\n", progName);
                    Usage();
                    exit(1);
                }
                envelopeOptions.recipients = ptrarray;
                str = (char *)optstate->value;
                do {
                    tok = strchr(str, ',');
                    if (tok)
                        *tok = '\0';
                    envelopeOptions.recipients[nrecipients++] = strdup(str);
                    if (tok)
                        str = tok + 1;
                } while (tok);
                envelopeOptions.recipients[nrecipients] = NULL;
                encryptOptions.recipients = envelopeOptions.recipients;
                certsonlyOptions.recipients = envelopeOptions.recipients;
                break;

            case 'u': {
                int usageType;

                usageType = atoi(strdup(optstate->value));
                if (usageType < certUsageSSLClient || usageType > certUsageAnyCA)
                    return -1;
                options.certUsage = (SECCertUsage)usageType;
                break;
            }
            case 'v':
                cms_verbose = 1;
                break;
        }
    }
    if (status == PL_OPT_BAD)
        Usage();
    PL_DestroyOptState(optstate);

    if (mode == UNKNOWN)
        Usage();

    if (mode != CERTSONLY && !batch) {
        rv = SECU_FileToItem(&input, inFile);
        if (rv != SECSuccess) {
            SECU_PrintError(progName, "unable to read infile");
            exit(1);
        }
    }
    if (cms_verbose) {
        fprintf(stderr, "received commands\n");
    }

    /* Call the NSS initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_InitReadWrite(SECU_ConfigDirectory(NULL));
    if (SECSuccess != rv) {
        SECU_PrintError(progName, "NSS_Init failed");
        exit(1);
    }
    if (cms_verbose) {
        fprintf(stderr, "NSS has been initialized.\n");
    }
    options.certHandle = CERT_GetDefaultCertDB();
    if (!options.certHandle) {
        SECU_PrintError(progName, "No default cert DB");
        exit(1);
    }
    if (cms_verbose) {
        fprintf(stderr, "Got default certdb\n");
    }
    if (options.password) {
        pwdata.source = PW_PLAINTEXT;
        pwdata.data = options.password;
    }
    if (options.pwfile) {
        pwdata.source = PW_FROMFILE;
        pwdata.data = options.pwfile;
    }
    pwcb = SECU_GetModulePassword;
    pwcb_arg = (void *)&pwdata;

    PK11_SetPasswordFunc(&SECU_GetModulePassword);

#if defined(_WIN32)
    if (outFile == stdout) {
        /* If we're going to write binary data to stdout, we must put stdout
        ** into O_BINARY mode or else outgoing \n's will become \r\n's.
        */
        int smrv = _setmode(_fileno(stdout), _O_BINARY);
        if (smrv == -1) {
            fprintf(stderr,
                    "%s: Cannot change stdout to binary mode. Use -o option instead.\n",
                    progName);
            return smrv;
        }
    }
#endif

    exitstatus = 0;
    switch (mode) {
        case DECODE: /* -D */
            decodeOptions.options = &options;
            if (encryptOptions.envFile) {
                /* Decoding encrypted-data, so get the bulkkey from an
                 * enveloped-data message.
                 */
                SECU_FileToItem(&envmsg, encryptOptions.envFile);
                decodeOptions.options = &options;
                encryptOptions.envmsg = decode(NULL, &envmsg, &decodeOptions);
                if (!encryptOptions.envmsg) {
                    SECU_PrintError(progName, "problem decoding env msg");
                    exitstatus = 1;
                    break;
                }
                rv = get_enc_params(&encryptOptions);
                decodeOptions.dkcb = dkcb;
                decodeOptions.bulkkey = encryptOptions.bulkkey;
            }
            if (!batch) {
                cmsg = decode(outFile, &input, &decodeOptions);
                if (!cmsg) {
                    SECU_PrintError(progName, "problem decoding");
                    exitstatus = 1;
                }
            } else {
                exitstatus = doBatchDecode(outFile, inFile, &decodeOptions);
            }
            break;
        case SIGN: /* -S */
            signOptions.options = &options;
            cmsg = signed_data(&signOptions);
            if (!cmsg) {
                SECU_PrintError(progName, "problem signing");
                exitstatus = 1;
            }
            break;
        case ENCRYPT: /* -C */
            if (!envFileName) {
                fprintf(stderr, "%s: you must specify an envelope file with -e.\n",
                        progName);
                exit(1);
            }
            encryptOptions.options = &options;
            encryptOptions.input = &input;
            encryptOptions.outfile = outFile;
            /* decode an enveloped-data message to get the bulkkey (create
             * a new one if neccessary)
             */
            if (!encryptOptions.envFile) {
                encryptOptions.envFile = PR_Open(envFileName,
                                                 PR_WRONLY | PR_CREATE_FILE, 00660);
                if (!encryptOptions.envFile) {
                    fprintf(stderr, "%s: failed to create file %s.\n", progName,
                            envFileName);
                    exit(1);
                }
            } else {
                SECU_FileToItem(&envmsg, encryptOptions.envFile);
                decodeOptions.options = &options;
                encryptOptions.envmsg = decode(NULL, &envmsg, &decodeOptions);
                if (encryptOptions.envmsg == NULL) {
                    SECU_PrintError(progName, "problem decrypting env msg");
                    exitstatus = 1;
                    break;
                }
            }
            rv = get_enc_params(&encryptOptions);
            /* create the encrypted-data message */
            cmsg = encrypted_data(&encryptOptions);
            if (!cmsg) {
                SECU_PrintError(progName, "problem encrypting");
                exitstatus = 1;
            }
            if (encryptOptions.bulkkey) {
                PK11_FreeSymKey(encryptOptions.bulkkey);
                encryptOptions.bulkkey = NULL;
            }
            break;
        case ENVELOPE: /* -E */
            envelopeOptions.options = &options;
            cmsg = enveloped_data(&envelopeOptions);
            if (!cmsg) {
                SECU_PrintError(progName, "problem enveloping");
                exitstatus = 1;
            }
            break;
        case CERTSONLY: /* -O */
            certsonlyOptions.options = &options;
            cmsg = signed_data_certsonly(&certsonlyOptions);
            if (!cmsg) {
                SECU_PrintError(progName, "problem with certs-only");
                exitstatus = 1;
            }
            break;
        default:
            fprintf(stderr, "One of options -D, -S or -E must be set.\n");
            Usage();
            exitstatus = 1;
    }

    if (signOptions.nickname) {
        PORT_Free(signOptions.nickname);
    }

    if ((mode == SIGN || mode == ENVELOPE || mode == CERTSONLY) &&
        (!exitstatus)) {
        PLArenaPool *arena = PORT_NewArena(1024);
        NSSCMSEncoderContext *ecx;
        SECItem output = { 0, 0, 0 };

        if (!arena) {
            fprintf(stderr, "%s: out of memory.\n", progName);
            exit(1);
        }

        if (cms_verbose) {
            fprintf(stderr, "cmsg [%p]\n", cmsg);
            fprintf(stderr, "arena [%p]\n", arena);
            if (pwcb_arg && (PW_PLAINTEXT == ((secuPWData *)pwcb_arg)->source))
                fprintf(stderr, "password [%s]\n",
                        ((secuPWData *)pwcb_arg)->data);
            else
                fprintf(stderr, "password [NULL]\n");
        }
        ecx = NSS_CMSEncoder_Start(cmsg,
                                   NULL, NULL,     /* DER output callback  */
                                   &output, arena, /* destination storage  */
                                   pwcb, pwcb_arg, /* password callback    */
                                   NULL, NULL,     /* decrypt key callback */
                                   NULL, NULL);    /* detached digests    */
        if (!ecx) {
            fprintf(stderr, "%s: cannot create encoder context.\n", progName);
            exit(1);
        }
        if (cms_verbose) {
            fprintf(stderr, "input len [%d]\n", input.len);
            {
                unsigned int j;
                for (j = 0; j < input.len; j++)
                    fprintf(stderr, "%2x%c", input.data[j], (j > 0 && j % 35 == 0) ? '\n' : ' ');
            }
        }
        if (input.len > 0) { /* skip if certs-only (or other zero content) */
            rv = NSS_CMSEncoder_Update(ecx, (char *)input.data, input.len);
            if (rv) {
                fprintf(stderr,
                        "%s: failed to add data to encoder.\n", progName);
                exit(1);
            }
        }
        rv = NSS_CMSEncoder_Finish(ecx);
        if (rv) {
            SECU_PrintError(progName, "failed to encode data");
            exit(1);
        }

        if (cms_verbose) {
            fprintf(stderr, "encoding passed\n");
        }
        fwrite(output.data, output.len, 1, outFile);
        if (cms_verbose) {
            fprintf(stderr, "wrote to file\n");
        }
        PORT_FreeArena(arena, PR_FALSE);
    }
    if (cmsg)
        NSS_CMSMessage_Destroy(cmsg);
    if (outFile != stdout)
        fclose(outFile);

    if (inFile != PR_STDIN) {
        PR_Close(inFile);
    }
    if (envFileName) {
        PORT_Free(envFileName);
    }
    if (encryptOptions.envFile) {
        PR_Close(encryptOptions.envFile);
    }

    SECITEM_FreeItem(&decodeOptions.content, PR_FALSE);
    SECITEM_FreeItem(&envmsg, PR_FALSE);
    SECITEM_FreeItem(&input, PR_FALSE);
    if (NSS_Shutdown() != SECSuccess) {
        SECU_PrintError(progName, "NSS_Shutdown failed");
        exitstatus = 1;
    }
    PR_Cleanup();
    return exitstatus;
}
