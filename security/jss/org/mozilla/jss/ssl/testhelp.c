/* 
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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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
 */
#include "cert.h"
#include "certt.h"
#include "base64.h"
#include "nspr.h"
#include "private/pprthred.h"
#include "ssl.h"
#include "sslproto.h"
#include "pk11func.h"
#include "key.h"
#include "secrng.h"
#include "secmod.h"
#include "jssl.h"

#include <jni.h>

#include <jssutil.h>

#define throwException(x)	/* do nothing */

static int cipherSuites[] = {
    SSL_RSA_WITH_RC4_128_MD5,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_WITH_DES_CBC_SHA,
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
    SSL_RSA_WITH_NULL_MD5,
    0
};

static void 
JSSL_SetSSLPolicies()
{
    int i;

    /* enable all the SSL2 cipher suites */
    for (i = SSL_EN_RC4_128_WITH_MD5;
         i <= SSL_EN_DES_192_EDE3_CBC_WITH_MD5; ++i) {
        SSL_SetPolicy(   i, SSL_ALLOWED);
        SSL_EnableCipher(i, SSL_ALLOWED);
    }

    /* enable all the SSL3 cipher suites */
    for (i = 0; cipherSuites[i] != 0;  ++i) {
        SSL_SetPolicy(   cipherSuites[i], SSL_ALLOWED);
        SSL_EnableCipher(cipherSuites[i], SSL_ALLOWED);
    }
}
