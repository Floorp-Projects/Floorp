/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_revocationmethod.c
 *
 * RevocationMethod Object Functions
 *
 */

#include "pkix_revocationmethod.h"
#include "pkix_tools.h"

/* Constructor of revocation method object. Does not create an object,
 * but just initializez PKIX_RevocationMethodStruct fields. Object
 * suppose to be already created. */
PKIX_Error *
pkix_RevocationMethod_Init(
    pkix_RevocationMethod *method,
    PKIX_RevocationMethodType methodType,
    PKIX_UInt32 flags,
    PKIX_UInt32 priority,
    pkix_LocalRevocationCheckFn localRevChecker,
    pkix_ExternalRevocationCheckFn externalRevChecker,
    void *plContext) 
{
    PKIX_ENTER(REVOCATIONMETHOD, "PKIX_RevocationMethod_Init");
    
    method->methodType = methodType;
    method->flags = flags;
    method->priority = priority;
    method->localRevChecker = localRevChecker;
    method->externalRevChecker = externalRevChecker;

    PKIX_RETURN(REVOCATIONMETHOD);
}

/* Data duplication data. Not create an object. Only initializes fields
 * in the new object by data from "object". */
PKIX_Error *
pkix_RevocationMethod_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object *newObject,
        void *plContext)
{
        pkix_RevocationMethod *method = NULL;

        PKIX_ENTER(REVOCATIONMETHOD, "pkix_RevocationMethod_Duplicate");
        PKIX_NULLCHECK_TWO(object, newObject);

        method = (pkix_RevocationMethod *)object;

        PKIX_CHECK(
            pkix_RevocationMethod_Init((pkix_RevocationMethod*)newObject,
                                       method->methodType,
                                       method->flags,
                                       method->priority,
                                       method->localRevChecker,
                                       method->externalRevChecker,
                                       plContext),
            PKIX_COULDNOTCREATEREVOCATIONMETHODOBJECT);

cleanup:

        PKIX_RETURN(REVOCATIONMETHOD);
}
