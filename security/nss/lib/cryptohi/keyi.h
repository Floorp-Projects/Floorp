/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: keyi.h,v 1.2 2012/04/25 14:49:41 gerv%gerv.net Exp $ */

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

SEC_END_PROTOS

#endif /* _KEYHI_H_ */
