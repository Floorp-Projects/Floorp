/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This program demonstrates the use of SSL_GetCipherSuiteInfo to avoid
 * all compiled-in knowledge of SSL cipher suites.
 *
 * Try: ./listsuites | grep -v : | sort -b +4rn -5 +1 -2 +2 -3 +3 -4 +5r -6
 */

#include <errno.h>
#include <stdio.h>
#include "nss.h"
#include "secport.h"
#include "secutil.h"
#include "ssl.h"

int
main(int argc, char **argv)
{
    const PRUint16 *cipherSuites = SSL_ImplementedCiphers;
    int i;
    int errCount = 0;
    SECStatus rv;
    PRErrorCode err;
    char *certDir = NULL;

    /* load policy from $SSL_DIR/pkcs11.txt, for testing */
    certDir = SECU_DefaultSSLDir();
    if (certDir) {
        rv = NSS_Init(certDir);
    } else {
        rv = NSS_NoDB_Init(NULL);
    }
    if (rv != SECSuccess) {
        err = PR_GetError();
        ++errCount;
        fprintf(stderr, "NSS_Init failed: %s\n", PORT_ErrorToString(err));
        goto out;
    }

    /* apply policy */
    rv = NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, NSS_USE_POLICY_IN_SSL, 0);
    if (rv != SECSuccess) {
        err = PR_GetError();
        ++errCount;
        fprintf(stderr, "NSS_SetAlgorithmPolicy failed: %s\n",
                PORT_ErrorToString(err));
        goto out;
    }

    /* update the default cipher suites according to the policy */
    rv = SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);
    if (rv != SECSuccess) {
        err = PR_GetError();
        ++errCount;
        fprintf(stderr, "SSL_OptionSetDefault failed: %s\n",
                PORT_ErrorToString(err));
        goto out;
    }

    fputs("This version of libSSL supports these cipher suites:\n\n", stdout);

    /* disable all the SSL3 cipher suites */
    for (i = 0; i < SSL_NumImplementedCiphers; i++) {
        PRUint16 suite = cipherSuites[i];
        PRBool enabled;
        SSLCipherSuiteInfo info;

        rv = SSL_CipherPrefGetDefault(suite, &enabled);
        if (rv != SECSuccess) {
            err = PR_GetError();
            ++errCount;
            fprintf(stderr,
                    "SSL_CipherPrefGetDefault didn't like value 0x%04x (i = %d): %s\n",
                    suite, i, PORT_ErrorToString(err));
            continue;
        }
        rv = SSL_GetCipherSuiteInfo(suite, &info, (int)(sizeof info));
        if (rv != SECSuccess) {
            err = PR_GetError();
            ++errCount;
            fprintf(stderr,
                    "SSL_GetCipherSuiteInfo didn't like value 0x%04x (i = %d): %s\n",
                    suite, i, PORT_ErrorToString(err));
            continue;
        }
        fprintf(stdout,
                "%s:\n" /* up to 37 spaces  */
                "  0x%04hx %-5s %-5s %-8s %3hd %-6s %-8s %-4s Domestic %-11s\n",
                info.cipherSuiteName, info.cipherSuite,
                info.keaTypeName, info.authAlgorithmName, info.symCipherName,
                info.effectiveKeyBits, info.macAlgorithmName,
                enabled ? "Enabled" : "Disabled",
                info.isFIPS ? "FIPS" : "",
                info.nonStandard ? "nonStandard" : "");
    }

out:
    rv = NSS_Shutdown();
    if (rv != SECSuccess) {
        err = PR_GetError();
        ++errCount;
        fprintf(stderr, "NSS_Shutdown failed: %s\n", PORT_ErrorToString(err));
    }

    return errCount;
}
