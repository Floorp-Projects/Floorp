/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * cmsutil -- A command to work with CMS data
 *
 * $Id: cmsutil.c,v 1.6 2000/09/19 19:34:46 chrisk%netscape.com Exp $
 */

#include "nspr.h"
#include "secutil.h"
#include "plgetopt.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "cdbhdl.h"
#include "secoid.h"
#include "cms.h"
#include "smime.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

extern void SEC_Init(void);		/* XXX */

static SECStatus
DigestFile(PLArenaPool *poolp, SECItem ***digests, FILE *inFile, SECAlgorithmID **algids)
{
    NSSCMSDigestContext *digcx;
    int nb;
    char ibuf[4096];
    SECStatus rv;

    digcx = NSS_CMSDigestContext_StartMultiple(algids);
    if (digcx == NULL)
	return SECFailure;

    for (;;) {
	if (feof(inFile)) break;
	nb = fread(ibuf, 1, sizeof(ibuf), inFile);
	if (nb == 0) {
	    if (ferror(inFile)) {
		PORT_SetError(SEC_ERROR_IO);
		NSS_CMSDigestContext_Cancel(digcx);
		return SECFailure;
	    }
	    /* eof */
	    break;
	}
	NSS_CMSDigestContext_Update(digcx, (const unsigned char *)ibuf, nb);
    }

    rv = NSS_CMSDigestContext_FinishMultiple(digcx, poolp, digests);

    return rv;
}


static void
Usage(char *progName)
{
    fprintf(stderr, "Usage:  %s [-D|-S|-E] [<options>] [-d dbdir] [-u certusage]\n", progName);
    fprintf(stderr, " -i infile     use infile as source of data (default: stdin)\n");
    fprintf(stderr, " -o outfile    use outfile as destination of data (default: stdout)\n");
    fprintf(stderr, " -d dbdir      key/cert database directory (default: ~/.netscape)\n");
    fprintf(stderr, " -p password   use password as key db password (default: prompt)\n");
    fprintf(stderr, " -u certusage  set type of certificate usage (default: certUsageEmailSigner)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, " -D            decode a CMS message\n");
    fprintf(stderr, "  -c content   use this detached content\n");
    fprintf(stderr, "  -n           suppress output of content\n");
    fprintf(stderr, "  -h num       generate email headers with info about CMS message\n");
    fprintf(stderr, " -S            create a CMS signed message\n");
    fprintf(stderr, "  -N nick      use certificate named \"nick\" for signing\n");
    fprintf(stderr, "  -T           do not include content in CMS message\n");
    fprintf(stderr, "  -G           include a signing time attribute\n");
    fprintf(stderr, "  -P           include a SMIMECapabilities attribute\n");
    fprintf(stderr, "  -Y nick      include a EncryptionKeyPreference attribute with cert\n");
    fprintf(stderr, " -E            create a CMS enveloped message (NYI)\n");
    fprintf(stderr, "  -r id,...    create envelope for these recipients,\n");
    fprintf(stderr, "               where id can be a certificate nickname or email address\n");
    fprintf(stderr, "\nCert usage codes:\n");
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

    exit(-1);
}

static CERTCertDBHandle certHandleStatic;	/* avoid having to allocate */

static CERTCertDBHandle *
OpenCertDB(char *progName)
{
    CERTCertDBHandle *certHandle;
    SECStatus rv;

    certHandle = &certHandleStatic;
    rv = CERT_OpenCertDB(certHandle, PR_FALSE, SECU_CertDBNameCallback, NULL);
    if (rv != SECSuccess) {
        SECU_PrintError(progName, "could not open cert database");
	return NULL;
    }

    return certHandle;
}

char *
ownpw(PK11SlotInfo *info, PRBool retry, void *arg)
{
	char * passwd = NULL;

	if ( (!retry) && arg ) {
		passwd = PL_strdup((char *)arg);
	}

	return passwd;
}

struct optionsStr {
    char *password;
    SECCertUsage certUsage;
    CERTCertDBHandle *certHandle;
};

struct decodeOptionsStr {
    FILE *contentFile;
    int headerLevel;
    PRBool suppressContent;
};

struct signOptionsStr {
    char *nickname;
    char *encryptionKeyPreferenceNick;
    PRBool signingTime;
    PRBool smimeProfile;
    PRBool detached;
};

struct envelopeOptionsStr {
    char **recipients;
};

static int
decode(FILE *out, FILE *infile, char *progName, struct optionsStr options, struct decodeOptionsStr decodeOptions)
{
    NSSCMSDecoderContext *dcx;
    NSSCMSMessage *cmsg;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd = NULL;
    NSSCMSEnvelopedData *envd;
    SECAlgorithmID **digestalgs;
    unsigned char buffer[32];
    int nlevels, i, nsigners, j;
    char *signercn;
    NSSCMSSignerInfo *si;
    SECOidTag typetag;
    SECItem **digests;
    PLArenaPool *poolp;
    int nb;
    char ibuf[4096];
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    SECItem *item;

    pwcb     = (options.password != NULL) ? ownpw                    : NULL;
    pwcb_arg = (options.password != NULL) ? (void *)options.password : NULL;

    dcx = NSS_CMSDecoder_Start(NULL, 
				NULL, NULL,		/* content callback */
				pwcb, pwcb_arg,		/* password callback */
				NULL, NULL);		/* decrypt key callback */
    
    for (;;) {
	if (feof(infile)) break;
	nb = fread(ibuf, 1, sizeof(ibuf), infile);
	if (nb == 0) {
	    if (ferror(infile)) {
		fprintf(stderr, "ERROR: file i/o error.\n");
		NSS_CMSDecoder_Cancel(dcx);
		return SECFailure;
	    }
	    /* eof */
	    break;
	}
	(void)NSS_CMSDecoder_Update(dcx, (const char *)ibuf, nb);
    }
    cmsg = NSS_CMSDecoder_Finish(dcx);
    if (cmsg == NULL)
	return -1;

    if (decodeOptions.headerLevel >= 0) {
	fprintf(out, "SMIME: ", decodeOptions.headerLevel, i);
    }

    nlevels = NSS_CMSMessage_ContentLevelCount(cmsg);
    for (i = 0; i < nlevels; i++) {
	cinfo = NSS_CMSMessage_ContentLevel(cmsg, i);
	typetag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);

	if (decodeOptions.headerLevel >= 0)
	    fprintf(out, "\tlevel=%d.%d; ", decodeOptions.headerLevel, nlevels - i);

	switch (typetag) {
	case SEC_OID_PKCS7_SIGNED_DATA:
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "type=signedData; ");
	    sigd = (NSSCMSSignedData *)NSS_CMSContentInfo_GetContent(cinfo);
	    if (sigd == NULL) {
		SECU_PrintError(progName, "problem finding signedData component");
		return -1;
	    }

	    /* if we have a content file, but no digests for this signedData */
	    if (decodeOptions.contentFile != NULL && !NSS_CMSSignedData_HasDigests(sigd)) {
		if ((poolp = PORT_NewArena(1024)) == NULL) {
		    fprintf(stderr, "Out of memory.\n");
		    return -1;
		}
		digestalgs = NSS_CMSSignedData_GetDigestAlgs(sigd);
		if (DigestFile (poolp, &digests, decodeOptions.contentFile, digestalgs) != SECSuccess) {
		    SECU_PrintError(progName, "problem computing message digest");
		    return -1;
		}
		if (NSS_CMSSignedData_SetDigests(sigd, digestalgs, digests) != SECSuccess) {
		    
		    SECU_PrintError(progName, "problem setting message digests");
		    return -1;
		}
		PORT_FreeArena(poolp, PR_FALSE);
	    }

	    /* still no digests? */
	    if (!NSS_CMSSignedData_HasDigests(sigd)) {
		SECU_PrintError(progName, "no message digests");
		return -1;
	    }

	    /* find out about signers */
	    nsigners = NSS_CMSSignedData_SignerInfoCount(sigd);
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "nsigners=%d; ", nsigners);
	    if (nsigners == 0) {
		/* must be a cert transport message */
	    } else {
		/* import the certificates */
		if (NSS_CMSSignedData_ImportCerts(sigd, options.certHandle, options.certUsage, PR_FALSE) != SECSuccess) {
		    SECU_PrintError(progName, "cert import failed");
		    return -1;
		}

		for (j = 0; j < nsigners; j++) {
		    si = NSS_CMSSignedData_GetSignerInfo(sigd, j);
		    
		    signercn = NSS_CMSSignerInfo_GetSignerCommonName(si);
		    if (signercn == NULL)
			signercn = "";
		    if (decodeOptions.headerLevel >= 0)
			fprintf(out, "\n\t\tsigner%d.id=\"%s\"; ", j, signercn);
		    (void)NSS_CMSSignedData_VerifySignerInfo(sigd, j, options.certHandle, options.certUsage);

		    if (decodeOptions.headerLevel >= 0)
			fprintf(out, "signer%d.status=%s; ", j, NSS_CMSUtil_VerificationStatusToString(NSS_CMSSignerInfo_GetVerificationStatus(si)));
		    /* XXX what do we do if we don't print headers? */
		}
	    }
	    break;
	case SEC_OID_PKCS7_ENVELOPED_DATA:
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "type=envelopedData; ");
	    envd = (NSSCMSEnvelopedData *)NSS_CMSContentInfo_GetContent(cinfo);
	    break;
	case SEC_OID_PKCS7_DATA:
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "type=data; ");
	    break;
	default:
	    break;
	}
	if (decodeOptions.headerLevel >= 0)
	    fprintf(out, "\n");
    }

    if (!decodeOptions.suppressContent) {
	if (decodeOptions.contentFile) {
	    char buffer[4096];
	    size_t nbytes;
	    /* detached content: print content file */
	    fseek(decodeOptions.contentFile, 0, SEEK_SET);
	    while ((nbytes = fread(buffer, 1, sizeof(buffer), decodeOptions.contentFile)) != 0) {
		fwrite(buffer, nbytes, 1, out);
	    }
	} else {
	    if ((item = NSS_CMSMessage_GetContent(cmsg)) != NULL) {
		fwrite(item->data, item->len, 1, out);
	    }
	}
    }

    NSS_CMSMessage_Destroy(cmsg);
    return 0;
}

static void
writeout(void *arg, const char *buf, unsigned long len)
{
    FILE *f = (FILE *)arg;

    if (f != NULL && buf != NULL)
	(void)fwrite(buf, len, 1, f);
}

static int
sign(FILE *out, FILE *infile, char *progName, struct optionsStr options, struct signOptionsStr signOptions)
{
    NSSCMSEncoderContext *ecx;
    NSSCMSMessage *cmsg;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd;
    NSSCMSSignerInfo *signerinfo;
    int nb;
    char ibuf[4096];
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    CERTCertificate *cert, *ekpcert;

    if (signOptions.nickname == NULL) {
	fprintf(stderr, "ERROR: please indicate the nickname of a certificate to sign with.\n");
	return SECFailure;
    }
	
    if ((cert = CERT_FindCertByNickname(options.certHandle, signOptions.nickname)) == NULL) {
	SECU_PrintError(progName, "the corresponding cert for key \"%s\" does not exist",
			    signOptions.nickname);
	return SECFailure;
    }

    /*
     * create the message object
     */
    cmsg = NSS_CMSMessage_Create(NULL);		/* create a message on its own pool */
    if (cmsg == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS message.\n");
	return SECFailure;
    }
    /*
     * build chain of objects: message->signedData->data
     */
    if ((sigd = NSS_CMSSignedData_Create(cmsg)) == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS signedData object.\n");
	NSS_CMSMessage_Destroy(cmsg);
	return SECFailure;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
	NSS_CMSMessage_Destroy(cmsg);
	return SECFailure;
    }

    cinfo = NSS_CMSSignedData_GetContentInfo(sigd);
    /* we're always passing data in and detaching optionally */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, signOptions.detached) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	NSS_CMSMessage_Destroy(cmsg);
	return SECFailure;
    }

    /* 
     * create & attach signer information
     */
    if ((signerinfo = NSS_CMSSignerInfo_Create(cmsg, cert, SEC_OID_SHA1)) == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS signerInfo object.\n");
	NSS_CMSMessage_Destroy(cmsg);
	return SECFailure;
    }

    /* we want the cert chain included for this one */
    if (NSS_CMSSignerInfo_IncludeCerts(signerinfo, NSSCMSCM_CertChain, options.certUsage) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot find cert chain.\n");
	NSS_CMSMessage_Destroy(cmsg);
	return SECFailure;
    }

    if (signOptions.signingTime) {
	if (NSS_CMSSignerInfo_AddSigningTime(signerinfo, PR_Now()) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add signingTime attribute.\n");
	    NSS_CMSMessage_Destroy(cmsg);
	    return SECFailure;
	}
    }
    if (signOptions.smimeProfile) {
	if (NSS_CMSSignerInfo_AddSMIMECaps(signerinfo) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add SMIMECaps attribute.\n");
	    NSS_CMSMessage_Destroy(cmsg);
	    return SECFailure;
	}
    }
    if (signOptions.encryptionKeyPreferenceNick) {
	/* get the cert, add it to the message */
	if ((ekpcert = CERT_FindCertByNickname(options.certHandle, signOptions.encryptionKeyPreferenceNick)) == NULL) {
	    SECU_PrintError(progName, "the corresponding cert for key \"%s\" does not exist",
				signOptions.encryptionKeyPreferenceNick);
	    NSS_CMSMessage_Destroy(cmsg);
	    return SECFailure;
	}
	if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, ekpcert, options.certHandle) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add SMIMEEncKeyPrefs attribute.\n");
	    NSS_CMSMessage_Destroy(cmsg);
	    return SECFailure;
	}
	if (NSS_CMSSignedData_AddCertificate(sigd, ekpcert) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add encryption certificate.\n");
	    NSS_CMSMessage_Destroy(cmsg);
	    return SECFailure;
	}
    } else {
	/* check signing cert for fitness as encryption cert */
	/* if yes, add signing cert as EncryptionKeyPreference */
	if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, cert, options.certHandle) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add default SMIMEEncKeyPrefs attribute.\n");
	    NSS_CMSMessage_Destroy(cmsg);
	    return SECFailure;
	}
    }

    if (NSS_CMSSignedData_AddSignerInfo(sigd, signerinfo) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot add CMS signerInfo object.\n");
	NSS_CMSMessage_Destroy(cmsg);
	return SECFailure;
    }

    /*
     * do not add signer independent certificates
     */

    pwcb     = (options.password != NULL) ? ownpw                    : NULL;
    pwcb_arg = (options.password != NULL) ? (void *)options.password : NULL;

    ecx = NSS_CMSEncoder_Start(cmsg, 
				writeout, out,		/* DER output callback */
				NULL, NULL,		/* destination storage */
				pwcb, pwcb_arg,		/* password callback */
				NULL, NULL,		/* decrypt key callback */
				NULL, NULL);		/* detached digests (not used, we feed) */
    
    for (;;) {
	if (feof(infile)) break;
	nb = fread(ibuf, 1, sizeof(ibuf), infile);
	if (nb == 0) {
	    if (ferror(infile)) {
		fprintf(stderr, "ERROR: file i/o error.\n");
		NSS_CMSEncoder_Cancel(ecx);
		return SECFailure;
	    }
	    /* eof */
	    break;
	}
	(void)NSS_CMSEncoder_Update(ecx, (const char *)ibuf, nb);
    }
    if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
	fprintf(stderr, "ERROR: DER encoding problem.\n");
	return SECFailure;
    }

    NSS_CMSMessage_Destroy(cmsg);
    return SECSuccess;
}

static int
envelope(FILE *out, FILE *infile, char *progName, struct optionsStr options, struct envelopeOptionsStr envelopeOptions)
{
    SECStatus retval = SECFailure;
    NSSCMSEncoderContext *ecx;
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSEnvelopedData *envd;
    NSSCMSRecipientInfo *recipientinfo;
    int cnt;
    int nb;
    char ibuf[4096];
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    CERTCertificate **recipientcerts;
    PLArenaPool *tmppoolp = NULL;
    SECOidTag bulkalgtag;
    int keysize, i;

    if ((cnt = NSS_CMSArray_Count(envelopeOptions.recipients)) == 0) {
	fprintf(stderr, "ERROR: please indicate the nickname of a certificate to sign with.\n");
	goto loser;
    }

    if ((tmppoolp = PORT_NewArena (1024)) == NULL) {
	fprintf(stderr, "ERROR: out of memory.\n");
	goto loser;
    }
	
    /* XXX find the recipient's certs by email address or nickname */
    if ((recipientcerts = (CERTCertificate **)NSS_CMSArray_Alloc(tmppoolp, cnt)) == NULL) {
	fprintf(stderr, "ERROR: out of memory.\n");
	goto loser;
    }

    for (i=0; envelopeOptions.recipients[i] != NULL; i++) {
	if ((recipientcerts[i] = CERT_FindCertByNicknameOrEmailAddr(options.certHandle, envelopeOptions.recipients[i])) == NULL) {
	    SECU_PrintError(progName, "cannot find certificate for \"%s\"", envelopeOptions.recipients[i]);
	    goto loser;
	}
    }
    recipientcerts[i] = NULL;

    /* find a nice bulk algorithm */
    if (NSS_SMIMEUtil_FindBulkAlgForRecipients(recipientcerts, &bulkalgtag, &keysize) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot find common bulk algorithm.\n");
	goto loser;
    }

    /*
     * create the message object
     */
    cmsg = NSS_CMSMessage_Create(NULL);		/* create a message on its own pool */
    if (cmsg == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS message.\n");
	goto loser;
    }
    /*
     * build chain of objects: message->envelopedData->data
     */
    if ((envd = NSS_CMSEnvelopedData_Create(cmsg, bulkalgtag, keysize)) == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS envelopedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_EnvelopedData(cmsg, cinfo, envd) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS envelopedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSEnvelopedData_GetContentInfo(envd);
    /* we're always passing data in, so the content is NULL */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }

    /* 
     * create & attach recipient information
     */
    for (i = 0; recipientcerts[i] != NULL; i++) {
	if ((recipientinfo = NSS_CMSRecipientInfo_Create(cmsg, recipientcerts[i])) == NULL) {
	    fprintf(stderr, "ERROR: cannot create CMS recipientInfo object.\n");
	    goto loser;
	}
	if (NSS_CMSEnvelopedData_AddRecipient(envd, recipientinfo) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add CMS recipientInfo object.\n");
	    goto loser;
	}
    }

    /* we might need a password for diffie hellman key agreement, so set it up... */
    pwcb     = (options.password != NULL) ? ownpw                    : NULL;
    pwcb_arg = (options.password != NULL) ? (void *)options.password : NULL;

    ecx = NSS_CMSEncoder_Start(cmsg, 
				writeout, out,		/* DER output callback */
				NULL, NULL,		/* destination storage */
				pwcb, pwcb_arg,		/* password callback */
				NULL, NULL,		/* decrypt key callback (not used) */
				NULL, NULL);		/* detached digests (not used) */
    
    for (;;) {
	if (feof(infile)) break;
	nb = fread(ibuf, 1, sizeof(ibuf), infile);
	if (nb == 0) {
	    if (ferror(infile)) {
		fprintf(stderr, "ERROR: file i/o error.\n");
		NSS_CMSEncoder_Cancel(ecx);
		goto loser;
	    }
	    /* eof */
	    break;
	}
	(void)NSS_CMSEncoder_Update(ecx, (const char *)ibuf, nb);
    }
    if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
	fprintf(stderr, "ERROR: DER encoding problem.\n");
	goto loser;
    }

    retval = SECSuccess;

loser:
    if (cmsg)
	NSS_CMSMessage_Destroy(cmsg);
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, PR_FALSE);
    return retval;
}

typedef enum { UNKNOWN, DECODE, SIGN, ENCRYPT } Mode;

int
main(int argc, char **argv)
{
    char *progName;
    FILE *outFile, *inFile;
    PLOptState *optstate;
    PLOptStatus status;
    Mode mode = UNKNOWN;
    struct decodeOptionsStr decodeOptions = { 0 };
    struct signOptionsStr signOptions = { 0 };
    struct envelopeOptionsStr envelopeOptions = { 0 };
    struct optionsStr options = { 0 };
    int exitstatus;
    static char *ptrarray[128] = { 0 };
    int nrecipients = 0;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    inFile = stdin;
    outFile = stdout;
    mode = UNKNOWN;
    decodeOptions.contentFile = NULL;
    decodeOptions.suppressContent = PR_FALSE;
    decodeOptions.headerLevel = -1;
    options.certUsage = certUsageEmailSigner;
    options.password = NULL;
    signOptions.nickname = NULL;
    signOptions.detached = PR_FALSE;
    signOptions.signingTime = PR_FALSE;
    signOptions.smimeProfile = PR_FALSE;
    signOptions.encryptionKeyPreferenceNick = NULL;
    envelopeOptions.recipients = NULL;

    /*
     * Parse command line arguments
     */
    optstate = PL_CreateOptState(argc, argv, "DSEnN:TGPY:h:p:i:c:d:o:s:u:r:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	case '?':
	    Usage(progName);
	    break;
	
	case 'D':
	    mode = DECODE;
	    break;
	case 'S':
	    mode = SIGN;
	    break;
	case 'E':
	    mode = ENCRYPT;
	    break;

	case 'n':
	    if (mode != DECODE) {
		fprintf(stderr, "%s: option -n only supported with option -D.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    decodeOptions.suppressContent = PR_TRUE;
	    break;

	case 'N':
	    if (mode != SIGN) {
		fprintf(stderr, "%s: option -N only supported with option -S.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.nickname = strdup(optstate->value);
	    break;

	case 'Y':
	    if (mode != SIGN) {
		fprintf(stderr, "%s: option -Y only supported with option -S.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.encryptionKeyPreferenceNick = strdup(optstate->value);
	    break;

	case 'T':
	    if (mode != SIGN) {
		fprintf(stderr, "%s: option -T only supported with option -S.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.detached = PR_TRUE;
	    break;

	case 'G':
	    if (mode != SIGN) {
		fprintf(stderr, "%s: option -G only supported with option -S.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.signingTime = PR_TRUE;
	    break;

	case 'P':
	    if (mode != SIGN) {
		fprintf(stderr, "%s: option -P only supported with option -S.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.smimeProfile = PR_TRUE;
	    break;

	case 'h':
	    if (mode != DECODE) {
		fprintf(stderr, "%s: option -h only supported with option -D.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    decodeOptions.headerLevel = atoi(optstate->value);
	    if (decodeOptions.headerLevel < 0) {
		fprintf(stderr, "option -h cannot have a negative value.\n");
		exit(1);
	    }
	    break;

	case 'p':
	    if (!optstate->value) {
		fprintf(stderr, "%s: option -p must have a value.\n", progName);
		Usage(progName);
		exit(1);
	    }
		
	    options.password = strdup(optstate->value);
	    break;

	case 'i':
	    if ((inFile = fopen(optstate->value, "r")) == NULL) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		exit(1);
	    }
	    break;

	case 'c':
	    if (mode != DECODE) {
		fprintf(stderr, "%s: option -c only supported with option -D.\n", progName);
		Usage(progName);
		exit(1);
	    }
	    if ((decodeOptions.contentFile = fopen(optstate->value, "r")) == NULL) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading.\n",
			progName, optstate->value);
		exit(1);
	    }
	    break;

	case 'o':
	    if ((outFile = fopen(optstate->value, "w")) == NULL) {
		fprintf(stderr, "%s: unable to open \"%s\" for writing\n",
			progName, optstate->value);
		exit(1);
	    }
	    break;

	case 'r':
	    if (!optstate->value) {
		fprintf(stderr, "%s: option -r must have a value.\n", progName);
		Usage(progName);
		exit(1);
	    }
#if 0
	    fprintf(stderr, "recipient = %s\n", optstate->value);
#endif
	    envelopeOptions.recipients = ptrarray;
	    envelopeOptions.recipients[nrecipients++] = strdup(optstate->value);
	    envelopeOptions.recipients[nrecipients] = NULL;
	    break;

	case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	case 'u': {
	    int usageType;

	    usageType = atoi (strdup(optstate->value));
	    if (usageType < certUsageSSLClient || usageType > certUsageAnyCA)
		return -1;
	    options.certUsage = (SECCertUsage)usageType;
	    break;
	  }
	      
	}
    }

    /* Call the libsec initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    SECU_PKCS11Init(PR_FALSE);
    RNG_SystemInfoForRNG();     /* SECU_PKCS11Init does not call this */
    SEC_Init();                 /* this does nothing, actually */

    /* open cert database */
    options.certHandle = OpenCertDB(progName);
    if (options.certHandle == NULL) {
	return -1;
    }
    CERT_SetDefaultCertDB(options.certHandle);

    exitstatus = 0;
    switch (mode) {
    case DECODE:
	if (decode(outFile, inFile, progName, options, decodeOptions)) {
	    SECU_PrintError(progName, "problem decoding");
	    exitstatus = 1;
	}
	break;
    case SIGN:
	if (sign(outFile, inFile, progName, options, signOptions)) {
	    SECU_PrintError(progName, "problem signing");
	    exitstatus = 1;
	}
	break;
    case ENCRYPT:
	if (envelope(outFile, inFile, progName, options, envelopeOptions)) {
	    SECU_PrintError(progName, "problem encrypting");
	    exitstatus = 1;
	}
	break;
    default:
	fprintf(stderr, "One of options -D, -S or -E must be set.\n");
	Usage(progName);
	exitstatus = 1;
    }
    if (outFile != stdout)
	fclose(outFile);

    exit(exitstatus);
}
