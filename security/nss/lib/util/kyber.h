/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef KYBER_UTIL_H
#define KYBER_UTIL_H

#define KYBER768_PUBLIC_KEY_BYTES 1184U
#define KYBER768_PRIVATE_KEY_BYTES 2400U
#define KYBER768_CIPHERTEXT_BYTES 1088U

#define KYBER_SHARED_SECRET_BYTES 32U
#define KYBER_KEYPAIR_COIN_BYTES 64U
#define KYBER_ENC_COIN_BYTES 32U

typedef enum {
    params_kyber_invalid,

    /*
     * The Kyber768 parameters specified in version 3.02 of the NIST submission
     * https://pq-crystals.org/kyber/data/kyber-specification-round3-20210804.pdf
     */
    params_kyber768_round3,

    /*
     * Identical to params_kyber768_round3 except that this parameter set allows
     * the use of a seed in `Kyber_Encapsulate` for testing.
     */
    params_kyber768_round3_test_mode,
} KyberParams;

#endif /* KYBER_UTIL_H */
