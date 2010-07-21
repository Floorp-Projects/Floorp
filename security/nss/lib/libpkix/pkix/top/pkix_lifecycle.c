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
 * pkix_lifecycle.c
 *
 * Top level initialize and shutdown functions
 *
 */

#include "pkix_lifecycle.h"

static PKIX_Boolean pkixIsInitialized;

/* Lock used by Logger - is reentrant by the same thread */
extern PKIX_PL_MonitorLock *pkixLoggerLock;

/* 
 * Following pkix_* variables are for debugging purpose. They should be taken
 * out eventually. The purpose is to verify cache tables usage (via debugger).
 */
int pkix_ccAddCount = 0;
int pkix_ccLookupCount = 0;
int pkix_ccRemoveCount = 0;
int pkix_cAddCount = 0;
int pkix_cLookupCount = 0;
int pkix_cRemoveCount = 0;
int pkix_ceAddCount = 0;
int pkix_ceLookupCount = 0;

PKIX_PL_HashTable *cachedCrlSigTable = NULL;
PKIX_PL_HashTable *cachedCertSigTable = NULL;
PKIX_PL_HashTable *cachedCertChainTable = NULL;
PKIX_PL_HashTable *cachedCertTable = NULL;
PKIX_PL_HashTable *cachedCrlEntryTable = NULL;
PKIX_PL_HashTable *aiaConnectionCache = NULL;
PKIX_PL_HashTable *httpSocketCache = NULL;

extern PKIX_List *pkixLoggers;
extern PKIX_List *pkixLoggersErrors;
extern PKIX_List *pkixLoggersDebugTrace;

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_Initialize (see comments in pkix.h)
 */
PKIX_Error *
PKIX_Initialize(
        PKIX_Boolean platformInitNeeded,
        PKIX_UInt32 desiredMajorVersion,
        PKIX_UInt32 minDesiredMinorVersion,
        PKIX_UInt32 maxDesiredMinorVersion,
        PKIX_UInt32 *pActualMinorVersion,
        void **pPlContext)
{
        void *plContext = NULL;

        PKIX_ENTER(LIFECYCLE, "PKIX_Initialize");
        PKIX_NULLCHECK_ONE(pPlContext);

        /*
         * If we are called a second time other than in the situation handled
         * above, we return a positive status.
         */
        if (pkixIsInitialized){
                /* Already initialized */
                PKIX_RETURN(LIFECYCLE);
        }

        PKIX_CHECK(PKIX_PL_Initialize
                (platformInitNeeded, PKIX_FALSE, &plContext),
                PKIX_INITIALIZEFAILED);

        *pPlContext = plContext;

        if (desiredMajorVersion != PKIX_MAJOR_VERSION){
                PKIX_ERROR(PKIX_MAJORVERSIONSDONTMATCH);
        }

        if ((minDesiredMinorVersion > PKIX_MINOR_VERSION) ||
            (maxDesiredMinorVersion < PKIX_MINOR_VERSION)){
                PKIX_ERROR(PKIX_MINORVERSIONNOTBETWEENDESIREDMINANDMAX);
        }

        *pActualMinorVersion = PKIX_MINOR_VERSION;

        /* Create Cache Tables
         * Do not initialize hash tables for object leak test */
#if !defined(PKIX_OBJECT_LEAK_TEST)
        PKIX_CHECK(PKIX_PL_HashTable_Create
                   (32, 0, &cachedCertSigTable, plContext),
                   PKIX_HASHTABLECREATEFAILED);
        
        PKIX_CHECK(PKIX_PL_HashTable_Create
                   (32, 0, &cachedCrlSigTable, plContext),
                   PKIX_HASHTABLECREATEFAILED);
        
        PKIX_CHECK(PKIX_PL_HashTable_Create
                       (32, 10, &cachedCertChainTable, plContext),
                   PKIX_HASHTABLECREATEFAILED);
        
        PKIX_CHECK(PKIX_PL_HashTable_Create
                       (32, 10, &cachedCertTable, plContext),
                   PKIX_HASHTABLECREATEFAILED);
        
        PKIX_CHECK(PKIX_PL_HashTable_Create
                   (32, 10, &cachedCrlEntryTable, plContext),
                   PKIX_HASHTABLECREATEFAILED);
        
        PKIX_CHECK(PKIX_PL_HashTable_Create
                   (5, 5, &aiaConnectionCache, plContext),
                   PKIX_HASHTABLECREATEFAILED);
        
#ifdef PKIX_SOCKETCACHE
        PKIX_CHECK(PKIX_PL_HashTable_Create
                   (5, 5, &httpSocketCache, plContext),
                   PKIX_HASHTABLECREATEFAILED);
#endif
        if (pkixLoggerLock == NULL) {
            PKIX_CHECK(PKIX_PL_MonitorLock_Create
                       (&pkixLoggerLock, plContext),
                       PKIX_MONITORLOCKCREATEFAILED);
        }
#else
        fnInvTable = PL_NewHashTable(0, pkix_ErrorGen_Hash,
                                     PL_CompareValues,
                                     PL_CompareValues, NULL, NULL);
        if (!fnInvTable) {
            PKIX_ERROR(PKIX_HASHTABLECREATEFAILED);
        }
        
        fnStackNameArr = PORT_ZNewArray(char*, MAX_STACK_DEPTH);
        if (!fnStackNameArr) {
            PKIX_ERROR(PKIX_HASHTABLECREATEFAILED);
        }

        fnStackInvCountArr = PORT_ZNewArray(PKIX_UInt32, MAX_STACK_DEPTH);
        if (!fnStackInvCountArr) {
            PKIX_ERROR(PKIX_HASHTABLECREATEFAILED);
        }
#endif /* PKIX_OBJECT_LEAK_TEST */

        pkixIsInitialized = PKIX_TRUE;

cleanup:

        PKIX_RETURN(LIFECYCLE);
}

/*
 * FUNCTION: PKIX_Shutdown (see comments in pkix.h)
 */
PKIX_Error *
PKIX_Shutdown(void *plContext)
{
        PKIX_List *savedPkixLoggers = NULL;
        PKIX_List *savedPkixLoggersErrors = NULL;
        PKIX_List *savedPkixLoggersDebugTrace = NULL;

        PKIX_ENTER(LIFECYCLE, "PKIX_Shutdown");

        if (!pkixIsInitialized){
                /* The library was not initialized */
                PKIX_RETURN(LIFECYCLE);
        }

        pkixIsInitialized = PKIX_FALSE;

        if (pkixLoggers) {
                savedPkixLoggers = pkixLoggers;
                savedPkixLoggersErrors = pkixLoggersErrors;
                savedPkixLoggersDebugTrace = pkixLoggersDebugTrace;
                pkixLoggers = NULL;
                pkixLoggersErrors = NULL;
                pkixLoggersDebugTrace = NULL;
                PKIX_DECREF(savedPkixLoggers);
                PKIX_DECREF(savedPkixLoggersErrors);
                PKIX_DECREF(savedPkixLoggersDebugTrace);
        }
        PKIX_DECREF(pkixLoggerLock);

        /* Destroy Cache Tables */
        PKIX_DECREF(cachedCertSigTable);
        PKIX_DECREF(cachedCrlSigTable);
        PKIX_DECREF(cachedCertChainTable);
        PKIX_DECREF(cachedCertTable);
        PKIX_DECREF(cachedCrlEntryTable);
        PKIX_DECREF(aiaConnectionCache);
        PKIX_DECREF(httpSocketCache);

        /* Clean up any temporary errors that happened during shutdown */
        if (pkixErrorList) {
            PKIX_PL_Object_DecRef((PKIX_PL_Object*)pkixErrorList, plContext);
            pkixErrorList = NULL;
        }

        PKIX_CHECK(PKIX_PL_Shutdown(plContext),
                PKIX_SHUTDOWNFAILED);

#ifdef PKIX_OBJECT_LEAK_TEST
        PORT_Free(fnStackInvCountArr);
        PORT_Free(fnStackNameArr);
        PL_HashTableDestroy(fnInvTable);
#endif

cleanup:

        PKIX_RETURN(LIFECYCLE);
}
