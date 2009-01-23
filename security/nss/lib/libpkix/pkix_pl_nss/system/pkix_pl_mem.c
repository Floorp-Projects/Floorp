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
