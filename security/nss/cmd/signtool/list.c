/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signtool.h"
#include "pk11func.h"
#include "certdb.h"

static int num_trav_certs = 0;
static SECStatus cert_trav_callback(CERTCertificate *cert, SECItem *k,
                                    void *data);

/*********************************************************************
 *
 * L i s t C e r t s
 */
int
ListCerts(char *key, int list_certs)
{
    int failed = 0;
    SECStatus rv;
    char *ugly_list;
    CERTCertDBHandle *db;

    CERTCertificate *cert;
    CERTVerifyLog errlog;

    errlog.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (errlog.arena == NULL) {
        out_of_memory();
    }
    errlog.head = NULL;
    errlog.tail = NULL;
    errlog.count = 0;

    ugly_list = PORT_ZAlloc(16);

    if (ugly_list == NULL) {
        out_of_memory();
    }

    *ugly_list = 0;

    db = CERT_GetDefaultCertDB();

    if (list_certs == 2) {
        PR_fprintf(outputFD, "\nS Certificates\n");
        PR_fprintf(outputFD, "- ------------\n");
    } else {
        PR_fprintf(outputFD, "\nObject signing certificates\n");
        PR_fprintf(outputFD, "---------------------------------------\n");
    }

    num_trav_certs = 0;

    /* Traverse ALL tokens in all slots, authenticating to them all */
    rv = PK11_TraverseSlotCerts(cert_trav_callback, (void *)&list_certs,
                                &pwdata);

    if (rv) {
        PR_fprintf(outputFD, "**Traverse of ALL slots & tokens failed**\n");
        return -1;
    }

    if (num_trav_certs == 0) {
        PR_fprintf(outputFD,
                   "You don't appear to have any object signing certificates.\n");
    }

    if (list_certs == 2) {
        PR_fprintf(outputFD, "- ------------\n");
    } else {
        PR_fprintf(outputFD, "---------------------------------------\n");
    }

    if (list_certs == 1) {
        PR_fprintf(outputFD,
                   "For a list including CA's, use \"%s -L\"\n", PROGRAM_NAME);
    }

    if (list_certs == 2) {
        PR_fprintf(outputFD,
                   "Certificates that can be used to sign objects have *'s to "
                   "their left.\n");
    }

    if (key) {
        /* Do an analysis of the given cert */

        cert = PK11_FindCertFromNickname(key, &pwdata);

        if (cert) {
            PR_fprintf(outputFD,
                       "\nThe certificate with nickname \"%s\" was found:\n",
                       cert->nickname);
            PR_fprintf(outputFD, "\tsubject name: %s\n", cert->subjectName);
            PR_fprintf(outputFD, "\tissuer name: %s\n", cert->issuerName);

            PR_fprintf(outputFD, "\n");

            rv = CERT_CertTimesValid(cert);
            if (rv != SECSuccess) {
                PR_fprintf(outputFD, "**This certificate is expired**\n");
            } else {
                PR_fprintf(outputFD, "This certificate is not expired.\n");
            }

            rv = CERT_VerifyCert(db, cert, PR_TRUE,
                                 certUsageObjectSigner, PR_Now(), &pwdata, &errlog);

            if (rv != SECSuccess) {
                failed = 1;
                if (errlog.count > 0) {
                    PR_fprintf(outputFD,
                               "**Certificate validation failed for the "
                               "following reason(s):**\n");
                } else {
                    PR_fprintf(outputFD, "**Certificate validation failed**");
                }
            } else {
                PR_fprintf(outputFD, "This certificate is valid.\n");
            }
            displayVerifyLog(&errlog);

        } else {
            failed = 1;
            PR_fprintf(outputFD,
                       "The certificate with nickname \"%s\" was NOT FOUND\n", key);
        }
    }

    if (errlog.arena != NULL) {
        PORT_FreeArena(errlog.arena, PR_FALSE);
    }

    if (failed) {
        return -1;
    }
    return 0;
}

/********************************************************************
 *
 * c e r t _ t r a v _ c a l l b a c k
 */
static SECStatus
cert_trav_callback(CERTCertificate *cert, SECItem *k, void *data)
{
    int list_certs = 1;
    char *name;

    if (data) {
        list_certs = *((int *)data);
    }

#define LISTING_USER_SIGNING_CERTS (list_certs == 1)
#define LISTING_ALL_CERTS (list_certs == 2)

    name = cert->nickname;
    if (name) {
        int isSigningCert;

        isSigningCert = cert->nsCertType & NS_CERT_TYPE_OBJECT_SIGNING;
        if (!isSigningCert && LISTING_USER_SIGNING_CERTS)
            return (SECSuccess);

        /* Display this name or email address */
        num_trav_certs++;

        if (LISTING_ALL_CERTS) {
            PR_fprintf(outputFD, "%s ", isSigningCert ? "*" : " ");
        }
        PR_fprintf(outputFD, "%s\n", name);

        if (LISTING_USER_SIGNING_CERTS) {
            int rv = SECFailure;
            if (rv) {
                CERTCertificate *issuerCert;
                issuerCert = CERT_FindCertIssuer(cert, PR_Now(),
                                                 certUsageObjectSigner);
                if (issuerCert) {
                    if (issuerCert->nickname && issuerCert->nickname[0]) {
                        PR_fprintf(outputFD, "    Issued by: %s\n",
                                   issuerCert->nickname);
                        rv = SECSuccess;
                    }
                    CERT_DestroyCertificate(issuerCert);
                }
            }
            if (rv && cert->issuerName && cert->issuerName[0]) {
                PR_fprintf(outputFD, "    Issued by: %s \n", cert->issuerName);
            }
            {
                char *expires;
                expires = DER_TimeChoiceDayToAscii(&cert->validity.notAfter);
                if (expires) {
                    PR_fprintf(outputFD, "    Expires: %s\n", expires);
                    PORT_Free(expires);
                }
            }

            rv = CERT_VerifyCertNow(cert->dbhandle, cert,
                                    PR_TRUE, certUsageObjectSigner, &pwdata);

            if (rv != SECSuccess) {
                rv = PORT_GetError();
                PR_fprintf(outputFD,
                           "    ++ Error ++ THIS CERTIFICATE IS NOT VALID (%s)\n",
                           secErrorString(rv));
            }
        }
    }

    return (SECSuccess);
}
