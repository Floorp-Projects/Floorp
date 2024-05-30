/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Although this is not an exported header file, code which uses elliptic
 * curve point operations will need to include it. */

#ifndef __ecl_h_
#define __ecl_h_

#include "blapi.h"
#include "ecl-exp.h"
#include "eclt.h"

SECStatus ec_Curve25519_pt_mul(SECItem *X, SECItem *k, SECItem *P);
SECStatus ec_Curve25519_pt_validate(const SECItem *px);
SECStatus ec_Curve25519_scalar_validate(const SECItem *scalar);

SECStatus ec_secp256r1_pt_mul(SECItem *X, SECItem *k, SECItem *P);
SECStatus ec_secp256r1_pt_validate(const SECItem *px);
SECStatus ec_secp256r1_scalar_validate(const SECItem *scalar);

SECStatus ec_secp256r1_sign_digest(ECPrivateKey *key, SECItem *signature,
                                   const SECItem *digest, const unsigned char *kb,
                                   const unsigned int kblen);
SECStatus ec_secp256r1_verify_digest(ECPublicKey *key, const SECItem *signature,
                                     const SECItem *digest);

SECStatus ec_secp521r1_pt_mul(SECItem *X, SECItem *k, SECItem *P);
SECStatus ec_secp521r1_pt_validate(const SECItem *px);
SECStatus ec_secp521r1_scalar_validate(const SECItem *scalar);

SECStatus ec_secp521r1_sign_digest(ECPrivateKey *key, SECItem *signature,
                                   const SECItem *digest, const unsigned char *kb,
                                   const unsigned int kblen);
SECStatus ec_secp521r1_verify_digest(ECPublicKey *key, const SECItem *signature,
                                     const SECItem *digest);

SECStatus ec_secp384r1_pt_mul(SECItem *X, SECItem *k, SECItem *P);
SECStatus ec_secp384r1_pt_validate(const SECItem *px);
SECStatus ec_secp384r1_scalar_validate(const SECItem *scalar);

SECStatus ec_secp384r1_sign_digest(ECPrivateKey *key, SECItem *signature,
                                   const SECItem *digest, const unsigned char *kb,
                                   const unsigned int kblen);
SECStatus ec_secp384r1_verify_digest(ECPublicKey *key, const SECItem *signature,
                                     const SECItem *digest);

#endif /* __ecl_h_ */
