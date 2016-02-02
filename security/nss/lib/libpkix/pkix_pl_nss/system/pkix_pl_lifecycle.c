/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_lifecycle.c
 *
 * Lifecycle Functions for the PKIX PL library.
 *
 */

#include "pkix_pl_lifecycle.h"

PKIX_Boolean pkix_pl_initialized = PKIX_FALSE;
pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
PRLock *classTableLock;
PRLogModuleInfo *pkixLog = NULL;

/*
 * PKIX_ALLOC_ERROR is a special error object hard-coded into the
 * pkix_error.o object file. It is thrown if system memory cannot be
 * allocated. PKIX_ALLOC_ERROR is immutable.
 * IncRef, DecRef, and Settor functions cannot be called.
 */

/* Keep this structure definition here for its is used only once here */
struct PKIX_Alloc_Error_ObjectStruct {
        PKIX_PL_Object header;
        PKIX_Error error;
};
typedef struct PKIX_Alloc_Error_ObjectStruct PKIX_Alloc_Error_Object;

static const PKIX_Alloc_Error_Object pkix_Alloc_Error_Data = {
    {
        PKIX_MAGIC_HEADER, 		/* PRUint64    magicHeader */
        (PKIX_UInt32)PKIX_ERROR_TYPE,   /* PKIX_UInt32 type */
        (PKIX_UInt32)1,                 /* PKIX_UInt32 references */
        /* Warning! Cannot Ref Count with NULL lock */
        (void *)0,                      /* PRLock *lock */
        (PKIX_PL_String *)0,            /* PKIX_PL_String *stringRep */
        (PKIX_UInt32)0,                 /* PKIX_UInt32 hashcode */
        (PKIX_Boolean)PKIX_FALSE,       /* PKIX_Boolean hashcodeCached */
    }, {
        (PKIX_ERRORCODE)0,              /* PKIX_ERRORCODE errCode; */
        (PKIX_ERRORCLASS)PKIX_FATAL_ERROR,/* PKIX_ERRORCLASS errClass */
        (PKIX_UInt32)SEC_ERROR_LIBPKIX_INTERNAL, /* default PL Error Code */
        (PKIX_Error *)0,                /* PKIX_Error *cause */
        (PKIX_PL_Object *)0,            /* PKIX_PL_Object *info */
   }
};

PKIX_Error* PKIX_ALLOC_ERROR(void)
{
    return (PKIX_Error *)&pkix_Alloc_Error_Data.error;
}

#ifdef PKIX_OBJECT_LEAK_TEST
SECStatus
pkix_pl_lifecycle_ObjectTableUpdate(int *objCountTable)
{
    int   typeCounter = 0;

    for (; typeCounter < PKIX_NUMTYPES; typeCounter++) {
        pkix_ClassTable_Entry *entry = &systemClasses[typeCounter];

        objCountTable[typeCounter] = entry->objCounter;
    }

    return SECSuccess;
}
#endif /* PKIX_OBJECT_LEAK_TEST */


PKIX_UInt32
pkix_pl_lifecycle_ObjectLeakCheck(int *initObjCountTable)
{
        unsigned int typeCounter = 0;
        PKIX_UInt32 numObjects = 0;
        char  classNameBuff[128];
        char *className = NULL;

        for (; typeCounter < PKIX_NUMTYPES; typeCounter++) {
                pkix_ClassTable_Entry *entry = &systemClasses[typeCounter];
                PKIX_UInt32 objCountDiff = entry->objCounter;

                if (initObjCountTable) {
                    PKIX_UInt32 initialCount = initObjCountTable[typeCounter];
                    objCountDiff = (entry->objCounter > initialCount) ?
                        entry->objCounter - initialCount : 0;
                }

                numObjects += objCountDiff;
                
                if (!pkixLog || !objCountDiff) {
                    continue;
                }
                className = entry->description;
                if (!className) {
                    className = classNameBuff;
                    PR_snprintf(className, 128, "Unknown(ref %d)", 
                            entry->objCounter);
                }

                PR_LOG(pkixLog, 1, ("Class %s leaked %d objects of "
                        "size %d bytes, total = %d bytes\n", className, 
                        objCountDiff, entry->typeObjectSize,
                        objCountDiff * entry->typeObjectSize));
        }
 
        return numObjects;
}

/*
 * PKIX_PL_Initialize (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Initialize(
        PKIX_Boolean platformInitNeeded,
        PKIX_Boolean useArenas,
        void **pPlContext)
{
        void *plContext = NULL;

        PKIX_ENTER(OBJECT, "PKIX_PL_Initialize");

        /*
         * This function can only be called once. If it has already been
         * called, we return a positive status.
         */
        if (pkix_pl_initialized) {
            PKIX_RETURN(OBJECT);
        }

        classTableLock = PR_NewLock();
        if (classTableLock == NULL) {
            return PKIX_ALLOC_ERROR();
        }

        if (PR_GetEnvSecure("NSS_STRICT_SHUTDOWN")) {
            pkixLog = PR_NewLogModule("pkix");
        }
        /*
         * Register Object, it is the base object of all other objects.
         */
        pkix_pl_Object_RegisterSelf(plContext);

        /*
         * Register Error and String, since they will be needed if
         * there is a problem in registering any other type.
         */
        pkix_Error_RegisterSelf(plContext);
        pkix_pl_String_RegisterSelf(plContext);


        /*
         * We register all other system types
         * (They don't need to be in order, but it's
         * easier to keep track of what types are registered
         * if we register them in the same order as their
         * numbers, defined in pkixt.h.
         */
        pkix_pl_BigInt_RegisterSelf(plContext);   /* 1-10 */
        pkix_pl_ByteArray_RegisterSelf(plContext);
        pkix_pl_HashTable_RegisterSelf(plContext);
        pkix_List_RegisterSelf(plContext);
        pkix_Logger_RegisterSelf(plContext);
        pkix_pl_Mutex_RegisterSelf(plContext);
        pkix_pl_OID_RegisterSelf(plContext);
        pkix_pl_RWLock_RegisterSelf(plContext);

        pkix_pl_CertBasicConstraints_RegisterSelf(plContext); /* 11-20 */
        pkix_pl_Cert_RegisterSelf(plContext);
        pkix_pl_CRL_RegisterSelf(plContext);
        pkix_pl_CRLEntry_RegisterSelf(plContext);
        pkix_pl_Date_RegisterSelf(plContext);
        pkix_pl_GeneralName_RegisterSelf(plContext);
        pkix_pl_CertNameConstraints_RegisterSelf(plContext);
        pkix_pl_PublicKey_RegisterSelf(plContext);
        pkix_TrustAnchor_RegisterSelf(plContext);

        pkix_pl_X500Name_RegisterSelf(plContext);   /* 21-30 */
        pkix_pl_HttpCertStoreContext_RegisterSelf(plContext);
        pkix_BuildResult_RegisterSelf(plContext);
        pkix_ProcessingParams_RegisterSelf(plContext);
        pkix_ValidateParams_RegisterSelf(plContext);
        pkix_ValidateResult_RegisterSelf(plContext);
        pkix_CertStore_RegisterSelf(plContext);
        pkix_CertChainChecker_RegisterSelf(plContext);
        pkix_RevocationChecker_RegisterSelf(plContext);
        pkix_CertSelector_RegisterSelf(plContext);

        pkix_ComCertSelParams_RegisterSelf(plContext);   /* 31-40 */
        pkix_CRLSelector_RegisterSelf(plContext);
        pkix_ComCRLSelParams_RegisterSelf(plContext);
        pkix_pl_CertPolicyInfo_RegisterSelf(plContext);
        pkix_pl_CertPolicyQualifier_RegisterSelf(plContext);
        pkix_pl_CertPolicyMap_RegisterSelf(plContext);
        pkix_PolicyNode_RegisterSelf(plContext);
        pkix_TargetCertCheckerState_RegisterSelf(plContext);
        pkix_BasicConstraintsCheckerState_RegisterSelf(plContext);
        pkix_PolicyCheckerState_RegisterSelf(plContext);

        pkix_pl_CollectionCertStoreContext_RegisterSelf(plContext); /* 41-50 */
        pkix_CrlChecker_RegisterSelf(plContext);
        pkix_ForwardBuilderState_RegisterSelf(plContext);
        pkix_SignatureCheckerState_RegisterSelf(plContext);
        pkix_NameConstraintsCheckerState_RegisterSelf(plContext);
#ifndef NSS_PKIX_NO_LDAP
        pkix_pl_LdapRequest_RegisterSelf(plContext);
        pkix_pl_LdapResponse_RegisterSelf(plContext);
        pkix_pl_LdapDefaultClient_RegisterSelf(plContext);
#endif
        pkix_pl_Socket_RegisterSelf(plContext);

        pkix_ResourceLimits_RegisterSelf(plContext); /* 51-59 */
        pkix_pl_MonitorLock_RegisterSelf(plContext);
        pkix_pl_InfoAccess_RegisterSelf(plContext);
        pkix_pl_AIAMgr_RegisterSelf(plContext);
        pkix_OcspChecker_RegisterSelf(plContext);
        pkix_pl_OcspCertID_RegisterSelf(plContext);
        pkix_pl_OcspRequest_RegisterSelf(plContext);
        pkix_pl_OcspResponse_RegisterSelf(plContext);
        pkix_pl_HttpDefaultClient_RegisterSelf(plContext);
        pkix_VerifyNode_RegisterSelf(plContext);
        pkix_EkuChecker_RegisterSelf(plContext);
        pkix_pl_CrlDp_RegisterSelf(plContext);

        if (pPlContext) {
            PKIX_CHECK(PKIX_PL_NssContext_Create
                       (0, useArenas, NULL, &plContext),
                       PKIX_NSSCONTEXTCREATEFAILED);
            
            *pPlContext = plContext;
        }

        pkix_pl_initialized = PKIX_TRUE;

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * PKIX_PL_Shutdown (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Shutdown(void *plContext)
{
#ifdef DEBUG
        PKIX_UInt32 numLeakedObjects = 0;
#endif

        PKIX_ENTER(OBJECT, "PKIX_PL_Shutdown");

        if (!pkix_pl_initialized) {
            /* The library was not initilized */
            PKIX_RETURN(OBJECT);
        }

        PR_DestroyLock(classTableLock);

        pkix_pl_HttpCertStore_Shutdown(plContext);

#ifdef DEBUG
        numLeakedObjects = pkix_pl_lifecycle_ObjectLeakCheck(NULL);
        if (PR_GetEnvSecure("NSS_STRICT_SHUTDOWN")) {
           PORT_Assert(numLeakedObjects == 0);
        }
#else
        pkix_pl_lifecycle_ObjectLeakCheck(NULL);
#endif

        if (plContext != NULL) {
                PKIX_PL_NssContext_Destroy(plContext);
        }

        pkix_pl_initialized = PKIX_FALSE;

        PKIX_RETURN(OBJECT);
}
