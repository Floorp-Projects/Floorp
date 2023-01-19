/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_nsscontext.c
 *
 * NSSContext Function Definitions
 *
 */


#include "pkix_pl_nsscontext.h"

#define PKIX_DEFAULT_MAX_RESPONSE_LENGTH               64 * 1024
#define PKIX_DEFAULT_COMM_TIMEOUT_SECONDS              60

#define PKIX_DEFAULT_CRL_RELOAD_DELAY_SECONDS        6 * 24 * 60 * 60
#define PKIX_DEFAULT_BAD_CRL_RELOAD_DELAY_SECONDS    60 * 60

/* --Public-NSSContext-Functions--------------------------- */

/*
 * FUNCTION: PKIX_PL_NssContext_Create
 * (see comments in pkix_samples_modules.h)
 */
PKIX_Error *
PKIX_PL_NssContext_Create(
        PKIX_UInt32 certificateUsage,
        PKIX_Boolean useNssArena,
        void *wincx,
        void **pNssContext)
{
        PKIX_PL_NssContext *context = NULL;
        PLArenaPool *arena = NULL;
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "PKIX_PL_NssContext_Create");
        PKIX_NULLCHECK_ONE(pNssContext);

        PKIX_CHECK(PKIX_PL_Malloc
                   (sizeof(PKIX_PL_NssContext), (void **)&context, NULL),
                   PKIX_MALLOCFAILED);

        if (useNssArena == PKIX_TRUE) {
                PKIX_CONTEXT_DEBUG("\t\tCalling PORT_NewArena\n");
                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        }
        
        context->arena = arena;
        context->certificateUsage = (SECCertificateUsage)certificateUsage;
        context->wincx = wincx;
        context->timeoutSeconds = PKIX_DEFAULT_COMM_TIMEOUT_SECONDS;
        context->maxResponseLength = PKIX_DEFAULT_MAX_RESPONSE_LENGTH;
        context->crlReloadDelay = PKIX_DEFAULT_CRL_RELOAD_DELAY_SECONDS;
        context->badDerCrlReloadDelay =
                             PKIX_DEFAULT_BAD_CRL_RELOAD_DELAY_SECONDS;
        context->certSignatureCheck = PKIX_TRUE;
        context->chainVerifyCallback.isChainValid = NULL;
        context->chainVerifyCallback.isChainValidArg = NULL;
        *pNssContext = context;

cleanup:

        PKIX_RETURN(CONTEXT);
}


/*
 * FUNCTION: PKIX_PL_NssContext_Destroy
 * (see comments in pkix_samples_modules.h)
 */
PKIX_Error *
PKIX_PL_NssContext_Destroy(
        void *nssContext)
{
        void *plContext = NULL;
        PKIX_PL_NssContext *context = NULL;

        PKIX_ENTER(CONTEXT, "PKIX_PL_NssContext_Destroy");
        PKIX_NULLCHECK_ONE(nssContext);

        context = (PKIX_PL_NssContext*)nssContext;

        if (context->arena != NULL) {
                PKIX_CONTEXT_DEBUG("\t\tCalling PORT_FreeArena\n");
                PORT_FreeArena(context->arena, PKIX_FALSE);
        }

        PKIX_PL_Free(nssContext, NULL);

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_GetCertUsage
 * DESCRIPTION:
 *
 *  This function obtains the platform-dependent SECCertificateUsage  parameter
 *  from the context object pointed to by "nssContext", storing the result at
 *  "pCertUsage".
 *
 * PARAMETERS:
 *  "nssContext"
 *      The address of the context object whose wincx parameter is to be
 *      obtained. Must be non-NULL.
 *  "pCertUsage"
 *      The address where the result is stored. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_NssContext_GetCertUsage(
        PKIX_PL_NssContext *nssContext,
        SECCertificateUsage *pCertUsage)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_GetCertUsage");
        PKIX_NULLCHECK_TWO(nssContext, pCertUsage);

        *pCertUsage = nssContext->certificateUsage;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_SetCertUsage
 * DESCRIPTION:
 *
 *  This function sets the platform-dependent SECCertificateUsage parameter in
 *  the context object pointed to by "nssContext" to the value provided in
 *  "certUsage".
 *
 * PARAMETERS:
 *  "certUsage"
 *      Platform-dependent value to be stored.
 *  "nssContext"
 *      The address of the context object whose wincx parameter is to be
 *      obtained. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_NssContext_SetCertUsage(
        SECCertificateUsage certUsage,
        PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_SetCertUsage");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->certificateUsage = certUsage;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_GetCertSignatureCheck
 * DESCRIPTION:
 *
 *  This function obtains the platform-dependent flag to turn on or off
 *  signature checks.
 *
 * PARAMETERS:
 *  "nssContext"
 *      The address of the context object whose wincx parameter is to be
 *      obtained. Must be non-NULL.
 *  "pCheckSig"
 *      The address where the result is stored. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_NssContext_GetCertSignatureCheck(
        PKIX_PL_NssContext *nssContext,
        PKIX_Boolean *pCheckSig)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_GetCertUsage");
        PKIX_NULLCHECK_TWO(nssContext, pCheckSig);

        *pCheckSig = nssContext->certSignatureCheck;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_SetCertSignatureCheck
 * DESCRIPTION:
 *
 *  This function sets the check signature flag in
 *  the context object pointed to by "nssContext" to the value provided in
 *  "checkSig".
 *
 * PARAMETERS:
 *  "checkSig"
 *      Boolean that tells whether or not to check the signatues on certs.
 *  "nssContext"
 *      The address of the context object whose wincx parameter is to be
 *      obtained. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_NssContext_SetCertSignatureCheck(
        PKIX_Boolean checkSig,
        PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_SetCertUsage");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->certSignatureCheck =  checkSig;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_GetWincx
 * DESCRIPTION:
 *
 *  This function obtains the platform-dependent wincx parameter from the
 *  context object pointed to by "nssContext", storing the result at "pWincx".
 *
 * PARAMETERS:
 *  "nssContext"
 *      The address of the context object whose wincx parameter is to be
 *      obtained. Must be non-NULL.
 *  "pWincx"
 *      The address where the result is stored. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_NssContext_GetWincx(
        PKIX_PL_NssContext *nssContext,
        void **pWincx)
{
        void *plContext = NULL;
        PKIX_PL_NssContext *context = NULL;

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_GetWincx");
        PKIX_NULLCHECK_TWO(nssContext, pWincx);

        context = (PKIX_PL_NssContext *)nssContext;

        *pWincx = context->wincx;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_SetWincx
 * DESCRIPTION:
 *
 *  This function sets the platform-dependent wincx parameter in the context
 *  object pointed to by "nssContext" to the value provided in "wincx".
 *
 * PARAMETERS:
 *  "wincx"
 *      Platform-dependent value to be stored.
 *  "nssContext"
 *      The address of the context object whose wincx parameter is to be
 *      obtained. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_NssContext_SetWincx(
        void *wincx,
        PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_SetWincx");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->wincx = wincx;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: PKIX_PL_NssContext_SetTimeout
 * DESCRIPTION:
 *
 * Sets user defined socket timeout for the validation
 * session. Default is 60 seconds.
 *
 */
PKIX_Error *
PKIX_PL_NssContext_SetTimeout(PKIX_UInt32 timeout,
                              PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "PKIX_PL_NssContext_SetTimeout");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->timeoutSeconds = timeout;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: PKIX_PL_NssContext_SetMaxResponseLen
 * DESCRIPTION:
 *
 * Sets user defined maximum transmission length of a message.
 *
 */
PKIX_Error *
PKIX_PL_NssContext_SetMaxResponseLen(PKIX_UInt32 len,
                                     PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "PKIX_PL_NssContext_SetMaxResponseLen");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->maxResponseLength = len;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: PKIX_PL_NssContext_SetCrlReloadDelay
 * DESCRIPTION:
 *
 * Sets user defined delay between attempts to load crl using
 * CRLDP.
 *
 */
PKIX_Error *
PKIX_PL_NssContext_SetCrlReloadDelay(PKIX_UInt32 delay,
                                     PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "PKIX_PL_NssContext_SetCrlReloadDelay");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->crlReloadDelay = delay;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: PKIX_PL_NssContext_SetBadDerCrlReloadDelay
 * DESCRIPTION:
 *
 * Sets user defined delay between attempts to load crl that
 * failed to decode.
 *
 */
PKIX_Error *
PKIX_PL_NssContext_SetBadDerCrlReloadDelay(PKIX_UInt32 delay,
                                             PKIX_PL_NssContext *nssContext)
{
        void *plContext = NULL;

        PKIX_ENTER(CONTEXT, "PKIX_PL_NssContext_SetBadDerCrlReloadDelay");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->badDerCrlReloadDelay = delay;

        PKIX_RETURN(CONTEXT);
}
