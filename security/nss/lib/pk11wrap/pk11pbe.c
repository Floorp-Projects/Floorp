/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plarena.h"

#include "blapit.h"
#include "seccomon.h"
#include "secitem.h"
#include "secport.h"
#include "hasht.h"
#include "pkcs11t.h"
#include "sechash.h"
#include "secasn1.h"
#include "secder.h"
#include "secoid.h"
#include "secerr.h"
#include "secmod.h"
#include "pk11func.h"
#include "secpkcs5.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "keyhi.h"

typedef struct SEC_PKCS5PBEParameterStr SEC_PKCS5PBEParameter;
struct SEC_PKCS5PBEParameterStr {
    PLArenaPool *poolp;
    SECItem salt;              /* octet string */
    SECItem iteration;         /* integer */
    SECItem keyLength;         /* PKCS5v2 only */
    SECAlgorithmID *pPrfAlgId; /* PKCS5v2 only */
    SECAlgorithmID prfAlgId;   /* PKCS5v2 only */
};

/* PKCS5 V2 has an algorithm ID for the encryption and for
 * the key generation. This is valid for SEC_OID_PKCS5_PBES2
 * and SEC_OID_PKCS5_PBMAC1
 */
struct sec_pkcs5V2ParameterStr {
    PLArenaPool *poolp;
    SECAlgorithmID pbeAlgId;    /* real pbe algorithms */
    SECAlgorithmID cipherAlgId; /* encryption/mac */
};

typedef struct sec_pkcs5V2ParameterStr sec_pkcs5V2Parameter;

/* template for PKCS 5 PBE Parameter.  This template has been expanded
 * based upon the additions in PKCS 12.  This should eventually be moved
 * if RSA updates PKCS 5.
 */
const SEC_ASN1Template SEC_PKCS5PBEParameterTemplate[] =
    {
      { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(SEC_PKCS5PBEParameter) },
      { SEC_ASN1_OCTET_STRING,
        offsetof(SEC_PKCS5PBEParameter, salt) },
      { SEC_ASN1_INTEGER,
        offsetof(SEC_PKCS5PBEParameter, iteration) },
      { 0 }
    };

const SEC_ASN1Template SEC_V2PKCS12PBEParameterTemplate[] =
    {
      { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS5PBEParameter) },
      { SEC_ASN1_OCTET_STRING, offsetof(SEC_PKCS5PBEParameter, salt) },
      { SEC_ASN1_INTEGER, offsetof(SEC_PKCS5PBEParameter, iteration) },
      { 0 }
    };

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

/* SECOID_PKCS5_PBKDF2 */
const SEC_ASN1Template SEC_PKCS5V2PBEParameterTemplate[] =
    {
      { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS5PBEParameter) },
      /* This is really a choice, but since we only understand this
     * choice, just inline it */
      { SEC_ASN1_OCTET_STRING, offsetof(SEC_PKCS5PBEParameter, salt) },
      { SEC_ASN1_INTEGER, offsetof(SEC_PKCS5PBEParameter, iteration) },
      { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
        offsetof(SEC_PKCS5PBEParameter, keyLength) },
      { SEC_ASN1_POINTER | SEC_ASN1_XTRN | SEC_ASN1_OPTIONAL,
        offsetof(SEC_PKCS5PBEParameter, pPrfAlgId),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
      { 0 }
    };

/* SEC_OID_PKCS5_PBES2, SEC_OID_PKCS5_PBMAC1 */
const SEC_ASN1Template SEC_PKCS5V2ParameterTemplate[] =
    {
      { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SEC_PKCS5PBEParameter) },
      { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(sec_pkcs5V2Parameter, pbeAlgId),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
      { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(sec_pkcs5V2Parameter, cipherAlgId),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
      { 0 }
    };

/*
 * maps a PBE algorithm to a crypto algorithm. for PKCS12 and PKCS5v1
 * for PKCS5v2 it returns SEC_OID_PKCS5_PBKDF2.
 */
SECOidTag
sec_pkcs5GetCryptoFromAlgTag(SECOidTag algorithm)
{
    switch (algorithm) {
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
            return SEC_OID_DES_EDE3_CBC;
        case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
        case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
        case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
            return SEC_OID_DES_CBC;
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
            return SEC_OID_RC2_CBC;
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
            return SEC_OID_RC4;
        case SEC_OID_PKCS5_PBKDF2:
        case SEC_OID_PKCS5_PBES2:
        case SEC_OID_PKCS5_PBMAC1:
            return SEC_OID_PKCS5_PBKDF2;
        default:
            break;
    }

    return SEC_OID_UNKNOWN;
}

/*
 * get a new PKCS5 V2 Parameter from the algorithm id.
 *  if arena is passed in, use it, otherwise create a new arena.
 */
sec_pkcs5V2Parameter *
sec_pkcs5_v2_get_v2_param(PLArenaPool *arena, SECAlgorithmID *algid)
{
    PLArenaPool *localArena = NULL;
    sec_pkcs5V2Parameter *pbeV2_param;
    SECStatus rv;

    if (arena == NULL) {
        localArena = arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
        if (arena == NULL) {
            return NULL;
        }
    }
    pbeV2_param = PORT_ArenaZNew(arena, sec_pkcs5V2Parameter);
    if (pbeV2_param == NULL) {
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(arena, pbeV2_param,
                            SEC_PKCS5V2ParameterTemplate, &algid->parameters);
    if (rv == SECFailure) {
        goto loser;
    }

    pbeV2_param->poolp = arena;
    return pbeV2_param;
loser:
    if (localArena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

void
sec_pkcs5_v2_destroy_v2_param(sec_pkcs5V2Parameter *param)
{
    if (param && param->poolp) {
        PORT_FreeArena(param->poolp, PR_TRUE);
    }
}

/* maps crypto algorithm from PBE algorithm.
 */
SECOidTag
SEC_PKCS5GetCryptoAlgorithm(SECAlgorithmID *algid)
{

    SECOidTag pbeAlg;
    SECOidTag cipherAlg;

    if (algid == NULL)
        return SEC_OID_UNKNOWN;

    pbeAlg = SECOID_GetAlgorithmTag(algid);
    cipherAlg = sec_pkcs5GetCryptoFromAlgTag(pbeAlg);
    if ((cipherAlg == SEC_OID_PKCS5_PBKDF2) &&
        (pbeAlg != SEC_OID_PKCS5_PBKDF2)) {
        sec_pkcs5V2Parameter *pbeV2_param;
        cipherAlg = SEC_OID_UNKNOWN;

        pbeV2_param = sec_pkcs5_v2_get_v2_param(NULL, algid);
        if (pbeV2_param != NULL) {
            cipherAlg = SECOID_GetAlgorithmTag(&pbeV2_param->cipherAlgId);
            sec_pkcs5_v2_destroy_v2_param(pbeV2_param);
        }
    }

    return cipherAlg;
}

/* check to see if an oid is a pbe algorithm
 */
PRBool
SEC_PKCS5IsAlgorithmPBEAlg(SECAlgorithmID *algid)
{
    return (PRBool)(SEC_PKCS5GetCryptoAlgorithm(algid) != SEC_OID_UNKNOWN);
}

PRBool
SEC_PKCS5IsAlgorithmPBEAlgTag(SECOidTag algtag)
{
    return (PRBool)(sec_pkcs5GetCryptoFromAlgTag(algtag) != SEC_OID_UNKNOWN);
}

/*
 * find the most appropriate PKCS5v2 overall oid tag from a regular
 * cipher/hash algorithm tag.
 */
static SECOidTag
sec_pkcs5v2_get_pbe(SECOidTag algTag)
{
    /* if it's a valid hash oid... */
    if (HASH_GetHashOidTagByHMACOidTag(algTag) != SEC_OID_UNKNOWN) {
        /* use the MAC tag */
        return SEC_OID_PKCS5_PBMAC1;
    }
    if (HASH_GetHashTypeByOidTag(algTag) != HASH_AlgNULL) {
        /* eliminate Hash algorithms */
        return SEC_OID_UNKNOWN;
    }
    if (PK11_AlgtagToMechanism(algTag) != CKM_INVALID_MECHANISM) {
        /* it's not a hash, if it has a PKCS #11 mechanism associated
         * with it, assume it's a cipher. (NOTE this will generate
         * some false positives). */
        return SEC_OID_PKCS5_PBES2;
    }
    return SEC_OID_UNKNOWN;
}

/*
 * maps PBE algorithm from crypto algorithm, assumes SHA1 hashing.
 *  input keyLen in bits.
 */
SECOidTag
SEC_PKCS5GetPBEAlgorithm(SECOidTag algTag, int keyLen)
{
    switch (algTag) {
        case SEC_OID_DES_EDE3_CBC:
            switch (keyLen) {
                case 168:
                case 192:
                case 0:
                    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC;
                case 128:
                case 92:
                    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC;
                default:
                    break;
            }
            break;
        case SEC_OID_DES_CBC:
            return SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC;
        case SEC_OID_RC2_CBC:
            switch (keyLen) {
                case 40:
                    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
                case 128:
                case 0:
                    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
                default:
                    break;
            }
            break;
        case SEC_OID_RC4:
            switch (keyLen) {
                case 40:
                    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4;
                case 128:
                case 0:
                    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4;
                default:
                    break;
            }
            break;
        default:
            return sec_pkcs5v2_get_pbe(algTag);
    }

    return SEC_OID_UNKNOWN;
}

/*
 * Some oids encode the key size in the oid, while the actual PKCS
 * PKCS #11 mechanism does not. In those cases we can't use
 * the PKCS #11 automated key length code to select the key size.
 */
static int
sec_pkcs5v2_key_length_by_oid(SECOidTag algorithm)
{
    switch (algorithm) {
        case SEC_OID_AES_128_CBC:
        case SEC_OID_CAMELLIA_128_CBC:
            return AES_128_KEY_LENGTH;
        case SEC_OID_AES_192_CBC:
        case SEC_OID_CAMELLIA_192_CBC:
            return AES_192_KEY_LENGTH;
        case SEC_OID_AES_256_CBC:
        case SEC_OID_CAMELLIA_256_CBC:
            return AES_256_KEY_LENGTH;
        default:
            break;
    }
    return -1;
}

/* find the keylength from the algorithm id */
static int
sec_pkcs5v2_default_key_length(SECOidTag algorithm)
{
    CK_MECHANISM_TYPE cryptoMech;
    int key_length = sec_pkcs5v2_key_length_by_oid(algorithm);
    if (key_length != -1) {
        return key_length;
    }
    cryptoMech = PK11_AlgtagToMechanism(algorithm);
    if (cryptoMech == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return -1;
    }
    return PK11_GetMaxKeyLength(cryptoMech);
}

/*
 * get the key length in bytes from a PKCS5 PBE
 */
static int
sec_pkcs5v2_key_length(SECAlgorithmID *algid, SECAlgorithmID *cipherAlgId)
{
    SECOidTag algorithm;
    PLArenaPool *arena = NULL;
    SEC_PKCS5PBEParameter p5_param;
    SECStatus rv;
    int length = -1;
    SECOidTag cipherAlg = SEC_OID_UNKNOWN;

    algorithm = SECOID_GetAlgorithmTag(algid);
    /* sanity check, they should all be PBKDF2 here */
    if (algorithm != SEC_OID_PKCS5_PBKDF2) {
        return -1;
    }

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL) {
        goto loser;
    }
    PORT_Memset(&p5_param, 0, sizeof(p5_param));
    rv = SEC_ASN1DecodeItem(arena, &p5_param,
                            SEC_PKCS5V2PBEParameterTemplate, &algid->parameters);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (cipherAlgId)
        cipherAlg = SECOID_GetAlgorithmTag(cipherAlgId);

    if (p5_param.keyLength.data != NULL) {
        /* if the length is given, accept that length. This
         * will allow us to decode old NSS encrypted data
         * where we used the MAX keysize for the algorithm,
         * but put an incorrect header for a different keysize.
         */
        length = DER_GetInteger(&p5_param.keyLength);
    } else {
        /* if the keylength was not specified, figure it
         * out from the oid */
        length = sec_pkcs5v2_default_key_length(cipherAlg);
    }

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return length;
}

/*
 *  get the key length in bytes needed for the PBE algorithm
 */
int
SEC_PKCS5GetKeyLength(SECAlgorithmID *algid)
{

    SECOidTag algorithm;

    if (algid == NULL)
        return SEC_OID_UNKNOWN;

    algorithm = SECOID_GetAlgorithmTag(algid);

    switch (algorithm) {
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
            return 24;
        case SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC:
        case SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC:
        case SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC:
            return 8;
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
            return 5;
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
            return 16;
        case SEC_OID_PKCS5_PBKDF2:
            return sec_pkcs5v2_key_length(algid, NULL);
        case SEC_OID_PKCS5_PBES2:
        case SEC_OID_PKCS5_PBMAC1: {
            sec_pkcs5V2Parameter *pbeV2_param;
            int length = -1;
            pbeV2_param = sec_pkcs5_v2_get_v2_param(NULL, algid);
            if (pbeV2_param != NULL) {
                length = sec_pkcs5v2_key_length(&pbeV2_param->pbeAlgId,
                                                &pbeV2_param->cipherAlgId);
                sec_pkcs5_v2_destroy_v2_param(pbeV2_param);
            }
            return length;
        }

        default:
            break;
    }
    return -1;
}

/* the PKCS12 V2 algorithms only encode the salt, there is no iteration
 * count so we need a check for V2 algorithm parameters.
 */
static PRBool
sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(SECOidTag algorithm)
{
    switch (algorithm) {
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC:
        case SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC:
            return PR_TRUE;
        default:
            break;
    }

    return PR_FALSE;
}

static PRBool
sec_pkcs5_is_algorithm_v2_pkcs5_algorithm(SECOidTag algorithm)
{
    switch (algorithm) {
        case SEC_OID_PKCS5_PBES2:
        case SEC_OID_PKCS5_PBMAC1:
        case SEC_OID_PKCS5_PBKDF2:
            return PR_TRUE;
        default:
            break;
    }

    return PR_FALSE;
}

/* destroy a pbe parameter.  it assumes that the parameter was
 * generated using the appropriate create function and therefor
 * contains an arena pool.
 */
static void
sec_pkcs5_destroy_pbe_param(SEC_PKCS5PBEParameter *pbe_param)
{
    if (pbe_param != NULL)
        PORT_FreeArena(pbe_param->poolp, PR_TRUE);
}

/* creates a PBE parameter based on the PBE algorithm.  the only required
 * parameters are algorithm and interation.  the return is a PBE parameter
 * which conforms to PKCS 5 parameter unless an extended parameter is needed.
 * this is primarily if keyLength and a variable key length algorithm are
 * specified.
 *   salt -  if null, a salt will be generated from random bytes.
 *   iteration - number of iterations to perform hashing.
 *   keyLength - only used in variable key length algorithms. if specified,
 *            should be in bytes.
 * once a parameter is allocated, it should be destroyed calling
 * sec_pkcs5_destroy_pbe_parameter or SEC_PKCS5DestroyPBEParameter.
 */
#define DEFAULT_SALT_LENGTH 16
static SEC_PKCS5PBEParameter *
sec_pkcs5_create_pbe_parameter(SECOidTag algorithm,
                               SECItem *salt,
                               int iteration,
                               int keyLength,
                               SECOidTag prfAlg)
{
    PLArenaPool *poolp = NULL;
    SEC_PKCS5PBEParameter *pbe_param = NULL;
    SECStatus rv = SECSuccess;
    void *dummy = NULL;

    if (iteration < 0) {
        return NULL;
    }

    poolp = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (poolp == NULL)
        return NULL;

    pbe_param = (SEC_PKCS5PBEParameter *)PORT_ArenaZAlloc(poolp,
                                                          sizeof(SEC_PKCS5PBEParameter));
    if (!pbe_param) {
        PORT_FreeArena(poolp, PR_TRUE);
        return NULL;
    }

    pbe_param->poolp = poolp;

    rv = SECFailure;
    if (salt && salt->data) {
        rv = SECITEM_CopyItem(poolp, &pbe_param->salt, salt);
    } else {
        /* sigh, the old interface generated salt on the fly, so we have to
         * preserve the semantics */
        pbe_param->salt.len = DEFAULT_SALT_LENGTH;
        pbe_param->salt.data = PORT_ArenaZAlloc(poolp, DEFAULT_SALT_LENGTH);
        if (pbe_param->salt.data) {
            rv = PK11_GenerateRandom(pbe_param->salt.data, DEFAULT_SALT_LENGTH);
        }
    }

    if (rv != SECSuccess) {
        PORT_FreeArena(poolp, PR_TRUE);
        return NULL;
    }

    /* encode the integer */
    dummy = SEC_ASN1EncodeInteger(poolp, &pbe_param->iteration,
                                  iteration);
    rv = (dummy) ? SECSuccess : SECFailure;

    if (rv != SECSuccess) {
        PORT_FreeArena(poolp, PR_FALSE);
        return NULL;
    }

    /*
     * for PKCS5 v2 Add the keylength and the prf
     */
    if (algorithm == SEC_OID_PKCS5_PBKDF2) {
        dummy = SEC_ASN1EncodeInteger(poolp, &pbe_param->keyLength,
                                      keyLength);
        rv = (dummy) ? SECSuccess : SECFailure;
        if (rv != SECSuccess) {
            PORT_FreeArena(poolp, PR_FALSE);
            return NULL;
        }
        rv = SECOID_SetAlgorithmID(poolp, &pbe_param->prfAlgId, prfAlg, NULL);
        if (rv != SECSuccess) {
            PORT_FreeArena(poolp, PR_FALSE);
            return NULL;
        }
        pbe_param->pPrfAlgId = &pbe_param->prfAlgId;
    }

    return pbe_param;
}

/* creates a algorithm ID containing the PBE algorithm and appropriate
 * parameters.  the required parameter is the algorithm.  if salt is
 * not specified, it is generated randomly.
 *
 * the returned SECAlgorithmID should be destroyed using
 * SECOID_DestroyAlgorithmID
 */
SECAlgorithmID *
sec_pkcs5CreateAlgorithmID(SECOidTag algorithm,
                           SECOidTag cipherAlgorithm,
                           SECOidTag prfAlg,
                           SECOidTag *pPbeAlgorithm,
                           int keyLength,
                           SECItem *salt,
                           int iteration)
{
    PLArenaPool *poolp = NULL;
    SECAlgorithmID *algid, *ret_algid = NULL;
    SECOidTag pbeAlgorithm = algorithm;
    SECItem der_param;
    void *dummy;
    SECStatus rv = SECFailure;
    SEC_PKCS5PBEParameter *pbe_param = NULL;
    sec_pkcs5V2Parameter pbeV2_param;

    if (iteration <= 0) {
        return NULL;
    }

    poolp = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (!poolp) {
        goto loser;
    }

    if (!SEC_PKCS5IsAlgorithmPBEAlgTag(algorithm) ||
        sec_pkcs5_is_algorithm_v2_pkcs5_algorithm(algorithm)) {
        /* use PKCS 5 v2 */
        SECItem *cipherParams;

        /*
         * if we ask for pkcs5 Algorithms directly, then the
         * application needs to supply the cipher algorithm,
         * otherwise we are implicitly using pkcs5 v2 and the
         * passed in algorithm is the encryption algorithm.
         */
        if (sec_pkcs5_is_algorithm_v2_pkcs5_algorithm(algorithm)) {
            if (cipherAlgorithm == SEC_OID_UNKNOWN) {
                goto loser;
            }
        } else {
            cipherAlgorithm = algorithm;
            /* force algorithm to be chosen below */
            algorithm = SEC_OID_PKCS5_PBKDF2;
        }

        pbeAlgorithm = SEC_OID_PKCS5_PBKDF2;
        /*
         * 'algorithm' is the overall algorithm oid tag used to wrap the
         * entire algorithm ID block. For PKCS5v1 and PKCS12, this
         * algorithm OID has encoded in it both the PBE KDF function
         * and the encryption algorithm. For PKCS 5v2, PBE KDF and
         * encryption/macing oids are encoded as parameters in
         * the algorithm ID block.
         *
         * Thus in PKCS5 v1 and PKCS12, this algorithm maps to a pkcs #11
         * mechanism, where as in PKCS 5v2, this algorithm tag does not map
         * directly to a PKCS #11 mechanim, instead the 2 oids in the
         * algorithm ID block map the the actual PKCS #11 mechanism.
         * algorithm is). We use choose this algorithm oid based on the
         * cipherAlgorithm to determine what this should be (MAC1 or PBES2).
         */
        if (algorithm == SEC_OID_PKCS5_PBKDF2) {
            /* choose mac or pbes */
            algorithm = sec_pkcs5v2_get_pbe(cipherAlgorithm);
        }

        /* set the PKCS5v2 specific parameters */
        if (keyLength == 0) {
            SECOidTag hashAlg = HASH_GetHashOidTagByHMACOidTag(cipherAlgorithm);
            if (hashAlg != SEC_OID_UNKNOWN) {
                keyLength = HASH_ResultLenByOidTag(hashAlg);
            } else {
                keyLength = sec_pkcs5v2_default_key_length(cipherAlgorithm);
            }
            if (keyLength <= 0) {
                goto loser;
            }
        }
        /* currently SEC_OID_HMAC_SHA1 is the default */
        if (prfAlg == SEC_OID_UNKNOWN) {
            prfAlg = SEC_OID_HMAC_SHA1;
        }

        /* build the PKCS5v2 cipher algorithm id */
        cipherParams = pk11_GenerateNewParamWithKeyLen(
            PK11_AlgtagToMechanism(cipherAlgorithm), keyLength);
        if (!cipherParams) {
            goto loser;
        }

        PORT_Memset(&pbeV2_param, 0, sizeof(pbeV2_param));

        rv = PK11_ParamToAlgid(cipherAlgorithm, cipherParams,
                               poolp, &pbeV2_param.cipherAlgId);
        SECITEM_FreeItem(cipherParams, PR_TRUE);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /* generate the parameter */
    pbe_param = sec_pkcs5_create_pbe_parameter(pbeAlgorithm, salt, iteration,
                                               keyLength, prfAlg);
    if (!pbe_param) {
        goto loser;
    }

    /* generate the algorithm id */
    algid = (SECAlgorithmID *)PORT_ArenaZAlloc(poolp, sizeof(SECAlgorithmID));
    if (algid == NULL) {
        goto loser;
    }

    der_param.data = NULL;
    der_param.len = 0;
    if (sec_pkcs5_is_algorithm_v2_pkcs5_algorithm(algorithm)) {
        /* first encode the PBE algorithm ID */
        dummy = SEC_ASN1EncodeItem(poolp, &der_param, pbe_param,
                                   SEC_PKCS5V2PBEParameterTemplate);
        if (dummy == NULL) {
            goto loser;
        }
        rv = SECOID_SetAlgorithmID(poolp, &pbeV2_param.pbeAlgId,
                                   pbeAlgorithm, &der_param);
        if (rv != SECSuccess) {
            goto loser;
        }

        /* now encode the Full PKCS 5 parameter */
        der_param.data = NULL;
        der_param.len = 0;
        dummy = SEC_ASN1EncodeItem(poolp, &der_param, &pbeV2_param,
                                   SEC_PKCS5V2ParameterTemplate);
    } else if (!sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(algorithm)) {
        dummy = SEC_ASN1EncodeItem(poolp, &der_param, pbe_param,
                                   SEC_PKCS5PBEParameterTemplate);
    } else {
        dummy = SEC_ASN1EncodeItem(poolp, &der_param, pbe_param,
                                   SEC_V2PKCS12PBEParameterTemplate);
    }
    if (dummy == NULL) {
        goto loser;
    }

    rv = SECOID_SetAlgorithmID(poolp, algid, algorithm, &der_param);
    if (rv != SECSuccess) {
        goto loser;
    }

    ret_algid = (SECAlgorithmID *)PORT_ZAlloc(sizeof(SECAlgorithmID));
    if (ret_algid == NULL) {
        goto loser;
    }

    rv = SECOID_CopyAlgorithmID(NULL, ret_algid, algid);
    if (rv != SECSuccess) {
        SECOID_DestroyAlgorithmID(ret_algid, PR_TRUE);
        ret_algid = NULL;
    } else if (pPbeAlgorithm) {
        *pPbeAlgorithm = pbeAlgorithm;
    }

loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_TRUE);
        algid = NULL;
    }

    if (pbe_param) {
        sec_pkcs5_destroy_pbe_param(pbe_param);
    }

    return ret_algid;
}

SECStatus
pbe_PK11AlgidToParam(SECAlgorithmID *algid, SECItem *mech)
{
    SEC_PKCS5PBEParameter p5_param;
    SECItem *salt = NULL;
    SECOidTag algorithm = SECOID_GetAlgorithmTag(algid);
    PLArenaPool *arena = NULL;
    SECStatus rv = SECFailure;
    unsigned char *paramData = NULL;
    unsigned char *pSalt = NULL;
    CK_ULONG iterations;
    int paramLen = 0;
    int iv_len;

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL) {
        goto loser;
    }

    /*
     * decode the algid based on the pbe type
     */
    PORT_Memset(&p5_param, 0, sizeof(p5_param));
    if (sec_pkcs5_is_algorithm_v2_pkcs12_algorithm(algorithm)) {
        iv_len = PK11_GetIVLength(PK11_AlgtagToMechanism(algorithm));
        rv = SEC_ASN1DecodeItem(arena, &p5_param,
                                SEC_V2PKCS12PBEParameterTemplate, &algid->parameters);
    } else if (algorithm == SEC_OID_PKCS5_PBKDF2) {
        iv_len = 0;
        rv = SEC_ASN1DecodeItem(arena, &p5_param,
                                SEC_PKCS5V2PBEParameterTemplate, &algid->parameters);
    } else {
        iv_len = PK11_GetIVLength(PK11_AlgtagToMechanism(algorithm));
        rv = SEC_ASN1DecodeItem(arena, &p5_param, SEC_PKCS5PBEParameterTemplate,
                                &algid->parameters);
    }

    if (iv_len < 0) {
        goto loser;
    }

    if (rv != SECSuccess) {
        goto loser;
    }

    /* get salt */
    salt = &p5_param.salt;
    iterations = (CK_ULONG)DER_GetInteger(&p5_param.iteration);

    /* allocate and fill in the PKCS #11 parameters
     * based on the algorithm. */
    if (algorithm == SEC_OID_PKCS5_PBKDF2) {
        SECOidTag prfAlgTag;
        CK_PKCS5_PBKD2_PARAMS *pbeV2_params =
            (CK_PKCS5_PBKD2_PARAMS *)PORT_ZAlloc(
                sizeof(CK_PKCS5_PBKD2_PARAMS) + salt->len);

        if (pbeV2_params == NULL) {
            goto loser;
        }
        paramData = (unsigned char *)pbeV2_params;
        paramLen = sizeof(CK_PKCS5_PBKD2_PARAMS);

        /* set the prf */
        prfAlgTag = SEC_OID_HMAC_SHA1;
        if (p5_param.pPrfAlgId &&
            p5_param.pPrfAlgId->algorithm.data != 0) {
            prfAlgTag = SECOID_GetAlgorithmTag(p5_param.pPrfAlgId);
        }
        switch (prfAlgTag) {
            case SEC_OID_HMAC_SHA1:
                pbeV2_params->prf = CKP_PKCS5_PBKD2_HMAC_SHA1;
                break;
            case SEC_OID_HMAC_SHA224:
                pbeV2_params->prf = CKP_PKCS5_PBKD2_HMAC_SHA224;
                break;
            case SEC_OID_HMAC_SHA256:
                pbeV2_params->prf = CKP_PKCS5_PBKD2_HMAC_SHA256;
                break;
            case SEC_OID_HMAC_SHA384:
                pbeV2_params->prf = CKP_PKCS5_PBKD2_HMAC_SHA384;
                break;
            case SEC_OID_HMAC_SHA512:
                pbeV2_params->prf = CKP_PKCS5_PBKD2_HMAC_SHA512;
                break;
            default:
                PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
                goto loser;
        }

        /* probably should fetch these from the prfAlgid */
        pbeV2_params->pPrfData = NULL;
        pbeV2_params->ulPrfDataLen = 0;
        pbeV2_params->saltSource = CKZ_SALT_SPECIFIED;
        pSalt = ((CK_CHAR_PTR)pbeV2_params) + sizeof(CK_PKCS5_PBKD2_PARAMS);
        if (salt->data) {
            PORT_Memcpy(pSalt, salt->data, salt->len);
        }
        pbeV2_params->pSaltSourceData = pSalt;
        pbeV2_params->ulSaltSourceDataLen = salt->len;
        pbeV2_params->iterations = iterations;
    } else {
        CK_PBE_PARAMS *pbe_params = NULL;
        pbe_params = (CK_PBE_PARAMS *)PORT_ZAlloc(sizeof(CK_PBE_PARAMS) +
                                                  salt->len + iv_len);
        if (pbe_params == NULL) {
            goto loser;
        }
        paramData = (unsigned char *)pbe_params;
        paramLen = sizeof(CK_PBE_PARAMS);

        pSalt = ((CK_CHAR_PTR)pbe_params) + sizeof(CK_PBE_PARAMS);
        pbe_params->pSalt = pSalt;
        if (salt->data) {
            PORT_Memcpy(pSalt, salt->data, salt->len);
        }
        pbe_params->ulSaltLen = salt->len;
        if (iv_len) {
            pbe_params->pInitVector =
                ((CK_CHAR_PTR)pbe_params) + sizeof(CK_PBE_PARAMS) + salt->len;
        }
        pbe_params->ulIteration = iterations;
    }

    /* copy into the mechanism sec item */
    mech->data = paramData;
    mech->len = paramLen;
    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }
    return SECSuccess;

loser:
    if (paramData) {
        PORT_Free(paramData);
    }
    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }
    return SECFailure;
}

/*
 * public, deprecated, not valid for pkcs5 v2
 *
 * use PK11_CreatePBEV2AlgorithmID or PK11_CreatePBEAlgorithmID to create
 * PBE algorithmID's directly.
 */
SECStatus
PBE_PK11ParamToAlgid(SECOidTag algTag, SECItem *param, PLArenaPool *arena,
                     SECAlgorithmID *algId)
{
    CK_PBE_PARAMS *pbe_param;
    SECItem pbeSalt;
    SECAlgorithmID *pbeAlgID = NULL;
    SECStatus rv;

    if (!param || !algId) {
        return SECFailure;
    }

    pbe_param = (CK_PBE_PARAMS *)param->data;
    pbeSalt.data = (unsigned char *)pbe_param->pSalt;
    pbeSalt.len = pbe_param->ulSaltLen;
    pbeAlgID = sec_pkcs5CreateAlgorithmID(algTag, SEC_OID_UNKNOWN,
                                          SEC_OID_UNKNOWN, NULL, 0,
                                          &pbeSalt, (int)pbe_param->ulIteration);
    if (!pbeAlgID) {
        return SECFailure;
    }

    rv = SECOID_CopyAlgorithmID(arena, algId, pbeAlgID);
    SECOID_DestroyAlgorithmID(pbeAlgID, PR_TRUE);
    return rv;
}

/*
 * public, Deprecated, This function is only for binary compatibility with
 * older applications. Does not support PKCS5v2.
 *
 * Applications should use PK11_PBEKeyGen() for keys and PK11_GetPBEIV() for
 * iv values rather than generating PBE bits directly.
 */
PBEBitGenContext *
PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
                  SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
                  unsigned int iterations)
{
    SECItem *context = NULL;
    SECItem mechItem;
    CK_PBE_PARAMS pbe_params;
    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;
    PK11SlotInfo *slot;
    PK11SymKey *symKey = NULL;
    unsigned char ivData[8];

    /* use the purpose to select the low level keygen algorithm */
    switch (bitGenPurpose) {
        case pbeBitGenIntegrityKey:
            switch (hashAlgorithm) {
                case SEC_OID_SHA1:
                    mechanism = CKM_PBA_SHA1_WITH_SHA1_HMAC;
                    break;
                case SEC_OID_MD2:
                    mechanism = CKM_NSS_PBE_MD2_HMAC_KEY_GEN;
                    break;
                case SEC_OID_MD5:
                    mechanism = CKM_NSS_PBE_MD5_HMAC_KEY_GEN;
                    break;
                default:
                    break;
            }
            break;
        case pbeBitGenCipherIV:
            if (bitsNeeded > 64) {
                break;
            }
            if (hashAlgorithm != SEC_OID_SHA1) {
                break;
            }
            mechanism = CKM_PBE_SHA1_DES3_EDE_CBC;
            break;
        case pbeBitGenCipherKey:
            if (hashAlgorithm != SEC_OID_SHA1) {
                break;
            }
            switch (bitsNeeded) {
                case 40:
                    mechanism = CKM_PBE_SHA1_RC4_40;
                    break;
                case 128:
                    mechanism = CKM_PBE_SHA1_RC4_128;
                    break;
                default:
                    break;
            }
        case pbeBitGenIDNull:
            break;
    }

    if (mechanism == CKM_INVALID_MECHANISM) {
        /* we should set an error, but this is a deprecated function, and
         * we are keeping bug for bug compatibility;)... */
        return NULL;
    }

    pbe_params.pInitVector = ivData;
    pbe_params.pPassword = pwitem->data;
    pbe_params.ulPasswordLen = pwitem->len;
    pbe_params.pSalt = salt->data;
    pbe_params.ulSaltLen = salt->len;
    pbe_params.ulIteration = iterations;
    mechItem.data = (unsigned char *)&pbe_params;
    mechItem.len = sizeof(pbe_params);

    slot = PK11_GetInternalSlot();
    symKey = PK11_RawPBEKeyGen(slot, mechanism,
                               &mechItem, pwitem, PR_FALSE, NULL);
    PK11_FreeSlot(slot);
    if (symKey != NULL) {
        if (bitGenPurpose == pbeBitGenCipherIV) {
            /* NOTE: this assumes that bitsNeeded is a multiple of 8! */
            SECItem ivItem;

            ivItem.data = ivData;
            ivItem.len = bitsNeeded / 8;
            context = SECITEM_DupItem(&ivItem);
        } else {
            SECItem *keyData;
            PK11_ExtractKeyValue(symKey);
            keyData = PK11_GetKeyData(symKey);

            /* assert bitsNeeded with length? */
            if (keyData) {
                context = SECITEM_DupItem(keyData);
            }
        }
        PK11_FreeSymKey(symKey);
    }

    return (PBEBitGenContext *)context;
}

/*
 * public, Deprecated, This function is only for binary compatibility with
 * older applications. Does not support PKCS5v2.
 *
 * Applications should use PK11_PBEKeyGen() for keys and PK11_GetIV() for
 * iv values rather than generating PBE bits directly.
 */
SECItem *
PBE_GenerateBits(PBEBitGenContext *context)
{
    return (SECItem *)context;
}

/*
 * public, Deprecated, This function is only for binary compatibility with
 * older applications. Does not support PKCS5v2.
 *
 * Applications should use PK11_PBEKeyGen() for keys and PK11_GetPBEIV() for
 * iv values rather than generating PBE bits directly.
 */
void
PBE_DestroyContext(PBEBitGenContext *context)
{
    SECITEM_FreeItem((SECItem *)context, PR_TRUE);
}

/*
 * public, deprecated. Replaced with PK11_GetPBEIV().
 */
SECItem *
SEC_PKCS5GetIV(SECAlgorithmID *algid, SECItem *pwitem, PRBool faulty3DES)
{
    /* pbe stuff */
    CK_MECHANISM_TYPE type;
    SECItem *param = NULL;
    SECItem *iv = NULL;
    SECItem src;
    int iv_len = 0;
    PK11SymKey *symKey;
    PK11SlotInfo *slot;
    CK_PBE_PARAMS_PTR pPBEparams;
    SECOidTag pbeAlg;

    pbeAlg = SECOID_GetAlgorithmTag(algid);
    if (sec_pkcs5_is_algorithm_v2_pkcs5_algorithm(pbeAlg)) {
        unsigned char *ivData;
        sec_pkcs5V2Parameter *pbeV2_param = NULL;

        /* can only return the IV if the crypto Algorithm exists */
        if (pbeAlg == SEC_OID_PKCS5_PBKDF2) {
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            goto loser;
        }
        pbeV2_param = sec_pkcs5_v2_get_v2_param(NULL, algid);
        if (pbeV2_param == NULL) {
            goto loser;
        }
        /* extract the IV from the cipher algid portion of our pkcs 5 v2
         * algorithm id */
        type = PK11_AlgtagToMechanism(
            SECOID_GetAlgorithmTag(&pbeV2_param->cipherAlgId));
        param = PK11_ParamFromAlgid(&pbeV2_param->cipherAlgId);
        sec_pkcs5_v2_destroy_v2_param(pbeV2_param);
        if (!param) {
            goto loser;
        }
        /* NOTE: NULL is a permissible return here */
        ivData = PK11_IVFromParam(type, param, &iv_len);
        src.data = ivData;
        src.len = iv_len;
        goto done;
    }

    type = PK11_AlgtagToMechanism(pbeAlg);
    param = PK11_ParamFromAlgid(algid);
    if (param == NULL) {
        goto done;
    }
    slot = PK11_GetInternalSlot();
    symKey = PK11_RawPBEKeyGen(slot, type, param, pwitem, faulty3DES, NULL);
    PK11_FreeSlot(slot);
    if (symKey == NULL) {
        goto loser;
    }
    PK11_FreeSymKey(symKey);
    pPBEparams = (CK_PBE_PARAMS_PTR)param->data;
    iv_len = PK11_GetIVLength(type);

    src.data = (unsigned char *)pPBEparams->pInitVector;
    src.len = iv_len;

done:
    iv = SECITEM_DupItem(&src);

loser:
    if (param) {
        SECITEM_ZfreeItem(param, PR_TRUE);
    }
    return iv;
}

/*
 * Subs from nss 3.x that are deprecated
 */
PBEBitGenContext *
__PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
                    SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
                    unsigned int iterations)
{
    PORT_Assert("__PBE_CreateContext is Deprecated" == NULL);
    return NULL;
}

SECItem *
__PBE_GenerateBits(PBEBitGenContext *context)
{
    PORT_Assert("__PBE_GenerateBits is Deprecated" == NULL);
    return NULL;
}

void
__PBE_DestroyContext(PBEBitGenContext *context)
{
    PORT_Assert("__PBE_DestroyContext is Deprecated" == NULL);
}

SECStatus
RSA_FormatBlock(SECItem *result, unsigned modulusLen,
                int blockType, SECItem *data)
{
    PORT_Assert("RSA_FormatBlock is Deprecated" == NULL);
    return SECFailure;
}

/****************************************************************************
 *
 * Now Do The PBE Functions Here...
 *
 ****************************************************************************/

static void
pk11_destroy_ck_pbe_params(CK_PBE_PARAMS *pbe_params)
{
    if (pbe_params) {
        if (pbe_params->pPassword)
            PORT_ZFree(pbe_params->pPassword, pbe_params->ulPasswordLen);
        if (pbe_params->pSalt)
            PORT_ZFree(pbe_params->pSalt, pbe_params->ulSaltLen);
        PORT_ZFree(pbe_params, sizeof(CK_PBE_PARAMS));
    }
}

/*
 * public, deprecated.  use PK11_CreatePBEAlgorithmID or
 * PK11_CreatePBEV2AlgorithmID instead. If you needthe pkcs #11 parameters,
 * use PK11_ParamFromAlgid from the algorithm id you created using
 * PK11_CreatePBEAlgorithmID or PK11_CreatePBEV2AlgorithmID.
 */
SECItem *
PK11_CreatePBEParams(SECItem *salt, SECItem *pwd, unsigned int iterations)
{
    CK_PBE_PARAMS *pbe_params = NULL;
    SECItem *paramRV = NULL;

    paramRV = SECITEM_AllocItem(NULL, NULL, sizeof(CK_PBE_PARAMS));
    if (!paramRV) {
        goto loser;
    }
    /* init paramRV->data with zeros. SECITEM_AllocItem does not do it */
    PORT_Memset(paramRV->data, 0, sizeof(CK_PBE_PARAMS));

    pbe_params = (CK_PBE_PARAMS *)paramRV->data;
    pbe_params->pPassword = (CK_CHAR_PTR)PORT_ZAlloc(pwd->len);
    if (!pbe_params->pPassword) {
        goto loser;
    }
    if (pwd->data) {
        PORT_Memcpy(pbe_params->pPassword, pwd->data, pwd->len);
    }
    pbe_params->ulPasswordLen = pwd->len;

    pbe_params->pSalt = (CK_CHAR_PTR)PORT_ZAlloc(salt->len);
    if (!pbe_params->pSalt) {
        goto loser;
    }
    PORT_Memcpy(pbe_params->pSalt, salt->data, salt->len);
    pbe_params->ulSaltLen = salt->len;

    pbe_params->ulIteration = (CK_ULONG)iterations;
    return paramRV;

loser:
    if (pbe_params)
        pk11_destroy_ck_pbe_params(pbe_params);
    if (paramRV)
        PORT_ZFree(paramRV, sizeof(SECItem));
    return NULL;
}

/*
 * public, deprecated.
 */
void
PK11_DestroyPBEParams(SECItem *pItem)
{
    if (pItem) {
        CK_PBE_PARAMS *params = (CK_PBE_PARAMS *)(pItem->data);
        if (params)
            pk11_destroy_ck_pbe_params(params);
        PORT_ZFree(pItem, sizeof(SECItem));
    }
}

/*
 * public, Partially supports PKCS5 V2 (some parameters are not controllable
 * through this interface). Use PK11_CreatePBEV2AlgorithmID() if you need
 * finer control these.
 */
SECAlgorithmID *
PK11_CreatePBEAlgorithmID(SECOidTag algorithm, int iteration, SECItem *salt)
{
    SECAlgorithmID *algid = NULL;
    algid = sec_pkcs5CreateAlgorithmID(algorithm,
                                       SEC_OID_UNKNOWN, SEC_OID_UNKNOWN, NULL,
                                       0, salt, iteration);
    return algid;
}

/*
 * public, fully support pkcs5v2.
 */
SECAlgorithmID *
PK11_CreatePBEV2AlgorithmID(SECOidTag pbeAlgTag, SECOidTag cipherAlgTag,
                            SECOidTag prfAlgTag, int keyLength, int iteration,
                            SECItem *salt)
{
    SECAlgorithmID *algid = NULL;
    algid = sec_pkcs5CreateAlgorithmID(pbeAlgTag, cipherAlgTag, prfAlgTag,
                                       NULL, keyLength, salt, iteration);
    return algid;
}

/*
 * private.
 */
PK11SymKey *
pk11_RawPBEKeyGenWithKeyType(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                             SECItem *params, CK_KEY_TYPE keyType, int keyLen,
                             SECItem *pwitem, void *wincx)
{
    CK_ULONG pwLen;
    /* do some sanity checks */
    if ((params == NULL) || (params->data == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if (type == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return NULL;
    }

    /* set the password pointer in the parameters... */
    if (type == CKM_PKCS5_PBKD2) {
        CK_PKCS5_PBKD2_PARAMS *pbev2_params;
        if (params->len < sizeof(CK_PKCS5_PBKD2_PARAMS)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
        }
        pbev2_params = (CK_PKCS5_PBKD2_PARAMS *)params->data;
        pbev2_params->pPassword = pwitem->data;
        pwLen = pwitem->len;
        pbev2_params->ulPasswordLen = &pwLen;
    } else {
        CK_PBE_PARAMS *pbe_params;
        if (params->len < sizeof(CK_PBE_PARAMS)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
        }
        pbe_params = (CK_PBE_PARAMS *)params->data;
        pbe_params->pPassword = pwitem->data;
        pbe_params->ulPasswordLen = pwitem->len;
    }

    /* generate the key (and sometimes the IV as a side effect...) */
    return pk11_TokenKeyGenWithFlagsAndKeyType(slot, type, params, keyType,
                                               keyLen, NULL,
                                               CKF_SIGN | CKF_ENCRYPT | CKF_DECRYPT | CKF_UNWRAP | CKF_WRAP,
                                               0, wincx);
}

/*
 * public, deprecated. use PK11_PBEKeyGen instead.
 */
PK11SymKey *
PK11_RawPBEKeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *mech,
                  SECItem *pwitem, PRBool faulty3DES, void *wincx)
{
    if (faulty3DES && (type == CKM_NSS_PBE_SHA1_TRIPLE_DES_CBC)) {
        type = CKM_NSS_PBE_SHA1_FAULTY_3DES_CBC;
    }
    return pk11_RawPBEKeyGenWithKeyType(slot, type, mech, -1, 0, pwitem, wincx);
}

/*
 * pubic, supports pkcs5 v2.
 *
 * Create symkey from a PBE key. The algid can be created with
 *  PK11_CreatePBEV2AlgorithmID and PK11_CreatePBEAlgorithmID, or by
 *  extraction of der data.
 */
PK11SymKey *
PK11_PBEKeyGen(PK11SlotInfo *slot, SECAlgorithmID *algid, SECItem *pwitem,
               PRBool faulty3DES, void *wincx)
{
    CK_MECHANISM_TYPE type;
    SECItem *param = NULL;
    PK11SymKey *symKey = NULL;
    SECOidTag pbeAlg;
    CK_KEY_TYPE keyType = -1;
    int keyLen = 0;

    pbeAlg = SECOID_GetAlgorithmTag(algid);
    /* if we're using PKCS5v2, extract the additional information we need
     * (key length, key type, and pbeAlg). */
    if (sec_pkcs5_is_algorithm_v2_pkcs5_algorithm(pbeAlg)) {
        CK_MECHANISM_TYPE cipherMech;
        sec_pkcs5V2Parameter *pbeV2_param;

        pbeV2_param = sec_pkcs5_v2_get_v2_param(NULL, algid);
        if (pbeV2_param == NULL) {
            return NULL;
        }
        cipherMech = PK11_AlgtagToMechanism(
            SECOID_GetAlgorithmTag(&pbeV2_param->cipherAlgId));
        pbeAlg = SECOID_GetAlgorithmTag(&pbeV2_param->pbeAlgId);
        param = PK11_ParamFromAlgid(&pbeV2_param->pbeAlgId);
        sec_pkcs5_v2_destroy_v2_param(pbeV2_param);
        keyLen = SEC_PKCS5GetKeyLength(algid);
        if (keyLen == -1) {
            keyLen = 0;
        }
        keyType = PK11_GetKeyType(cipherMech, keyLen);
    } else {
        param = PK11_ParamFromAlgid(algid);
    }

    if (param == NULL) {
        goto loser;
    }

    type = PK11_AlgtagToMechanism(pbeAlg);
    if (type == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }
    if (faulty3DES && (type == CKM_NSS_PBE_SHA1_TRIPLE_DES_CBC)) {
        type = CKM_NSS_PBE_SHA1_FAULTY_3DES_CBC;
    }
    symKey = pk11_RawPBEKeyGenWithKeyType(slot, type, param, keyType, keyLen,
                                          pwitem, wincx);

loser:
    if (param) {
        SECITEM_ZfreeItem(param, PR_TRUE);
    }
    return symKey;
}

/*
 * public, supports pkcs5v2
 */
SECItem *
PK11_GetPBEIV(SECAlgorithmID *algid, SECItem *pwitem)
{
    return SEC_PKCS5GetIV(algid, pwitem, PR_FALSE);
}

CK_MECHANISM_TYPE
pk11_GetPBECryptoMechanism(SECAlgorithmID *algid, SECItem **param,
                           SECItem *pbe_pwd, PRBool faulty3DES)
{
    int keyLen = 0;
    SECOidTag algTag = SEC_PKCS5GetCryptoAlgorithm(algid);
    CK_MECHANISM_TYPE mech = PK11_AlgtagToMechanism(algTag);
    CK_MECHANISM_TYPE returnedMechanism = CKM_INVALID_MECHANISM;
    SECItem *iv = NULL;

    if (mech == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }
    if (PK11_GetIVLength(mech)) {
        iv = SEC_PKCS5GetIV(algid, pbe_pwd, faulty3DES);
        if (iv == NULL) {
            goto loser;
        }
    }

    keyLen = SEC_PKCS5GetKeyLength(algid);

    *param = pk11_ParamFromIVWithLen(mech, iv, keyLen);
    if (*param == NULL) {
        goto loser;
    }
    returnedMechanism = mech;

loser:
    if (iv) {
        SECITEM_FreeItem(iv, PR_TRUE);
    }
    return returnedMechanism;
}

/*
 * Public, supports pkcs5 v2
 *
 * Get the crypto mechanism directly from the pbe algorithmid.
 *
 * It's important to go directly from the algorithm id so that we can
 * handle both the PKCS #5 v1, PKCS #12, and PKCS #5 v2 cases.
 *
 * This function returns both the mechanism and the parameter for the mechanism.
 * The caller is responsible for freeing the parameter.
 */
CK_MECHANISM_TYPE
PK11_GetPBECryptoMechanism(SECAlgorithmID *algid, SECItem **param,
                           SECItem *pbe_pwd)
{
    return pk11_GetPBECryptoMechanism(algid, param, pbe_pwd, PR_FALSE);
}
