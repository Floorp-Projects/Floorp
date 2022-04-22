/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "p12plcy.h"
#include "secoid.h"
#include "secport.h"
#include "secpkcs5.h"
#include "secerr.h"

#define PKCS12_NULL 0x0000

typedef struct pkcs12SuiteMapStr {
    SECOidTag algTag;
    unsigned int keyLengthBits; /* in bits */
    unsigned long suite;
    PRBool allowed;
    PRBool preferred;
} pkcs12SuiteMap;

static pkcs12SuiteMap pkcs12SuiteMaps[] = {
    { SEC_OID_RC4, 40, PKCS12_RC4_40, PR_FALSE, PR_FALSE },
    { SEC_OID_RC4, 128, PKCS12_RC4_128, PR_FALSE, PR_FALSE },
    { SEC_OID_RC2_CBC, 40, PKCS12_RC2_CBC_40, PR_FALSE, PR_TRUE },
    { SEC_OID_RC2_CBC, 128, PKCS12_RC2_CBC_128, PR_FALSE, PR_FALSE },
    { SEC_OID_DES_CBC, 64, PKCS12_DES_56, PR_FALSE, PR_FALSE },
    { SEC_OID_DES_EDE3_CBC, 192, PKCS12_DES_EDE3_168, PR_FALSE, PR_FALSE },
    { SEC_OID_AES_128_CBC, 128, PKCS12_AES_CBC_128, PR_FALSE, PR_FALSE },
    { SEC_OID_AES_192_CBC, 192, PKCS12_AES_CBC_192, PR_FALSE, PR_FALSE },
    { SEC_OID_AES_256_CBC, 256, PKCS12_AES_CBC_256, PR_FALSE, PR_FALSE },
    { SEC_OID_UNKNOWN, 0, PKCS12_NULL, PR_FALSE, PR_FALSE },
    { SEC_OID_UNKNOWN, 0, 0L, PR_FALSE, PR_FALSE }
};

/* determine if algid is an algorithm which is allowed */
static PRBool
sec_PKCS12Allowed(SECOidTag alg)
{
    PRUint32 policy;
    SECStatus rv;

    rv = NSS_GetAlgorithmPolicy(alg, &policy);
    if (rv != SECSuccess) {
        return PR_FALSE;
    }
    if (policy & NSS_USE_ALG_IN_PKCS12) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool
SEC_PKCS12DecryptionAllowed(SECAlgorithmID *algid)
{
    SECOidTag algId;

    algId = SEC_PKCS5GetCryptoAlgorithm(algid);
    if (algId == SEC_OID_UNKNOWN) {
        return PR_FALSE;
    }
    return sec_PKCS12Allowed(algId);
}

/* is any encryption allowed? */
PRBool
SEC_PKCS12IsEncryptionAllowed(void)
{
    int i;

    for (i = 0; pkcs12SuiteMaps[i].algTag != SEC_OID_UNKNOWN; i++) {
        /* we're going to return true here if any of the traditional
         * algorithms are enabled */
        if (sec_PKCS12Allowed(pkcs12SuiteMaps[i].algTag)) {
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

/* keep the traditional enable/disable for old ciphers so old applications
 * continue to work. This only works for the traditional pkcs12 values,
 * you need to use NSS_SetAlgorithmPolicy directly for other ciphers. */
SECStatus
SEC_PKCS12EnableCipher(long which, int on)
{
    int i;
    PRUint32 set = on ? NSS_USE_ALG_IN_PKCS12 : 0;
    PRUint32 clear = on ? 0 : NSS_USE_ALG_IN_PKCS12;

    for (i = 0; pkcs12SuiteMaps[i].suite != 0L; i++) {
        if (pkcs12SuiteMaps[i].suite == (unsigned long)which) {
            return NSS_SetAlgorithmPolicy(pkcs12SuiteMaps[i].algTag, set, clear);
        }
    }
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}

SECStatus
SEC_PKCS12SetPreferredCipher(long which, int on)
{
    /* nothing looked at the preferences in the suite maps, so this function
     * has always been a noop */
    return SECSuccess;
}
