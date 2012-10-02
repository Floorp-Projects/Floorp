/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_mem.c
 *
 * Memory Management Functions
 *
 */

#include "pkix_pl_mem.h"

/*
 * FUNCTION: PKIX_PL_Malloc (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Malloc(
        PKIX_UInt32 size,
        void **pMemory,
        void *plContext)
{
        PKIX_PL_NssContext *nssContext = NULL;
        void *result = NULL;

        PKIX_ENTER(MEM, "PKIX_PL_Malloc");
        PKIX_NULLCHECK_ONE(pMemory);

        if (size == 0){
                *pMemory = NULL;
        } else {

                nssContext = (PKIX_PL_NssContext *)plContext; 

                if (nssContext != NULL && nssContext->arena != NULL) {
                    PKIX_MEM_DEBUG("\tCalling PORT_ArenaAlloc.\n");
                    *pMemory = PORT_ArenaAlloc(nssContext->arena, size);
                } else {
                    PKIX_MEM_DEBUG("\tCalling PR_Malloc.\n");
                    result = (void *) PR_Malloc(size);

                    if (result == NULL) {
                        PKIX_MEM_DEBUG("Fatal Error Occurred: "
                                        "PR_Malloc failed.\n");
                        PKIX_ERROR_ALLOC_ERROR();
                    } else {
                        *pMemory = result;
                    }
                }
        }

cleanup:
        PKIX_RETURN(MEM);
}

/*
 * FUNCTION: PKIX_PL_Calloc (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Calloc(
        PKIX_UInt32 nElem,
        PKIX_UInt32 elSize,
        void **pMemory,
        void *plContext)
{
        PKIX_PL_NssContext *nssContext = NULL;
        void *result = NULL;

        PKIX_ENTER(MEM, "PKIX_PL_Calloc");
        PKIX_NULLCHECK_ONE(pMemory);

        if ((nElem == 0) || (elSize == 0)){
                *pMemory = NULL;
        } else {

                nssContext = (PKIX_PL_NssContext *)plContext; 

                if (nssContext != NULL && nssContext->arena != NULL) {
                    PKIX_MEM_DEBUG("\tCalling PORT_ArenaAlloc.\n");
                    *pMemory = PORT_ArenaAlloc(nssContext->arena, elSize);
                } else {
                    PKIX_MEM_DEBUG("\tCalling PR_Calloc.\n");
                    result = (void *) PR_Calloc(nElem, elSize);

                    if (result == NULL) {
                        PKIX_MEM_DEBUG("Fatal Error Occurred: "
                                        "PR_Calloc failed.\n");
                        PKIX_ERROR_ALLOC_ERROR();
                    } else {
                        *pMemory = result;
                    }
                }
        }

cleanup:

        PKIX_RETURN(MEM);
}

/*
 * FUNCTION: PKIX_PL_Realloc (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Realloc(
        void *ptr,
        PKIX_UInt32 size,
        void **pMemory,
        void *plContext)
{
        PKIX_PL_NssContext *nssContext = NULL;
        void *result = NULL;

        PKIX_ENTER(MEM, "PKIX_PL_Realloc");
        PKIX_NULLCHECK_ONE(pMemory);

        nssContext = (PKIX_PL_NssContext *)plContext; 

        if (nssContext != NULL && nssContext->arena != NULL) {
                PKIX_MEM_DEBUG("\tCalling PORT_ArenaAlloc.\n");
                result = PORT_ArenaAlloc(nssContext->arena, size);

                if (result){
                        PKIX_MEM_DEBUG("\tCalling PORT_Memcpy.\n");
                        PORT_Memcpy(result, ptr, size);
                }
                *pMemory = result;
        } else {
                PKIX_MEM_DEBUG("\tCalling PR_Realloc.\n");
                result = (void *) PR_Realloc(ptr, size);

                if (result == NULL) {
                        if (size == 0){
                                *pMemory = NULL;
                        } else {
                                PKIX_MEM_DEBUG
                                        ("Fatal Error Occurred: "
                                        "PR_Realloc failed.\n");
                                PKIX_ERROR_ALLOC_ERROR();
                        }
                } else {
                        *pMemory = result;
                }
        }

cleanup:

        PKIX_RETURN(MEM);
}

/*
 * FUNCTION: PKIX_PL_Free (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Free(
        void *ptr,
        /* ARGSUSED */ void *plContext)
{
        PKIX_PL_NssContext *context = NULL;

        PKIX_ENTER(MEM, "PKIX_PL_Free");

        context = (PKIX_PL_NssContext *) plContext;
        if (context == NULL || context->arena == NULL) {
            PKIX_MEM_DEBUG("\tCalling PR_Free.\n");
            (void) PR_Free(ptr);
        }

        PKIX_RETURN(MEM);
}
