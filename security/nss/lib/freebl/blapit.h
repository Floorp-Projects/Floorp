/*
 * blapit.h - public data structures for the crypto library
 *
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
 *
 * $Id: blapit.h,v 1.6 2002/11/16 03:21:53 nelsonb%netscape.com Exp $
 */

#ifndef _BLAPIT_H_
#define _BLAPIT_H_

#include "seccomon.h"
#include "plarena.h"

/* RC2 operation modes */
#define NSS_RC2			0
#define NSS_RC2_CBC		1

/* RC5 operation modes */
#define NSS_RC5                 0
#define NSS_RC5_CBC             1

/* DES operation modes */
#define NSS_DES			0
#define NSS_DES_CBC		1
#define NSS_DES_EDE3		2
#define NSS_DES_EDE3_CBC	3

#define DES_KEY_LENGTH		8	/* Bytes */

/* AES operation modes */
#define NSS_AES                 0
#define NSS_AES_CBC             1

#define DSA_SIGNATURE_LEN 	40	/* Bytes */
#define DSA_SUBPRIME_LEN	20	/* Bytes */

/*
 * Number of bytes each hash algorithm produces
 */
#define MD2_LENGTH		16	/* Bytes */
#define MD5_LENGTH		16	/* Bytes */
#define SHA1_LENGTH		20	/* Bytes */
#define SHA256_LENGTH 		32 	/* bytes */
#define SHA384_LENGTH 		48 	/* bytes */
#define SHA512_LENGTH 		64 	/* bytes */
#define HASH_LENGTH_MAX         SHA512_LENGTH

/*
 * Input block size for each hash algorithm.
 */

#define SHA256_BLOCK_LENGTH 	 64 	/* bytes */
#define SHA384_BLOCK_LENGTH 	128 	/* bytes */
#define SHA512_BLOCK_LENGTH 	128 	/* bytes */

#define NSS_FREEBL_DEFAULT_CHUNKSIZE 2048

/*
 * The FIPS 186 algorithm for generating primes P and Q allows only 9
 * distinct values for the length of P, and only one value for the
 * length of Q.
 * The algorithm uses a variable j to indicate which of the 9 lengths
 * of P is to be used.
 * The following table relates j to the lengths of P and Q in bits.
 *
 *	j	bits in P	bits in Q
 *	_	_________	_________
 *	0	 512		160
 *	1	 576		160
 *	2	 640		160
 *	3	 704		160
 *	4	 768		160
 *	5	 832		160
 *	6	 896		160
 *	7	 960		160
 *	8	1024		160
 *
 * The FIPS-186 compliant PQG generator takes j as an input parameter.
 */

#define DSA_Q_BITS       160
#define DSA_MAX_P_BITS	1024
#define DSA_MIN_P_BITS	 512

/*
 * function takes desired number of bits in P,
 * returns index (0..8) or -1 if number of bits is invalid.
 */
#define PQG_PBITS_TO_INDEX(bits) ((((bits)-512) % 64) ? -1 : (int)((bits)-512)/64)

/*
 * function takes index (0-8)
 * returns number of bits in P for that index, or -1 if index is invalid.
 */
#define PQG_INDEX_TO_PBITS(j) (((unsigned)(j) > 8) ? -1 : (512 + 64 * (j)))


/***************************************************************************
** Opaque objects 
*/

struct DESContextStr        ;
struct RC2ContextStr        ;
struct RC4ContextStr        ;
struct RC5ContextStr        ;
struct AESContextStr        ;
struct MD2ContextStr        ;
struct MD5ContextStr        ;
struct SHA1ContextStr       ;
struct SHA256ContextStr     ;
struct SHA512ContextStr     ;

typedef struct DESContextStr        DESContext;
typedef struct RC2ContextStr        RC2Context;
typedef struct RC4ContextStr        RC4Context;
typedef struct RC5ContextStr        RC5Context;
typedef struct AESContextStr        AESContext;
typedef struct MD2ContextStr        MD2Context;
typedef struct MD5ContextStr        MD5Context;
typedef struct SHA1ContextStr       SHA1Context;
typedef struct SHA256ContextStr     SHA256Context;
typedef struct SHA512ContextStr     SHA512Context;
/* SHA384Context is really a SHA512ContextStr.  This is not a mistake. */
typedef struct SHA512ContextStr     SHA384Context;

/***************************************************************************
** RSA Public and Private Key structures
*/

/* member names from PKCS#1, section 7.1 */
struct RSAPublicKeyStr {
    PRArenaPool * arena;
    SECItem modulus;
    SECItem publicExponent;
};
typedef struct RSAPublicKeyStr RSAPublicKey;

/* member names from PKCS#1, section 7.2 */
struct RSAPrivateKeyStr {
    PRArenaPool * arena;
    SECItem version;
    SECItem modulus;
    SECItem publicExponent;
    SECItem privateExponent;
    SECItem prime1;
    SECItem prime2;
    SECItem exponent1;
    SECItem exponent2;
    SECItem coefficient;
};
typedef struct RSAPrivateKeyStr RSAPrivateKey;


/***************************************************************************
** DSA Public and Private Key and related structures
*/

struct PQGParamsStr {
    PRArenaPool *arena;
    SECItem prime;    /* p */
    SECItem subPrime; /* q */
    SECItem base;     /* g */
    /* XXX chrisk: this needs to be expanded to hold j and validationParms (RFC2459 7.3.2) */
};
typedef struct PQGParamsStr PQGParams;

struct PQGVerifyStr {
    PRArenaPool * arena;	/* includes this struct, seed, & h. */
    unsigned int  counter;
    SECItem       seed;
    SECItem       h;
};
typedef struct PQGVerifyStr PQGVerify;

struct DSAPublicKeyStr {
    PQGParams params;
    SECItem publicValue;
};
typedef struct DSAPublicKeyStr DSAPublicKey;

struct DSAPrivateKeyStr {
    PQGParams params;
    SECItem publicValue;
    SECItem privateValue;
};
typedef struct DSAPrivateKeyStr DSAPrivateKey;

/***************************************************************************
** Diffie-Hellman Public and Private Key and related structures
** Structure member names suggested by PKCS#3.
*/

struct DHParamsStr {
    PRArenaPool * arena;
    SECItem prime; /* p */
    SECItem base; /* g */
};
typedef struct DHParamsStr DHParams;

struct DHPublicKeyStr {
    PRArenaPool * arena;
    SECItem prime;
    SECItem base;
    SECItem publicValue;
};
typedef struct DHPublicKeyStr DHPublicKey;

struct DHPrivateKeyStr {
    PRArenaPool * arena;
    SECItem prime;
    SECItem base;
    SECItem publicValue;
    SECItem privateValue;
};
typedef struct DHPrivateKeyStr DHPrivateKey;


#endif /* _BLAPIT_H_ */
