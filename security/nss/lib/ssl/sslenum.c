/*
 * Table enumerating all implemented cipher suites
 * Part of public API.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Stephen Henson <stephen.henson@gemplus.com>
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* $Id: sslenum.c,v 1.18 2012/03/06 00:26:31 wtc%google.com Exp $ */

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
 */
const PRUint16 SSL_ImplementedCiphers[] = {
    /* 256-bit */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
#ifdef NSS_ENABLE_ECC
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA,

    /* 128-bit */
#ifdef NSS_ENABLE_ECC
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
#endif /* NSS_ENABLE_ECC */
    TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_DSS_WITH_RC4_128_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
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
