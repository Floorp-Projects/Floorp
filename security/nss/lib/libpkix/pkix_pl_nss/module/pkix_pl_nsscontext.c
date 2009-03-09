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
 * pkix_pl_nsscontext.c
 *
 * NSSContext Function Definitions
 *
 */


#include "pkix_pl_nsscontext.h"

#define PKIX_DEFAULT_MAX_RESPONSE_LENGTH               64 * 1024
#define PKIX_DEFAULT_COMM_TIMEOUT_SECONDS              60


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
        PRArenaPool *arena = NULL;
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
 * FUNCTION: pkix_pl_NssContext_SetTimeout
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

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_SetTimeout");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->timeoutSeconds = timeout;

        PKIX_RETURN(CONTEXT);
}

/*
 * FUNCTION: pkix_pl_NssContext_SetMaxResponseLen
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

        PKIX_ENTER(CONTEXT, "pkix_pl_NssContext_SetMaxResponseLen");
        PKIX_NULLCHECK_ONE(nssContext);

        nssContext->maxResponseLength = len;

        PKIX_RETURN(CONTEXT);
}
