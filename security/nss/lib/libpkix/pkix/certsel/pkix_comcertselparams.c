/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_comcertselparams.c
 *
 * ComCertSelParams Object Functions
 *
 */

#include "pkix_comcertselparams.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_ComCertSelParams_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCertSelParams_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ComCertSelParams *params = NULL;

        PKIX_ENTER(COMCERTSELPARAMS, "pkix_ComCertSelParams_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a comCertSelParams object */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_COMCERTSELPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTCOMCERTSELPARAMS);

        params = (PKIX_ComCertSelParams *)object;

        PKIX_DECREF(params->subject);
        PKIX_DECREF(params->policies);
        PKIX_DECREF(params->cert);
        PKIX_DECREF(params->nameConstraints);
        PKIX_DECREF(params->pathToNames);
        PKIX_DECREF(params->subjAltNames);
        PKIX_DECREF(params->date);
        PKIX_DECREF(params->extKeyUsage);
        PKIX_DECREF(params->certValid);
        PKIX_DECREF(params->issuer);
        PKIX_DECREF(params->serialNumber);
        PKIX_DECREF(params->authKeyId);
        PKIX_DECREF(params->subjKeyId);
        PKIX_DECREF(params->subjPubKey);
        PKIX_DECREF(params->subjPKAlgId);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCertSelParams_Duplicate
 * (see comments for PKIX_PL_DuplicateCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCertSelParams_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_ComCertSelParams *params = NULL;
        PKIX_ComCertSelParams *paramsDuplicate = NULL;

        PKIX_ENTER(COMCERTSELPARAMS, "pkix_ComCertSelParams_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_COMCERTSELPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTCOMCERTSELPARAMS);

        params = (PKIX_ComCertSelParams *)object;

        PKIX_CHECK(PKIX_ComCertSelParams_Create(&paramsDuplicate, plContext),
                    PKIX_COMCERTSELPARAMSCREATEFAILED);

        paramsDuplicate->minPathLength = params->minPathLength;
        paramsDuplicate->matchAllSubjAltNames = params->matchAllSubjAltNames;

        PKIX_DUPLICATE(params->subject, &paramsDuplicate->subject, plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->policies, &paramsDuplicate->policies, plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        if (params->cert){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)params->cert,
                            (PKIX_PL_Object **)&paramsDuplicate->cert,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }

        PKIX_DUPLICATE
                (params->nameConstraints,
                &paramsDuplicate->nameConstraints,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->pathToNames,
                &paramsDuplicate->pathToNames,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->subjAltNames,
                &paramsDuplicate->subjAltNames,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        if (params->date){
                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            ((PKIX_PL_Object *)params->date,
                            (PKIX_PL_Object **)&paramsDuplicate->date,
                            plContext),
                            PKIX_OBJECTDUPLICATEFAILED);
        }

        paramsDuplicate->keyUsage = params->keyUsage;

        PKIX_DUPLICATE(params->certValid,
                &paramsDuplicate->certValid,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->issuer,
                &paramsDuplicate->issuer,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->serialNumber,
                &paramsDuplicate->serialNumber,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->authKeyId,
                &paramsDuplicate->authKeyId,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->subjKeyId,
                &paramsDuplicate->subjKeyId,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->subjPubKey,
                &paramsDuplicate->subjPubKey,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE(params->subjPKAlgId,
                &paramsDuplicate->subjPKAlgId,
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        paramsDuplicate->leafCertFlag = params->leafCertFlag;

        *pNewObject = (PKIX_PL_Object *)paramsDuplicate;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(paramsDuplicate);
        }

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCertSelParams_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_COMCERTSELPARAMS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_ComCertSelParams_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry* entry = &systemClasses[PKIX_COMCERTSELPARAMS_TYPE];

        PKIX_ENTER(COMCERTSELPARAMS, "pkix_ComCertSelParams_RegisterSelf");

        entry->description = "ComCertSelParams";
        entry->typeObjectSize = sizeof(PKIX_ComCertSelParams);
        entry->destructor = pkix_ComCertSelParams_Destroy;
        entry->duplicateFunction = pkix_ComCertSelParams_Duplicate;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/* --Public-Functions--------------------------------------------- */


/*
 * FUNCTION: PKIX_ComCertSelParams_Create (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_Create(
        PKIX_ComCertSelParams **pParams,
        void *plContext)
{
        PKIX_ComCertSelParams *params = NULL;

        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_Create");
        PKIX_NULLCHECK_ONE(pParams);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_COMCERTSELPARAMS_TYPE,
                    sizeof (PKIX_ComCertSelParams),
                    (PKIX_PL_Object **)&params,
                    plContext),
                    PKIX_COULDNOTCREATECOMMONCERTSELPARAMSOBJECT);

        /* initialize fields */
        params->version = 0xFFFFFFFF;
        params->minPathLength = -1;
        params->matchAllSubjAltNames = PKIX_TRUE;
        params->subject = NULL;
        params->policies = NULL;
        params->cert = NULL;
        params->nameConstraints = NULL;
        params->pathToNames = NULL;
        params->subjAltNames = NULL;
        params->extKeyUsage = NULL;
        params->keyUsage = 0;
        params->extKeyUsage = NULL;
        params->keyUsage = 0;
        params->date = NULL;
        params->certValid = NULL;
        params->issuer = NULL;
        params->serialNumber = NULL;
        params->authKeyId = NULL;
        params->subjKeyId = NULL;
        params->subjPubKey = NULL;
        params->subjPKAlgId = NULL;
        params->leafCertFlag = PKIX_FALSE;

        *pParams = params;

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);

}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetSubject (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetSubject(
        PKIX_ComCertSelParams *params,
        PKIX_PL_X500Name **pSubject,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetSubject");
        PKIX_NULLCHECK_TWO(params, pSubject);

        PKIX_INCREF(params->subject);

        *pSubject = params->subject;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetSubject (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetSubject(
        PKIX_ComCertSelParams *params,
        PKIX_PL_X500Name *subject,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetSubject");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->subject);

        PKIX_INCREF(subject);

        params->subject = subject;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}


/*
 * FUNCTION: PKIX_ComCertSelParams_GetBasicConstraints
 *      (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetBasicConstraints(
        PKIX_ComCertSelParams *params,
        PKIX_Int32 *pMinPathLength,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_GetBasicConstraints");
        PKIX_NULLCHECK_TWO(params, pMinPathLength);

        *pMinPathLength = params->minPathLength;

        PKIX_RETURN(COMCERTSELPARAMS);
}


/*
 * FUNCTION: PKIX_ComCertSelParams_SetBasicConstraints
 *      (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetBasicConstraints(
        PKIX_ComCertSelParams *params,
        PKIX_Int32 minPathLength,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_SetBasicConstraints");
        PKIX_NULLCHECK_ONE(params);

        params->minPathLength = minPathLength;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}


/*
 * FUNCTION: PKIX_ComCertSelParams_GetPolicy (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetPolicy(
        PKIX_ComCertSelParams *params,
        PKIX_List **pPolicy, /* List of PKIX_PL_OID */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetPolicy");
        PKIX_NULLCHECK_TWO(params, pPolicy);

        PKIX_INCREF(params->policies);
        *pPolicy = params->policies;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetPolicy (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetPolicy(
        PKIX_ComCertSelParams *params,
        PKIX_List *policy, /* List of PKIX_PL_OID */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetPolicy");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->policies);
        PKIX_INCREF(policy);
        params->policies = policy;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);
cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetCertificate
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetCertificate(
        PKIX_ComCertSelParams *params,
        PKIX_PL_Cert **pCert,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetCertificate");
        PKIX_NULLCHECK_TWO(params, pCert);

        PKIX_INCREF(params->cert);
        *pCert = params->cert;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetCertificate
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetCertificate(
        PKIX_ComCertSelParams *params,
        PKIX_PL_Cert *cert,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetCertificate");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->cert);
        PKIX_INCREF(cert);
        params->cert = cert;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);
cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetCertificateValid
 *      (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetCertificateValid(
        PKIX_ComCertSelParams *params,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_GetCertificateValid");

        PKIX_NULLCHECK_TWO(params, pDate);

        PKIX_INCREF(params->date);
        *pDate = params->date;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetCertificateValid
 *      (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetCertificateValid(
        PKIX_ComCertSelParams *params,
        PKIX_PL_Date *date,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_SetCertificateValid");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->date);
        PKIX_INCREF(date);
        params->date = date;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);
cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetNameConstraints
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetNameConstraints(
        PKIX_ComCertSelParams *params,
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_GetNameConstraints");
        PKIX_NULLCHECK_TWO(params, pNameConstraints);

        PKIX_INCREF(params->nameConstraints);

        *pNameConstraints = params->nameConstraints;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetNameConstraints
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetNameConstraints(
        PKIX_ComCertSelParams *params,
        PKIX_PL_CertNameConstraints *nameConstraints,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_SetNameConstraints");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->nameConstraints);
        PKIX_INCREF(nameConstraints);
        params->nameConstraints = nameConstraints;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetPathToNames
 * (see comments in pkix_certsel.h)
 */PKIX_Error *
PKIX_ComCertSelParams_GetPathToNames(
        PKIX_ComCertSelParams *params,
        PKIX_List **pNames,  /* list of PKIX_PL_GeneralName */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetPathToNames");
        PKIX_NULLCHECK_TWO(params, pNames);

        PKIX_INCREF(params->pathToNames);

        *pNames = params->pathToNames;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetPathToNames
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetPathToNames(
        PKIX_ComCertSelParams *params,
        PKIX_List *names,  /* list of PKIX_PL_GeneralName */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetPathToNames");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->pathToNames);
        PKIX_INCREF(names);

        params->pathToNames = names;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_AddPathToName
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_AddPathToName(
        PKIX_ComCertSelParams *params,
        PKIX_PL_GeneralName *name,
        void *plContext)
{
        PKIX_List *pathToNamesList = NULL;

        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_AddPathToName");
        PKIX_NULLCHECK_ONE(params);

        if (name == NULL) {
                goto cleanup;
        }

        if (params->pathToNames == NULL) {
                /* Create a list for name item */
                PKIX_CHECK(PKIX_List_Create(&pathToNamesList, plContext),
                    PKIX_LISTCREATEFAILED);

                params->pathToNames = pathToNamesList;
        }

        PKIX_CHECK(PKIX_List_AppendItem
                    (params->pathToNames, (PKIX_PL_Object *)name, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetSubjAltNames
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetSubjAltNames(
        PKIX_ComCertSelParams *params,
        PKIX_List **pNames, /* list of PKIX_PL_GeneralName */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetSubjAltNames");
        PKIX_NULLCHECK_TWO(params, pNames);

        PKIX_INCREF(params->subjAltNames);

        *pNames = params->subjAltNames;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetSubjAltNames
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetSubjAltNames(
        PKIX_ComCertSelParams *params,
        PKIX_List *names,  /* list of PKIX_PL_GeneralName */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetSubjAltNames");
        PKIX_NULLCHECK_TWO(params, names);

        PKIX_DECREF(params->subjAltNames);
        PKIX_INCREF(names);

        params->subjAltNames = names;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_AddSubjAltNames
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_AddSubjAltName(
        PKIX_ComCertSelParams *params,
        PKIX_PL_GeneralName *name,
        void *plContext)
{
        PKIX_List *list = NULL;

        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_AddSubjAltName");
        PKIX_NULLCHECK_TWO(params, name);

        if (params->subjAltNames == NULL) {
                PKIX_CHECK(PKIX_List_Create(&list, plContext),
                        PKIX_LISTCREATEFAILED);
                params->subjAltNames = list;
        }

        PKIX_CHECK(PKIX_List_AppendItem
                    (params->subjAltNames, (PKIX_PL_Object *)name, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS)
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetMatchAllSubjAltNames
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetMatchAllSubjAltNames(
        PKIX_ComCertSelParams *params,
        PKIX_Boolean *pMatch,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_GetMatchAllSubjAltNames");
        PKIX_NULLCHECK_TWO(params, pMatch);

        *pMatch = params->matchAllSubjAltNames;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetMatchAllSubjAltNames
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetMatchAllSubjAltNames(
        PKIX_ComCertSelParams *params,
        PKIX_Boolean match,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_SetMatchAllSubjAltNames");
        PKIX_NULLCHECK_ONE(params);

        params->matchAllSubjAltNames = match;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetExtendedKeyUsage
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetExtendedKeyUsage(
        PKIX_ComCertSelParams *params,
        PKIX_List **pExtKeyUsage, /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_GetExtendedKeyUsage");
        PKIX_NULLCHECK_TWO(params, pExtKeyUsage);

        PKIX_INCREF(params->extKeyUsage);
        *pExtKeyUsage = params->extKeyUsage;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetExtendedKeyUsage
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetExtendedKeyUsage(
        PKIX_ComCertSelParams *params,
        PKIX_List *extKeyUsage,  /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_SetExtendedKeyUsage");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->extKeyUsage);
        PKIX_INCREF(extKeyUsage);

        params->extKeyUsage = extKeyUsage;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetKeyUsage
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetKeyUsage(
        PKIX_ComCertSelParams *params,
        PKIX_UInt32 *pKeyUsage,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_GetKeyUsage");
        PKIX_NULLCHECK_TWO(params, pKeyUsage);

        *pKeyUsage = params->keyUsage;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetKeyUsage
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetKeyUsage(
        PKIX_ComCertSelParams *params,
        PKIX_UInt32 keyUsage,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                "PKIX_ComCertSelParams_SetKeyUsage");
        PKIX_NULLCHECK_ONE(params);

        params->keyUsage = keyUsage;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetIssuer
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetIssuer(
        PKIX_ComCertSelParams *params,
        PKIX_PL_X500Name **pIssuer,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetIssuer");
        PKIX_NULLCHECK_TWO(params, pIssuer);

        PKIX_INCREF(params->issuer);
        *pIssuer = params->issuer;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetIssuer
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetIssuer(
        PKIX_ComCertSelParams *params,
        PKIX_PL_X500Name *issuer,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetIssuer");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->issuer);
        PKIX_INCREF(issuer);
        params->issuer = issuer;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetSerialNumber
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetSerialNumber(
        PKIX_ComCertSelParams *params,
        PKIX_PL_BigInt **pSerialNumber,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetSerialNumber");
        PKIX_NULLCHECK_TWO(params, pSerialNumber);

        PKIX_INCREF(params->serialNumber);
        *pSerialNumber = params->serialNumber;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetSerialNumber
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetSerialNumber(
        PKIX_ComCertSelParams *params,
        PKIX_PL_BigInt *serialNumber,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetSerialNumber");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->serialNumber);
        PKIX_INCREF(serialNumber);
        params->serialNumber = serialNumber;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetVersion
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetVersion(
        PKIX_ComCertSelParams *params,
        PKIX_UInt32 *pVersion,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetVersion");
        PKIX_NULLCHECK_TWO(params, pVersion);

        *pVersion = params->version;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetVersion
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetVersion(
        PKIX_ComCertSelParams *params,
        PKIX_Int32 version,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetVersion");
        PKIX_NULLCHECK_ONE(params);

        params->version = version;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetSubjKeyIdentifier
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetSubjKeyIdentifier(
        PKIX_ComCertSelParams *params,
        PKIX_PL_ByteArray **pSubjKeyId,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_GetSubjKeyIdentifier");
        PKIX_NULLCHECK_TWO(params, pSubjKeyId);

        PKIX_INCREF(params->subjKeyId);

        *pSubjKeyId = params->subjKeyId;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetSubjKeyIdentifier
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetSubjKeyIdentifier(
        PKIX_ComCertSelParams *params,
        PKIX_PL_ByteArray *subjKeyId,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_SetSubjKeyIdentifier");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->subjKeyId);
        PKIX_INCREF(subjKeyId);

        params->subjKeyId = subjKeyId;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetAuthorityKeyIdentifier
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetAuthorityKeyIdentifier(
        PKIX_ComCertSelParams *params,
        PKIX_PL_ByteArray **pAuthKeyId,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_GetAuthorityKeyIdentifier");
        PKIX_NULLCHECK_TWO(params, pAuthKeyId);

        PKIX_INCREF(params->authKeyId);

        *pAuthKeyId = params->authKeyId;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetAuthorityKeyIdentifier
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetAuthorityKeyIdentifier(
        PKIX_ComCertSelParams *params,
        PKIX_PL_ByteArray *authKeyId,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_SetAuthKeyIdentifier");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->authKeyId);
        PKIX_INCREF(authKeyId);

        params->authKeyId = authKeyId;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetSubjPubKey
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetSubjPubKey(
        PKIX_ComCertSelParams *params,
        PKIX_PL_PublicKey **pSubjPubKey,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetSubjPubKey");
        PKIX_NULLCHECK_TWO(params, pSubjPubKey);

        PKIX_INCREF(params->subjPubKey);

        *pSubjPubKey = params->subjPubKey;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetSubjPubKey
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetSubjPubKey(
        PKIX_ComCertSelParams *params,
        PKIX_PL_PublicKey *subjPubKey,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS,
                    "PKIX_ComCertSelParams_SetSubjPubKey");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->subjPubKey);
        PKIX_INCREF(subjPubKey);

        params->subjPubKey = subjPubKey;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetSubjPKAlgId
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_GetSubjPKAlgId(
        PKIX_ComCertSelParams *params,
        PKIX_PL_OID **pAlgId,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetSubjPKAlgId");
        PKIX_NULLCHECK_TWO(params, pAlgId);

        PKIX_INCREF(params->subjPKAlgId);

        *pAlgId = params->subjPKAlgId;

cleanup:
        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetSubjPKAlgId
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetSubjPKAlgId(
        PKIX_ComCertSelParams *params,
        PKIX_PL_OID *algId,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetSubjPKAlgId");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->subjPKAlgId);
        PKIX_INCREF(algId);

        params->subjPKAlgId = algId;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_GetLeafCertFlag
 * (see comments in pkix_certsel.h)
 */
PKIX_Error*
PKIX_ComCertSelParams_GetLeafCertFlag(
        PKIX_ComCertSelParams *params,
        PKIX_Boolean *pLeafFlag,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_GetLeafCertFlag");
        PKIX_NULLCHECK_TWO(params, pLeafFlag);

        *pLeafFlag = params->leafCertFlag;

        PKIX_RETURN(COMCERTSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCertSelParams_SetLeafCertFlag
 * (see comments in pkix_certsel.h)
 */
PKIX_Error *
PKIX_ComCertSelParams_SetLeafCertFlag(
        PKIX_ComCertSelParams *params,
        PKIX_Boolean leafFlag,
        void *plContext)
{
        PKIX_ENTER(COMCERTSELPARAMS, "PKIX_ComCertSelParams_SetLeafCertFlag");
        PKIX_NULLCHECK_ONE(params);

        params->leafCertFlag = leafFlag;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCERTSELPARAMS);
}
