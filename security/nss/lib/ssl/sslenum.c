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
 * The ciphers are listed in the following order:
 * - stronger ciphers before weaker ciphers
 * - national ciphers before international ciphers
 * - faster ciphers before slower ciphers
 *
 * National ciphers such as Camellia are listed before international ciphers
 * such as AES and RC4 to allow servers that prefer Camellia to negotiate
 * Camellia without having to disable AES and RC4, which are needed for
 * interoperability with clients that don't yet implement Camellia.
 *
 * The ordering of cipher suites in this table must match the ordering in
 * the cipherSuites table in ssl3con.c.
 *
 * If new ECC cipher suites are added, also update the ssl3CipherSuite arrays
 * in ssl3ecc.c.
 *
 * Finally, update the ssl_V3_SUITES_IMPLEMENTED macro in sslimpl.h.
 */
const PRUint16 SSL_ImplementedCiphers[] = {
    /* AES-GCM */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
#endif /* NSS_ENABLE_ECC */
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_128_GCM_SHA256,

    /* 256-bit */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
#ifdef NSS_ENABLE_ECC
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA256,

    /* 128-bit */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
#endif /* NSS_ENABLE_ECC */
    TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_DSS_WITH_RC4_128_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
#ifdef NSS_ENABLE_ECC
    TLS_ECDH_RSA_WITH_RC4_128_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    TLS_RSA_WITH_SEED_CBC_SHA,
    TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA256,

    /* 112-bit 3DES */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
#ifdef NSS_ENABLE_ECC
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,

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
