/*
 * Table enumerating all implemented cipher suites
 * Part of public API.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: sslenum.c,v 1.1 2000/03/31 19:34:53 relyea%netscape.com Exp $
 */

#include "ssl.h"
#include "sslproto.h"

const PRUint16 SSL_ImplementedCiphers[] = {

    SSL_RSA_WITH_NULL_MD5,
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
    SSL_RSA_WITH_RC4_128_MD5,
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
    SSL_RSA_WITH_DES_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,

    SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA,
    SSL_FORTEZZA_DMS_WITH_NULL_SHA,
    SSL_FORTEZZA_DMS_WITH_RC4_128_SHA,

    TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA,
    TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,

    /* SSL2 cipher suites. */
    SSL_EN_RC4_128_WITH_MD5,
    SSL_EN_RC4_128_EXPORT40_WITH_MD5,
    SSL_EN_RC2_128_CBC_WITH_MD5,
    SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,
    SSL_EN_DES_64_CBC_WITH_MD5,
    SSL_EN_DES_192_EDE3_CBC_WITH_MD5,

    /* Netscape "experimental" cipher suites. */
    SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_FIPS_WITH_DES_CBC_SHA,

    0

};

const PRUint16 SSL_NumImplementedCiphers = 
    (sizeof SSL_ImplementedCiphers) / (sizeof SSL_ImplementedCiphers[0]) - 1;

