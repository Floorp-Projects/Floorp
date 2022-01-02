/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SECPKCS5_H_
#define _SECPKCS5_H_

#include "plarena.h"
#include "secitem.h"
#include "seccomon.h"
#include "secoidt.h"
#include "hasht.h"

typedef SECItem *(*SEC_PKCS5GetPBEPassword)(void *arg);

/* used for V2 PKCS 12 Draft Spec */
typedef enum {
    pbeBitGenIDNull = 0,
    pbeBitGenCipherKey = 0x01,
    pbeBitGenCipherIV = 0x02,
    pbeBitGenIntegrityKey = 0x03
} PBEBitGenID;

typedef enum {
    NSSPKCS5_PBKDF1 = 0,
    NSSPKCS5_PBKDF2 = 1,
    NSSPKCS5_PKCS12_V2 = 2
} NSSPKCS5PBEType;

typedef struct NSSPKCS5PBEParameterStr NSSPKCS5PBEParameter;

struct NSSPKCS5PBEParameterStr {
    PLArenaPool *poolp;
    SECItem salt;      /* octet string */
    SECItem iteration; /* integer */
    SECItem keyLength; /* integer */

    /* used locally */
    int iter;
    int keyLen;
    int ivLen;
    unsigned char *ivData;
    HASH_HashType hashType;
    NSSPKCS5PBEType pbeType;
    SECAlgorithmID prfAlg;
    PBEBitGenID keyID;
    SECOidTag encAlg;
    PRBool is2KeyDES;
};

SEC_BEGIN_PROTOS
/* Create a PKCS5 Algorithm ID
 * The algorithm ID is set up using the PKCS #5 parameter structure
 *  algorithm is the PBE algorithm ID for the desired algorithm
 *  pbe is a pbe param block with all the info needed to create the
 *   algorithm id.
 * If an error occurs or the algorithm specified is not supported
 * or is not a password based encryption algorithm, NULL is returned.
 * Otherwise, a pointer to the algorithm id is returned.
 */
extern SECAlgorithmID *
nsspkcs5_CreateAlgorithmID(PLArenaPool *arena, SECOidTag algorithm,
                           NSSPKCS5PBEParameter *pbe);

/*
 * Convert an Algorithm ID to a PBE Param.
 * NOTE: this does not suppport PKCS 5 v2 because it's only used for the
 * keyDB which only support PKCS 5 v1, PFX, and PKCS 12.
 */
NSSPKCS5PBEParameter *
nsspkcs5_AlgidToParam(SECAlgorithmID *algid);

/*
 * Convert an Algorithm ID to a PBE Param.
 * NOTE: this does not suppport PKCS 5 v2 because it's only used for the
 * keyDB which only support PKCS 5 v1, PFX, and PKCS 12.
 */
NSSPKCS5PBEParameter *
nsspkcs5_NewParam(SECOidTag alg, HASH_HashType hashType, SECItem *salt,
                  int iterationCount);

/* Encrypt/Decrypt data using password based encryption.
 *  algid is the PBE algorithm identifier,
 *  pwitem is the password,
 *  src is the source for encryption/decryption,
 *  encrypt is PR_TRUE for encryption, PR_FALSE for decryption.
 * The key and iv are generated based upon PKCS #5 then the src
 * is either encrypted or decrypted.  If an error occurs, NULL
 * is returned, otherwise the ciphered contents is returned.
 */
extern SECItem *
nsspkcs5_CipherData(NSSPKCS5PBEParameter *, SECItem *pwitem,
                    SECItem *src, PRBool encrypt, PRBool *update);

extern SECItem *
nsspkcs5_ComputeKeyAndIV(NSSPKCS5PBEParameter *, SECItem *pwitem,
                         SECItem *iv, PRBool faulty3DES);

/* Destroys PBE parameter */
extern void
nsspkcs5_DestroyPBEParameter(NSSPKCS5PBEParameter *param);

HASH_HashType HASH_FromHMACOid(SECOidTag oid);
SECOidTag HASH_HMACOidFromHash(HASH_HashType);

/* fips selftest */
extern SECStatus
sftk_fips_pbkdf_PowerUpSelfTests(void);

SEC_END_PROTOS

#endif
