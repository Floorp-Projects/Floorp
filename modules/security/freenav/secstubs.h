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
#ifndef _SECSTUBS_H_
#define _SECSTUBS_H_

SEC_BEGIN_PROTOS

CERTCertificate *
CERT_DupCertificate(CERTCertificate *cert);

void
CERT_DestroyCertificate(CERTCertificate *cert);

CERTCertificate *
CERT_NewTempCertificate (CERTCertDBHandle *handle, SECItem *derCert,
			 char *nickname, PRBool isperm, PRBool copyDER);

CERTCertDBHandle *
CERT_GetDefaultCertDB(void);

CERTCertificate *
CERT_DecodeCertFromPackage(char *certbuf, int certlen);

char *
CERT_HTMLCertInfo(CERTCertificate *cert, PRBool showImages, PRBool showIssuer);

PRBool
CERT_CompareCertsForRedirection(CERTCertificate *c1, CERTCertificate *c2);

unsigned long
SSL_SecurityCapabilities(void);

int
SSL_SetSockPeerID(PRFileDesc *fd, char *peerID);

int
SSL_SecurityStatus(PRFileDesc *fd, int *on, char **cipher,
		   int *keySize, int *secretKeySize,
		   char **issuer, char **subject);

CERTCertificate *
SSL_PeerCertificate(PRFileDesc *fd);

PRBool
SSL_IsDomestic(void);

SECStatus
RNG_RNGInit(void);

SECStatus
RNG_GenerateGlobalRandomBytes(void *dest, size_t len);

size_t
RNG_GetNoise(void *buf, size_t maxbytes);

void
RNG_SystemInfoForRNG(void);

void RNG_FileForRNG(char *filename);

SECStatus
RNG_RandomUpdate(void *data, size_t bytes);

SECStatus
MD5_HashBuf(unsigned char *dest, const unsigned char *src,
	    uint32 src_length);

MD5Context *
MD5_NewContext(void);

void
MD5_DestroyContext(MD5Context *cx, PRBool freeit);

void
MD5_Begin(MD5Context *cx);

void
MD5_Update(MD5Context *cx, const unsigned char *input, unsigned int inputLen);

void
MD5_End(MD5Context *cx, unsigned char *digest,
	unsigned int *digestLen, unsigned int maxDigestLen);

SECStatus
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length);

SHA1Context *
SHA1_NewContext(void);

void
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit);

void
SHA1_Begin(SHA1Context *cx);

void
SHA1_Update(SHA1Context *cx, const unsigned char *input, unsigned int inputLen);

void
SHA1_End(SHA1Context *cx, unsigned char *digest,
	 unsigned int *digestLen, unsigned int maxDigestLen);

char *
BTOA_DataToAscii(const unsigned char *data, unsigned int len);

unsigned char *
ATOB_AsciiToData(const char *string, unsigned int *lenp);

SEC_END_PROTOS

#endif /* _SECSTUBS_H_ */
