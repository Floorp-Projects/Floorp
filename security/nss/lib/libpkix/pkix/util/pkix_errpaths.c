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

