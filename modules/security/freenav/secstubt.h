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
#ifndef _SECSTUBT_H_
#define _SECSTUBT_H_

typedef struct _item {
    unsigned char *data;
    unsigned long len;
} SECItem;

struct CERTCertificateStr {
    SECItem derCert;
};

typedef struct CERTCertificateStr CERTCertificate;
typedef struct _certdb CERTCertDBHandle;
typedef struct _md5context MD5Context;
typedef struct _sha1context SHA1Context;

#define MD5_LENGTH	16
#define SHA1_LENGTH	20

typedef enum _SECStatus {
    SECWouldBlock = -2,
    SECFailure = -1,
    SECSuccess = 0
} SECStatus;

#define SSL_SECURITY_STATUS_NOOPT -1
#define SSL_SECURITY_STATUS_OFF           0
#define SSL_SECURITY_STATUS_ON_HIGH       1
#define SSL_SECURITY_STATUS_ON_LOW        2
#define SSL_SECURITY_STATUS_FORTEZZA      3

#define SSL_SC_RSA              0x00000001L
#define SSL_SC_MD2              0x00000010L
#define SSL_SC_MD5              0x00000020L
#define SSL_SC_RC2_CBC          0x00001000L
#define SSL_SC_RC4              0x00002000L
#define SSL_SC_DES_CBC          0x00004000L
#define SSL_SC_DES_EDE3_CBC     0x00008000L
#define SSL_SC_IDEA_CBC         0x00010000L

#endif /* _SECSTUBT_H_ */
