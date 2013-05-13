/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secutil.h"
#include "plgetopt.h"
#include "cert.h"
#include "secoid.h"
#include "cryptohi.h"

/* maximum supported modulus length in bits (indicate problem if over this) */
#define MAX_MODULUS (1024)


static void Usage(char *progName)
{
    fprintf(stderr, "Usage: %s [aAvf] [certtocheck] [issuingcert]\n",
	    progName);
    fprintf(stderr, "%-20s Cert to check is base64 encoded\n",
	    "-a");
    fprintf(stderr, "%-20s Issuer's cert is base64 encoded\n",
	    "-A");
    fprintf(stderr, "%-20s Verbose (indicate decoding progress etc.)\n",
	    "-v");
    fprintf(stderr, "%-20s Force sanity checks even if pretty print fails.\n",
	    "-f");
    fprintf(stderr, "%-20s Define an output file to use (default is stdout)\n",
	    "-o output");
    fprintf(stderr, "%-20s Specify the input type (no default)\n",
	    "-t type");
    exit(-1);
}


/*
 * Check integer field named fieldName, printing out results and
 * returning the length of the integer in bits
 */   

static
int checkInteger(SECItem *intItem, char *fieldName, int verbose) 
{
    int len, bitlen;
    if (verbose) {
	printf("Checking %s\n", fieldName);
    }

    len = intItem->len;

    if (len && (intItem->data[0] & 0x80)) {
	printf("PROBLEM: %s is NEGATIVE 2's-complement integer.\n",
	       fieldName);
    }


    /* calculate bit length and check for unnecessary leading zeros */
    bitlen = len << 3;
    if (len > 1 && intItem->data[0] == 0) {
	/* leading zero byte(s) */
	if (!(intItem->data[1] & 0x80)) {
	    printf("PROBLEM: %s has unneeded leading zeros.  Violates DER.\n",
		   fieldName);
	}
	/* strip leading zeros in length calculation */
	{
	    int i=0;
	    while (bitlen > 8 && intItem->data[i] == 0) {
		bitlen -= 8;
		i++;
	    }
	}
    }
    return bitlen;
}




static
void checkName(CERTName *n, char *fieldName, int verbose)
{
    char *v=0;
    if (verbose) {
	printf("Checking %s\n", fieldName);
    }

    v = CERT_GetCountryName(n);
    if (!v) {
	printf("PROBLEM: %s lacks Country Name (C)\n",
	       fieldName);
    }
    PORT_Free(v);

    v = CERT_GetOrgName(n);
    if (!v) {
	printf("PROBLEM: %s lacks Organization Name (O)\n",
	       fieldName);
    }
    PORT_Free(v);

    v = CERT_GetOrgUnitName(n);
    if (!v) {
	printf("WARNING: %s lacks Organization Unit Name (OU)\n",
	       fieldName);
    }
    PORT_Free(v);	

    v = CERT_GetCommonName(n);
    if (!v) {
	printf("PROBLEM: %s lacks Common Name (CN)\n",
	       fieldName);
    }
    PORT_Free(v);
}


static
SECStatus
OurVerifyData(unsigned char *buf, int len, SECKEYPublicKey *key,
	      SECItem *sig, SECAlgorithmID *sigAlgorithm)
{
    SECStatus rv;
    VFYContext *cx;
    SECOidData *sigAlgOid, *oiddata;
    SECOidTag sigAlgTag;
    SECOidTag hashAlgTag;
    int showDigestOid=0;

    cx = VFY_CreateContextWithAlgorithmID(key, sig, sigAlgorithm, &hashAlgTag, 
                                          NULL);
    if (cx == NULL)
	return SECFailure;

    sigAlgOid = SECOID_FindOID(&sigAlgorithm->algorithm);
    if (sigAlgOid == 0)
	return SECFailure;
    sigAlgTag = sigAlgOid->offset;


    if (showDigestOid) {
	oiddata = SECOID_FindOIDByTag(hashAlgTag);
	if ( oiddata ) {
	    printf("PROBLEM: (cont) Digest OID is %s\n", oiddata->desc);
	} else {
	    SECU_PrintAsHex(stdout,
			    &oiddata->oid, "PROBLEM: UNKNOWN OID", 0);
	}
    }

    rv = VFY_Begin(cx);
    if (rv == SECSuccess) {
	rv = VFY_Update(cx, buf, len);
	if (rv == SECSuccess)
	    rv = VFY_End(cx);
    }

    VFY_DestroyContext(cx, PR_TRUE);
    return rv;
}



static
SECStatus
OurVerifySignedData(CERTSignedData *sd, CERTCertificate *cert)
{
    SECItem sig;
    SECKEYPublicKey *pubKey = 0;
    SECStatus rv;

    /* check the certificate's validity */
    rv = CERT_CertTimesValid(cert);
    if ( rv ) {
	return(SECFailure);
    }

    /* get cert's public key */
    pubKey = CERT_ExtractPublicKey(cert);
    if ( !pubKey ) {
	return(SECFailure);
    }

    /* check the signature */
    sig = sd->signature;
    DER_ConvertBitString(&sig);
    rv = OurVerifyData(sd->data.data, sd->data.len, pubKey, &sig,
		       &sd->signatureAlgorithm);

    SECKEY_DestroyPublicKey(pubKey);

    if ( rv ) {
	return(SECFailure);
    }

    return(SECSuccess);
}




static
CERTCertificate *createEmptyCertificate(void)
{
    PLArenaPool *arena = 0;
    CERTCertificate *c = 0;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
	return 0;
    }


    c = (CERTCertificate *) PORT_ArenaZAlloc(arena, sizeof(CERTCertificate));

    if (c) {
	c->referenceCount = 1;
	c->arena = arena;
    } else {
	PORT_FreeArena(arena,PR_TRUE);
    }

    return c;
}    




int main(int argc, char **argv)
{
    int rv, verbose=0, force=0;
    int ascii=0, issuerAscii=0;
    char *progName=0;
    PRFileDesc *inFile=0, *issuerCertFile=0;
    SECItem derCert, derIssuerCert;
    PLArenaPool *arena=0;
    CERTSignedData *signedData=0;
    CERTCertificate *cert=0, *issuerCert=0;
    SECKEYPublicKey *rsapubkey=0;
    SECAlgorithmID md5WithRSAEncryption, md2WithRSAEncryption;
    SECAlgorithmID sha1WithRSAEncryption, rsaEncryption;
    SECItem spk;
    int selfSigned=0;
    int invalid=0;
    char *inFileName = NULL, *issuerCertFileName = NULL;
    PLOptState *optstate;
    PLOptStatus status;

    PORT_Memset(&md5WithRSAEncryption, 0, sizeof(md5WithRSAEncryption));
    PORT_Memset(&md2WithRSAEncryption, 0, sizeof(md2WithRSAEncryption));
    PORT_Memset(&sha1WithRSAEncryption, 0, sizeof(sha1WithRSAEncryption));
    PORT_Memset(&rsaEncryption, 0, sizeof(rsaEncryption));

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    optstate = PL_CreateOptState(argc, argv, "aAvf");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	switch (optstate->option) {
	  case 'v':
	    verbose = 1;
	    break;

	  case 'f':
	    force = 1;
	    break;

	  case 'a':
	    ascii = 1;
	    break;

	  case 'A':
	    issuerAscii = 1;
	    break;

	  case '\0':
	    if (!inFileName)
		inFileName = PL_strdup(optstate->value);
	    else if (!issuerCertFileName)
		issuerCertFileName = PL_strdup(optstate->value);
	    else
		Usage(progName);
	    break;
	}
    }

    if (!inFileName || !issuerCertFileName || status == PL_OPT_BAD) {
	/* insufficient or excess args */
	Usage(progName);
    }

    inFile = PR_Open(inFileName, PR_RDONLY, 0);
    if (!inFile) {
	fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
	                 progName, inFileName);
	exit(1);
    }

    issuerCertFile = PR_Open(issuerCertFileName, PR_RDONLY, 0);
    if (!issuerCertFile) {
	fprintf(stderr, "%s: unable to open \"%s\" for reading\n",
	                 progName, issuerCertFileName);
	exit(1);
    }

    if (SECU_ReadDERFromFile(&derCert, inFile, ascii) != SECSuccess) {
	printf("Couldn't read input certificate as DER binary or base64\n");
	exit(1);
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == 0) {
	fprintf(stderr,"%s: can't allocate scratch arena!", progName);
	exit(1);
    }

    if (issuerCertFile) {
	CERTSignedData *issuerCertSD=0;
	if (SECU_ReadDERFromFile(&derIssuerCert, issuerCertFile, issuerAscii)
	    != SECSuccess) {
	    printf("Couldn't read issuer certificate as DER binary or base64.\n");
	    exit(1);
	}
	issuerCertSD = PORT_ArenaZNew(arena, CERTSignedData);
	if (!issuerCertSD) {
	    fprintf(stderr,"%s: can't allocate issuer signed data!", progName);
	    exit(1);
	}
	rv = SEC_ASN1DecodeItem(arena, issuerCertSD, 
	                        SEC_ASN1_GET(CERT_SignedDataTemplate),
				&derIssuerCert);
	if (rv) {
	    fprintf(stderr, "%s: Issuer cert isn't X509 SIGNED Data?\n",
		    progName);
	    exit(1);
	}
	issuerCert = createEmptyCertificate();
	if (!issuerCert) {
	    printf("%s: can't allocate space for issuer cert.", progName);
	    exit(1);
	}
	rv = SEC_ASN1DecodeItem(arena, issuerCert, 
	                    SEC_ASN1_GET(CERT_CertificateTemplate),
			    &issuerCertSD->data);
	if (rv) {
	    printf("%s: Does not appear to be an X509 Certificate.\n",
		   progName);
	    exit(1);
	}
    }

    signedData =  PORT_ArenaZNew(arena,CERTSignedData);
    if (!signedData) {
	fprintf(stderr,"%s: can't allocate signedData!", progName);
	exit(1);
    }

    rv = SEC_ASN1DecodeItem(arena, signedData, 
                            SEC_ASN1_GET(CERT_SignedDataTemplate), 
			    &derCert);
    if (rv) {
	fprintf(stderr, "%s: Does not appear to be X509 SIGNED Data.\n",
		progName);
	exit(1);
    }

    if (verbose) {
	printf("Decoded ok as X509 SIGNED data.\n");
    }

    cert = createEmptyCertificate();
    if (!cert) {
	fprintf(stderr, "%s: can't allocate cert", progName);
	exit(1);
    }

    rv = SEC_ASN1DecodeItem(arena, cert, 
                        SEC_ASN1_GET(CERT_CertificateTemplate), 
			&signedData->data);
    if (rv) {
	fprintf(stderr, "%s: Does not appear to be an X509 Certificate.\n",
		progName);
	exit(1);
    }


    if (verbose) {
	printf("Decoded ok as an X509 certificate.\n");
    }

    SECU_RegisterDynamicOids();
    rv = SECU_PrintSignedData(stdout, &derCert, "Certificate", 0,
			      SECU_PrintCertificate);

    if (rv) {
	fprintf(stderr, "%s: Unable to pretty print cert. Error: %d\n",
		progName, PORT_GetError());
	if (!force) {
	    exit(1);
	}
    }


    /* Do various checks on the cert */

    printf("\n");

    /* Check algorithms */
    SECOID_SetAlgorithmID(arena, &md5WithRSAEncryption,
		       SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION, NULL);

    SECOID_SetAlgorithmID(arena, &md2WithRSAEncryption,
		       SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION, NULL);

    SECOID_SetAlgorithmID(arena, &sha1WithRSAEncryption,
		       SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION, NULL);

    SECOID_SetAlgorithmID(arena, &rsaEncryption,
		       SEC_OID_PKCS1_RSA_ENCRYPTION, NULL);

    {
	int isMD5RSA = (SECOID_CompareAlgorithmID(&cert->signature,
					       &md5WithRSAEncryption) == 0);
	int isMD2RSA = (SECOID_CompareAlgorithmID(&cert->signature,
					       &md2WithRSAEncryption) == 0);
	int isSHA1RSA = (SECOID_CompareAlgorithmID(&cert->signature,
					       &sha1WithRSAEncryption) == 0);

	if (verbose) {
	    printf("\nDoing algorithm checks.\n");
	}

	if (!(isMD5RSA || isMD2RSA || isSHA1RSA)) {
	    printf("PROBLEM: Signature not PKCS1 MD5, MD2, or SHA1 + RSA.\n");
	} else if (!isMD5RSA) {
	    printf("WARNING: Signature not PKCS1 MD5 with RSA Encryption\n");
	}

	if (SECOID_CompareAlgorithmID(&cert->signature,
				   &signedData->signatureAlgorithm)) {
	    printf("PROBLEM: Algorithm in sig and certInfo don't match.\n");
	}
    }

    if (SECOID_CompareAlgorithmID(&cert->subjectPublicKeyInfo.algorithm,
			       &rsaEncryption)) {
	printf("PROBLEM: Public key algorithm is not PKCS1 RSA Encryption.\n");
    }

    /* Check further public key properties */
    spk = cert->subjectPublicKeyInfo.subjectPublicKey;
    DER_ConvertBitString(&spk);

    if (verbose) {
	printf("\nsubjectPublicKey DER\n");
	rv = DER_PrettyPrint(stdout, &spk, PR_FALSE);
	printf("\n");
    }

    rsapubkey = (SECKEYPublicKey *) 
	             PORT_ArenaZAlloc(arena,sizeof(SECKEYPublicKey));
    if (!rsapubkey) {
	fprintf(stderr, "%s: rsapubkey allocation failed.\n", progName);
	exit(1);
    }

    rv = SEC_ASN1DecodeItem(arena, rsapubkey, 
                            SEC_ASN1_GET(SECKEY_RSAPublicKeyTemplate), &spk);
    if (rv) {
	printf("PROBLEM: subjectPublicKey is not a DER PKCS1 RSAPublicKey.\n");
    } else {
	int mlen;
	int pubexp;
	if (verbose) {
	    printf("Decoded RSA Public Key ok.  Doing key checks.\n");
	}
	PORT_Assert(rsapubkey->keyType == rsaKey); /* XXX RSA */
	mlen = checkInteger(&rsapubkey->u.rsa.modulus, "Modulus", verbose);
	printf("INFO: Public Key modulus length in bits: %d\n", mlen);
	if (mlen > MAX_MODULUS) {
	    printf("PROBLEM: Modulus length exceeds %d bits.\n",
		   MAX_MODULUS);
	}
	if (mlen < 512) {
	    printf("WARNING: Short modulus.\n");
	}
	if (mlen != (1 << (ffs(mlen)-1))) {
	    printf("WARNING: Unusual modulus length (not a power of two).\n");
	}
	checkInteger(&rsapubkey->u.rsa.publicExponent, "Public Exponent",
                     verbose);
	pubexp = DER_GetInteger(&rsapubkey->u.rsa.publicExponent);
	if (pubexp != 17 && pubexp != 3 && pubexp != 65537) {
	    printf("WARNING: Public exponent not any of: 3, 17, 65537\n");
	}
    }


    /* Name checks */
    checkName(&cert->issuer, "Issuer Name", verbose);
    checkName(&cert->subject, "Subject Name", verbose);

    if (issuerCert) {
	SECComparison c =
	    CERT_CompareName(&cert->issuer, &issuerCert->subject);
	if (c) {
         printf("PROBLEM: Issuer Name and Subject in Issuing Cert differ\n");
        }
    }

    /* Check if self-signed */
    selfSigned = (CERT_CompareName(&cert->issuer, &cert->subject) == 0);
    if (selfSigned) {
	printf("INFO: Certificate is self signed.\n");
    } else {
	printf("INFO: Certificate is NOT self-signed.\n");
    }


    /* Validity time check */
    if (CERT_CertTimesValid(cert) == SECSuccess) {
	printf("INFO: Inside validity period of certificate.\n");
    } else {
	printf("PROBLEM: Not in validity period of certificate.\n");
	invalid = 1;
    }

    /* Signature check if self-signed */
    if (selfSigned && !invalid) {
	if (rsapubkey->u.rsa.modulus.len) {
	    SECStatus ver;
	    if (verbose) {
		printf("Checking self signature.\n");
	    }
	    ver = OurVerifySignedData(signedData, cert);
	    if (ver != SECSuccess) {
		printf("PROBLEM: Verification of self-signature failed!\n");
	    } else {
		printf("INFO: Self-signature verifies ok.\n");
	    }
	} else {
	    printf("INFO: Not checking signature due to key problems.\n");
	}
    } else if (!selfSigned && !invalid && issuerCert) {
	SECStatus ver;
	ver = OurVerifySignedData(signedData, issuerCert);
	if (ver != SECSuccess) {
	    printf("PROBLEM: Verification of issuer's signature failed!\n");
	} else {
	    printf("INFO: Issuer's signature verifies ok.\n");
	}
    } else {
	printf("INFO: Not checking signature.\n");
    }

    return 0;    
}



