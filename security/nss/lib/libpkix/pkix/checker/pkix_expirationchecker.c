/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_expirationchecker.c
 *
 * Functions for expiration validation
 *
 */


#include "pkix_expirationchecker.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_ExpirationChecker_Check
 * (see comments for PKIX_CertChainChecker_CheckCallback in pkix_checker.h)
 */
PKIX_Error *
pkix_ExpirationChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        PKIX_PL_Date *testDate = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_ExpirationChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&testDate, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        PKIX_CHECK(PKIX_PL_Cert_CheckValidity(cert, testDate, plContext),
                    PKIX_CERTCHECKVALIDITYFAILED);

cleanup:

        PKIX_DECREF(testDate);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_ExpirationChecker_Initialize
 * DESCRIPTION:
 *
 *  Creates a new CertChainChecker and stores it at "pChecker", where it will
 *  used by pkix_ExpirationChecker_Check to check that the certificate has not
 *  expired with respect to the Date pointed to by "testDate." If "testDate"
 *  is NULL, then the CertChainChecker will check that a certificate has not
 *  expired with respect to the current date and time.
 *
 * PARAMETERS:
 *  "testDate"
 *      Address of Date representing the point in time at which the cert is to
 *      be validated. If "testDate" is NULL, the current date and time is used.
 *  "pChecker"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_ExpirationChecker_Initialize(
        PKIX_PL_Date *testDate,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        PKIX_PL_Date *myDate = NULL;
        PKIX_PL_Date *nowDate = NULL;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_ExpirationChecker_Initialize");
        PKIX_NULLCHECK_ONE(pChecker);

        /* if testDate is NULL, we use the current time */
        if (!testDate){
                PKIX_CHECK(PKIX_PL_Date_Create_UTCTime
                            (NULL, &nowDate, plContext),
                            PKIX_DATECREATEUTCTIMEFAILED);
                myDate = nowDate;
        } else {
                myDate = testDate;
        }

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_ExpirationChecker_Check,
                    PKIX_TRUE,
                    PKIX_FALSE,
                    NULL,
                    (PKIX_PL_Object *)myDate,
                    pChecker,
                    plContext),
                    PKIX_CERTCHAINCHECKERCREATEFAILED);

cleanup:

        PKIX_DECREF(nowDate);

        PKIX_RETURN(CERTCHAINCHECKER);

}
