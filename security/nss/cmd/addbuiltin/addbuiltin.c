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
 * $Id: addbuiltin.c,v 1.8 2004/05/17 20:08:34 ian.mcgreer%sun.com Exp $
 */

#include "nss.h"
#include "cert.h"
#include "certdb.h"
#include "secutil.h"
#include "pk11func.h"

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
		return "CKT_NETSCAPE_TRUSTED_DELEGATOR|CKT_NETSCAPE_TRUSTED";
	} else {
		return "CKT_NETSCAPE_TRUSTED";
	}
    } else {
	if (trust & CERTDB_TRUSTED_CA) {
		return "CKT_NETSCAPE_TRUSTED_DELEGATOR";
	} else {
		return "CKT_NETSCAPE_VALID";
	}
    }
    return "CKT_NETSCAPE_VALID"; /* not reached */
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
    printf("CKA_CLASS CK_OBJECT_CLASS CKO_NETSCAPE_TRUST\n");
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
    printf("CKA_TRUST_CLIENT_AUTH CK_TRUST CKT_NETSCAPE_TRUSTED\n");*/
    printf("CKA_TRUST_DIGITAL_SIGNATURE CK_TRUST CKT_NETSCAPE_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_NON_REPUDIATION CK_TRUST CKT_NETSCAPE_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_KEY_ENCIPHERMENT CK_TRUST CKT_NETSCAPE_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_DATA_ENCIPHERMENT CK_TRUST CKT_NETSCAPE_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_KEY_AGREEMENT CK_TRUST CKT_NETSCAPE_TRUSTED_DELEGATOR\n");
    printf("CKA_TRUST_KEY_CERT_SIGN CK_TRUST CKT_NETSCAPE_TRUSTED_DELEGATOR\n");
#endif
    printf("CKA_TRUST_STEP_UP_APPROVED CK_BBOOL %s\n",
                trust->sslFlags & CERTDB_GOVT_APPROVED_CA ? 
                "CK_TRUE" : "CK_FALSE");


    PORT_Free(sdder->data);
    return(rv);

}

printheader() {
    printf("# \n"
     "# The contents of this file are subject to the Mozilla Public\n"
     "# License Version 1.1 (the \"License\"); you may not use this file\n"
     "# except in compliance with the License. You may obtain a copy of\n"
     "# the License at http://www.mozilla.org/MPL/\n"
     "# \n"
     "# Software distributed under the License is distributed on an \"AS\n"
     "# IS\" basis, WITHOUT WARRANTY OF ANY KIND, either express or\n"
     "# implied. See the License for the specific language governing\n"
     "# rights and limitations under the License.\n"
     "# \n"
     "# The Original Code is the Netscape security libraries.\n"
     "# \n"
     "# The Initial Developer of the Original Code is Netscape\n"
     "# Communications Corporation.  Portions created by Netscape are \n"
     "# Copyright (C) 1994-2000 Netscape Communications Corporation.  All\n"
     "# Rights Reserved.\n"
     "# \n"
     "# Contributor(s):\n"
     "# \n"
     "# Alternatively, the contents of this file may be used under the\n"
     "# terms of the GNU General Public License Version 2 or later (the\n"
     "# \"GPL\"), in which case the provisions of the GPL are applicable \n"
     "# instead of those above.  If you wish to allow use of your \n"
     "# version of this file only under the terms of the GPL and not to\n"
     "# allow others to use your version of this file under the MPL,\n"
     "# indicate your decision by deleting the provisions above and\n"
     "# replace them with the notice and other provisions required by\n"
     "# the GPL.  If you do not delete the provisions above, a recipient\n"
     "# may use your version of this file under either the MPL or the\n"
     "# GPL.\n"
     "#\n"
     "CVS_ID \"@(#) $RCSfile: addbuiltin.c,v $ $Revision: 1.8 $ $Date: 2004/05/17 20:08:34 $ $Name:  $\"\n"
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
     "#  CKA_NETSCAPE_EMAIL       ASCII7                  (unused here)\n"
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
     "CKA_CLASS CK_OBJECT_CLASS CKO_NETSCAPE_BUILTIN_ROOT_LIST\n"
     "CKA_TOKEN CK_BBOOL CK_TRUE\n"
     "CKA_PRIVATE CK_BBOOL CK_FALSE\n"
     "CKA_MODIFIABLE CK_BBOOL CK_FALSE\n"
     "CKA_LABEL UTF8 \"Mozilla Builtin Roots\"\n");
}

static void Usage(char *progName)
{
    fprintf(stderr, "%s -n nickname -t trust\n", progName);
    fprintf(stderr, 
            "read a der-encoded cert from stdin in, and output\n"
            "it to stdout in a format suitable for the builtin root module.\n"
            "example: %s -n MyCA -t \"C,C,C\" < myca.der >> certdata.txt\n"
            "(pipe through atob if the cert is b64-encoded)\n");
    fprintf(stderr, "%15s nickname to assign to builtin cert.\n", 
                    "-n nickname");
    fprintf(stderr, "%15s default trust flags (cCTpPuw).\n",
                    "-t trust");
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

main(int argc, char **argv)
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
	fprintf(stderr, "%s: you must specify both a nickname and trust.\n");
	Usage(progName);
    }

    if (addbuiltin.options[opt_Input].activated) {
	infile = PR_Open(addbuiltin.options[opt_Input].arg, PR_RDONLY, 00660);
	if (!infile) {
	    fprintf(stderr, "%s: failed to open input file.\n");
	    exit(1);
	}
    } else {
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
