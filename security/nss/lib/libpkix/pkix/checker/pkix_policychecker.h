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
 * pkix_policychecker.h
 *
 * Header file for policy checker.
 *
 */

#ifndef _PKIX_POLICYCHECKER_H
#define _PKIX_POLICYCHECKER_H

#include "pkix_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PKIX_PolicyCheckerStateStruct PKIX_PolicyCheckerState;

struct PKIX_PolicyCheckerStateStruct{
        PKIX_PL_OID *certPoliciesExtension;             /* const */
        PKIX_PL_OID *policyMappingsExtension;           /* const */
        PKIX_PL_OID *policyConstraintsExtension;        /* const */
        PKIX_PL_OID *inhibitAnyPolicyExtension;         /* const */
        PKIX_PL_OID *anyPolicyOID;                      /* const */
        PKIX_Boolean initialIsAnyPolicy;                /* const */
        PKIX_PolicyNode *validPolicyTree;
        PKIX_List *userInitialPolicySet;                /* immutable */
        PKIX_List *mappedUserInitialPolicySet;
        PKIX_Boolean policyQualifiersRejected;
        PKIX_Boolean initialPolicyMappingInhibit;
        PKIX_Boolean initialExplicitPolicy;
        PKIX_Boolean initialAnyPolicyInhibit;
        PKIX_UInt32 explicitPolicy;
        PKIX_UInt32 inhibitAnyPolicy;
        PKIX_UInt32 policyMapping;
        PKIX_UInt32 numCerts;
        PKIX_UInt32 certsProcessed;
        PKIX_PolicyNode *anyPolicyNodeAtBottom;
        PKIX_PolicyNode *newAnyPolicyNode;
        /*
         * The following variables do not survive from one
         * certificate to the next. They are needed at each
         * level of recursive routines, any by placing them
         * in the state object we can pass fewer arguments.
         */
        PKIX_Boolean certPoliciesCritical;
        PKIX_List *mappedPolicyOIDs;
};

PKIX_Error *
pkix_PolicyChecker_Initialize(
        PKIX_List *initialPolicies,
        PKIX_Boolean policyQualifiersRejected,
        PKIX_Boolean initialPolicyMappingInhibit,
        PKIX_Boolean initialExplicitPolicy,
        PKIX_Boolean initialAnyPolicyInhibit,
        PKIX_UInt32 numCerts,
        PKIX_CertChainChecker **pChecker,
        void *plContext);

/* --Private-Functions-------------------------------------------- */

PKIX_Error *
pkix_PolicyCheckerState_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_POLICYCHECKER_H */
