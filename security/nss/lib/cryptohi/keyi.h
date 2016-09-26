/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _KEYI_H_
#define _KEYI_H_

SEC_BEGIN_PROTOS
/* NSS private functions */
/* map an oid to a keytype... actually this function and it's converse
 *  are good candidates for public functions..  */
KeyType seckey_GetKeyType(SECOidTag pubKeyOid);

/* extract the 'encryption' (could be signing) and hash oids from and
 * algorithm, key and parameters (parameters is the parameters field
 * of a algorithm ID structure (SECAlgorithmID)*/
SECStatus sec_DecodeSigAlg(const SECKEYPublicKey *key, SECOidTag sigAlg,
                           const SECItem *param, SECOidTag *encalg, SECOidTag *hashalg);

/*
 * Set the point encoding of a SECKEYPublicKey from the OID.
 * This has to be called on any SECKEYPublicKey holding a SECKEYECPublicKey
 * before it can be used. The encoding is used to dermine the public key size.
 */
SECStatus seckey_SetPointEncoding(PLArenaPool *arena, SECKEYPublicKey *pubKey);

SEC_END_PROTOS

#endif /* _KEYHI_H_ */
