/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file was generated from
 *   https://github.com/pq-crystals/kyber/commit/e0d1c6ff
 *
 * Files from that repository are listed here surrounded by
 * "* begin: [file] *" and "* end: [file] *" comments.
 *
 * The following changes have been made:
 *  - include guards have been removed,
 *  - include directives have been removed,
 *  - "#ifdef KYBER90S" blocks have been evaluated with "KYBER90S" undefined,
 *  - functions outside of kem.c have been made static.
*/

/** begin: ref/LICENSE **
Public Domain (https://creativecommons.org/share-your-work/public-domain/cc0/);
or Apache 2.0 License (https://www.apache.org/licenses/LICENSE-2.0.html).

For Keccak and AES we are using public-domain
code from sources and by authors listed in
comments on top of the respective files.
** end: ref/LICENSE **/

/** begin: ref/AUTHORS **
Joppe Bos,
Léo Ducas,
Eike Kiltz,
Tancrède Lepoint,
Vadim Lyubashevsky,
John Schanck,
Peter Schwabe,
Gregor Seiler,
Damien Stehlé
** end: ref/AUTHORS **/

#ifndef KYBER_PQCRYSTALS_REF_H
#define KYBER_PQCRYSTALS_REF_H

/** begin: ref/api.h **/
#include <stdint.h>

#define pqcrystals_kyber512_SECRETKEYBYTES 1632
#define pqcrystals_kyber512_PUBLICKEYBYTES 800
#define pqcrystals_kyber512_CIPHERTEXTBYTES 768
#define pqcrystals_kyber512_KEYPAIRCOINBYTES 64
#define pqcrystals_kyber512_ENCCOINBYTES 32
#define pqcrystals_kyber512_BYTES 32

#define pqcrystals_kyber512_ref_SECRETKEYBYTES pqcrystals_kyber512_SECRETKEYBYTES
#define pqcrystals_kyber512_ref_PUBLICKEYBYTES pqcrystals_kyber512_PUBLICKEYBYTES
#define pqcrystals_kyber512_ref_CIPHERTEXTBYTES pqcrystals_kyber512_CIPHERTEXTBYTES
#define pqcrystals_kyber512_ref_KEYPAIRCOINBYTES pqcrystals_kyber512_KEYPAIRCOINBYTES
#define pqcrystals_kyber512_ref_ENCCOINBYTES pqcrystals_kyber512_ENCCOINBYTES
#define pqcrystals_kyber512_ref_BYTES pqcrystals_kyber512_BYTES

int pqcrystals_kyber512_ref_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
int pqcrystals_kyber512_ref_keypair(uint8_t *pk, uint8_t *sk);
int pqcrystals_kyber512_ref_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
int pqcrystals_kyber512_ref_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int pqcrystals_kyber512_ref_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#define pqcrystals_kyber512_90s_ref_SECRETKEYBYTES pqcrystals_kyber512_SECRETKEYBYTES
#define pqcrystals_kyber512_90s_ref_PUBLICKEYBYTES pqcrystals_kyber512_PUBLICKEYBYTES
#define pqcrystals_kyber512_90s_ref_CIPHERTEXTBYTES pqcrystals_kyber512_CIPHERTEXTBYTES
#define pqcrystals_kyber512_90s_ref_KEYPAIRCOINBYTES pqcrystals_kyber512_KEYPAIRCOINBYTES
#define pqcrystals_kyber512_90s_ref_ENCCOINBYTES pqcrystals_kyber512_ENCCOINBYTES
#define pqcrystals_kyber512_90s_ref_BYTES pqcrystals_kyber512_BYTES

int pqcrystals_kyber512_90s_ref_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
int pqcrystals_kyber512_90s_ref_keypair(uint8_t *pk, uint8_t *sk);
int pqcrystals_kyber512_90s_ref_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
int pqcrystals_kyber512_90s_ref_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int pqcrystals_kyber512_90s_ref_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#define pqcrystals_kyber768_SECRETKEYBYTES 2400
#define pqcrystals_kyber768_PUBLICKEYBYTES 1184
#define pqcrystals_kyber768_CIPHERTEXTBYTES 1088
#define pqcrystals_kyber768_KEYPAIRCOINBYTES 64
#define pqcrystals_kyber768_ENCCOINBYTES 32
#define pqcrystals_kyber768_BYTES 32

#define pqcrystals_kyber768_ref_SECRETKEYBYTES pqcrystals_kyber768_SECRETKEYBYTES
#define pqcrystals_kyber768_ref_PUBLICKEYBYTES pqcrystals_kyber768_PUBLICKEYBYTES
#define pqcrystals_kyber768_ref_CIPHERTEXTBYTES pqcrystals_kyber768_CIPHERTEXTBYTES
#define pqcrystals_kyber768_ref_KEYPAIRCOINBYTES pqcrystals_kyber768_KEYPAIRCOINBYTES
#define pqcrystals_kyber768_ref_ENCCOINBYTES pqcrystals_kyber768_ENCCOINBYTES
#define pqcrystals_kyber768_ref_BYTES pqcrystals_kyber768_BYTES

int pqcrystals_kyber768_ref_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
int pqcrystals_kyber768_ref_keypair(uint8_t *pk, uint8_t *sk);
int pqcrystals_kyber768_ref_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
int pqcrystals_kyber768_ref_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int pqcrystals_kyber768_ref_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#define pqcrystals_kyber768_90s_ref_SECRETKEYBYTES pqcrystals_kyber768_SECRETKEYBYTES
#define pqcrystals_kyber768_90s_ref_PUBLICKEYBYTES pqcrystals_kyber768_PUBLICKEYBYTES
#define pqcrystals_kyber768_90s_ref_CIPHERTEXTBYTES pqcrystals_kyber768_CIPHERTEXTBYTES
#define pqcrystals_kyber768_90s_ref_KEYPAIRCOINBYTES pqcrystals_kyber768_KEYPAIRCOINBYTES
#define pqcrystals_kyber768_90s_ref_ENCCOINBYTES pqcrystals_kyber768_ENCCOINBYTES
#define pqcrystals_kyber768_90s_ref_BYTES pqcrystals_kyber768_BYTES

int pqcrystals_kyber768_90s_ref_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
int pqcrystals_kyber768_90s_ref_keypair(uint8_t *pk, uint8_t *sk);
int pqcrystals_kyber768_90s_ref_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
int pqcrystals_kyber768_90s_ref_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int pqcrystals_kyber768_90s_ref_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#define pqcrystals_kyber1024_SECRETKEYBYTES 3168
#define pqcrystals_kyber1024_PUBLICKEYBYTES 1568
#define pqcrystals_kyber1024_CIPHERTEXTBYTES 1568
#define pqcrystals_kyber1024_KEYPAIRCOINBYTES 64
#define pqcrystals_kyber1024_ENCCOINBYTES 32
#define pqcrystals_kyber1024_BYTES 32

#define pqcrystals_kyber1024_ref_SECRETKEYBYTES pqcrystals_kyber1024_SECRETKEYBYTES
#define pqcrystals_kyber1024_ref_PUBLICKEYBYTES pqcrystals_kyber1024_PUBLICKEYBYTES
#define pqcrystals_kyber1024_ref_CIPHERTEXTBYTES pqcrystals_kyber1024_CIPHERTEXTBYTES
#define pqcrystals_kyber1024_ref_KEYPAIRCOINBYTES pqcrystals_kyber1024_KEYPAIRCOINBYTES
#define pqcrystals_kyber1024_ref_ENCCOINBYTES pqcrystals_kyber1024_ENCCOINBYTES
#define pqcrystals_kyber1024_ref_BYTES pqcrystals_kyber1024_BYTES

int pqcrystals_kyber1024_ref_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
int pqcrystals_kyber1024_ref_keypair(uint8_t *pk, uint8_t *sk);
int pqcrystals_kyber1024_ref_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
int pqcrystals_kyber1024_ref_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int pqcrystals_kyber1024_ref_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#define pqcrystals_kyber1024_90s_ref_SECRETKEYBYTES pqcrystals_kyber1024_SECRETKEYBYTES
#define pqcrystals_kyber1024_90s_ref_PUBLICKEYBYTES pqcrystals_kyber1024_PUBLICKEYBYTES
#define pqcrystals_kyber1024_90s_ref_CIPHERTEXTBYTES pqcrystals_kyber1024_CIPHERTEXTBYTES
#define pqcrystals_kyber1024_90s_ref_KEYPAIRCOINBYTES pqcrystals_kyber1024_KEYPAIRCOINBYTES
#define pqcrystals_kyber1024_90s_ref_ENCCOINBYTES pqcrystals_kyber1024_ENCCOINBYTES
#define pqcrystals_kyber1024_90s_ref_BYTES pqcrystals_kyber1024_BYTES

int pqcrystals_kyber1024_90s_ref_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);
int pqcrystals_kyber1024_90s_ref_keypair(uint8_t *pk, uint8_t *sk);
int pqcrystals_kyber1024_90s_ref_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);
int pqcrystals_kyber1024_90s_ref_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int pqcrystals_kyber1024_90s_ref_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);
/** end: ref/api.h **/

#endif // KYBER_PQCRYSTALS_REF_H
