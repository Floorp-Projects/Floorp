/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_errpaths.c
 *
 * Error Handling Helper Functions
 *
 */

#define PKIX_STDVARS_POINTER
#include "pkix_error.h"

const PKIX_StdVars zeroStdVars;

PKIX_Error*
PKIX_DoThrow(PKIX_StdVars * stdVars, PKIX_ERRORCLASS errClass, 
             PKIX_ERRORCODE errCode, PKIX_ERRORCLASS overrideClass,
             void *plContext)
{
    if (!pkixErrorReceived && !pkixErrorResult && pkixErrorList) { 
        pkixTempResult = PKIX_List_GetItem(pkixErrorList, 0, 
                         (PKIX_PL_Object**)&pkixReturnResult,
                                           plContext); 
    } else { 
        pkixTempResult = (PKIX_Error*)pkix_Throw(errClass, myFuncName, errCode,
                                                 overrideClass, pkixErrorResult,
                                                 &pkixReturnResult, plContext);
    }
    if (pkixReturnResult) {
        if (pkixErrorResult != PKIX_ALLOC_ERROR()) {
            PKIX_DECREF(pkixErrorResult);
        }
        pkixTempResult = pkixReturnResult;
    } else if (pkixErrorResult) {
        if (pkixTempResult != PKIX_ALLOC_ERROR()) {
            PKIX_DECREF(pkixTempResult);
        }
        pkixTempResult = pkixErrorResult;
    }
    if (pkixErrorList) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object*)pkixErrorList, plContext);
        pkixErrorList = NULL;
    }
    return pkixTempResult;
}

PKIX_Error *
PKIX_DoReturn(PKIX_StdVars * stdVars, PKIX_ERRORCLASS errClass,
              PKIX_Boolean doLogger, void *plContext)
{
    PKIX_OBJECT_UNLOCK(lockedObject);
    if (pkixErrorReceived || pkixErrorResult || pkixErrorList)
	return PKIX_DoThrow(stdVars, errClass, pkixErrorCode, pkixErrorClass,
                             plContext);
    /* PKIX_DEBUG_EXIT(type); */
    if (doLogger)
	_PKIX_DEBUG_TRACE(pkixLoggersDebugTrace, "<<<", PKIX_LOGGER_LEVEL_TRACE);
    return NULL;
}

/* PKIX_DoAddError - creates the list of received error if it does not exist
 * yet and adds newly received error into the list. */
void
PKIX_DoAddError(PKIX_StdVars *stdVars, PKIX_Error *error, void * plContext)
{
    PKIX_List *localList = NULL;
    PKIX_Error *localError = NULL;
    PKIX_Boolean listCreated = PKIX_FALSE;

    if (!pkixErrorList) {
        localError = PKIX_List_Create(&localList, plContext);
        if (localError)
            goto cleanup;
        listCreated = PKIX_TRUE;
    } else {
        localList = pkixErrorList;
    }

    localError = PKIX_List_AppendItem(localList, (PKIX_PL_Object*)error,
                                      plContext);
    PORT_Assert (localError == NULL);
    if (localError != NULL) {
        if (listCreated) {
            /* ignore the error code of DecRef function */
            PKIX_PL_Object_DecRef((PKIX_PL_Object*)localList, plContext);
            localList = NULL;
        }
    } else {
        pkixErrorList = localList;
    }

cleanup:

    if (localError && localError != PKIX_ALLOC_ERROR()) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object*)localError, plContext);
    }

    if (error && error != PKIX_ALLOC_ERROR()) {
        PKIX_PL_Object_DecRef((PKIX_PL_Object*)error, plContext);
    }
}

