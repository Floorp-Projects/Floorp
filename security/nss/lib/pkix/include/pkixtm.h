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

#ifndef PKIXTM_H
#define PKIXTM_H

#ifdef DEBUG
static const char PKIXTM_CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/include/Attic/pkixtm.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:04:49 $ $Name:  $";
#endif /* DEBUG */

/*
 * pkixtm.h
 *
 * This file contains the module-private type definitions for the 
 * PKIX part-1 objects.  Mostly, this file contains the actual 
 * structure definitions for the NSSPKIX types declared in nsspkixt.h.
 */

#ifndef NSSPKIXT_H
#include "nsspkixt.h"
#endif /* NSSPKIXT_H */

PR_BEGIN_EXTERN_C

/*
 * Attribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Attribute       ::=     SEQUENCE {
 *          type            AttributeType,
 *          values  SET OF AttributeValue
 *                  -- at least one value is required -- }
 *
 */

struct NSSPKIXAttributeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSBER *ber;
  NSSDER *der;
  nssASN1Item asn1type;
  nssASN1Item **asn1values;
  NSSPKIXAttributeType *type;
  PRUint32 valuesCount;
};

/*
 * AttributeTypeAndValue
 *
 * This structure contains an attribute type (indicated by an OID), 
 * and the type-specific value.  RelativeDistinguishedNamess consist
 * of a set of these.  These are distinct from Attributes (which have
 * SET of values), from AttributeDescriptions (which have qualifiers
 * on the types), and from AttributeValueAssertions (which assert a
 * a value comparison under some matching rule).
 *
 * From RFC 2459:
 *
 *  AttributeTypeAndValue           ::=     SEQUENCE {
 *          type    AttributeType,
 *          value   AttributeValue }
 * 
 */

struct NSSPKIXAttributeTypeAndValueStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  nssASN1Item asn1type;
  nssASN1Item asn1value;
  NSSPKIXAttributeType *type;
  NSSUTF8 *utf8;
};

/*
 * X520Name
 *
 * From RFC 2459:
 *
 *  X520name        ::= CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-name)),
 *        printableString       PrintableString (SIZE (1..ub-name)),
 *        universalString       UniversalString (SIZE (1..ub-name)),
 *        utf8String            UTF8String (SIZE (1..ub-name)),
 *        bmpString             BMPString (SIZE(1..ub-name))   }
 *
 *
 *  ub-name INTEGER ::=     32768
 *
 */

struct NSSPKIXX520NameStr {
  nssASN1Item string;
  NSSUTF8 *utf8;
  NSSDER *der;
  PRBool wasPrintable;
  PRBool inArena;
};

/*
 * From RFC 2459:
 *
 *  X520CommonName  ::=      CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-common-name)),
 *        printableString       PrintableString (SIZE (1..ub-common-name)),
 *        universalString       UniversalString (SIZE (1..ub-common-name)),
 *        utf8String            UTF8String (SIZE (1..ub-common-name)),
 *        bmpString             BMPString (SIZE(1..ub-common-name))   }
 * 
 *  ub-common-name  INTEGER ::=     64
 *
 */

struct NSSPKIXX520CommonNameStr {
};

/*
 * Name
 *
 * This structure contains a union of the possible name formats,
 * which at the moment is limited to an RDNSequence.
 *
 * From RFC 2459:
 *
 *  Name            ::=   CHOICE { -- only one possibility for now --
 *                                   rdnSequence  RDNSequence }
 *
 */

struct NSSPKIXNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *ber;
  NSSDER *der;
  NSSUTF8 *utf;
  NSSPKIXNameChoice choice;
  union {
    NSSPKIXRDNSequence *rdnSequence;
  } u;
};

/*
 * RDNSequence
 *
 * This structure contains a sequence of RelativeDistinguishedName
 * objects.
 *
 * From RFC 2459:
 *
 *  RDNSequence     ::=   SEQUENCE OF RelativeDistinguishedName
 *
 */

struct NSSPKIXRDNSequenceStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSBER *ber;
  NSSDER *der;
  NSSUTF8 *utf8;
  PRUint32 count;
  NSSPKIXRelativeDistinguishedName **rdns;
};

/*
 * RelativeDistinguishedName
 *
 * This structure contains an unordered set of AttributeTypeAndValue 
 * objects.  RDNs are used to distinguish a set of objects underneath 
 * a common object.
 *
 * Often, a single ATAV is sufficient to make a unique distinction.
 * For example, if a company assigns its people unique uid values,
 * then in the Name "uid=smith,ou=People,o=Acme,c=US" the "uid=smith"
 * ATAV by itself forms an RDN.  However, sometimes a set of ATAVs is
 * needed.  For example, if a company needed to distinguish between
 * two Smiths by specifying their corporate divisions, then in the
 * Name "(cn=Smith,ou=Sales),ou=People,o=Acme,c=US" the parenthesised
 * set of ATAVs forms the RDN.
 *
 * From RFC 2459:
 *
 *  RelativeDistinguishedName  ::=
 *                      SET SIZE (1 .. MAX) OF AttributeTypeAndValue
 *
 */

struct NSSPKIXRelativeDistinguishedNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSBER *ber;
  NSSUTF8 *utf8;
  PRUint32 count;
  NSSPKIXAttributeTypeAndValue **atavs;
};

/*
 * Certificate
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Certificate  ::=  SEQUENCE  {
 *       tbsCertificate       TBSCertificate,
 *       signatureAlgorithm   AlgorithmIdentifier,
 *       signature            BIT STRING  }
 *
 */

struct NSSPKIXCertificateStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXTBSCertificate *tbsCertificate;
  NSSPKIXAlgorithmIdentifier *signatureAlgorithm;
  NSSItem *signature;
};

/*
 * TBSCertificate
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TBSCertificate  ::=  SEQUENCE  {
 *       version         [0]  Version DEFAULT v1,
 *       serialNumber         CertificateSerialNumber,
 *       signature            AlgorithmIdentifier,
 *       issuer               Name,
 *       validity             Validity,
 *       subject              Name,
 *       subjectPublicKeyInfo SubjectPublicKeyInfo,
 *       issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
 *                            -- If present, version shall be v2 or v3
 *       subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
 *                            -- If present, version shall be v2 or v3
 *       extensions      [3]  Extensions OPTIONAL
 *                            -- If present, version shall be v3 --  }
 *
 */

struct NSSPKIXTBSCertificateStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXVersion version;
  NSSPKIXCertificateSerialNumber serialNumber;
  NSSPKIXAlgorithmIdentifier *signature;
  NSSPKIXName *issuer;
  NSSPKIXValidity *validity;
  NSSPKIXName *subject;
  NSSPKIXSubjectPublicKeyInfo *subjectPublicKeyInfo;
  NSSPKIXUniqueIdentifier *issuerUniqueID;
  NSSPKIXUniqueIdentifier *subjectUniqueID;
  NSSPKIXExtensions *extensions;
};

/*
 * Validity
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Validity ::= SEQUENCE {
 *       notBefore      Time,
 *       notAfter       Time }
 *
 */

struct NSSPKIXValidityStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * Time
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Time ::= CHOICE {
 *       utcTime        UTCTime,
 *       generalTime    GeneralizedTime }
 *
 */

struct NSSPKIXTimeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSBER *ber;
  nssASN1Item asn1item;
  PRTime prTime;
  PRBool prTimeValid;
};

/*
 * SubjectPublicKeyInfo
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectPublicKeyInfo  ::=  SEQUENCE  {
 *       algorithm            AlgorithmIdentifier,
 *       subjectPublicKey     BIT STRING  }
 *
 */

struct NSSPKIXSubjectPublicKeyInfoStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXAlgorithmIdentifier *algorithm;
  NSSItem *subjectPublicKey;
};

/*
 * Extensions
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
 *
 */

struct NSSPKIXExtensionsStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * Extension
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Extension  ::=  SEQUENCE  {
 *       extnID      OBJECT IDENTIFIER,
 *       critical    BOOLEAN DEFAULT FALSE,
 *       extnValue   OCTET STRING  }
 *
 */

struct NSSPKIXExtensionStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSOID *extnID;
  PRBool critical;
  NSSItem *extnValue;
};

/*
 * CertificateList
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificateList  ::=  SEQUENCE  {
 *       tbsCertList          TBSCertList,
 *       signatureAlgorithm   AlgorithmIdentifier,
 *       signature            BIT STRING  }
 *
 */

struct NSSPKIXCertificateListStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXTBSCertList *tbsCertList;
  NSSPKIXAlgorithmIdentifier *signatureAlgorithm;
  NSSItem *signature;
};

/*
 * TBSCertList
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TBSCertList  ::=  SEQUENCE  {
 *       version                 Version OPTIONAL,
 *                                    -- if present, shall be v2
 *       signature               AlgorithmIdentifier,
 *       issuer                  Name,
 *       thisUpdate              Time,
 *       nextUpdate              Time OPTIONAL,
 *       revokedCertificates     SEQUENCE OF SEQUENCE  {
 *            userCertificate         CertificateSerialNumber,
 *            revocationDate          Time,
 *            crlEntryExtensions      Extensions OPTIONAL
 *                                           -- if present, shall be v2
 *                                 }  OPTIONAL,
 *       crlExtensions           [0] Extensions OPTIONAL
 *                                           -- if present, shall be v2 -- }
 *
 */

struct NSSPKIXTBSCertListStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXVersion version;
  NSSPKIXAlgorithmIdentifier *signature;
  NSSPKIXName *issuer;
  -time- thisUpdate;
  -time- nextUpdate;
  NSSPKIXrevokedCertificates *revokedCertificates;
  NSSPKIXExtensions *crlExtensions;  
};

/*
 * revokedCertificates
 *
 * This is a "helper type" to simplify handling of TBSCertList objects.
 *
 *       revokedCertificates     SEQUENCE OF SEQUENCE  {
 *            userCertificate         CertificateSerialNumber,
 *            revocationDate          Time,
 *            crlEntryExtensions      Extensions OPTIONAL
 *                                           -- if present, shall be v2
 *                                 }  OPTIONAL,
 *
 */

struct NSSPKIXrevokedCertificatesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * revokedCertificate
 *
 * This is a "helper type" to simplify handling of TBSCertList objects.
 *
 *                                           SEQUENCE  {
 *            userCertificate         CertificateSerialNumber,
 *            revocationDate          Time,
 *            crlEntryExtensions      Extensions OPTIONAL
 *                                           -- if present, shall be v2
 *                                 }  OPTIONAL,
 *
 */

struct NSSPKIXrevokedCertificateStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXCertificateSerialNumber *userCertificate;
  -time- revocationDate;
  NSSPKIXExtensions *crlEntryExtensions;
};

/*
 * AlgorithmIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 * (1988 syntax)
 *
 *  AlgorithmIdentifier  ::=  SEQUENCE  {
 *       algorithm               OBJECT IDENTIFIER,
 *       parameters              ANY DEFINED BY algorithm OPTIONAL  }
 *                                  -- contains a value of the type
 *                                  -- registered for use with the
 *                                  -- algorithm object identifier value
 *
 *
 */

struct NSSPKIXAlgorithmIdentifierStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSBER *ber;
  NSSOID *algorithm;
  NSSItem *parameters;
};

/*
 * -- types related to NSSPKIXAlgorithmIdentifiers:
 *
 *  Dss-Sig-Value  ::=  SEQUENCE  {
 *       r       INTEGER,
 *       s       INTEGER  }
 *  
 *  DomainParameters ::= SEQUENCE {
 *       p       INTEGER, -- odd prime, p=jq +1
 *       g       INTEGER, -- generator, g
 *       q       INTEGER, -- factor of p-1
 *       j       INTEGER OPTIONAL, -- subgroup factor, j>= 2
 *       validationParms  ValidationParms OPTIONAL }
 *  
 *  ValidationParms ::= SEQUENCE {
 *       seed             BIT STRING,
 *       pgenCounter      INTEGER }
 *  
 *  Dss-Parms  ::=  SEQUENCE  {
 *       p             INTEGER,
 *       q             INTEGER,
 *       g             INTEGER  }
 *
 */

/*
 * ORAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ORAddress ::= SEQUENCE {
 *     built-in-standard-attributes BuiltInStandardAttributes,
 *     built-in-domain-defined-attributes
 *                          BuiltInDomainDefinedAttributes OPTIONAL,
 *     -- see also teletex-domain-defined-attributes
 *     extension-attributes ExtensionAttributes OPTIONAL }
 *  --      The OR-address is semantically absent from the OR-name if the
 *  --      built-in-standard-attribute sequence is empty and the
 *  --      built-in-domain-defined-attributes and extension-attributes are
 *  --      both omitted.
 *
 */

struct NSSPKIXORAddressStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXBuiltInStandardAttributes *builtInStandardAttributes;
  NSSPKIXBuiltInDomainDefinedAttributes *builtInDomainDefinedAttributes;
  NSSPKIXExtensionsAttributes *extensionAttributes;
};

/*
 * BuiltInStandardAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BuiltInStandardAttributes ::= SEQUENCE {
 *     country-name CountryName OPTIONAL,
 *     administration-domain-name AdministrationDomainName OPTIONAL,
 *     network-address      [0] NetworkAddress OPTIONAL,
 *     -- see also extended-network-address
 *     terminal-identifier  [1] TerminalIdentifier OPTIONAL,
 *     private-domain-name  [2] PrivateDomainName OPTIONAL,
 *     organization-name    [3] OrganizationName OPTIONAL,
 *     -- see also teletex-organization-name
 *     numeric-user-identifier      [4] NumericUserIdentifier OPTIONAL,
 *     personal-name        [5] PersonalName OPTIONAL,
 *     -- see also teletex-personal-name
 *     organizational-unit-names    [6] OrganizationalUnitNames OPTIONAL
 *     -- see also teletex-organizational-unit-names -- }
 *
 */

struct NSSPKIXBuiltInStandardAttributesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXCountryName *countryName;
  NSSPKIXAdministrationDomainName *administrationDomainName;
  NSSPKIXNetworkAddress *networkAddress;
  NSSPKIXTerminalIdentifier *terminalIdentifier;
  NSSPKIXPrivateDomainName *privateDomainName;
  NSSPKIXOrganizationName *organizationName;
  NSSPKIXNumericUserIdentifier *numericUserIdentifier;
  NSSPKIXPersonalName *personalName;
  NSSPKIXOrganizationalUnitNames *organizationalUnitNames;
};

/*
 * PersonalName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PersonalName ::= SET {
 *     surname [0] PrintableString (SIZE (1..ub-surname-length)),
 *     given-name [1] PrintableString
 *                          (SIZE (1..ub-given-name-length)) OPTIONAL,
 *     initials [2] PrintableString (SIZE (1..ub-initials-length)) OPTIONAL,
 *     generation-qualifier [3] PrintableString
 *                  (SIZE (1..ub-generation-qualifier-length)) OPTIONAL }
 *
 */

struct NSSPKIXPersonalNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSUTF8 *surname;
  NSSUTF8 *givenName;
  NSSUTF8 *initials;
  NSSUTF8 *generationQualifier;
};

/*
 * OrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  OrganizationalUnitNames ::= SEQUENCE SIZE (1..ub-organizational-units)
 *                                          OF OrganizationalUnitName
 * 
 */

struct NSSPKIXOrganizationalUnitNamesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * BuiltInDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BuiltInDomainDefinedAttributes ::= SEQUENCE SIZE
 *                                  (1..ub-domain-defined-attributes) OF
 *                                  BuiltInDomainDefinedAttribute
 *
 */

struct NSSPKIXBuiltInDomainDefinedAttributesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * BuiltInDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BuiltInDomainDefinedAttribute ::= SEQUENCE {
 *     type PrintableString (SIZE
 *                          (1..ub-domain-defined-attribute-type-length)),
 *     value PrintableString (SIZE
 *                          (1..ub-domain-defined-attribute-value-length))}
 *
 */

struct NSSPKIXBuiltInDomainDefinedAttributeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSUTF8 *type;
  NSSUTF8 *value;
};

/*
 * ExtensionAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtensionAttributes ::= SET SIZE (1..ub-extension-attributes) OF
 *                          ExtensionAttribute
 *
 */

struct NSSPKIXExtensionAttributesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * ExtensionAttribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtensionAttribute ::=  SEQUENCE {
 *     extension-attribute-type [0] INTEGER (0..ub-extension-attributes),
 *     extension-attribute-value [1]
 *                          ANY DEFINED BY extension-attribute-type }
 *
 */

struct NSSPKIXExtensionAttributeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXExtensionsAttributeType extensionAttributeType;
  NSSItem *extensionAttributeValue;
};

/*
 * TeletexPersonalName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexPersonalName ::= SET {
 *     surname [0] TeletexString (SIZE (1..ub-surname-length)),
 *     given-name [1] TeletexString
 *                  (SIZE (1..ub-given-name-length)) OPTIONAL,
 *     initials [2] TeletexString (SIZE (1..ub-initials-length)) OPTIONAL,
 *     generation-qualifier [3] TeletexString (SIZE
 *                  (1..ub-generation-qualifier-length)) OPTIONAL }
 *
 */

struct NSSPKIXTeletexPersonalNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSUTF8 *surname;
  NSSUTF8 *givenName;
  NSSUTF8 *initials;
  NSSUTF8 *generationQualifier;
};

/*
 * TeletexOrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexOrganizationalUnitNames ::= SEQUENCE SIZE
 *          (1..ub-organizational-units) OF TeletexOrganizationalUnitName
 *
 */

struct NSSPKIXTeletexOrganizationalUnitNamesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * PDSParameter
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PDSParameter ::= SET {
 *     printable-string PrintableString
 *                  (SIZE(1..ub-pds-parameter-length)) OPTIONAL,
 *     teletex-string TeletexString
 *                  (SIZE(1..ub-pds-parameter-length)) OPTIONAL }
 *
 */

struct NSSPKIXPDSParameterStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSUTF8 *printableString;
  NSSTUF8 *teletexString;
};

/*
 * UnformattedPostalAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  UnformattedPostalAddress ::= SET {
 *     printable-address SEQUENCE SIZE (1..ub-pds-physical-address-lines) OF
 *             PrintableString (SIZE (1..ub-pds-parameter-length)) OPTIONAL,
 *     teletex-string TeletexString
 *           (SIZE (1..ub-unformatted-address-length)) OPTIONAL }
 *
 */

struct NSSPKIXUnformattedPostalAddressStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
  NSSUTF8 *teletexString;
};

/*
 * ExtendedNetworkAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtendedNetworkAddress ::= CHOICE {
 *     e163-4-address SEQUENCE {
 *          number [0] NumericString (SIZE (1..ub-e163-4-number-length)),
 *          sub-address [1] NumericString
 *                  (SIZE (1..ub-e163-4-sub-address-length)) OPTIONAL },
 *     psap-address [0] PresentationAddress }
 *
 */

struct NSSPKIXExtendedNetworkAddressStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXExtendedNetworkAddressChoice choice;
  union {
    NSSe1634address *e1634Address;
    NSSPKIXPresentationAddress *psapAddress;
  } u;
};

/*
 * e163-4-address
 *
 * Helper structure for ExtendedNetworkAddress.
 * -- fgmr comments --
 *
 * From RFC 2459:
 * 
 *     e163-4-address SEQUENCE {
 *          number [0] NumericString (SIZE (1..ub-e163-4-number-length)),
 *          sub-address [1] NumericString
 *                  (SIZE (1..ub-e163-4-sub-address-length)) OPTIONAL },
 * 
 */

struct NSSe1634addressStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSUTF8 *number;
  NSSUTF8 *subAddress;
};

/*
 * PresentationAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PresentationAddress ::= SEQUENCE {
 *          pSelector       [0] EXPLICIT OCTET STRING OPTIONAL,
 *          sSelector       [1] EXPLICIT OCTET STRING OPTIONAL,
 *          tSelector       [2] EXPLICIT OCTET STRING OPTIONAL,
 *          nAddresses      [3] EXPLICIT SET SIZE (1..MAX) OF OCTET STRING }
 *
 */

struct NSSPKIXPresentationAddressStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSItem *pSelector;
  NSSItem *sSelector;
  NSSItem *tSelector;
  NSSItem *nAddresses[]; --fgmr--
};

/*
 * TeletexDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexDomainDefinedAttributes ::= SEQUENCE SIZE
 *     (1..ub-domain-defined-attributes) OF TeletexDomainDefinedAttribute
 * 
 */

struct NSSPKIXTeletexDomainDefinedAttributesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * TeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexDomainDefinedAttribute ::= SEQUENCE {
 *          type TeletexString
 *                 (SIZE (1..ub-domain-defined-attribute-type-length)),
 *          value TeletexString
 *                 (SIZE (1..ub-domain-defined-attribute-value-length)) }
 * 
 */

struct NSSPKIXTeletexDomainDefinedAttributeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSUTF8 *type;
  NSSUTF8 *value;
};

/*
 * AuthorityKeyIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AuthorityKeyIdentifier ::= SEQUENCE {
 *        keyIdentifier             [0] KeyIdentifier            OPTIONAL,
 *        authorityCertIssuer       [1] GeneralNames             OPTIONAL,
 *        authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
 *      -- authorityCertIssuer and authorityCertSerialNumber shall both
 *      -- be present or both be absent
 *
 */

struct NSSPKIXAuthorityKeyIdentifierStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXKeyIdentifier *keyIdentifier;
  NSSPKIXGeneralNames *authorityCertIssuer;
  NSSPKIXCertificateSerialNumber *authorityCertSerialNumber;
};

/*
 * KeyUsage
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  KeyUsage ::= BIT STRING {
 *       digitalSignature        (0),
 *       nonRepudiation          (1),
 *       keyEncipherment         (2),
 *       dataEncipherment        (3),
 *       keyAgreement            (4),
 *       keyCertSign             (5),
 *       cRLSign                 (6),
 *       encipherOnly            (7),
 *       decipherOnly            (8) }
 *
 */

struct NSSPKIXKeyUsageStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXKeyUsageValue keyUsage;
};

/*
 * PrivateKeyUsagePeriod
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PrivateKeyUsagePeriod ::= SEQUENCE {
 *       notBefore       [0]     GeneralizedTime OPTIONAL,
 *       notAfter        [1]     GeneralizedTime OPTIONAL }
 *       -- either notBefore or notAfter shall be present
 *
 */

struct NSSPKIXPrivateKeyUsagePeriodStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  --time--
  --time--
};

/*
 * CertificatePolicies
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
 *
 */

struct NSSPKIXCertificatePoliciesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * PolicyInformation
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyInformation ::= SEQUENCE {
 *       policyIdentifier   CertPolicyId,
 *       policyQualifiers   SEQUENCE SIZE (1..MAX) OF
 *               PolicyQualifierInfo OPTIONAL }
 *
 */

struct NSSPKIXPolicyInformationStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXCertPolicyId *policyIdentifier;
  NSSPKIXPolicyQualifierInfo *policyQualifiers[];
  --fgmr--
};

/*
 * PolicyQualifierInfo
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyQualifierInfo ::= SEQUENCE {
 *         policyQualifierId  PolicyQualifierId,
 *         qualifier        ANY DEFINED BY policyQualifierId }
 *
 */

struct NSSPKIXPolicyQualifierInfoStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXPolicyQualifierId *policyQualifierId;
  NSSItem *qualifier;
};

/*
 * UserNotice
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  UserNotice ::= SEQUENCE {
 *       noticeRef        NoticeReference OPTIONAL,
 *       explicitText     DisplayText OPTIONAL}
 *
 */

struct NSSPKIXUserNoticeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXNoticeReference *noticeRef;
  NSSPKIXDisplayText *explicitText;
};

/*
 * NoticeReference
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NoticeReference ::= SEQUENCE {
 *       organization     DisplayText,
 *       noticeNumbers    SEQUENCE OF INTEGER }
 *
 */

struct NSSPKIXNoticeReferenceStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXDisplayText *organization;
  NSSItem *noticeNumbers[]; --fgmr--
  ...
};

/*
 * PolicyMappings
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
 *       issuerDomainPolicy      CertPolicyId,
 *       subjectDomainPolicy     CertPolicyId }
 *
 */

struct NSSPKIXPolicyMappingsStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXpolicyMapping *policyMappings[]; --fgmr--
  ...
};

/*
 * policyMapping
 *
 * Helper structure for PolicyMappings
 *
 *                                               SEQUENCE {
 *       issuerDomainPolicy      CertPolicyId,
 *       subjectDomainPolicy     CertPolicyId }
 *
 */

struct NSSPKIXpolicyMappingStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXCertPolicyId *issuerDomainPolicy;
  NSSPKIXCertPolicyId *subjectDomainPolicy;
};

/*
 * GeneralName
 *
 * This structure contains a union of the possible general names,
 * of which there are several.
 *
 * From RFC 2459:
 *
 *  GeneralName ::= CHOICE {
 *       otherName                       [0]     AnotherName,
 *       rfc822Name                      [1]     IA5String,
 *       dNSName                         [2]     IA5String,
 *       x400Address                     [3]     ORAddress,
 *       directoryName                   [4]     Name,
 *       ediPartyName                    [5]     EDIPartyName,
 *       uniformResourceIdentifier       [6]     IA5String,
 *       iPAddress                       [7]     OCTET STRING,
 *       registeredID                    [8]     OBJECT IDENTIFIER }
 *
 */

struct NSSPKIXGeneralNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXGeneralNameChoice choice;
  union {
    NSSPKIXAnotherName *otherName;
    NSSUTF8 *rfc822Name;
    NSSUTF8 *dNSName;
    NSSPKIXORAddress *x400Address;
    NSSPKIXName *directoryName;
    NSSEDIPartyName *ediPartyName;
    NSSUTF8 *uniformResourceIdentifier;
    NSSItem *iPAddress;
    NSSOID *registeredID;
  } u;
};

/*
 * GeneralNames
 *
 * This structure contains a sequence of GeneralName objects.
 *
 * From RFC 2459:
 *
 *  GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 *
 */

struct NSSPKIXGeneralNamesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * AnotherName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AnotherName ::= SEQUENCE {
 *       type-id    OBJECT IDENTIFIER,
 *       value      [0] EXPLICIT ANY DEFINED BY type-id }
 *
 */

struct NSSPKIXAnotherNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSOID *typeId;
  NSSItem *value;
};

/*
 * EDIPartyName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *
 *  EDIPartyName ::= SEQUENCE {
 *       nameAssigner            [0]     DirectoryString OPTIONAL,
 *       partyName               [1]     DirectoryString }
 *
 */

struct NSSPKIXEDIPartyNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXDirectoryString *nameAssigner;
  NSSPKIXDirectoryString *partyname;
};

/*
 * SubjectDirectoryAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectDirectoryAttributes ::= SEQUENCE SIZE (1..MAX) OF Attribute
 *
 */

struct NSSPKIXSubjectDirectoryAttributesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * BasicConstraints
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BasicConstraints ::= SEQUENCE {
 *       cA                      BOOLEAN DEFAULT FALSE,
 *       pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
 *
 */

struct NSSPKIXBasicConstraintsStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  PRBool cA;
  PRInt32 pathLenConstraint; --fgmr--
};

/*
 * NameConstraints
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NameConstraints ::= SEQUENCE {
 *       permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
 *       excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
 *
 */

struct NSSPKIXNameConstraintsStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXGeneralSubtrees *permittedSubtrees;
  NSSPKIXGeneralSubtrees *excludedSubtrees;
};

/*
 * GeneralSubtrees
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 */

struct NSSPKIXGeneralSubtreesStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * GeneralSubtree
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  GeneralSubtree ::= SEQUENCE {
 *       base                    GeneralName,
 *       minimum         [0]     BaseDistance DEFAULT 0,
 *       maximum         [1]     BaseDistance OPTIONAL }
 *
 */

struct NSSPKIXGeneralSubtreeStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXGeneralName;
  NSSPKIXBaseDistance minimum;
  NSSPKIXBaseDistance maximum;
};

/*
 * PolicyConstraints
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyConstraints ::= SEQUENCE {
 *       requireExplicitPolicy           [0] SkipCerts OPTIONAL,
 *       inhibitPolicyMapping            [1] SkipCerts OPTIONAL }
 * 
 */

struct NSSPKIXPolicyConstraintsStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXSkipCerts *requireExplicitPolicy;
  NSSPKIXSkipCerts *inhibitPolicyMapping;
};

/*
 * CRLDistPointsSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CRLDistPointsSyntax ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
 *
 */

struct NSSPKIXCRLDistPointsSyntaxStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * DistributionPoint
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DistributionPoint ::= SEQUENCE {
 *       distributionPoint       [0]     DistributionPointName OPTIONAL,
 *       reasons                 [1]     ReasonFlags OPTIONAL,
 *       cRLIssuer               [2]     GeneralNames OPTIONAL }
 *
 */

struct NSSPKIXDistributionPointStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXDistributionPointName *distributionPoint;
  NSSPKIXReasonFlags *reasons;
  NSSPKIXGeneralNames *cRLIssuer;
};

/*
 * DistributionPointName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DistributionPointName ::= CHOICE {
 *       fullName                [0]     GeneralNames,
 *       nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
 *
 */

struct NSSPKIXDistributionPointNameStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXDistributionPointNameChoice choice;
  union {
    NSSPKIXGeneralNames *fullName;
    NSSPKIXRelativeDistinguishedName *nameRelativeToCRLIssuer;
  } u;
};

/*
 * ReasonFlags
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ReasonFlags ::= BIT STRING {
 *       unused                  (0),
 *       keyCompromise           (1),
 *       cACompromise            (2),
 *       affiliationChanged      (3),
 *       superseded              (4),
 *       cessationOfOperation    (5),
 *       certificateHold         (6) }
 *
 */

struct NSSPKIXReasonFlagsStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXReasonFlagsMask reasonFlags;
};

/*
 * ExtKeyUsageSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
 *
 */

struct NSSPKIXExtKeyUsageSyntaxStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * AuthorityInfoAccessSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AuthorityInfoAccessSyntax  ::=
 *          SEQUENCE SIZE (1..MAX) OF AccessDescription
 *
 */

struct NSSPKIXAuthorityInfoAccessSyntaxStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  ...
};

/*
 * AccessDescription
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AccessDescription  ::=  SEQUENCE {
 *          accessMethod          OBJECT IDENTIFIER,
 *          accessLocation        GeneralName  }
 *
 */

struct NSSPKIXAccessDescriptionStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSOID *accessMethod;
  NSSPKIXGeneralName *accessLocation;
};

/*
 * IssuingDistributionPoint
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  IssuingDistributionPoint ::= SEQUENCE {
 *       distributionPoint       [0] DistributionPointName OPTIONAL,
 *       onlyContainsUserCerts   [1] BOOLEAN DEFAULT FALSE,
 *       onlyContainsCACerts     [2] BOOLEAN DEFAULT FALSE,
 *       onlySomeReasons         [3] ReasonFlags OPTIONAL,
 *       indirectCRL             [4] BOOLEAN DEFAULT FALSE }
 *
 */

struct NSSPKIXIssuingDistributionPointStr {
  NSSArena *arena;
  PRBool i_allocated_arena;
  NSSDER *der;
  NSSPKIXDistributionPointName *distributionPoint;
  PRBool onlyContainsUserCerts;
  PRBool onlyContainsCACerts;
  NSSPKIXReasonFlags onlySomeReasons;
  PRBool indirectCRL;
};

PR_END_EXTERN_C

#endif /* PKIXTM_H */
