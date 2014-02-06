/*
 * Table enumerating all implemented cipher suites
 * Part of public API.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslproto.h"

/*
 * The ordering of cipher suites in this table must match the ordering in
 * the cipherSuites table in ssl3con.c.
 *
 * If new ECC cipher suites are added, also update the ssl3CipherSuite arrays
 * in ssl3ecc.c.
 *
 * Finally, update the ssl_V3_SUITES_IMPLEMENTED macro in sslimpl.h.
 *
 * The ordering is as follows:
 *    * No-encryption cipher suites last
 *    * Export/weak/obsolete cipher suites before no-encryption cipher suites
 *    * Order by key exchange algorithm: ECDHE, then DHE, then ECDH, RSA.
 *    * Within key agreement sections, order by symmetric encryption algorithm:
 *      AES-128, then Camellia-128, then AES-256, then Camellia-256, then SEED,
 *      then FIPS-3DES, then 3DES, then RC4. AES is commonly accepted as a
 *      strong cipher internationally, and is often hardware-accelerated.
 *      Camellia also has wide international support across standards
 *      organizations. SEED is only recommended by the Korean government. 3DES
 *      only provides 112 bits of security. RC4 is now deprecated or forbidden
 *      by many standards organizations.
 *    * Within symmetric algorithm sections, order by message authentication
 *      algorithm: GCM, then HMAC-SHA1, then HMAC-SHA256, then HMAC-MD5.
 *    * Within message authentication algorithm sections, order by asymmetric
 *      signature algorithm: ECDSA, then RSA, then DSS.
 *
 * Exception: Because some servers ignore the high-order byte of the cipher
 * suite ID, we must be careful about adding cipher suites with IDs larger
 * than 0x00ff; see bug 946147. For these broken servers, the first four cipher
 * suites, with the MSB zeroed, look like:
 *      TLS_KRB5_EXPORT_WITH_RC4_40_MD5 { 0x00,0x2B }
 *      TLS_RSA_WITH_AES_128_CBC_SHA { 0x00,0x2F }
 *      TLS_RSA_WITH_3DES_EDE_CBC_SHA { 0x00,0x0A }
 *      TLS_RSA_WITH_DES_CBC_SHA { 0x00,0x09 }
 * The broken server only supports the third and fourth ones and will select
 * the third one.
 */
const PRUint16 SSL_ImplementedCiphers[] = {
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    /* TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA must appear before
     * TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA to work around bug 946147.
     */
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
#endif /* NSS_ENABLE_ECC */

    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_DSS_WITH_RC4_128_SHA,

#ifdef NSS_ENABLE_ECC
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDH_RSA_WITH_RC4_128_SHA,
#endif /* NSS_ENABLE_ECC */

    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_RSA_WITH_SEED_CBC_SHA,
    SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,

    /* 56-bit DES "domestic" cipher suites */
    SSL_DHE_RSA_WITH_DES_CBC_SHA,
    SSL_DHE_DSS_WITH_DES_CBC_SHA,
    SSL_RSA_FIPS_WITH_DES_CBC_SHA,
    SSL_RSA_WITH_DES_CBC_SHA,

    /* export ciphersuites with 1024-bit public key exchange keys */
    TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,
    TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA,

    /* export ciphersuites with 512-bit public key exchange keys */
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,

    /* ciphersuites with no encryption */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_NULL_SHA,
    TLS_ECDH_RSA_WITH_NULL_SHA,
    TLS_ECDH_ECDSA_WITH_NULL_SHA,
#endif /* NSS_ENABLE_ECC */
    SSL_RSA_WITH_NULL_SHA,
    TLS_RSA_WITH_NULL_SHA256,
    SSL_RSA_WITH_NULL_MD5,

    /* SSL2 cipher suites. */
    SSL_EN_RC4_128_WITH_MD5,
    SSL_EN_RC2_128_CBC_WITH_MD5,
    SSL_EN_DES_192_EDE3_CBC_WITH_MD5,  /* actually 112, not 192 */
    SSL_EN_DES_64_CBC_WITH_MD5,
    SSL_EN_RC4_128_EXPORT40_WITH_MD5,
    SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,

    0

};

const PRUint16 SSL_NumImplementedCiphers = 
    (sizeof SSL_ImplementedCiphers) / (sizeof SSL_ImplementedCiphers[0]) - 1;

const PRUint16 *
SSL_GetImplementedCiphers(void)
{
    return SSL_ImplementedCiphers;
}

PRUint16
SSL_GetNumImplementedCiphers(void)
{
    return SSL_NumImplementedCiphers;
}
