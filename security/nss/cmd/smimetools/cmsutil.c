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
 * $Id: cmsutil.c,v 1.10 2000/10/06 21:45:01 nelsonb%netscape.com Exp $
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
#include "nss.h"
#include "smime.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

extern void SEC_Init(void);		/* XXX */
char *progName = NULL;

static SECStatus
DigestFile(PLArenaPool *poolp, SECItem ***digests, SECItem *input,
           SECAlgorithmID **algids)
{
    NSSCMSDigestContext *digcx;

    digcx = NSS_CMSDigestContext_StartMultiple(algids);
    if (digcx == NULL)
	return SECFailure;

    NSS_CMSDigestContext_Update(digcx, input->data, input->len);

    return NSS_CMSDigestContext_FinishMultiple(digcx, poolp, digests);
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
    struct optionsStr *options;
    PRFileDesc *contentFile;
    int headerLevel;
    PRBool suppressContent;
    NSSCMSGetDecryptKeyCallback dkcb;
    PK11SymKey *bulkkey;
};

struct signOptionsStr {
    struct optionsStr *options;
    char *nickname;
    char *encryptionKeyPreferenceNick;
    PRBool signingTime;
    PRBool smimeProfile;
    PRBool detached;
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
decode(FILE *out, SECItem *output, SECItem *input, 
       struct decodeOptionsStr decodeOptions)
{
    NSSCMSDecoderContext *dcx;
    NSSCMSMessage *cmsg;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd = NULL;
    NSSCMSEnvelopedData *envd;
    NSSCMSEncryptedData *encd;
    SECAlgorithmID **digestalgs;
    int nlevels, i, nsigners, j;
    char *signercn;
    NSSCMSSignerInfo *si;
    SECOidTag typetag;
    SECItem **digests;
    PLArenaPool *poolp;
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    SECItem *item, sitem = { 0, 0, 0 };

    pwcb     = (decodeOptions.options->password != NULL) ? ownpw : NULL;
    pwcb_arg = (decodeOptions.options->password != NULL) ? 
                  (void *)decodeOptions.options->password : NULL;

    if (decodeOptions.contentFile) {
	/* detached content: grab content file */
	SECU_FileToItem(&sitem, decodeOptions.contentFile);
	item = &sitem;
    }

    dcx = NSS_CMSDecoder_Start(NULL, 
                               NULL, NULL,         /* content callback     */
                               pwcb, pwcb_arg,     /* password callback    */
			       decodeOptions.dkcb, /* decrypt key callback */
                               decodeOptions.bulkkey);
    (void)NSS_CMSDecoder_Update(dcx, input->data, input->len);
    cmsg = NSS_CMSDecoder_Finish(dcx);
    if (cmsg == NULL) {
	fprintf(stderr, "%s: failed to decode message.\n", progName);
	return NULL;
    }

    if (decodeOptions.headerLevel >= 0) {
	/*fprintf(out, "SMIME: ", decodeOptions.headerLevel, i);*/
	fprintf(out, "SMIME: ");
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
		SECU_PrintError(progName, 
		                "problem finding signedData component");
		goto loser;
	    }

	    /* if we have a content file, but no digests for this signedData */
	    if (decodeOptions.contentFile != NULL && !NSS_CMSSignedData_HasDigests(sigd)) {
		if ((poolp = PORT_NewArena(1024)) == NULL) {
		    fprintf(stderr, "Out of memory.\n");
		    goto loser;
		}
		digestalgs = NSS_CMSSignedData_GetDigestAlgs(sigd);
		if (DigestFile (poolp, &digests, item, digestalgs) 
		      != SECSuccess) {
		    SECU_PrintError(progName, 
		                    "problem computing message digest");
		    goto loser;
		}
		if (NSS_CMSSignedData_SetDigests(sigd, digestalgs, digests) != SECSuccess) {
		    
		    SECU_PrintError(progName, 
		                    "problem setting message digests");
		    goto loser;
		}
		PORT_FreeArena(poolp, PR_FALSE);
	    }

	    /* import the certificates */
	    if (NSS_CMSSignedData_ImportCerts(sigd, 
	                                     decodeOptions.options->certHandle, 
	                                     decodeOptions.options->certUsage, 
	                                     PR_FALSE) 
	          != SECSuccess) {
		SECU_PrintError(progName, "cert import failed");
		goto loser;
	    }

	    /* find out about signers */
	    nsigners = NSS_CMSSignedData_SignerInfoCount(sigd);
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "nsigners=%d; ", nsigners);
	    if (nsigners == 0) {
		/* must be a cert transport message */
		SECStatus rv;
		/* XXX workaround for bug #54014 */
		NSS_CMSSignedData_ImportCerts(sigd, 
                                             decodeOptions.options->certHandle, 
		                             decodeOptions.options->certUsage, 
		                             PR_TRUE);
		rv = NSS_CMSSignedData_VerifyCertsOnly(sigd, 
		                             decodeOptions.options->certHandle, 
		                             decodeOptions.options->certUsage);
		if (rv != SECSuccess) {
		    fprintf(stderr, "Verify certs-only failed!\n");
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
		si = NSS_CMSSignedData_GetSignerInfo(sigd, j);
		signercn = NSS_CMSSignerInfo_GetSignerCommonName(si);
		if (signercn == NULL)
		    signercn = "";
		if (decodeOptions.headerLevel >= 0)
		    fprintf(out, "\n\t\tsigner%d.id=\"%s\"; ", j, signercn);
		(void)NSS_CMSSignedData_VerifySignerInfo(sigd, j, 
		                             decodeOptions.options->certHandle, 
		                             decodeOptions.options->certUsage);
		if (decodeOptions.headerLevel >= 0)
		    fprintf(out, "signer%d.status=%s; ", j, 
		            NSS_CMSUtil_VerificationStatusToString(
		                  NSS_CMSSignerInfo_GetVerificationStatus(si)));
		    /* XXX what do we do if we don't print headers? */
	    }
	    break;
	case SEC_OID_PKCS7_ENVELOPED_DATA:
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "type=envelopedData; ");
	    envd = (NSSCMSEnvelopedData *)NSS_CMSContentInfo_GetContent(cinfo);
	    break;
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	    if (decodeOptions.headerLevel >= 0)
		fprintf(out, "type=encryptedData; ");
	    encd = (NSSCMSEncryptedData *)NSS_CMSContentInfo_GetContent(cinfo);
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
	if (!decodeOptions.contentFile) 
	    item = NSS_CMSMessage_GetContent(cmsg);
	SECITEM_CopyItem(NULL, output, item);
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
signed_data(struct signOptionsStr signOptions)
{
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd;
    NSSCMSSignerInfo *signerinfo;
    CERTCertificate *cert, *ekpcert;

    if (signOptions.nickname == NULL) {
	fprintf(stderr, 
        "ERROR: please indicate the nickname of a certificate to sign with.\n");
	return NULL;
    }
    if ((cert = CERT_FindCertByNickname(signOptions.options->certHandle, 
                                        signOptions.nickname)) == NULL) {
	SECU_PrintError(progName, 
	                "the corresponding cert for key \"%s\" does not exist",
	                signOptions.nickname);
	return NULL;
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
    if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSSignedData_GetContentInfo(sigd);
    /* we're always passing data in and detaching optionally */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, 
                                           signOptions.detached) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }
    /* 
     * create & attach signer information
     */
    if ((signerinfo = NSS_CMSSignerInfo_Create(cmsg, cert, SEC_OID_SHA1)) 
          == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS signerInfo object.\n");
	goto loser;
    }
    /* we want the cert chain included for this one */
    if (NSS_CMSSignerInfo_IncludeCerts(signerinfo, NSSCMSCM_CertChain, 
                                       signOptions.options->certUsage) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot find cert chain.\n");
	goto loser;
    }
    if (signOptions.signingTime) {
	if (NSS_CMSSignerInfo_AddSigningTime(signerinfo, PR_Now()) 
	      != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add signingTime attribute.\n");
	    goto loser;
	}
    }
    if (signOptions.smimeProfile) {
	if (NSS_CMSSignerInfo_AddSMIMECaps(signerinfo) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add SMIMECaps attribute.\n");
	    goto loser;
	}
    }
    if (signOptions.encryptionKeyPreferenceNick) {
	/* get the cert, add it to the message */
	if ((ekpcert = CERT_FindCertByNickname(signOptions.options->certHandle, 
	                               signOptions.encryptionKeyPreferenceNick))
	      == NULL) {
	    SECU_PrintError(progName, 
	                 "the corresponding cert for key \"%s\" does not exist",
	                    signOptions.encryptionKeyPreferenceNick);
	    goto loser;
	}
	if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, ekpcert, 
	                                        signOptions.options->certHandle)
	      != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add SMIMEEncKeyPrefs attribute.\n");
	    goto loser;
	}
	if (NSS_CMSSignedData_AddCertificate(sigd, ekpcert) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add encryption certificate.\n");
	    goto loser;
	}
    } else {
	/* check signing cert for fitness as encryption cert */
	/* if yes, add signing cert as EncryptionKeyPreference */
	if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, cert, 
	                                        signOptions.options->certHandle)
	      != SECSuccess) {
	    fprintf(stderr, 
	            "ERROR: cannot add default SMIMEEncKeyPrefs attribute.\n");
	    goto loser;
	}
    }
    if (NSS_CMSSignedData_AddSignerInfo(sigd, signerinfo) != SECSuccess) {
	fprintf(stderr, "ERROR: cannot add CMS signerInfo object.\n");
	goto loser;
    }
    return cmsg;
loser:
    NSS_CMSMessage_Destroy(cmsg);
    return NULL;
}

static NSSCMSMessage *
enveloped_data(struct envelopeOptionsStr envelopeOptions)
{
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSEnvelopedData *envd;
    NSSCMSRecipientInfo *recipientinfo;
    CERTCertificate **recipientcerts;
    CERTCertDBHandle *dbhandle;
    PLArenaPool *tmppoolp = NULL;
    SECOidTag bulkalgtag;
    int keysize, i;
    int cnt;
    dbhandle = envelopeOptions.options->certHandle;
    /* count the recipients */
    if ((cnt = NSS_CMSArray_Count(envelopeOptions.recipients)) == 0) {
	fprintf(stderr, "ERROR: please name at least one recipient.\n");
	goto loser;
    }
    if ((tmppoolp = PORT_NewArena (1024)) == NULL) {
	fprintf(stderr, "ERROR: out of memory.\n");
	goto loser;
    }
    /* XXX find the recipient's certs by email address or nickname */
    if ((recipientcerts = 
         (CERTCertificate **)PORT_ArenaZAlloc(tmppoolp, 
					     (cnt+1)*sizeof(CERTCertificate*)))
            == NULL) {
	fprintf(stderr, "ERROR: out of memory.\n");
	goto loser;
    }
    for (i=0; envelopeOptions.recipients[i] != NULL; i++) {
	if ((recipientcerts[i] = 
	      CERT_FindCertByNicknameOrEmailAddr(dbhandle,  
	                                        envelopeOptions.recipients[i]))
	        == NULL) {
	    SECU_PrintError(progName, "cannot find certificate for \"%s\"", 
	                    envelopeOptions.recipients[i]);
	    goto loser;
	}
    }
    recipientcerts[i] = NULL;
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
    if ((envd = NSS_CMSEnvelopedData_Create(cmsg, bulkalgtag, keysize)) 
          == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS envelopedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_EnvelopedData(cmsg, cinfo, envd) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS envelopedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSEnvelopedData_GetContentInfo(envd);
    /* we're always passing data in, so the content is NULL */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }
    /* 
     * create & attach recipient information
     */
    for (i = 0; recipientcerts[i] != NULL; i++) {
	if ((recipientinfo = NSS_CMSRecipientInfo_Create(cmsg, 
	                                                 recipientcerts[i])) 
	      == NULL) {
	    fprintf(stderr, "ERROR: cannot create CMS recipientInfo object.\n");
	    goto loser;
	}
	if (NSS_CMSEnvelopedData_AddRecipient(envd, recipientinfo) 
	      != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add CMS recipientInfo object.\n");
	    goto loser;
	}
    }
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, PR_FALSE);
    return cmsg;
loser:
    if (cmsg)
	NSS_CMSMessage_Destroy(cmsg);
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, PR_FALSE);
    return NULL;
}

PK11SymKey *dkcb(void *arg, SECAlgorithmID *algid)
{
    return (PK11SymKey*)arg;
}

static SECStatus
get_enc_params(struct encryptOptionsStr *encryptOptions)
{
    struct envelopeOptionsStr envelopeOptions;
    SECStatus rv = SECFailure;
    NSSCMSMessage *env_cmsg;
    NSSCMSContentInfo *cinfo;
    PK11SymKey *bulkkey = NULL;
    SECOidTag bulkalgtag;
    int keysize;
    int i, nlevels;
    /*
     * construct an enveloped data message to obtain bulk keys
     */
    if (encryptOptions->envmsg) {
	env_cmsg = encryptOptions->envmsg; /* get it from an old message */
    } else {
	SECItem dummyOut = { 0, 0, 0 };
	SECItem dummyIn  = { 0, 0, 0 };
	char str[] = "Hello!";
	PLArenaPool *tmparena = PORT_NewArena(1024);
	dummyIn.data = str;
	dummyIn.len = strlen(str);
	envelopeOptions.options = encryptOptions->options;
	envelopeOptions.recipients = encryptOptions->recipients;
	env_cmsg = enveloped_data(envelopeOptions);
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
	    bulkalgtag = NSS_CMSContentInfo_GetContentEncAlgTag(cinfo);
	    keysize = NSS_CMSContentInfo_GetBulkKeySize(cinfo);
	    bulkkey = NSS_CMSContentInfo_GetBulkKey(cinfo);
	    break;
	}
    }
    if (i == nlevels) {
	fprintf(stderr, "%s: could not retrieve enveloped data.", progName);
	goto loser;
    }
    encryptOptions->bulkalgtag = bulkalgtag;
    encryptOptions->bulkkey = bulkkey;
    encryptOptions->keysize = keysize;
    rv = SECSuccess;
loser:
    if (env_cmsg)
	NSS_CMSMessage_Destroy(env_cmsg);
    return rv;
}

static NSSCMSMessage *
encrypted_data(struct encryptOptionsStr encryptOptions)
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
    if ((encd = NSS_CMSEncryptedData_Create(cmsg, encryptOptions.bulkalgtag, 
                                                  encryptOptions.keysize)) 
           == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS encryptedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_EncryptedData(cmsg, cinfo, encd)
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS encryptedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSEncryptedData_GetContentInfo(encd);
    /* we're always passing data in, so the content is NULL */
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }
    ecx = NSS_CMSEncoder_Start(cmsg, NULL, NULL, &derOut, tmppoolp, NULL, NULL,
                               dkcb, encryptOptions.bulkkey, NULL, NULL);
    if (!ecx) {
	fprintf(stderr, "%s: cannot create encoder context.\n", progName);
	goto loser;
    }
    rv = NSS_CMSEncoder_Update(ecx, encryptOptions.input->data, 
                                    encryptOptions.input->len);
    if (rv) {
	fprintf(stderr, "%s: failed to add data to encoder.\n", progName);
	goto loser;
    }
    rv = NSS_CMSEncoder_Finish(ecx);
    if (rv) {
	fprintf(stderr, "%s: failed to encrypt data.\n", progName);
	goto loser;
    }
    fwrite(derOut.data, derOut.len, 1, encryptOptions.outfile);
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
signed_data_certsonly(struct certsonlyOptionsStr certsonlyOptions)
{
    NSSCMSMessage *cmsg = NULL;
    NSSCMSContentInfo *cinfo;
    NSSCMSSignedData *sigd;
    CERTCertificate **certs;
    CERTCertDBHandle *dbhandle;
    PLArenaPool *tmppoolp = NULL;
    int i, cnt;
    dbhandle = certsonlyOptions.options->certHandle;
    if ((cnt = NSS_CMSArray_Count(certsonlyOptions.recipients)) == 0) {
	fprintf(stderr, 
        "ERROR: please indicate the nickname of a certificate to sign with.\n");
	goto loser;
    }
    if ((tmppoolp = PORT_NewArena (1024)) == NULL) {
	fprintf(stderr, "ERROR: out of memory.\n");
	goto loser;
    }
    if ((certs = 
         (CERTCertificate **)PORT_ArenaZAlloc(tmppoolp, 
					     (cnt+1)*sizeof(CERTCertificate*)))
            == NULL) {
	fprintf(stderr, "ERROR: out of memory.\n");
	goto loser;
    }
    for (i=0; certsonlyOptions.recipients[i] != NULL; i++) {
	if ((certs[i] = 
	      CERT_FindCertByNicknameOrEmailAddr(dbhandle,
	                                        certsonlyOptions.recipients[i]))
	        == NULL) {
	    SECU_PrintError(progName, "cannot find certificate for \"%s\"", 
	                    certsonlyOptions.recipients[i]);
	    goto loser;
	}
    }
    certs[i] = NULL;
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
    if ((sigd = NSS_CMSSignedData_CreateCertsOnly(cmsg, certs[0], PR_TRUE))
          == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS signedData object.\n");
	goto loser;
    }
    for (i=1; i<cnt; i++) {
	if (NSS_CMSSignedData_AddCertChain(sigd, certs[i])) {
	    fprintf(stderr, "ERROR: cannot add cert chain for \"%s\".\n",
	            certsonlyOptions.recipients[i]);
	    goto loser;
	}
    }
    cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
    if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
	goto loser;
    }
    cinfo = NSS_CMSSignedData_GetContentInfo(sigd);
    if (NSS_CMSContentInfo_SetContent_Data(cmsg, cinfo, NULL, PR_FALSE) 
	   != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, PR_FALSE);
    return cmsg;
loser:
    if (cmsg)
	NSS_CMSMessage_Destroy(cmsg);
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, PR_FALSE);
    return NULL;
}

typedef enum { UNKNOWN, DECODE, SIGN, ENCRYPT, ENVELOPE, CERTSONLY } Mode;

#if 0
void
parse_message_for_recipients(PRFileDesc *inFile, 
                             struct envelopeOptionsStr *envelopeOptions)
{
    SECItem filedata;
    SECStatus rv;
    rv = SECU_FileToItem(&filedata, inFile);
}
#endif

int
main(int argc, char **argv)
{
    FILE *outFile;
    NSSCMSMessage *cmsg;
    PRFileDesc *inFile;
    PLOptState *optstate;
    PLOptStatus status;
    Mode mode = UNKNOWN;
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
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
    SECItem input = { 0, 0, 0};
    SECItem output = { 0, 0, 0};
    SECItem dummy = { 0, 0, 0 };
    SECItem envmsg = { 0, 0, 0 };
    SECStatus rv;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    inFile = PR_STDIN;
    outFile = stdout;
    envFileName = NULL;
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
                                 "CDSEOnN:TGPY:h:p:i:c:d:e:o:s:u:r:");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	case '?':
	    Usage(progName);
	    break;
	
	case 'C':
	    mode = ENCRYPT;
	    break;
	case 'D':
	    mode = DECODE;
	    break;
	case 'S':
	    mode = SIGN;
	    break;
	case 'E':
	    mode = ENVELOPE;
	    break;
	case 'O':
	    mode = CERTSONLY;
	    break;

	case 'n':
	    if (mode != DECODE) {
		fprintf(stderr, 
		        "%s: option -n only supported with option -D.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    decodeOptions.suppressContent = PR_TRUE;
	    break;

	case 'N':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -N only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.nickname = strdup(optstate->value);
	    break;

	case 'Y':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -Y only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.encryptionKeyPreferenceNick = strdup(optstate->value);
	    break;

	case 'T':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -T only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.detached = PR_TRUE;
	    break;

	case 'G':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -G only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.signingTime = PR_TRUE;
	    break;

	case 'P':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -P only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.smimeProfile = PR_TRUE;
	    break;

	case 'h':
	    if (mode != DECODE) {
		fprintf(stderr, 
		        "%s: option -h only supported with option -D.\n", 
		        progName);
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
	    inFile = PR_Open(optstate->value, PR_RDONLY, 00660);
	    if (inFile == NULL) {
		fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
			progName, optstate->value);
		exit(1);
	    }
	    SECU_FileToItem(&input, inFile);
	    PR_Close(inFile);
	    break;

	case 'c':
	    if (mode != DECODE) {
		fprintf(stderr, 
		        "%s: option -c only supported with option -D.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    if ((decodeOptions.contentFile = 
	          PR_Open(optstate->value, PR_RDONLY, 006600)) == NULL) {
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
	    str = optstate->value;
	    do {
		tok = strchr(str, ',');
		if (tok) *tok = '\0';
		envelopeOptions.recipients[nrecipients++] = strdup(str);
		if (tok) str = tok + 1;
	    } while (tok);
	    envelopeOptions.recipients[nrecipients] = NULL;
	    encryptOptions.recipients = envelopeOptions.recipients;
	    certsonlyOptions.recipients = envelopeOptions.recipients;
	    break;

	case 'd':
	    SECU_ConfigDirectory(optstate->value);
	    break;

	case 'e':
	    envFileName = strdup(optstate->value);
	    encryptOptions.envFile = PR_Open(envFileName, PR_RDONLY, 00660);
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
    rv = NSS_InitReadWrite(SECU_ConfigDirectory(NULL));
    if (SECSuccess != rv) {
	SECU_PrintError(progName, "NSS_Init failed");
	exit(1);
    }
    options.certHandle = CERT_GetDefaultCertDB();
    if (!options.certHandle) {
	SECU_PrintError(progName, "No default cert DB");
	exit(1);
    }

    exitstatus = 0;
    switch (mode) {
    case DECODE:
	decodeOptions.options = &options;
	if (encryptOptions.envFile) {
	    /* Decoding encrypted-data, so get the bulkkey from an
	     * enveloped-data message.
	     */
	    SECU_FileToItem(&envmsg, encryptOptions.envFile);
	    decodeOptions.options = &options;
	    encryptOptions.envmsg = decode(NULL, &dummy, &envmsg, 
	                                   decodeOptions);
	    rv = get_enc_params(&encryptOptions);
	    decodeOptions.dkcb = dkcb;
	    decodeOptions.bulkkey = encryptOptions.bulkkey;
	}
	cmsg = decode(outFile, &output, &input, decodeOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem decoding");
	    exitstatus = 1;
	}
	fwrite(output.data, output.len, 1, outFile);
	break;
    case SIGN:
	signOptions.options = &options;
	cmsg = signed_data(signOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem signing");
	    exitstatus = 1;
	}
	break;
    case ENCRYPT:
	if (!envFileName) {
	    fprintf(stderr, "%s: you must specify an envelope file with -e.\n",
	            progName);
	    exit(1);
	}
	encryptOptions.options = &options;
	encryptOptions.input = &input;
	encryptOptions.outfile = outFile;
	if (!encryptOptions.envFile) {
	    encryptOptions.envFile = PR_Open(envFileName, 
	                                     PR_WRONLY|PR_CREATE_FILE, 00660);
	    if (!encryptOptions.envFile) {
		fprintf(stderr, "%s: failed to create file %s.\n", progName,
		        envFileName);
		exit(1);
	    }
	} else {
	    SECU_FileToItem(&envmsg, encryptOptions.envFile);
	    decodeOptions.options = &options;
	    encryptOptions.envmsg = decode(NULL, &dummy, &envmsg, 
	                                   decodeOptions);
	}
	/* decode an enveloped-data message to get the bulkkey (create
	 * a new one if neccessary)
	 */
	rv = get_enc_params(&encryptOptions);
	/* create the encrypted-data message */
	cmsg = encrypted_data(encryptOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem encrypting");
	    exitstatus = 1;
	}
	break;
    case ENVELOPE:
	envelopeOptions.options = &options;
#if 0
	if (!envelopeOptions.recipients)
	    parse_message_for_recipients(myIn, &envelopeOptions);
#endif
	cmsg = enveloped_data(envelopeOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem enveloping");
	    exitstatus = 1;
	}
	break;
    case CERTSONLY:
	certsonlyOptions.options = &options;
	cmsg = signed_data_certsonly(certsonlyOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem with certs-only");
	    exitstatus = 1;
	}
	break;
    default:
	fprintf(stderr, "One of options -D, -S or -E must be set.\n");
	Usage(progName);
	exitstatus = 1;
    }
    if (mode == SIGN || mode == ENVELOPE || mode == CERTSONLY) {
	PLArenaPool *arena = PORT_NewArena(1024);
	NSSCMSEncoderContext *ecx;
	SECItem output = { 0, 0, 0 };
	if (!arena) {
	    fprintf(stderr, "%s: out of memory.\n", progName);
	    exit(1);
	}
	pwcb     = (options.password != NULL) ? ownpw                    : NULL;
	pwcb_arg = (options.password != NULL) ? (void *)options.password : NULL;
	ecx = NSS_CMSEncoder_Start(cmsg, 
                                   NULL, NULL,     /* DER output callback  */
                                   &output, arena, /* destination storage  */
                                   pwcb, pwcb_arg, /* password callback    */
                                   NULL, NULL,     /* decrypt key callback */
                                   NULL, NULL );   /* detached digests    */
	if (!ecx) {
	    fprintf(stderr, "%s: cannot create encoder context.\n", progName);
	    exit(1);
	}
	if (input.len > 0) { /* skip if certs-only (or other zero content) */
	    rv = NSS_CMSEncoder_Update(ecx, input.data, input.len);
	    if (rv) {
		fprintf(stderr, 
		        "%s: failed to add data to encoder.\n", progName);
		exit(1);
	    }
	}
	rv = NSS_CMSEncoder_Finish(ecx);
	if (rv) {
	    fprintf(stderr, "%s: failed to encode data.\n", progName);
	    exit(1);
	}
	/*PR_Write(output.data, output.len);*/
	fwrite(output.data, output.len, 1, outFile);
	PORT_FreeArena(arena, PR_FALSE);
    }
    if (cmsg)
	NSS_CMSMessage_Destroy(cmsg);
    if (outFile != stdout)
	fclose(outFile);

    if (decodeOptions.contentFile)
	PR_Close(decodeOptions.contentFile);
    exit(exitstatus);
}
