/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_namechainingchecker.c
 *
 * Functions for name chaining validation
 *
 */


#include "pkix_namechainingchecker.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_NameChainingChecker_Check
 * (see comments for PKIX_CertChainChecker_CheckCallback in pkix_checker.h)
 */
PKIX_Error *
pkix_NameChainingChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        PKIX_PL_X500Name *prevSubject = NULL;
        PKIX_PL_X500Name *currIssuer = NULL;
        PKIX_PL_X500Name *currSubject = NULL;
        PKIX_Boolean result;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_NameChainingChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&prevSubject, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetIssuer(cert, &currIssuer, plContext),
                    PKIX_CERTGETISSUERFAILED);

        if (prevSubject){
                PKIX_CHECK(PKIX_PL_X500Name_Match
                            (prevSubject, currIssuer, &result, plContext),
                            PKIX_X500NAMEMATCHFAILED);
                if (!result){
                        PKIX_ERROR(PKIX_NAMECHAININGCHECKFAILED);
                }
        } else {
                PKIX_ERROR(PKIX_NAMECHAININGCHECKFAILED);
        }

        PKIX_CHECK(PKIX_PL_Cert_GetSubject(cert, &currSubject, plContext),
                    PKIX_CERTGETSUBJECTFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_SetCertChainCheckerState
                    (checker, (PKIX_PL_Object *)currSubject, plContext),
                    PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);

cleanup:

        PKIX_DECREF(prevSubject);
        PKIX_DECREF(currIssuer);
        PKIX_DECREF(currSubject);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_NameChainingChecker_Initialize
 * DESCRIPTION:
 *
 *  Creates a new CertChainChecker and stores it at "pChecker", where it will
 *  be used by pkix_NameChainingChecker_Check to check that the issuer name
 *  of the certificate matches the subject name in the checker's state. The
 *  X500Name pointed to by "trustedCAName" is used to initialize the checker's
 *  state.
 *
 * PARAMETERS:
 *  "trustedCAName"
 *      Address of X500Name representing the trusted CA Name used to
 *      initialize the state of this checker. Must be non-NULL.
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
pkix_NameChainingChecker_Initialize(
        PKIX_PL_X500Name *trustedCAName,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        PKIX_ENTER(CERTCHAINCHECKER, "PKIX_NameChainingChecker_Initialize");
        PKIX_NULLCHECK_TWO(pChecker, trustedCAName);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_NameChainingChecker_Check,
                    PKIX_FALSE,
                    PKIX_FALSE,
                    NULL,
                    (PKIX_PL_Object *)trustedCAName,
                    pChecker,
                    plContext),
                    PKIX_CERTCHAINCHECKERCREATEFAILED);

cleanup:

        PKIX_RETURN(CERTCHAINCHECKER);

}
