/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */

/*
 * Functions and types used by https servers to send (download) pre-encrypted 
 * files over SSL connections that use Fortezza ciphersuites.
 *
 * ***** BEGIN LICENSE BLOCK *****
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

#include "seccomon.h"
#include "prio.h"

typedef struct PEHeaderStr PEHeader;

#define PE_MIME_TYPE "application/pre-encrypted"


/*
 * unencrypted header. The 'top' half of this header is generic. The union
 * is type specific, and may include bulk cipher type information
 * (Fortezza supports only Fortezza Bulk encryption). Only fortezza 
 * pre-encrypted is defined.
 */
typedef struct PEFortezzaHeaderStr PEFortezzaHeader;
typedef struct PEFortezzaGeneratedHeaderStr PEFortezzaGeneratedHeader;
typedef struct PEFixedKeyHeaderStr PEFixedKeyHeader;
typedef struct PERSAKeyHeaderStr PERSAKeyHeader;

struct PEFortezzaHeaderStr {
    unsigned char key[12];      /* Ks wrapped MEK */
    unsigned char iv[24];       /* iv for this MEK */
    unsigned char hash[20];     /* SHA hash of file */
    unsigned char serial[8];    /* serial number of the card that owns
                                     * Ks */
};

struct PEFortezzaGeneratedHeaderStr {
    unsigned char key[12];      /* TEK wrapped MEK */
    unsigned char iv[24];       /* iv for this MEK */
    unsigned char hash[20];     /* SHA hash of file */
    unsigned char Ra[128];      /* RA to generate TEK */
    unsigned char Y[128];       /* Y to generate TEK */
};

struct PEFixedKeyHeaderStr {
    unsigned char pkcs11Mech[4];  /* Symetric key operation */
    unsigned char labelLen[2];	  /* length of the token label */
    unsigned char keyIDLen[2];	  /* length of the token Key ID */
    unsigned char ivLen[2];	  /* length of IV */
    unsigned char keyLen[2];	  /* length of key (DES3_ECB encrypted) */
    unsigned char data[1];	  /* start of data */
};

struct PERSAKeyHeaderStr {
    unsigned char pkcs11Mech[4];  /* Symetric key operation */
    unsigned char issuerLen[2];	  /* length of cert issuer */
    unsigned char serialLen[2];	  /* length of the cert serial */
    unsigned char ivLen[2];	  /* length of IV */
    unsigned char keyLen[2];	  /* length of key (RSA encrypted) */
    unsigned char data[1];	  /* start of data */
};

/* macros to get at the variable length data fields */
#define PEFIXED_Label(header) (header->data)
#define PEFIXED_KeyID(header) (&header->data[GetInt2(header->labelLen)])
#define PEFIXED_IV(header) (&header->data[GetInt2(header->labelLen)\
						+GetInt2(header->keyIDLen)])
#define PEFIXED_Key(header) (&header->data[GetInt2(header->labelLen)\
			+GetInt2(header->keyIDLen)+GetInt2(header->keyLen)])
#define PERSA_Issuer(header) (header->data)
#define PERSA_Serial(header) (&header->data[GetInt2(header->issuerLen)])
#define PERSA_IV(header) (&header->data[GetInt2(header->issuerLen)\
						+GetInt2(header->serialLen)])
#define PERSA_Key(header) (&header->data[GetInt2(header->issuerLen)\
			+GetInt2(header->serialLen)+GetInt2(header->keyLen)])
struct PEHeaderStr {
    unsigned char magic  [2];		/* always 0xC0DE */
    unsigned char len    [2];		/* length of PEHeader */
    unsigned char type   [2];		/* FORTEZZA, DIFFIE-HELMAN, RSA */
    unsigned char version[2];		/* version number: 1.0 */
    union {
        PEFortezzaHeader          fortezza;
        PEFortezzaGeneratedHeader g_fortezza;
	PEFixedKeyHeader          fixed;
	PERSAKeyHeader            rsa;
    } u;
};

#define PE_CRYPT_INTRO_LEN 8
#define PE_INTRO_LEN 4
#define PE_BASE_HEADER_LEN  8

#define PRE_BLOCK_SIZE 8         /* for decryption blocks */


/*
 * Platform neutral encode/decode macros.
 */
#define GetInt2(c) ((c[0] << 8) | c[1])
#define GetInt4(c) (((unsigned long)c[0] << 24)|((unsigned long)c[1] << 16)\
			|((unsigned long)c[2] << 8)| ((unsigned long)c[3]))
#define PutInt2(c,i) ((c[1] = (i) & 0xff), (c[0] = ((i) >> 8) & 0xff))
#define PutInt4(c,i) ((c[0]=((i) >> 24) & 0xff),(c[1]=((i) >> 16) & 0xff),\
			(c[2] = ((i) >> 8) & 0xff), (c[3] = (i) & 0xff))

/*
 * magic numbers.
 */
#define PRE_MAGIC		0xc0de
#define PRE_VERSION		0x1010
#define PRE_FORTEZZA_FILE	0x00ff  /* pre-encrypted file on disk */
#define PRE_FORTEZZA_STREAM	0x00f5  /* pre-encrypted file in stream */
#define PRE_FORTEZZA_GEN_STREAM	0x00f6  /* Generated pre-encrypted file */
#define PRE_FIXED_FILE		0x000f  /* fixed key on disk */
#define PRE_RSA_FILE		0x001f  /* RSA in file */
#define PRE_FIXED_STREAM	0x0005  /* fixed key in stream */

/*
 * internal implementation info
 */


/* convert an existing stream header to a version with local parameters */
PEHeader *SSL_PreencryptedStreamToFile(PRFileDesc *fd, PEHeader *,
				       int *headerSize);

/* convert an existing file header to one suitable for streaming out */
PEHeader *SSL_PreencryptedFileToStream(PRFileDesc *fd, PEHeader *,
				       int *headerSize);

