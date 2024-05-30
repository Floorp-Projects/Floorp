/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _KEYI_H_
#define _KEYI_H_
#include "secerr.h"

SEC_BEGIN_PROTOS
/* NSS private functions */
/* map an oid to a keytype... actually this function and it's converse
 *  are good candidates for public functions..  */
KeyType seckey_GetKeyType(SECOidTag pubKeyOid);

/*
 * Pulls the hash algorithm, signing algorithm, and key type out of a
 * composite algorithm.
 *
 * key: pointer to the public key. Should be NULL if called for a sign operation.
 * sigAlg: the composite algorithm to dissect.
 * hashalg: address of a SECOidTag which will be set with the hash algorithm.
 * encalg: address of a SECOidTag which will be set with the signing alg.
 * mechp: address of a PCKS #11 Mechanism which will be set to the
 *  combined hash/encrypt mechanism. If set to CKM_INVALID_MECHANISM, the code
 *  will fall back to external hashing.
 * mechparams: address of a SECItem will set to the parameters for the combined
 *  hash/encrypt mechanism.
 *
 * Returns: SECSuccess if the algorithm was acceptable, SECFailure if the
 *  algorithm was not found or was not a signing algorithm.
 */
SECStatus sec_DecodeSigAlg(const SECKEYPublicKey *key, SECOidTag sigAlg,
                           const SECItem *param, SECOidTag *encalg,
                           SECOidTag *hashalg, CK_MECHANISM_TYPE *mech,
                           SECItem *mechparams);

/* just get the 'encryption' oid from the combined signature oid */
SECOidTag sec_GetEncAlgFromSigAlg(SECOidTag sigAlg);

/* extract the RSA-PSS hash algorithms and salt length from
 * parameters, taking into account of the default implications.
 *
 * (parameters is the parameters field of a algorithm ID structure
 * (SECAlgorithmID)*/
SECStatus sec_DecodeRSAPSSParams(PLArenaPool *arena,
                                 const SECItem *params,
                                 SECOidTag *hashAlg,
                                 SECOidTag *maskHashAlg,
                                 unsigned long *saltLength);

/* convert the encoded RSA-PSS parameters into PKCS #11 mechanism parameters */
SECStatus sec_DecodeRSAPSSParamsToMechanism(PLArenaPool *arena,
                                            const SECItem *params,
                                            CK_RSA_PKCS_PSS_PARAMS *mech,
                                            SECOidTag *hashAlg);

SEC_END_PROTOS

#endif /* _KEYHI_H_ */
