/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef PKIXM_H
#define PKIXM_H

#ifdef DEBUG
static const char PKIXM_CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/include/Attic/pkixm.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:03:32 $ $Name:  $";
#endif /* DEBUG */

/*
 * pkixm.h
 *
 * This file contains the private type definitions for the 
 * PKIX part-1 objects.  Mostly, this file contains the actual 
 * structure definitions for the NSSPKIX types declared in nsspkixt.h.
 */

#ifndef PKIXTM_H
#include "pkixtm.h"
#endif /* PKIXTM_H */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

PR_BEGIN_EXTERN_C

/*
 * nss_pkix_Attribute_v_create
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttribute *
nss_pkix_Attribute_v_create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  PRUint32 count,
  va_list ap
);

#ifdef DEBUG

/*
 * nss_pkix_Attribute_add_pointer
 *
 * This method is only present in debug builds.
 *
 * This module-private routine adds an NSSPKIXAttribute pointer to
 * the internal pointer-tracker.  This routine should only be used 
 * by the NSSPKIX module.  This routine returns a PRStatus value; 
 * upon error it will place an error on the error stack and return 
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INTERNAL_ERROR
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nss_pkix_Attribute_add_pointer
(
  const NSSPKIXAttribute *p
);

/*
 * nss_pkix_Attribute_remove_pointer
 *
 * This method is only present in debug builds.
 *
 * This module-private routine removes a valid NSSPKIXAttribute
 * pointer from the internal pointer-tracker.  This routine should 
 * only be used by the NSSPKIX module.  This routine returns a 
 * PRStatus value; upon error it will place an error on the error 
 * stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INTERNAL_ERROR
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nss_pkix_Attribute_remove_pointer
(
  const NSSPKIXAttribute *p
);

#endif /* DEBUG */


#ifdef DEBUG

NSS_EXTERN PRStatus
nss_pkix_AttributeTypeAndValue_add_pointer
(
  const NSSPKIXAttributeTypeAndValue *p
);

NSS_EXTERN PRStatus
nss_pkix_AttributeTypeAndValue_remove_pointer
(
  const NSSPKIXAttributeTypeAndValue *p
);

#endif /* DEBUG */

/*
 * nss_pkix_X520Name_DoUTF8
 *
 */

NSS_EXTERN PR_STATUS
nss_pkix_X520Name_DoUTF8
(
  NSSPKIXX520Name *name
);

#ifdef DEBUG

NSS_EXTERN PRStatus
nss_pkix_X520Name_add_pointer
(
  const NSSPKIXX520Name *p
);

NSS_EXTERN PRStatus
nss_pkix_X520Name_remove_pointer
(
  const NSSPKIXX520Name *p
);

#endif /* DEBUG */

/*
 * nss_pkix_X520CommonName_DoUTF8
 *
 */

NSS_EXTERN PR_STATUS
nss_pkix_X520CommonName_DoUTF8
(
  NSSPKIXX520CommonName *name
);

#ifdef DEBUG

NSS_EXTERN PRStatus
nss_pkix_X520CommonName_add_pointer
(
  const NSSPKIXX520CommonName *p
);

NSS_EXTERN PRStatus
nss_pkix_X520CommonName_remove_pointer
(
  const NSSPKIXX520CommonName *p
);


NSS_EXTERN PRStatus
nss_pkix_Name_add_pointer
(
  const NSSPKIXName *p
);

NSS_EXTERN PRStatus
nss_pkix_Name_remove_pointer
(
  const NSSPKIXName *p
);

/*
 * nss_pkix_RDNSequence_v_create
 */

NSS_EXTERN NSSPKIXRDNSequence *
nss_pkix_RDNSequence_v_create
(
  NSSArena *arenaOpt,
  PRUint32 count,
  va_list ap
);

/*
 * nss_pkix_RDNSequence_Clear
 *
 * Wipes out cached data.
 */

NSS_EXTERN PRStatus
nss_pkix_RDNSequence_Clear
(
  NSSPKIXRDNSequence *rdnseq
);

#ifdef DEBUG

#ifdef NSSDEBUG

NSS_EXTERN PRStatus
nss_pkix_RDNSequence_register
(
  NSSPKIXRDNSequence *rdnseq
);

NSS_EXTERN PRStatus
nss_pkix_RDNSequence_deregister
(
  NSSPKIXRDNSequence *rdnseq
);

#endif /* NSSDEBUG */

NSS_EXTERN PRStatus
nss_pkix_RDNSequence_add_pointer
(
  const NSSPKIXRDNSequence *p
);

NSS_EXTERN PRStatus
nss_pkix_RDNSequence_remove_pointer
(
  const NSSPKIXRDNSequence *p
);

#endif /* DEBUG */

/*
 * nss_pkix_RelativeDistinguishedName_v_create
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nss_pkix_RelativeDistinguishedName_V_Create
(
  NSSArena *arenaOpt,
  PRUint32 count,
  va_list ap
);

/*
 * nss_pkix_RelativeDistinguishedName_Clear
 *
 * Wipes out cached data.
 */

NSS_EXTERN PRStatus
nss_pkix_RelativeDistinguishedName_Clear
(
  NSSPKIXRelativeDistinguishedName *rdn
);

#ifdef DEBUG

#ifdef NSSDEBUG

NSS_EXTERN PRStatus
nss_pkix_RelativeDistinguishedName_register
(
  NSSPKIXRelativeDistinguishedName *rdn
);

NSS_EXTERN PRStatus
nss_pkix_RelativeDistinguishedName_deregister
(
  NSSPKIXRelativeDistinguishedName *rdn
);

#endif /* NSSDEBUG */

NSS_EXTERN PRStatus
nss_pkix_RelativeDistinguishedName_add_pointer
(
  const NSSPKIXRelativeDistinguishedName *p
);

NSS_EXTERN PRStatus
nss_pkix_RelativeDistinguishedName_remove_pointer
(
  const NSSPKIXRelativeDistinguishedName *p
);

NSS_EXTERN PRStatus
nss_pkix_Certificate_add_pointer
(
  const NSSPKIXCertificate *p
);

NSS_EXTERN PRStatus
nss_pkix_Certificate_remove_pointer
(
  const NSSPKIXCertificate *p
);

NSS_EXTERN PRStatus
nss_pkix_TBSCertificate_add_pointer
(
  const NSSPKIXTBSCertificate *p
);

NSS_EXTERN PRStatus
nss_pkix_TBSCertificate_remove_pointer
(
  const NSSPKIXTBSCertificate *p
);

NSS_EXTERN PRStatus
nss_pkix_Validity_add_pointer
(
  const NSSPKIXValidity *p
);

NSS_EXTERN PRStatus
nss_pkix_Validity_remove_pointer
(
  const NSSPKIXValidity *p
);

NSS_EXTERN PRStatus
nss_pkix_SubjectPublicKeyInfo_add_pointer
(
  const NSSPKIXSubjectPublicKeyInfo *p
);

NSS_EXTERN PRStatus
nss_pkix_SubjectPublicKeyInfo_remove_pointer
(
  const NSSPKIXSubjectPublicKeyInfo *p
);

NSS_EXTERN PRStatus
nss_pkix_CertificateList_add_pointer
(
  const NSSPKIXCertificateList *p
);

NSS_EXTERN PRStatus
nss_pkix_CertificateList_remove_pointer
(
  const NSSPKIXCertificateList *p
);

NSS_EXTERN PRStatus
nss_pkix_TBSCertList_add_pointer
(
  const NSSPKIXTBSCertList *p
);

NSS_EXTERN PRStatus
nss_pkix_TBSCertList_remove_pointer
(
  const NSSPKIXTBSCertList *p
);

NSS_EXTERN PRStatus
nss_pkix_revokedCertificates_add_pointer
(
  const NSSPKIXrevokedCertificates *p
);

NSS_EXTERN PRStatus
nss_pkix_revokedCertificates_remove_pointer
(
  const NSSPKIXrevokedCertificates *p
);

NSS_EXTERN PRStatus
nss_pkix_revokedCertificate_add_pointer
(
  const NSSPKIXrevokedCertificate *p
);

NSS_EXTERN PRStatus
nss_pkix_revokedCertificate_remove_pointer
(
  const NSSPKIXrevokedCertificate *p
);

NSS_EXTERN PRStatus
nss_pkix_AlgorithmIdentifier_add_pointer
(
  const NSSPKIXAlgorithmIdentifier *p
);

NSS_EXTERN PRStatus
nss_pkix_AlgorithmIdentifier_remove_pointer
(
  const NSSPKIXAlgorithmIdentifier *p
);

NSS_EXTERN PRStatus
nss_pkix_ORAddress_add_pointer
(
  const NSSPKIXORAddress *p
);

NSS_EXTERN PRStatus
nss_pkix_ORAddress_remove_pointer
(
  const NSSPKIXORAddress *p
);

NSS_EXTERN PRStatus
nss_pkix_BuiltInStandardAttributes_add_pointer
(
  const NSSPKIXBuiltInStandardAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_BuiltInStandardAttributes_remove_pointer
(
  const NSSPKIXBuiltInStandardAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_PersonalName_add_pointer
(
  const NSSPKIXPersonalName *p
);

NSS_EXTERN PRStatus
nss_pkix_PersonalName_remove_pointer
(
  const NSSPKIXPersonalName *p
);

NSS_EXTERN PRStatus
nss_pkix_OrganizationalUnitNames_add_pointer
(
  const NSSPKIXOrganizationalUnitNames *p
);

NSS_EXTERN PRStatus
nss_pkix_OrganizationalUnitNames_remove_pointer
(
  const NSSPKIXOrganizationalUnitNames *p
);

NSS_EXTERN PRStatus
nss_pkix_BuiltInDomainDefinedAttributes_add_pointer
(
  const NSSPKIXBuiltInDomainDefinedAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_BuiltInDomainDefinedAttributes_remove_pointer
(
  const NSSPKIXBuiltInDomainDefinedAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_BuiltInDomainDefinedAttribute_add_pointer
(
  const NSSPKIXBuiltInDomainDefinedAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_BuiltInDomainDefinedAttribute_remove_pointer
(
  const NSSPKIXBuiltInDomainDefinedAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtensionAttributes_add_pointer
(
  const NSSPKIXExtensionAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtensionAttributes_remove_pointer
(
  const NSSPKIXExtensionAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtensionAttribute_add_pointer
(
  const NSSPKIXExtensionAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtensionAttribute_remove_pointer
(
  const NSSPKIXExtensionAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexPersonalName_add_pointer
(
  const NSSPKIXTeletexPersonalName *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexPersonalName_remove_pointer
(
  const NSSPKIXTeletexPersonalName *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexOrganizationalUnitNames_add_pointer
(
  const NSSPKIXTeletexOrganizationalUnitNames *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexOrganizationalUnitNames_remove_pointer
(
  const NSSPKIXTeletexOrganizationalUnitNames *p
);

NSS_EXTERN PRStatus
nss_pkix_PDSParameter_add_pointer
(
  const NSSPKIXPDSParameter *p
);

NSS_EXTERN PRStatus
nss_pkix_PDSParameter_remove_pointer
(
  const NSSPKIXPDSParameter *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtendedNetworkAddress_add_pointer
(
  const NSSPKIXExtendedNetworkAddress *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtendedNetworkAddress_remove_pointer
(
  const NSSPKIXExtendedNetworkAddress *p
);

NSS_EXTERN PRStatus
nss_pkix_e1634Address_add_pointer
(
  const NSSPKIXe1634Address *p
);

NSS_EXTERN PRStatus
nss_pkix_e1634Address_remove_pointer
(
  const NSSPKIXe1634Address *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexDomainDefinedAttributes_add_pointer
(
  const NSSPKIXTeletexDomainDefinedAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexDomainDefinedAttributes_remove_pointer
(
  const NSSPKIXTeletexDomainDefinedAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexDomainDefinedAttribute_add_pointer
(
  const NSSPKIXTeletexDomainDefinedAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_TeletexDomainDefinedAttribute_remove_pointer
(
  const NSSPKIXTeletexDomainDefinedAttribute *p
);

NSS_EXTERN PRStatus
nss_pkix_AuthorityKeyIdentifier_add_pointer
(
  const NSSPKIXAuthorityKeyIdentifier *p
);

NSS_EXTERN PRStatus
nss_pkix_AuthorityKeyIdentifier_remove_pointer
(
  const NSSPKIXAuthorityKeyIdentifier *p
);

NSS_EXTERN PRStatus
nss_pkix_KeyUsage_add_pointer
(
  const NSSPKIXKeyUsage *p
);

NSS_EXTERN PRStatus
nss_pkix_KeyUsage_remove_pointer
(
  const NSSPKIXKeyUsage *p
);

NSS_EXTERN PRStatus
nss_pkix_PrivateKeyUsagePeriod_add_pointer
(
  const NSSPKIXPrivateKeyUsagePeriod *p
);

NSS_EXTERN PRStatus
nss_pkix_PrivateKeyUsagePeriod_remove_pointer
(
  const NSSPKIXPrivateKeyUsagePeriod *p
);

NSS_EXTERN PRStatus
nss_pkix_CertificatePolicies_add_pointer
(
  const NSSPKIXCertificatePolicies *p
);

NSS_EXTERN PRStatus
nss_pkix_CertificatePolicies_remove_pointer
(
  const NSSPKIXCertificatePolicies *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyInformation_add_pointer
(
  const NSSPKIXPolicyInformation *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyInformation_remove_pointer
(
  const NSSPKIXPolicyInformation *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyQualifierInfo_add_pointer
(
  const NSSPKIXPolicyQualifierInfo *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyQualifierInfo_remove_pointer
(
  const NSSPKIXPolicyQualifierInfo *p
);

NSS_EXTERN PRStatus
nss_pkix_UserNotice_add_pointer
(
  const NSSPKIXUserNotice *p
);

NSS_EXTERN PRStatus
nss_pkix_UserNotice_remove_pointer
(
  const NSSPKIXUserNotice *p
);

NSS_EXTERN PRStatus
nss_pkix_NoticeReference_add_pointer
(
  const NSSPKIXNoticeReference *p
);

NSS_EXTERN PRStatus
nss_pkix_NoticeReference_remove_pointer
(
  const NSSPKIXNoticeReference *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyMappings_add_pointer
(
  const NSSPKIXPolicyMappings *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyMappings_remove_pointer
(
  const NSSPKIXPolicyMappings *p
);

NSS_EXTERN PRStatus
nss_pkix_policyMapping_add_pointer
(
  const NSSPKIXpolicyMapping *p
);

NSS_EXTERN PRStatus
nss_pkix_policyMapping_remove_pointer
(
  const NSSPKIXpolicyMapping *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralName_add_pointer
(
  const NSSPKIXGeneralName *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralName_remove_pointer
(
  const NSSPKIXGeneralName *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralNames_add_pointer
(
  const NSSPKIXGeneralNames *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralNames_remove_pointer
(
  const NSSPKIXGeneralNames *p
);

NSS_EXTERN PRStatus
nss_pkix_AnotherName_add_pointer
(
  const NSSPKIXAnotherName *p
);

NSS_EXTERN PRStatus
nss_pkix_AnotherName_remove_pointer
(
  const NSSPKIXAnotherName *p
);

NSS_EXTERN PRStatus
nss_pkix_EDIPartyName_add_pointer
(
  const NSSPKIXEDIPartyName *p
);

NSS_EXTERN PRStatus
nss_pkix_EDIPartyName_remove_pointer
(
  const NSSPKIXEDIPartyName *p
);

NSS_EXTERN PRStatus
nss_pkix_SubjectDirectoryAttributes_add_pointer
(
  const NSSPKIXSubjectDirectoryAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_SubjectDirectoryAttributes_remove_pointer
(
  const NSSPKIXSubjectDirectoryAttributes *p
);

NSS_EXTERN PRStatus
nss_pkix_BasicConstraints_add_pointer
(
  const NSSPKIXBasicConstraints *p
);

NSS_EXTERN PRStatus
nss_pkix_BasicConstraints_remove_pointer
(
  const NSSPKIXBasicConstraints *p
);

NSS_EXTERN PRStatus
nss_pkix_NameConstraints_add_pointer
(
  const NSSPKIXNameConstraints *p
);

NSS_EXTERN PRStatus
nss_pkix_NameConstraints_remove_pointer
(
  const NSSPKIXNameConstraints *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralSubtrees_add_pointer
(
  const NSSPKIXGeneralSubtrees *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralSubtrees_remove_pointer
(
  const NSSPKIXGeneralSubtrees *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralSubtree_add_pointer
(
  const NSSPKIXGeneralSubtree *p
);

NSS_EXTERN PRStatus
nss_pkix_GeneralSubtree_remove_pointer
(
  const NSSPKIXGeneralSubtree *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyConstraints_add_pointer
(
  const NSSPKIXPolicyConstraints *p
);

NSS_EXTERN PRStatus
nss_pkix_PolicyConstraints_remove_pointer
(
  const NSSPKIXPolicyConstraints *p
);

NSS_EXTERN PRStatus
nss_pkix_CRLDistPointsSyntax_add_pointer
(
  const NSSPKIXCRLDistPointsSyntax *p
);

NSS_EXTERN PRStatus
nss_pkix_CRLDistPointsSyntax_remove_pointer
(
  const NSSPKIXCRLDistPointsSyntax *p
);

NSS_EXTERN PRStatus
nss_pkix_DistributionPoint_add_pointer
(
  const NSSPKIXDistributionPoint *p
);

NSS_EXTERN PRStatus
nss_pkix_DistributionPoint_remove_pointer
(
  const NSSPKIXDistributionPoint *p
);

NSS_EXTERN PRStatus
nss_pkix_DistributionPointName_add_pointer
(
  const NSSPKIXDistributionPointName *p
);

NSS_EXTERN PRStatus
nss_pkix_DistributionPointName_remove_pointer
(
  const NSSPKIXDistributionPointName *p
);

NSS_EXTERN PRStatus
nss_pkix_ReasonFlags_add_pointer
(
  const NSSPKIXReasonFlags *p
);

NSS_EXTERN PRStatus
nss_pkix_ReasonFlags_remove_pointer
(
  const NSSPKIXReasonFlags *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtKeyUsageSyntax_add_pointer
(
  const NSSPKIXExtKeyUsageSyntax *p
);

NSS_EXTERN PRStatus
nss_pkix_ExtKeyUsageSyntax_remove_pointer
(
  const NSSPKIXExtKeyUsageSyntax *p
);

NSS_EXTERN PRStatus
nss_pkix_AuthorityInfoAccessSyntax_add_pointer
(
  const NSSPKIXAuthorityInfoAccessSyntax *p
);

NSS_EXTERN PRStatus
nss_pkix_AuthorityInfoAccessSyntax_remove_pointer
(
  const NSSPKIXAuthorityInfoAccessSyntax *p
);

NSS_EXTERN PRStatus
nss_pkix_AccessDescription_add_pointer
(
  const NSSPKIXAccessDescription *p
);

NSS_EXTERN PRStatus
nss_pkix_AccessDescription_remove_pointer
(
  const NSSPKIXAccessDescription *p
);

NSS_EXTERN PRStatus
nss_pkix_IssuingDistributionPoint_add_pointer
(
  const NSSPKIXIssuingDistributionPoint *p
);

NSS_EXTERN PRStatus
nss_pkix_IssuingDistributionPoint_remove_pointer
(
  const NSSPKIXIssuingDistributionPoint *p
);

#endif /* DEBUG */

PR_END_EXTERN_C

#endif /* PKIXM_H */
