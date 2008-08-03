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
 * pkix_pl_nameconstraints.h
 *
 * Name Constraints Object Definitions
 *
 */

#ifndef _PKIX_PL_NAMECONSTRAINTS_H
#define _PKIX_PL_NAMECONSTRAINTS_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PKIX_PL_CertNameConstraintsStruct {
        PLArenaPool *arena;
        CERTNameConstraints **nssNameConstraintsList;
        PKIX_UInt32 numNssNameConstraints;
        PKIX_List *permittedList; /* list of PKIX_PL_GeneralName */
        PKIX_List *excludedList; /* list of PKIX_PL_GeneralName */
};

/* see source file for function documentation */

PKIX_Error *pkix_pl_CertNameConstraints_RegisterSelf(void *plContext);

PKIX_Error *pkix_pl_CertNameConstraints_Create(
        CERTCertificate *nssCert,
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext);

PKIX_Error *
pkix_pl_CertNameConstraints_CreateWithNames(
        PKIX_List *names, /* List of PKIX_PL_GeneralName */
        PKIX_PL_CertNameConstraints **pNameConstraints,
        void *plContext);

PKIX_Error *
pkix_pl_CertNameConstraints_CheckNameSpaceNssNames(
        CERTGeneralName *nssSubjectNames,
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_Boolean *pCheckPass,
        void *plContext);

PKIX_Error *
pkix_pl_CertNameConstraints_CheckNameSpacePkixNames(
        PKIX_List *nameList,
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_Boolean *pCheckPass,
        void *plContext);


PKIX_Error *pkix_pl_CertNameConstraints_Merge(
        PKIX_PL_CertNameConstraints *firstNC,
        PKIX_PL_CertNameConstraints *secondNC,
        PKIX_PL_CertNameConstraints **pMergedNC,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_NAMECONSTRAINTS_H */
