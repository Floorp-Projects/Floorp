/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "secnav.h"
#include "libmocha.h"
#include "prlink.h"
#include "prinrval.h"

int
SSL_SetSockPeerID(PRFileDesc *fd, char *peerID)
{
    return(0);
}

void
SECNAV_Posting(PRFileDesc *fd)
{
    return;
}

void
SECNAV_HTTPHead(PRFileDesc *fd)
{
    return;
}

void
SECNAV_RegisterNetlibMimeConverters()
{
    return;
}

char *
SECNAV_MungeString(const char *unmunged_string)
{
    return(NULL);
}

char *
SECNAV_UnMungeString(const char *munged_string)
{
    return(NULL);
}

PRBool
SECNAV_GenKeyFromChoice(void *proto_win, LO_Element *form,
			char *choiceString, char *challenge,
			char *typeString, char *pqgString,
			char **pValue, PRBool *pDone)
{
    return(PR_FALSE);
}


char **
SECNAV_GetKeyChoiceList(char *type, char *pqgString)
{
    return(NULL);
}

PRBool
SECNAV_SecurityDialog(MWContext *context, int state)
{
	return (state == SD_INSECURE_POST_FROM_INSECURE_DOC);
}

JSObject *
lm_DefinePkcs11(MochaDecoder *decoder)
{
    return((JSObject *)1);
}

JSObject *
lm_DefineCrypto(MochaDecoder *decoder)
{
    return((JSObject *)1);
}

void
NET_InitCertLdapProtocol(void)
{
}


CERTCertificate *
CERT_DupCertificate(CERTCertificate *cert)
{
    return(NULL);
}

void
CERT_DestroyCertificate(CERTCertificate *cert)
{
    return;
}

CERTCertificate *
CERT_NewTempCertificate (CERTCertDBHandle *handle, SECItem *derCert,
			 char *nickname, PRBool isperm, PRBool copyDER)
{
    return(NULL);
}

CERTCertDBHandle *
CERT_GetDefaultCertDB(void)
{
    return(NULL);
}

CERTCertificate *
CERT_DecodeCertFromPackage(char *certbuf, int certlen)
{
    return(NULL);
}

char *
CERT_HTMLCertInfo(CERTCertificate *cert, PRBool showImages, PRBool showIssuer)
{
    return(NULL);
}

PRBool
CERT_CompareCertsForRedirection(CERTCertificate *c1, CERTCertificate *c2)
{
    return(PR_FALSE);
}

const char *
SECNAV_GetPolicyNameString(void)
{
    return(NULL);
}

int
SECNAV_InitConfigObject(void)
{
    return(0);
}

int
SECNAV_RunInitialSecConfig(void)
{
    return(0);
}

void
SECNAV_EarlyInit(void)
{
    return;
}

void
SECNAV_Init(void)
{
    return;
}

void
SECNAV_Shutdown(void)
{
    return;
}

void
SECNAV_SecurityAdvisor(void *proto_win, URL_Struct *url)
{
    return;
}

char *
SECNAV_MakeCertButtonString(CERTCertificate *cert)
{
    return(NULL);
}

int
SECNAV_SecURLData(char *which, NET_StreamClass *stream, MWContext *cx)
{
    return(-1);
}

char *
SECNAV_SecURLContentType(char *which)
{
    return(NULL);
}

int
SECNAV_SecHandleSecurityAdvisorURL(MWContext *cx, const char *which)
{
    return(-1);
}

void
SECNAV_HandleInternalSecURL(URL_Struct *url, MWContext *cx)
{
    return;
}

PRBool
SSL_IsDomestic(void)
{
    return(PR_FALSE);
}

SECStatus
SECNAV_ComputeFortezzaProxyChallengeResponse(MWContext *context,
					     char *asciiChallenge,
					     char **signature_out,
					     char **clientRan_out,
					     char **certChain_out)
{
    return(SECFailure);
}


char *
SECNAV_PrettySecurityStatus(int level, unsigned char *status)
{
    return(NULL);
}

char *
SECNAV_SecurityVersion(PRBool longForm)
{
    return("N");
}

char *
SECNAV_SSLCapabilities(void)
{
    return(NULL);
}

unsigned char *
SECNAV_SSLSocketStatus(PRFileDesc *fd, int *return_security_level)
{
    return(NULL);
}

unsigned char *
SECNAV_CopySSLSocketStatus(unsigned char *status)
{
    return(NULL);
}


unsigned int
SECNAV_SSLSocketStatusLength(unsigned char *status)
{
    return(0);
}

char *
SECNAV_SSLSocketCertString(unsigned char *status)
{
    return(NULL);
}

PRBool
SECNAV_CompareCertsForRedirection(unsigned char *status1,
				  unsigned char *status2)
{
    return(PR_FALSE);
}

CERTCertificate *
SECNAV_CertFromSSLSocketStatus(unsigned char *status)
{
    return(NULL);
}

SECStatus
RNG_RNGInit(void)
{
    return(SECSuccess);
}

/*
 * probably need to provide something here
 */
SECStatus
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
	/* This is a temporary implementation to avoid */
	/* the cache filename horkage. This needs to have a more */
	/* secure/free implementation here - Gagan */

	char* output=dest;
	size_t i;
	srand((unsigned int) PR_IntervalToMilliseconds(PR_IntervalNow()));
	for (i=0;i<len; i++)
	{
		int t = rand();
		*output = (char) (t % 256);
		output++;
	}
    return(SECSuccess);
}

/*
** Get the "noisiest" information available on the system.
*/
size_t
RNG_GetNoise(void *buf, size_t maxbytes)
{
    return(0);
}

/*
** RNG_SystemInfoForRNG should be called before any use of SSL. It
** gathers up the system specific information to help seed the
** state of the global random number generator.
*/
void
RNG_SystemInfoForRNG(void)
{
    return;
}

/* 
** Use the contents (and stat) of a file to help seed the
** global random number generator.
*/
void RNG_FileForRNG(char *filename)
{
    return;
}

/*
** Update the global random number generator with more seeding
** material
*/
SECStatus
RNG_RandomUpdate(void *data, size_t bytes)
{
    return(SECFailure);
}

PRStaticLinkTable jsl_nodl_tab[1];

void
_java_jsl_init(void)
{
}

DIGESTS *PR_CALLBACK
SOB_calculate_digest(void XP_HUGE *data, long length)
{
    return(NULL);
}


int PR_CALLBACK
SOB_verify_digest(ZIG *siglist, const char *name, DIGESTS *dig)
{
    return(-1);
}

void PR_CALLBACK
SOB_destroy (ZIG *zig)
{
    return;
}

char *
SOB_get_error (int status)
{
    return(NULL);
}

ZIG_Context *
SOB_find(ZIG *zig, char *pattern, int type)
{
    return(NULL);
}

int
SOB_find_next(ZIG_Context *ctx, SOBITEM **it)
{
    return(-1);
}

void
SOB_find_end(ZIG_Context *ctx)
{
    return;
}

char *
SOB_get_url (ZIG *zig)
{
    return(NULL);
}

ZIG *
SOB_new (void)
{
    return(NULL);
}

int
SOB_set_callback (int type, ZIG *zig,
		  int (*fn) (int status, ZIG *zig, 
			     const char *metafile,
			     char *pathname, char *errortext))
{
    return(-1);
}

int PR_CALLBACK
SOB_cert_attribute(int attrib, ZIG *zig, long keylen, void *key, 
		   void **result, unsigned long *length)
{
    return(-1);
}

int PR_CALLBACK
SOB_stash_cert(ZIG *zig, long keylen, void *key)
{
    return(-1);
}

int SOB_parse_manifest(char XP_HUGE *raw_manifest, long length, 
		       const char *path, const char *url, ZIG *zig)
{
    return(-1);
}

void
SECNAV_signedAppletPrivileges(void *proto_win, char *javaPrin, 
			      char *javaTarget, char *risk, int isCert)
{
    return;
}

void
SECNAV_signedAppletPrivilegesOnMozillaThread(void *proto_win, char *javaPrin,
                                             char *javaTarget, char *risk, int isCert)
{
    return;
}

char *
SOB_JAR_list_certs (void)
{
    return(NULL);
}

int
SOB_JAR_validate_archive (char *filename)
{
    return(-1);
}

void *
SOB_JAR_new_hash (int alg)
{
    return(NULL);
}

void *
SOB_JAR_hash (int alg, void *cookie, int length, void *data)
{
    return(NULL);
}

void *
SOB_JAR_end_hash (int alg, void *cookie)
{
    return(NULL);
}

int
SOB_JAR_sign_archive (char *nickname, char *password, char *sf, char *outsig)
{
    return(-1);
}

int
SOB_set_context (ZIG *zig, MWContext *mw)
{
    return(-1);
}

int
SOB_pass_archive(int format, char *filename, const char *url, ZIG *zig)
{
    return(-1);
}

int
SOB_get_metainfo(ZIG *zig, char *name, char *header, void **info,
		 unsigned long *length)
{
    return(-1);
}

int
SOB_verified_extract(ZIG *zig, char *path, char *outpath)
{
    return(-1);
}

/*
** Hash a non-null terminated string "src" into "dest" using MD5
*/
SECStatus
MD5_HashBuf(unsigned char *dest, const unsigned char *src,
	    uint32 src_length)
{
    return(SECFailure);
}


/*
** Create a new MD5 context
*/
MD5Context *
MD5_NewContext(void)
{
    return(NULL);
}

/*
** Destroy an MD5 secure hash context.
**	"cx" the context
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
void
MD5_DestroyContext(MD5Context *cx, PRBool freeit)
{
    return;
}

/*
** Reset an MD5 context, preparing it for a fresh round of hashing
*/
void
MD5_Begin(MD5Context *cx)
{
    return;
}

/*
** Update the MD5 hash function with more data.
**	"cx" the context
**	"input" the data to hash
**	"inputLen" the amount of data to hash
*/
void
MD5_Update(MD5Context *cx, const unsigned char *input, unsigned int inputLen)
{
    return;
}

/*
** Finish the MD5 hash function. Produce the digested results in "digest"
**	"cx" the context
**	"digest" where the 16 bytes of digest data are stored
**	"digestLen" where the digest length (16) is stored
**	"maxDigestLen" the maximum amount of data that can ever be
**	   stored in "digest"
*/
void
MD5_End(MD5Context *cx, unsigned char *digest,
	unsigned int *digestLen, unsigned int maxDigestLen)
{
    return;
}

/*
** Hash a non-null terminated string "src" into "dest" using SHA-1
*/
SECStatus
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
    return(SECFailure);
}

/*
** Create a new SHA-1 context
*/
SHA1Context *
SHA1_NewContext(void)
{
    return(NULL);
}

/*
** Destroy a SHA-1 secure hash context.
**	"cx" the context
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
void
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
    return;
}

/*
** Reset a SHA-1 context, preparing it for a fresh round of hashing
*/
void
SHA1_Begin(SHA1Context *cx)
{
    return;
}

/*
** Update the SHA-1 hash function with more data.
**	"cx" the context
**	"input" the data to hash
**	"inputLen" the amount of data to hash
*/
void
SHA1_Update(SHA1Context *cx, const unsigned char *input, unsigned int inputLen)
{
    return;
}

/*
** Finish the SHA-1 hash function. Produce the digested results in "digest"
**	"cx" the context
**	"digest" where the 16 bytes of digest data are stored
**	"digestLen" where the digest length (20) is stored
**	"maxDigestLen" the maximum amount of data that can ever be
**	   stored in "digest"
*/
void
SHA1_End(SHA1Context *cx, unsigned char *digest,
	 unsigned int *digestLen, unsigned int maxDigestLen)
{
    return;
}

/*
** Return a malloc'd ascii string which is the base64 encoded
** version of the input string.
*/
char *
BTOA_DataToAscii(const unsigned char *data, unsigned int len)
{
    return(NULL);
}

/*
** Return a malloc'd string which is the base64 decoded version
** of the input string; set *lenp to the length of the returned data.
*/
unsigned char *
ATOB_AsciiToData(const char *string, unsigned int *lenp)
{
    return(NULL);
}


