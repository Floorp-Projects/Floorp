/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
