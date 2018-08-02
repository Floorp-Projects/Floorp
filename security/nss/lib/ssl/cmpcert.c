/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include "prerror.h"
#include "secitem.h"
#include "prnetdb.h"
#include "cert.h"
#include "nspr.h"
#include "secder.h"
#include "key.h"
#include "nss.h"

/*
 * Look to see if any of the signers in the cert chain for "cert" are found
 * in the list of caNames.
 * Returns SECSuccess if so, SECFailure if not.
 */
SECStatus
NSS_CmpCertChainWCANames(CERTCertificate *cert, CERTDistNames *caNames)
{
    SECItem *caname;
    CERTCertificate *curcert;
    CERTCertificate *oldcert;
    int j;
    int depth;
    SECItem issuerName;

    if (!cert || !caNames || !caNames->nnames || !caNames->names ||
        !caNames->names->data)
        return SECFailure;
    depth = 0;
    curcert = CERT_DupCertificate(cert);

    while (curcert) {
        issuerName = curcert->derIssuer;

        for (j = 0; j < caNames->nnames; j++) {
            caname = &caNames->names[j];
            if (SECITEM_CompareItem(&issuerName, caname) == SECEqual) {
                CERT_DestroyCertificate(curcert);
                return SECSuccess;
            }
        }
        if ((depth <= 20) &&
            (SECITEM_CompareItem(&curcert->derIssuer, &curcert->derSubject) !=
             SECEqual)) {
            oldcert = curcert;
            curcert = CERT_FindCertByName(curcert->dbhandle,
                                          &curcert->derIssuer);
            CERT_DestroyCertificate(oldcert);
            depth++;
        } else {
            CERT_DestroyCertificate(curcert);
            curcert = NULL;
        }
    }
    return SECFailure;
}
