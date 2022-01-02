/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This program can be used to check the validity of a NSS crypto policy
 * configuration file, specified using a config= line.
 *
 * Exit codes:
 *  failure: 2
 *  warning: 1
 *  success: 0
 */

#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include "utilparst.h"
#include "nss.h"
#include "secport.h"
#include "secutil.h"
#include "secmod.h"
#include "ssl.h"
#include "prenv.h"

const char *sWarn = "WARN";
const char *sInfo = "INFO";

void
get_tls_info(SSLProtocolVariant protocolVariant, const char *display)
{
    SSLVersionRange vrange_supported, vrange_enabled;
    unsigned num_enabled = 0;
    PRBool failed = PR_FALSE;

    /* We assume SSL v2 is inactive, and therefore SSL_VersionRangeGetDefault
     * gives complete information. */
    if ((SSL_VersionRangeGetSupported(protocolVariant, &vrange_supported) != SECSuccess) ||
        (SSL_VersionRangeGetDefault(protocolVariant, &vrange_enabled) != SECSuccess) ||
        !vrange_enabled.min ||
        !vrange_enabled.max ||
        vrange_enabled.max < vrange_supported.min ||
        vrange_enabled.min > vrange_supported.max) {
        failed = PR_TRUE;
    } else {
        if (vrange_enabled.min < vrange_supported.min) {
            vrange_enabled.min = vrange_supported.min;
        }
        if (vrange_enabled.max > vrange_supported.max) {
            vrange_enabled.max = vrange_supported.max;
        }
        if (vrange_enabled.min > vrange_enabled.max) {
            failed = PR_TRUE;
        }
    }
    if (failed) {
        num_enabled = 0;
    } else {
        num_enabled = vrange_enabled.max - vrange_enabled.min + 1;
    }
    fprintf(stderr, "NSS-POLICY-%s: NUMBER-OF-%s-VERSIONS: %u\n",
            num_enabled ? sInfo : sWarn, display, num_enabled);
    if (!num_enabled) {
        PR_SetEnv("NSS_POLICY_WARN=1");
    }
}

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

int
main(int argc, char **argv)
{
    const PRUint16 *cipherSuites = SSL_ImplementedCiphers;
    int i;
    SECStatus rv;
    SECMODModule *module = NULL;
    char path[PATH_MAX];
    const char *filename;
    char moduleSpec[1024 + PATH_MAX];
    unsigned num_enabled = 0;
    int result = 0;
    int fullPathLen;

    if (argc != 2) {
        fprintf(stderr, "Syntax: nss-policy-check <path-to-policy-file>\n");
        result = 2;
        goto loser_no_shutdown;
    }

    fullPathLen = strlen(argv[1]);

    if (!fullPathLen || PR_Access(argv[1], PR_ACCESS_READ_OK) != PR_SUCCESS) {
        fprintf(stderr, "Error: cannot read file %s\n", argv[1]);
        result = 2;
        goto loser_no_shutdown;
    }

    if (fullPathLen >= PATH_MAX) {
        fprintf(stderr, "Error: filename parameter is too long\n");
        result = 2;
        goto loser_no_shutdown;
    }

    path[0] = 0;
    filename = argv[1] + fullPathLen - 1;
    while ((filename > argv[1]) && (*filename != NSSUTIL_PATH_SEPARATOR[0])) {
        filename--;
    }

    if (filename == argv[1]) {
        PORT_Strcpy(path, ".");
    } else {
        filename++; /* Go past the path separator. */
        PORT_Strncat(path, argv[1], (filename - argv[1]));
    }

    PR_SetEnv("NSS_IGNORE_SYSTEM_POLICY=1");
    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
        fprintf(stderr, "NSS_Init failed: %s\n", PORT_ErrorToString(PR_GetError()));
        result = 2;
        goto loser_no_shutdown;
    }

    PR_SetEnv("NSS_POLICY_LOADED=0");
    PR_SetEnv("NSS_POLICY_FAIL=0");
    PR_SetEnv("NSS_POLICY_WARN=0");

    sprintf(moduleSpec,
            "name=\"Policy File\" "
            "parameters=\"configdir='sql:%s' "
            "secmod='%s' "
            "flags=readOnly,noCertDB,forceSecmodChoice,forceOpen\" "
            "NSS=\"flags=internal,moduleDB,skipFirst,moduleDBOnly,critical,printPolicyFeedback\"",
            path, filename);

    module = SECMOD_LoadModule(moduleSpec, NULL, PR_TRUE);
    if (!module || !module->loaded || atoi(PR_GetEnvSecure("NSS_POLICY_LOADED")) != 1) {
        fprintf(stderr, "Error: failed to load policy file\n");
        result = 2;
        goto loser;
    }

    rv = SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);
    if (rv != SECSuccess) {
        fprintf(stderr, "enable SSL_SECURITY failed: %s\n", PORT_ErrorToString(PR_GetError()));
        result = 2;
        goto loser;
    }

    for (i = 0; i < SSL_NumImplementedCiphers; i++) {
        PRUint16 suite = cipherSuites[i];
        PRBool enabled;
        SSLCipherSuiteInfo info;

        rv = SSL_CipherPrefGetDefault(suite, &enabled);
        if (rv != SECSuccess) {
            fprintf(stderr,
                    "SSL_CipherPrefGetDefault didn't like value 0x%04x (i = %d): %s\n",
                    suite, i, PORT_ErrorToString(PR_GetError()));
            continue;
        }
        rv = SSL_GetCipherSuiteInfo(suite, &info, (int)(sizeof info));
        if (rv != SECSuccess) {
            fprintf(stderr,
                    "SSL_GetCipherSuiteInfo didn't like value 0x%04x (i = %d): %s\n",
                    suite, i, PORT_ErrorToString(PR_GetError()));
            continue;
        }
        if (enabled) {
            ++num_enabled;
            fprintf(stderr, "NSS-POLICY-INFO: ciphersuite %s is enabled\n", info.cipherSuiteName);
        }
    }
    fprintf(stderr, "NSS-POLICY-%s: NUMBER-OF-CIPHERSUITES: %u\n", num_enabled ? sInfo : sWarn, num_enabled);
    if (!num_enabled) {
        PR_SetEnv("NSS_POLICY_WARN=1");
    }

    get_tls_info(ssl_variant_stream, "TLS");
    get_tls_info(ssl_variant_datagram, "DTLS");

    if (atoi(PR_GetEnvSecure("NSS_POLICY_FAIL")) != 0) {
        result = 2;
    } else if (atoi(PR_GetEnvSecure("NSS_POLICY_WARN")) != 0) {
        result = 1;
    }

loser:
    if (module) {
        SECMOD_DestroyModule(module);
    }
    rv = NSS_Shutdown();
    if (rv != SECSuccess) {
        fprintf(stderr, "NSS_Shutdown failed: %s\n", PORT_ErrorToString(PR_GetError()));
        result = 2;
    }
loser_no_shutdown:
    if (result == 2) {
        fprintf(stderr, "NSS-POLICY-FAIL\n");
    } else if (result == 1) {
        fprintf(stderr, "NSS-POLICY-WARN\n");
    }
    return result;
}
