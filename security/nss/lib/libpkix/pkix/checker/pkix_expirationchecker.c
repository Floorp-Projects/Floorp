/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
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
