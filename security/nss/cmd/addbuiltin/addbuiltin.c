/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Tool for converting builtin CA certs.
 *
 * $Id: addbuiltin.c,v 1.16 2011/04/13 00:10:21 rrelyea%redhat.com Exp $
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

void dumpbytes(unsigned char *buf, int len)
{
    int i;
    for (i=0; i < len; i++) {
	if ((i !=0) && ((i & 0xf) == 0)) {
	    printf("\n");
	}
	printf("\\%03o",buf[i]);
    }
    printf("\n");
}

char *getTrustString(unsigned int trust)
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
    { SEC_ASN1_INTEGER, offsetof(CERTCertificate,serialNumber) },
    { 0 }
};

static SECStatus
ConvertCertificate(SECItem *sdder, char *nickname, CERTCertTrust *trust)
{
    SECStatus rv = SECSuccess;
    CERTCertificate *cert;
    unsigned char sha1_hash[SHA1_LENGTH];
    unsigned char md5_hash[MD5_LENGTH];
    SECItem *serial = NULL;

    cert = CERT_DecodeDERCertificate(sdder, PR_FALSE, nickname);
    if (!cert) {
	return SECFailure;
    }
    serial = SEC_ASN1EncodeItem(NULL,NULL,cert,serialTemplate);
    if (!serial) {
	return SECFailure;
    }

    printf("\n#\n# Certificate \"%s\"\n#\n",nickname);
    printf("CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE\n");
    printf("CKA_TOKEN CK_BBOOL CK_TRUE\n");
    printf("CKA_PRIVATE CK_BBOOL CK_FALSE\n");
    printf("CKA_MODIFIABLE CK_BBOOL CK_FALSE\n");
    printf("CKA_LABEL UTF8 \"%s\"\n",nickname);
    printf("CKA_CERTIFICATE_TYPE CK_CERTIFICATE_TYPE CKC_X_509\n");
    printf("CKA_SUBJECT MULTILINE_OCTAL\n");
    dumpbytes(cert->derSubject.data,cert->derSubject.len);
    printf("END\n");
    printf("CKA_ID UTF8 \"0\"\n");
    printf("CKA_ISSUER MULTILINE_OCTAL\n");
    dumpbytes(cert->derIssuer.data,cert->derIssuer.len);
    printf("END\n");
    printf("CKA_SERIAL_NUMBER MULTILINE_OCTAL\n");
    dumpbytes(serial->data,serial->len);
    printf("END\n");
    printf("CKA_VALUE MULTILINE_OCTAL\n");
    dumpbytes(sdder->data,sdder->len);
    printf("END\n");

    PK11_HashBuf(SEC_OID_SHA1, sha1_hash, sdder->data, sdder->len);
    PK11_HashBuf(SEC_OID_MD5, md5_hash, sdder->data, sdder->len);
    printf("\n# Trust for Certificate \"%s\"\n",nickname);
    printf("CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST\n");
    printf("CKA_TOKEN CK_BBOOL CK_TRUE\n");
    printf("CKA_PRIVATE CK_BBOOL CK_FALSE\n");
    printf("CKA_MODIFIABLE CK_BBOOL CK_FALSE\n");
    printf("CKA_LABEL UTF8 \"%s\"\n",nickname);
    printf("CKA_CERT_SHA1_HASH MULTILINE_OCTAL\n");
    dumpbytes(sha1_hash,SHA1_LENGTH);
    printf("END\n");
    printf("CKA_CERT_MD5_HASH MULTILINE_OCTAL\n");
    dumpbytes(md5_hash,MD5_LENGTH);
    printf("END\n");

    printf("CKA_ISSUER MULTILINE_OCTAL\n");
    dumpbytes(cert->derIssuer.data,cert->derIssuer.len);
    printf("END\n");
    printf("CKA_SERIAL_NUMBER MULTILINE_OCTAL\n");
    dumpbytes(serial->data,serial->len);
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
    printf("CKA_TRUST_STEP_UP_APPROVED CK_BBOOL %s\n",
                trust->sslFlags & CERTDB_GOVT_APPROVED_CA ? 
                "CK_TRUE" : "CK_FALSE");


    PORT_Free(sdder->data);
    return(rv);

}

void printheader() {
    printf("# \n"
"# ***** BEGIN LICENSE BLOCK *****\n"
"# Version: MPL 1.1/GPL 2.0/LGPL 2.1\n"
"#\n"
"# The contents of this file are subject to the Mozilla Public License Version\n"
"# 1.1 (the \"License\"); you may not use this file except in compliance with\n"
"# the License. You may obtain a copy of the License at\n"
"# http://www.mozilla.org/MPL/\n"
"#\n"
"# Software distributed under the License is distributed on an \"AS IS\" basis,\n"
"# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License\n"
"# for the specific language governing rights and limitations under the\n"
"# License.\n"
"#\n"
"# The Original Code is the Netscape security libraries..\n"
"#\n"
"# The Initial Developer of the Original Code is\n"
"# Netscape Communications Corporation.\n"
"# Portions created by the Initial Developer are Copyright (C) 1994-2000\n"
"# the Initial Developer. All Rights Reserved.\n"
"#\n"
"# Contributor(s):\n"
"#\n"
"# Alternatively, the contents of this file may be used under the terms of\n"
"# either the GNU General Public License Version 2 or later (the \"GPL\"), or\n"
"# the GNU Lesser General Public License Version 2.1 or later (the \"LGPL\"),\n"
"# in which case the provisions of the GPL or the LGPL are applicable instead\n"
"# of those above. If you wish to allow use of your version of this file only\n"
"# under the terms of either the GPL or the LGPL, and not to allow others to\n"
"# use your version of this file under the terms of the MPL, indicate your\n"
"# decision by deleting the provisions above and replace them with the notice\n"
"# and other provisions required by the GPL or the LGPL. If you do not delete\n"
"# the provisions above, a recipient may use your version of this file under\n"
"# the terms of any one of the MPL, the GPL or the LGPL.\n"
"#\n"
"# ***** END LICENSE BLOCK *****\n"
     "#\n"
     "CVS_ID \"@(#) $RCSfile: addbuiltin.c,v $ $Revision: 1.16 $ $Date: 2011/04/13 00:10:21 $\"\n"
     "\n"
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
     "#  -- Attribute --          -- type --              -- value --\n"
     "#  CKA_CLASS                CK_OBJECT_CLASS         CKO_CERTIFICATE\n"
     "#  CKA_TOKEN                CK_BBOOL                CK_TRUE\n"
     "#  CKA_PRIVATE              CK_BBOOL                CK_FALSE\n"
     "#  CKA_MODIFIABLE           CK_BBOOL                CK_FALSE\n"
     "#  CKA_LABEL                UTF8                    (varies)\n"
     "#  CKA_CERTIFICATE_TYPE     CK_CERTIFICATE_TYPE     CKC_X_509\n"
     "#  CKA_SUBJECT              DER+base64              (varies)\n"
     "#  CKA_ID                   byte array              (varies)\n"
     "#  CKA_ISSUER               DER+base64              (varies)\n"
     "#  CKA_SERIAL_NUMBER        DER+base64              (varies)\n"
     "#  CKA_VALUE                DER+base64              (varies)\n"
     "#  CKA_NSS_EMAIL            ASCII7                  (unused here)\n"
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

static void Usage(char *progName)
{
    fprintf(stderr, "%s -n nickname -t trust [-i certfile]\n", progName);
    fprintf(stderr, 
            "\tRead a der-encoded cert from certfile or stdin, and output\n"
            "\tit to stdout in a format suitable for the builtin root module.\n"
            "\tExample: %s -n MyCA -t \"C,C,C\" -i myca.der >> certdata.txt\n"
            "\t(pipe through atob if the cert is b64-encoded)\n", progName);
    fprintf(stderr, "%-15s nickname to assign to builtin cert.\n", 
                    "-n nickname");
    fprintf(stderr, "%-15s trust flags (cCTpPuw).\n", "-t trust");
    fprintf(stderr, "%-15s file to read (default stdin)\n", "-i certfile");
    exit(-1);
}

enum {
    opt_Input = 0,
    opt_Nickname,
    opt_Trust
};

static secuCommandFlag addbuiltin_options[] =
{
	{ /* opt_Input         */  'i', PR_TRUE, 0, PR_FALSE },
	{ /* opt_Nickname      */  'n', PR_TRUE, 0, PR_FALSE },
	{ /* opt_Trust         */  't', PR_TRUE, 0, PR_FALSE }
};

int main(int argc, char **argv)
{
    SECStatus rv;
    char *nickname;
    char *trusts;
    char *progName;
    PRFileDesc *infile;
    CERTCertTrust trust = { 0 };
    SECItem derCert = { 0 };

    secuCommand addbuiltin = { 0 };
    addbuiltin.numOptions = sizeof(addbuiltin_options)/sizeof(secuCommandFlag);
    addbuiltin.options = addbuiltin_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &addbuiltin);

    if (rv != SECSuccess)
	Usage(progName);

    if (!addbuiltin.options[opt_Nickname].activated &&
        !addbuiltin.options[opt_Trust].activated) {
	fprintf(stderr, "%s: you must specify both a nickname and trust.\n",
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

    nickname = strdup(addbuiltin.options[opt_Nickname].arg);
    trusts = strdup(addbuiltin.options[opt_Trust].arg);

    NSS_NoDB_Init(NULL);

    rv = CERT_DecodeTrustString(&trust, trusts);
    if (rv) {
	fprintf(stderr, "%s: incorrectly formatted trust string.\n", progName);
	Usage(progName);
    }

    SECU_FileToItem(&derCert, infile);
    
    /*printheader();*/

    rv = ConvertCertificate(&derCert, nickname, &trust);
    if (rv) {
	fprintf(stderr, "%s: failed to convert certificate.\n", progName);
	exit(1);
    }
    
    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }

    return(SECSuccess);
}
