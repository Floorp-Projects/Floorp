/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  JARSIGN
 *
 *  Routines used in signing archives.
 */


#include "jar.h"
#include "jarint.h"
#include "secpkcs7.h"
#include "pk11func.h"
#include "sechash.h"

/* from libevent.h */
typedef void (*ETVoidPtrFunc) (void * data);

/* key database wrapper */
/* static SECKEYKeyDBHandle *jar_open_key_database (void); */
/* CHUNQ is our bite size */

#define CHUNQ 64000
#define FILECHUNQ 32768

/*
 *  J A R _ c a l c u l a t e _ d i g e s t
 *
 *  Quick calculation of a digest for
 *  the specified block of memory. Will calculate
 *  for all supported algorithms, now MD5.
 *
 *  This version supports huge pointers for WIN16.
 *
 */
JAR_Digest * PR_CALLBACK 
JAR_calculate_digest(void *data, long length)
{
    PK11Context *md5  = 0;
    PK11Context *sha1 = 0;
    JAR_Digest *dig   = PORT_ZNew(JAR_Digest);
    long chunq;
    unsigned int md5_length, sha1_length;

    if (dig == NULL) {
	/* out of memory allocating digest */
	return NULL;
    }

    md5  = PK11_CreateDigestContext(SEC_OID_MD5);
    sha1 = PK11_CreateDigestContext(SEC_OID_SHA1);

    if (length >= 0) {
	PK11_DigestBegin (md5);
	PK11_DigestBegin (sha1);

	do {
	    chunq = length;

	    PK11_DigestOp(md5,  (unsigned char*)data, chunq);
	    PK11_DigestOp(sha1, (unsigned char*)data, chunq);
	    length -= chunq;
	    data = ((char *) data + chunq);
	}
	while (length > 0);

	PK11_DigestFinal (md5,	dig->md5,  &md5_length,  MD5_LENGTH);
	PK11_DigestFinal (sha1, dig->sha1, &sha1_length, SHA1_LENGTH);

	PK11_DestroyContext (md5,  PR_TRUE);
	PK11_DestroyContext (sha1, PR_TRUE);
    }
    return dig;
}

/*
 *  J A R _ d i g e s t _ f i l e
 *
 *  Calculates the MD5 and SHA1 digests for a file
 *  present on disk, and returns these in JAR_Digest struct.
 *
 */
int 
JAR_digest_file (char *filename, JAR_Digest *dig)
{
    JAR_FILE fp;
    PK11Context *md5 = 0;
    PK11Context *sha1 = 0;
    unsigned char *buf = (unsigned char *) PORT_ZAlloc (FILECHUNQ);
    int num;
    unsigned int md5_length, sha1_length;

    if (buf == NULL) {
	/* out of memory */
	return JAR_ERR_MEMORY;
    }

    if ((fp = JAR_FOPEN (filename, "rb")) == 0) {
	/* perror (filename); FIX XXX XXX XXX XXX XXX XXX */
	PORT_Free (buf);
	return JAR_ERR_FNF;
    }

    md5 = PK11_CreateDigestContext (SEC_OID_MD5);
    sha1 = PK11_CreateDigestContext (SEC_OID_SHA1);

    if (md5 == NULL || sha1 == NULL) {
	/* can't generate digest contexts */
	PORT_Free (buf);
	JAR_FCLOSE (fp);
	return JAR_ERR_GENERAL;
    }

    PK11_DigestBegin (md5);
    PK11_DigestBegin (sha1);

    while (1) {
	if ((num = JAR_FREAD (fp, buf, FILECHUNQ)) == 0)
	    break;

	PK11_DigestOp (md5, buf, num);
	PK11_DigestOp (sha1, buf, num);
    }

    PK11_DigestFinal (md5, dig->md5, &md5_length, MD5_LENGTH);
    PK11_DigestFinal (sha1, dig->sha1, &sha1_length, SHA1_LENGTH);

    PK11_DestroyContext (md5, PR_TRUE);
    PK11_DestroyContext (sha1, PR_TRUE);

    PORT_Free (buf);
    JAR_FCLOSE (fp);

    return 0;
}

/*
 *  J A R _ o p e n _ k e y _ d a t a b a s e
 *
 */

void* 
jar_open_key_database(void)
{
    return NULL;
}

int 
jar_close_key_database(void *keydb)
{
    /* We never do close it */
    return 0;
}


/*
 *  j a r _ c r e a t e _ p k 7
 *
 */

static void jar_pk7_out (void *arg, const char *buf, unsigned long len)
{
    JAR_FWRITE ((JAR_FILE) arg, buf, len);
}

int 
jar_create_pk7(CERTCertDBHandle *certdb, void *keydb, CERTCertificate *cert, 
               char *password, JAR_FILE infp, JAR_FILE outfp)
{
    SEC_PKCS7ContentInfo *cinfo;
    const SECHashObject *hashObj;
    char *errstring;
    void *mw = NULL;
    void *hashcx;
    unsigned int len;
    int status = 0;
    SECStatus rv;
    SECItem digest;
    unsigned char digestdata[32];
    unsigned char buffer[4096];

    if (outfp == NULL || infp == NULL || cert == NULL)
	return JAR_ERR_GENERAL;

    /* we sign with SHA */
    hashObj = HASH_GetHashObject(HASH_AlgSHA1);

    hashcx = (* hashObj->create)();
    if (hashcx == NULL)
	return JAR_ERR_GENERAL;

    (* hashObj->begin)(hashcx);
    while (1) {
	int nb = JAR_FREAD(infp, buffer, sizeof buffer);
	if (nb == 0) { /* eof */
	    break;
	}
	(* hashObj->update) (hashcx, buffer, nb);
    }
    (* hashObj->end)(hashcx, digestdata, &len, 32);
    (* hashObj->destroy)(hashcx, PR_TRUE);

    digest.data = digestdata;
    digest.len = len;

    /* signtool must use any old context it can find since it's
	 calling from inside javaland. */
    PORT_SetError (0);
    cinfo = SEC_PKCS7CreateSignedData(cert, certUsageObjectSigner, NULL,
                                      SEC_OID_SHA1, &digest, NULL, mw);
    if (cinfo == NULL)
	return JAR_ERR_PK7;

    rv = SEC_PKCS7IncludeCertChain(cinfo, NULL);
    if (rv != SECSuccess) {
	status = PORT_GetError();
	SEC_PKCS7DestroyContentInfo(cinfo);
	return status;
    }

    /* Having this here forces signtool to always include signing time. */
    rv = SEC_PKCS7AddSigningTime(cinfo);
    /* don't check error */
    PORT_SetError(0);

    /* if calling from mozilla thread*/
    rv = SEC_PKCS7Encode(cinfo, jar_pk7_out, outfp, NULL, NULL, mw);
    if (rv != SECSuccess)
	status = PORT_GetError();
    SEC_PKCS7DestroyContentInfo (cinfo);
    if (rv != SECSuccess) {
	errstring = JAR_get_error (status);
	return ((status < 0) ? status : JAR_ERR_GENERAL);
    }
    return 0;
}
