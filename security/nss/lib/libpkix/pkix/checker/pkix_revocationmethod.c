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
