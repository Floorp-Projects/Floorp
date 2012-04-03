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
 * pkix_pl_cert.h
 *
 * Certificate Object Definitions
 *
 */

#ifndef _PKIX_PL_CERT_H
#define _PKIX_PL_CERT_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PKIX_PL_CertStruct {
        CERTCertificate *nssCert;  /* Must be the first field.  The
                                    * cert_NSSCertFromPKIXCert function in
                                    * lib/certhigh/certvfypkix.c depends on
                                    * this. */
        CERTGeneralName *nssSubjAltNames;
        PLArenaPool *arenaNameConstraints;
        PKIX_PL_X500Name *issuer;
        PKIX_PL_X500Name *subject;
        PKIX_List *subjAltNames;
        PKIX_Boolean subjAltNamesAbsent;
        PKIX_PL_OID *publicKeyAlgId;
        PKIX_PL_PublicKey *publicKey;
        PKIX_PL_BigInt *serialNumber;
        PKIX_List *critExtOids;
        PKIX_PL_ByteArray *subjKeyId;
        PKIX_Boolean subjKeyIdAbsent;
        PKIX_PL_ByteArray *authKeyId;
        PKIX_Boolean authKeyIdAbsent;
        PKIX_List *extKeyUsages;
        PKIX_Boolean extKeyUsagesAbsent;
        PKIX_PL_CertBasicConstraints *certBasicConstraints;
        PKIX_Boolean basicConstraintsAbsent;
        PKIX_List *certPolicyInfos;
        PKIX_Boolean policyInfoAbsent;
        PKIX_Boolean policyMappingsAbsent;
        PKIX_List *certPolicyMappings; /* List of PKIX_PL_CertPolicyMap */
        PKIX_Boolean policyConstraintsProcessed;
        PKIX_Int32 policyConstraintsExplicitPolicySkipCerts;
        PKIX_Int32 policyConstraintsInhibitMappingSkipCerts;
        PKIX_Boolean inhibitAnyPolicyProcessed;
        PKIX_Int32 inhibitAnySkipCerts;
        PKIX_PL_CertNameConstraints *nameConstraints;
        PKIX_Boolean nameConstraintsAbsent;
        PKIX_Boolean cacheFlag;
        PKIX_CertStore *store;
        PKIX_List *authorityInfoAccess; /* list of PKIX_PL_InfoAccess */
        PKIX_List *subjectInfoAccess; /* list of PKIX_PL_InfoAccess */
        PKIX_Boolean isUserTrustAnchor;
        PKIX_List *crldpList; /* list of CRL DPs based on der in nssCert arena.
                               * Destruction is needed for pkix object and
                               * not for undelying der as it is a part
                               * nssCert arena. */ 
};

/* see source file for function documentation */

PKIX_Error *
pkix_pl_Cert_RegisterSelf(void *plContext);

PKIX_Error *
pkix_pl_Cert_CreateWithNSSCert(
        CERTCertificate *nssCert,
        PKIX_PL_Cert **pCert,
        void *plContext);

PKIX_Error *
pkix_pl_Cert_CreateToList(
        SECItem *derCertItem,
        PKIX_List *certList,
        void *plContext);

PKIX_Error *
pkix_pl_Cert_CheckSubjectAltNameConstraints(
        PKIX_PL_Cert *cert,
        PKIX_PL_CertNameConstraints *nameConstraints,
        PKIX_Boolean matchAll,
        void *plContext);

PKIX_Error *
pkix_pl_Cert_ToString_Helper(
        PKIX_PL_Cert *cert,
        PKIX_Boolean partialString,
        PKIX_PL_String **pString,
        void *plContext);

PKIX_Error *
pkix_pl_Cert_CheckExtendedKeyUsage(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 requiredExtendedKeyUsages,
        PKIX_Boolean *pPass,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_CERT_H */
