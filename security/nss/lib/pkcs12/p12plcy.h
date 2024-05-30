/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _P12PLCY_H_
#define _P12PLCY_H_

#include "secoid.h"
#include "ciferfam.h"

SEC_BEGIN_PROTOS

/* is this encryption algorithm allowed in PKCS #12 by policy? */
/* pbeAlg is either a full PBE for pkcsv5v1 and pkcs12pbe; or
 *                  a cipher alg for pkcs5v2,
 * hmacAlg is an HMAC algorith. Must be included for pkcs5v2
 *                   and is ignored if pbeAlg is pkcs5v2 or pkcs12pbe */
extern PRBool SEC_PKCS12CipherAllowed(SECOidTag pbeAlg, SECOidTag hmacAlg);

/* for the algid specified, can we decrypt it ? 
 * both encryption and hash used in the hmac must be enabled.
 * legacy/decrypt is sufficient */
extern PRBool SEC_PKCS12DecryptionAllowed(SECAlgorithmID *algid);

/* for integrity, we mark if we are signing or verifying in the call. Oid
 * is the hash oid */
extern PRBool SEC_PKCS12IntegrityHashAllowed(SECOidTag hashAlg, PRBool verify);

/* is encryption allowed? */
extern PRBool SEC_PKCS12IsEncryptionAllowed(void);

/* enable a cipher for encryption/decryption */
extern SECStatus SEC_PKCS12EnableCipher(long which, int on);

/* return the preferred cipher for encryption */
extern SECStatus SEC_PKCS12SetPreferredCipher(long which, int on);

SEC_END_PROTOS
#endif
