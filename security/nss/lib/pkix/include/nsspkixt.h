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

#ifndef NSSPKIXT_H
#define NSSPKIXT_H

#ifdef DEBUG
static const char NSSPKIXT_CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/include/Attic/nsspkixt.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:02:44 $ $Name:  $";
#endif /* DEBUG */

/*
 * nsspkixt.h
 *
 * This file contains the public type definitions for the PKIX part-1
 * objects.  The contents are strongly based on the types defined in
 * RFC 2459 {fgmr: and others, e.g., s/mime?}.  The type names have
 * been prefixed with "NSSPKIX."
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

PR_BEGIN_EXTERN_C

/*
 * A Note About Strings
 *
 * RFC 2459 refers to several types of strings, including UniversalString,
 * BMPString, UTF8String, TeletexString, PrintableString, IA5String, and
 * more.  As with the rest of the NSS libraries, all strings are converted
 * to UTF8 form as soon as possible, and dealt with in that form from
 * then on.
 *
 */

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

struct NSSPKIXAttributeStr;
typedef struct NSSPKIXAttributeStr NSSPKIXAttribute;

/*
 * AttributeType
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AttributeType           ::=   OBJECT IDENTIFIER
 *
 */

typedef NSSOID NSSPKIXAttributeType;

/*
 * AttributeValue
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AttributeValue          ::=   ANY
 *
 */

typedef NSSItem NSSPKIXAttributeValue;

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

struct NSSPKIXAttributeTypeAndValueStr;
typedef struct NSSPKIXAttributeTypeAndValueStr NSSPKIXAttributeTypeAndValue;

/*
 * X520Name
 *
 * -- fgmr comments --
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
 */

struct NSSPKIXX520NameStr;
typedef struct NSSPKIXX520NameStr NSSPKIXX520Name;

/*
 * X520CommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520CommonName  ::=      CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-common-name)),
 *        printableString       PrintableString (SIZE (1..ub-common-name)),
 *        universalString       UniversalString (SIZE (1..ub-common-name)),
 *        utf8String            UTF8String (SIZE (1..ub-common-name)),
 *        bmpString             BMPString (SIZE(1..ub-common-name))   }
 * 
 */

struct NSSPKIXX520CommonNameStr;
typedef struct NSSPKIXX520CommonNameStr NSSPKIXX520CommonName;

/*
 * X520LocalityName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520LocalityName ::= CHOICE {
 *        teletexString       TeletexString (SIZE (1..ub-locality-name)),
 *        printableString     PrintableString (SIZE (1..ub-locality-name)),
 *        universalString     UniversalString (SIZE (1..ub-locality-name)),
 *        utf8String          UTF8String (SIZE (1..ub-locality-name)),
 *        bmpString           BMPString (SIZE(1..ub-locality-name))   }
 * 
 */

struct NSSPKIXX520LocalityNameStr;
typedef struct NSSPKIXX520LocalityNameStr NSSPKIXX520LocalityName;

/*
 * X520StateOrProvinceName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520StateOrProvinceName         ::= CHOICE {
 *        teletexString       TeletexString (SIZE (1..ub-state-name)),
 *        printableString     PrintableString (SIZE (1..ub-state-name)),
 *        universalString     UniversalString (SIZE (1..ub-state-name)),
 *        utf8String          UTF8String (SIZE (1..ub-state-name)),
 *        bmpString           BMPString (SIZE(1..ub-state-name))   }
 *
 */

struct NSSPKIXX520StateOrProvinceNameStr;
typedef struct NSSPKIXX520StateOrProvinceNameStr NSSPKIXX520StateOrProvinceName;

/*
 * X520OrganizationName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520OrganizationName ::= CHOICE {
 *    teletexString     TeletexString (SIZE (1..ub-organization-name)),
 *    printableString   PrintableString (SIZE (1..ub-organization-name)),
 *    universalString   UniversalString (SIZE (1..ub-organization-name)),
 *    utf8String        UTF8String (SIZE (1..ub-organization-name)),
 *    bmpString         BMPString (SIZE(1..ub-organization-name))   }
 *
 */

struct NSSPKIXX520OrganizationNameStr;
typedef struct NSSPKIXX520OrganizationNameStr NSSPKIXX520OrganizationName;

/*
 * X520OrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520OrganizationalUnitName ::= CHOICE {
 *   teletexString    TeletexString (SIZE (1..ub-organizational-unit-name)),
 *   printableString        PrintableString
 *                        (SIZE (1..ub-organizational-unit-name)),
 *   universalString        UniversalString
 *                        (SIZE (1..ub-organizational-unit-name)),
 *   utf8String       UTF8String (SIZE (1..ub-organizational-unit-name)),
 *   bmpString        BMPString (SIZE(1..ub-organizational-unit-name))   }
 *
 */

struct NSSPKIXX520OrganizationalUnitNameStr;
typedef struct NSSPKIXX520OrganizationalUnitNameStr NSSPKIXX520OrganizationalUnitName;

/*
 * X520Title
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520Title ::=   CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-title)),
 *        printableString       PrintableString (SIZE (1..ub-title)),
 *        universalString       UniversalString (SIZE (1..ub-title)),
 *        utf8String            UTF8String (SIZE (1..ub-title)),
 *        bmpString             BMPString (SIZE(1..ub-title))   }
 *
 */

struct NSSPKIXX520TitleStr;
typedef struct NSSPKIXX520TitleStr NSSPKIXX520Title;

/*
 * X520dnQualifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520dnQualifier ::=     PrintableString
 *
 */

struct NSSPKIXX520dnQualifierStr;
typedef struct NSSPKIXX520dnQualifierStr NSSPKIXX520dnQualifier;

/*
 * X520countryName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520countryName ::=     PrintableString (SIZE (2)) -- IS 3166 codes
 *
 */

struct NSSPKIXX520countryNameStr;
typedef struct NSSPKIXX520countryNameStr NSSPKIXX520countryName;

/*
 * Pkcs9email
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Pkcs9email ::= IA5String (SIZE (1..ub-emailaddress-length))
 *
 */

struct NSSPKIXPkcs9emailStr;
typedef struct NSSPKIXPkcs9emailStr NSSPKIXPkcs9email;

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

struct NSSPKIXNameStr;
typedef struct NSSPKIXNameStr NSSPKIXName;

/*
 * NameChoice
 *
 * This enumeration is used to specify choice within a name.
 */

enum NSSPKIXNameChoiceEnum {
  NSSPKIXNameChoice_NSSinvalid = -1,
  NSSPKIXNameChoice_rdnSequence
};
typedef enum NSSPKIXNameChoiceEnum NSSPKIXNameChoice;

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

struct NSSPKIXRDNSequenceStr;
typedef struct NSSPKIXRDNSequenceStr NSSPKIXRDNSequence;

/*
 * DistinguishedName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DistinguishedName       ::=   RDNSequence
 *
 */

typedef NSSPKIXRDNSequence NSSPKIXDistinguishedName;

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

struct NSSPKIXRelativeDistinguishedNameStr;
typedef struct NSSPKIXRelativeDistinguishedNameStr NSSPKIXRelativeDistinguishedName;

/*
 * DirectoryString
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DirectoryString ::= CHOICE {
 *        teletexString             TeletexString (SIZE (1..MAX)),
 *        printableString           PrintableString (SIZE (1..MAX)),
 *        universalString           UniversalString (SIZE (1..MAX)),
 *        utf8String              UTF8String (SIZE (1..MAX)),
 *        bmpString               BMPString (SIZE(1..MAX))   }
 *
 */

struct NSSPKIXDirectoryStringStr;
typedef struct NSSPKIXDirectoryStringStr NSSPKIXDirectoryString;

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

struct NSSPKIXCertificateStr;
typedef struct NSSPKIXCertificateStr NSSPKIXCertificate;

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

struct NSSPKIXTBSCertificateStr;
typedef struct NSSPKIXTBSCertificateStr NSSPKIXTBSCertificate;

/*
 * Version
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 *
 */

enum NSSPKIXVersionEnum {
  NSSPKIXVersion_NSSinvalid = -1,
  NSSPKIXVersion_v1 = 0,
  NSSPKIXVersion_v2 = 1,
  NSSPKIXVersion_v3 = 2
};
typedef enum NSSPKIXVersionEnum NSSPKIXVersion;

/*
 * CertificateSerialNumber
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificateSerialNumber  ::=  INTEGER
 *
 */

typedef NSSBER NSSPKIXCertificateSerialNumber;

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

struct NSSPKIXValidityStr;
typedef struct NSSPKIXValidityStr NSSPKIXValidity;

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

struct NSSPKIXTimeStr;
typedef struct NSSPKIXTimeStr NSSPKIXTime;

/*
 * UniqueIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  UniqueIdentifier  ::=  BIT STRING
 *
 */

typedef NSSBitString NSSPKIXUniqueIdentifier;

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

struct NSSPKIXSubjectPublicKeyInfoStr;
typedef NSSPKIXSubjectPublicKeyInfoStr NSSPKIXSubjectPublicKeyInfo;

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

struct NSSPKIXExtensionsStr;
typedef struct NSSPKIXExtensionsStr NSSPKIXExtensions;

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

struct NSSPKIXExtensionStr;
typedef struct NSSPKIXExtensionStr NSSPKIXExtension;

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

struct NSSPKIXCertificateListStr;
typedef struct NSSPKIXCertificateListStr NSSPKIXCertificateList;

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

struct NSSPKIXTBSCertListStr;
typedef struct NSSPKIXTBSCertListStr NSSPKIXTBSCertList;

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

struct NSSPKIXrevokedCertificatesStr;
typedef struct NSSPKIXrevokedCertificatesStr NSSPKIXrevokedCertificates;

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

struct NSSPKIXrevokedCertificateStr;
typedef struct NSSPKIXrevokedCertificateStr NSSPKIXrevokedCertificate;

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

struct NSSPKIXAlgorithmIdentifierStr;
typedef NSSPKIXAlgorithmIdentifierStr NSSPKIXAlgorithmIdentifier;

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

struct NSSPKIXORAddressStr;
typedef struct NSSPKIXORAddressStr NSSPKIXORAddress;

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

struct NSSPKIXBuiltInStandardAttributesStr;
typedef struct NSSPKIXBuiltInStandardAttributesStr NSSPKIXBuiltInStandardAttributes;

/*
 * CountryName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CountryName ::= [APPLICATION 1] CHOICE {
 *     x121-dcc-code NumericString
 *                  (SIZE (ub-country-name-numeric-length)),
 *     iso-3166-alpha2-code PrintableString
 *                  (SIZE (ub-country-name-alpha-length)) }
 *
 */

struct NSSPKIXCountryNameStr;
typedef struct NSSPKIXCountryNameStr NSSPKIXCountryName;

/*
 * AdministrationDomainName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AdministrationDomainName ::= [APPLICATION 2] CHOICE {
 *     numeric NumericString (SIZE (0..ub-domain-name-length)),
 *     printable PrintableString (SIZE (0..ub-domain-name-length)) }
 *
 */

struct NSSPKIXAdministrationDomainNameStr;
typedef struct NSSPKIXAdministrationDomainNameStr NSSPKIXAdministrationDomainName;

/*
 * X121Address
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X121Address ::= NumericString (SIZE (1..ub-x121-address-length))
 *
 */

struct NSSPKIXX121AddressStr;
typedef struct NSSPKIXX121AddressStr NSSPKIXX121Address;

/*
 * NetworkAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NetworkAddress ::= X121Address  -- see also extended-network-address
 *
 */

struct NSSPKIXNetworkAddressStr;
typedef struct NSSPKIXNetworkAddressStr NSSPKIXNetworkAddress;

/*
 * TerminalIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TerminalIdentifier ::= PrintableString (SIZE (1..ub-terminal-id-length))
 *
 */

struct NSSPKIXTerminalIdentifierStr;
typedef struct NSSPKIXTerminalIdentifierStr NSSPKIXTerminalIdentifier;

/*
 * PrivateDomainName
 *
 * -- fgmr comments -- 
 *
 *  PrivateDomainName ::= CHOICE {
 *     numeric NumericString (SIZE (1..ub-domain-name-length)),
 *     printable PrintableString (SIZE (1..ub-domain-name-length)) }
 *
 */

struct NSSPKIXPrivateDomainNameStr;
typedef struct NSSPKIXPrivateDomainNameStr NSSPKIXPrivateDomainName;

/*
 * OrganizationName
 *
 * -- fgmr comments --
 *
 *  OrganizationName ::= PrintableString
 *                              (SIZE (1..ub-organization-name-length))
 * 
 */

struct NSSPKIXOrganizationNameStr;
typedef struct NSSPKIXOrganizationNameStr NSSPKIXOrganizationName;

/*
 * NumericUserIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NumericUserIdentifier ::= NumericString
 *                              (SIZE (1..ub-numeric-user-id-length))
 * 
 */

struct NSSPKIXNumericUserIdentifierStr;
typedef struct NSSPKIXNumericUserIdentifierStr NSSPKIXNumericUserIdentifier;

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

struct NSSPKIXPersonalNameStr;
typedef NSSPKIXPersonalNameStr NSSPKIXPersonalName;

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

struct NSSPKIXOrganizationalUnitNamesStr;
typedef NSSPKIXOrganizationalUnitNamesStr NSSPKIXOrganizationalUnitNames;

/*
 * OrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  OrganizationalUnitName ::= PrintableString (SIZE
 *                          (1..ub-organizational-unit-name-length))
 *
 */

struct NSSPKIXOrganizationalUnitNameStr;
typedef struct NSSPKIXOrganizationalUnitNameStr NSSPKIXOrganizationalUnitName;

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

struct NSSPKIXBuiltInDomainDefinedAttributesStr;
typedef struct NSSPKIXBuiltInDomainDefinedAttributesStr NSSPKIXBuiltInDomainDefinedAttributes;

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

struct NSSPKIXBuiltInDomainDefinedAttributeStr;
typedef struct NSSPKIXBuiltInDomainDefinedAttributeStr NSSPKIXBuiltInDomainDefinedAttribute;

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

struct NSSPKIXExtensionAttributesStr;
typedef struct NSSPKIXExtensionAttributesStr NSSPKIXExtensionAttributes;

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

struct NSSPKIXExtensionAttributeStr;
typedef struct NSSPKIXExtensionAttributeStr NSSPKIXExtensionAttribute;

/*
 * ExtensionAttributeType
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  common-name INTEGER ::= 1
 *  teletex-common-name INTEGER ::= 2
 *  teletex-organization-name INTEGER ::= 3
 *  teletex-personal-name INTEGER ::= 4
 *  teletex-organizational-unit-names INTEGER ::= 5
 *  teletex-domain-defined-attributes INTEGER ::= 6
 *  pds-name INTEGER ::= 7
 *  physical-delivery-country-name INTEGER ::= 8
 *  postal-code INTEGER ::= 9
 *  physical-delivery-office-name INTEGER ::= 10
 *  physical-delivery-office-number INTEGER ::= 11
 *  extension-OR-address-components INTEGER ::= 12
 *  physical-delivery-personal-name INTEGER ::= 13
 *  physical-delivery-organization-name INTEGER ::= 14
 *  extension-physical-delivery-address-components INTEGER ::= 15
 *  unformatted-postal-address INTEGER ::= 16
 *  street-address INTEGER ::= 17
 *  post-office-box-address INTEGER ::= 18
 *  poste-restante-address INTEGER ::= 19
 *  unique-postal-name INTEGER ::= 20
 *  local-postal-attributes INTEGER ::= 21
 *  extended-network-address INTEGER ::= 22
 *  terminal-type  INTEGER ::= 23
 *
 */

enum NSSPKIXExtensionAttributeTypeEnum {
  NSSPKIXExtensionAttributeType_NSSinvalid = -1,
  NSSPKIXExtensionAttributeType_CommonName = 1,
  NSSPKIXExtensionAttributeType_TeletexCommonName = 2,
  NSSPKIXExtensionAttributeType_TeletexOrganizationName = 3,
  NSSPKIXExtensionAttributeType_TeletexPersonalName = 4,
  NSSPKIXExtensionAttributeType_TeletexOrganizationalUnitNames = 5,
  NSSPKIXExtensionAttributeType_TeletexDomainDefinedAttributes = 6,
  NSSPKIXExtensionAttributeType_PdsName = 7,
  NSSPKIXExtensionAttributeType_PhysicalDeliveryCountryName = 8,
  NSSPKIXExtensionAttributeType_PostalCode = 9,
  NSSPKIXExtensionAttributeType_PhysicalDeliveryOfficeName = 10,
  NSSPKIXExtensionAttributeType_PhysicalDeliveryOfficeNumber = 11,
  NSSPKIXExtensionAttributeType_ExtensionOrAddressComponents = 12,
  NSSPKIXExtensionAttributeType_PhysicalDeliveryPersonalName = 13,
  NSSPKIXExtensionAttributeType_PhysicalDeliveryOrganizationName = 14,
  NSSPKIXExtensionAttributeType_ExtensionPhysicalDeliveryAddressComponents = 15,
  NSSPKIXExtensionAttributeType_UnformattedPostalAddress = 16,
  NSSPKIXExtensionAttributeType_StreetAddress = 17,
  NSSPKIXExtensionAttributeType_PostOfficeBoxAddress = 18,
  NSSPKIXExtensionAttributeType_PosteRestanteAddress = 19,
  NSSPKIXExtensionAttributeType_UniquePostalName = 20,
  NSSPKIXExtensionAttributeType_LocalPostalAttributes = 21,
  NSSPKIXExtensionAttributeType_ExtendedNetworkAddress = 22,
  NSSPKIXExtensionAttributeType_TerminalType = 23
};
typedef enum NSSPKIXExtensionAttributeTypeEnum NSSPKIXExtensionAttributeType;

/*
 * CommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CommonName ::= PrintableString (SIZE (1..ub-common-name-length))
 *
 */

struct NSSPKIXCommonNameStr;
typedef struct NSSPKIXCommonNameStr NSSPKIXCommonName;

/*
 * TeletexCommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexCommonName ::= TeletexString (SIZE (1..ub-common-name-length))
 *
 */

struct NSSPKIXTeletexCommonNameStr;
typedef struct NSSPKIXTeletexCommonNameStr NSSPKIXTeletexCommonName;

/*
 * TeletexOrganizationName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexOrganizationName ::=
 *                  TeletexString (SIZE (1..ub-organization-name-length))
 *
 */

struct NSSPKIXTeletexOrganizationNameStr;
typedef struct NSSPKIXTeletexOrganizationNameStr NSSPKIXTeletexOrganizationName;

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

struct NSSPKIXTeletexPersonalNameStr;
typedef struct NSSPKIXTeletexPersonalNameStr NSSPKIXTeletexPersonalName;

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

struct NSSPKIXTeletexOrganizationalUnitNamesStr;
typedef struct NSSPKIXTeletexOrganizationalUnitNamesStr NSSPKIXTeletexOrganizationalUnitNames;

/*
 * TeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexOrganizationalUnitName ::= TeletexString
 *                          (SIZE (1..ub-organizational-unit-name-length))
 *  
 */

struct NSSPKIXTeletexOrganizationalUnitNameStr;
typedef struct NSSPKIXTeletexOrganizationalUnitNameStr NSSPKIXTeletexOrganizationalUnitName;

/*
 * PDSName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PDSName ::= PrintableString (SIZE (1..ub-pds-name-length))
 *
 */

struct NSSPKIXPDSNameStr;
typedef struct NSSPKIXPDSNameStr NSSPKIXPDSName;

/*
 * PhysicalDeliveryCountryName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PhysicalDeliveryCountryName ::= CHOICE {
 *     x121-dcc-code NumericString (SIZE (ub-country-name-numeric-length)),
 *     iso-3166-alpha2-code PrintableString
 *                          (SIZE (ub-country-name-alpha-length)) }
 *
 */

struct NSSPKIXPhysicalDeliveryCountryNameStr;
typedef struct NSSPKIXPhysicalDeliveryCountryNameStr NSSPKIXPhysicalDeliveryCountryName;

/*
 * PostalCode
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PostalCode ::= CHOICE {
 *     numeric-code NumericString (SIZE (1..ub-postal-code-length)),
 *     printable-code PrintableString (SIZE (1..ub-postal-code-length)) }
 *
 */

struct NSSPKIXPostalCodeStr;
typedef struct NSSPKIXPostalCodeStr NSSPKIXPostalCode;

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

struct NSSPKIXPDSParameterStr;
typedef struct NSSPKIXPDSParameterStr NSSPKIXPDSParameter;

/*
 * PhysicalDeliveryOfficeName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PhysicalDeliveryOfficeName ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXPhysicalDeliveryOfficeName;

/*
 * PhysicalDeliveryOfficeNumber
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PhysicalDeliveryOfficeNumber ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXPhysicalDeliveryOfficeNumber;

/*
 * ExtensionORAddressComponents
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtensionORAddressComponents ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXExtensionORAddressComponents;

/*
 * PhysicalDeliveryPersonalName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PhysicalDeliveryPersonalName ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXPhysicalDeliveryPersonalName;

/*
 * PhysicalDeliveryOrganizationName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PhysicalDeliveryOrganizationName ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXPhysicalDeliveryOrganizationName;

/*
 * ExtensionPhysicalDeliveryAddressComponents
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtensionPhysicalDeliveryAddressComponents ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXExtensionPhysicalDeliveryAddressComponents;

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

struct NSSPKIXUnformattedPostalAddressStr;
typedef struct NSSPKIXUnformattedPostalAddressStr NSSPKIXUnformattedPostalAddress;

/*
 * StreetAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  StreetAddress ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXStreetAddress;

/*
 * PostOfficeBoxAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PostOfficeBoxAddress ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXPostOfficeBoxAddress;

/*
 * PosteRestanteAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PosteRestanteAddress ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXPosteRestanteAddress;

/*
 * UniquePostalName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  UniquePostalName ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXUniquePostalName;

/*
 * LocalPostalAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  LocalPostalAttributes ::= PDSParameter
 *
 */

typedef NSSPKIXPDSParameter NSSPKIXLocalPostalAttributes;

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

struct NSSPKIXExtendedNetworkAddressStr;
typedef struct NSSPKIXExtendedNetworkAddressStr NSSPKIXExtendedNetworkAddress;

/*
 * NSSPKIXExtendedNetworkAddressChoice
 *
 * Helper enumeration for ExtendedNetworkAddress
 * -- fgmr comments --
 *
 */

enum NSSPKIXExtendedNetworkAddressEnum {
  NSSPKIXExtendedNetworkAddress_NSSinvalid = -1,
  NSSPKIXExtendedNetworkAddress_e1634Address,
  NSSPKIXExtendedNetworkAddress_psapAddress
};
typedef enum NSSPKIXExtendedNetworkAddressEnum NSSPKIXExtendedNetworkAddressChoice;

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

struct NSSe1634addressStr;
typedef struct NSSe1634addressStr NSSe1634address;

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

struct NSSPKIXPresentationAddressStr;
typedef struct NSSPKIXPresentationAddressStr NSSPKIXPresentationAddress;

/*
 * TerminalType
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TerminalType ::= INTEGER {
 *     telex (3),
 *     teletex (4),
 *     g3-facsimile (5),
 *     g4-facsimile (6),
 *     ia5-terminal (7),
 *     videotex (8) } (0..ub-integer-options)
 *
 */

enum NSSPKIXTerminalTypeEnum {
  NSSPKIXTerminalType_NSSinvalid = -1,
  NSSPKIXTerminalType_telex = 3,
  NSSPKIXTerminalType_teletex = 4,
  NSSPKIXTerminalType_g3Facsimile = 5,
  NSSPKIXTerminalType_g4Facsimile = 6,
  NSSPKIXTerminalType_iA5Terminal = 7,
  NSSPKIXTerminalType_videotex = 8
};
typedef enum NSSPKIXTerminalTypeEnum NSSPKIXTerminalType;

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

struct NSSPKIXTeletexDomainDefinedAttributesStr;
typedef struct NSSPKIXTeletexDomainDefinedAttributesStr NSSPKIXTeletexDomainDefinedAttributes;

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

struct NSSPKIXTeletexDomainDefinedAttributeStr;
typedef struct NSSPKIXTeletexDomainDefinedAttributeStr NSSPKIXTeletexDomainDefinedAttribute;

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

struct NSSPKIXAuthorityKeyIdentifierStr;
typedef struct NSSPKIXAuthorityKeyIdentifierStr NSSPKIXAuthorityKeyIdentifier;

/*
 * KeyIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  KeyIdentifier ::= OCTET STRING
 *
 */

typedef NSSItem NSSPKIXKeyIdentifier;

/*
 * SubjectKeyIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectKeyIdentifier ::= KeyIdentifier
 *
 */

typedef NSSPKIXKeyIdentifier NSSPKIXSubjectKeyIdentifier;

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

struct NSSPKIXKeyUsageStr;
typedef struct NSSPKIXKeyUsageStr NSSPKIXKeyUsage;

/*
 * KeyUsageValue
 *
 * -- helper for testing many key usages at once
 *
 */

enum NSSPKIXKeyUsageValueEnum {
  NSSPKIXKeyUsage_NSSinvalid = 0,
  NSSPKIXKeyUsage_DigitalSignature = 0x001,
  NSSPKIXKeyUsage_NonRepudiation   = 0x002,
  NSSPKIXKeyUsage_KeyEncipherment  = 0x004,
  NSSPKIXKeyUsage_DataEncipherment = 0x008,
  NSSPKIXKeyUsage_KeyAgreement     = 0x010,
  NSSPKIXKeyUsage_KeyCertSign      = 0x020,
  NSSPKIXKeyUsage_CRLSign          = 0x040,
  NSSPKIXKeyUsage_EncipherOnly     = 0x080,
  NSSPKIXKeyUsage_DecipherOnly     = 0x100
};
typedef enum NSSPKIXKeyUsageValueEnum NSSPKIXKeyUsageValue;

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

struct NSSPKIXPrivateKeyUsagePeriodStr;
typedef struct NSSPKIXPrivateKeyUsagePeriodStr NSSPKIXPrivateKeyUsagePeriod;

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

struct NSSPKIXCertificatePoliciesStr;
typedef struct NSSPKIXCertificatePoliciesStr NSSPKIXCertificatePolicies;

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

struct NSSPKIXPolicyInformationStr;
typedef struct NSSPKIXPolicyInformationStr NSSPKIXPolicyInformation;

/*
 * CertPolicyId
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertPolicyId ::= OBJECT IDENTIFIER
 *
 */

typedef NSSOID NSSPKIXCertPolicyId;

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

struct NSSPKIXPolicyQualifierInfoStr;
typedef NSSPKIXPolicyQualifierInfoStr NSSPKIXPolicyQualifierInfo;

/*
 * PolicyQualifierId
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyQualifierId ::=
 *      OBJECT IDENTIFIER ( id-qt-cps | id-qt-unotice )
 *
 */

typedef NSSOID NSSPKIXPolicyQualifierId;

/*
 * CPSuri
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CPSuri ::= IA5String
 *
 */

struct NSSPKIXCPSuriStr;
typedef struct NSSPKIXCPSuriStr NSSPKIXCPSuri;

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

struct NSSPKIXUserNoticeStr;
typedef struct NSSPKIXUserNoticeStr NSSPKIXUserNotice;

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

struct NSSPKIXNoticeReferenceStr;
typedef struct NSSPKIXNoticeReferenceStr NSSPKIXNoticeReference;

/*
 * DisplayText
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DisplayText ::= CHOICE {
 *       visibleString    VisibleString  (SIZE (1..200)),
 *       bmpString        BMPString      (SIZE (1..200)),
 *       utf8String       UTF8String     (SIZE (1..200)) }
 * 
 */

struct NSSPKIXDisplayTextStr;
typedef struct NSSPKIXDisplayTextStr NSSPKIXDisplayText;

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

struct NSSPKIXPolicyMappingsStr;
typedef struct NSSPKIXPolicyMappingsStr NSSPKIXPolicyMappings;

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

struct NSSPKIXpolicyMappingStr;
typedef struct NSSPKIXpolicyMappingStr NSSPKIXpolicyMapping;

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

struct NSSPKIXGeneralNameStr;
typedef struct NSSPKIXGeneralNameStr NSSPKIXGeneralName;

/*
 * GeneralNameChoice
 *
 * This enumerates the possible general name types.
 */

enum NSSPKIXGeneralNameChoiceEnum {
  NSSPKIXGeneralNameChoice_NSSinvalid = -1,
  NSSPKIXGeneralNameChoice_otherName = 0,
  NSSPKIXGeneralNameChoice_rfc822Name = 1,
  NSSPKIXGeneralNameChoice_dNSName = 2,
  NSSPKIXGeneralNameChoice_x400Address = 3,
  NSSPKIXGeneralNameChoice_directoryName = 4,
  NSSPKIXGeneralNameChoice_ediPartyName = 5,
  NSSPKIXGeneralNameChoice_uniformResourceIdentifier = 6,
  NSSPKIXGeneralNameChoice_iPAddress = 7,
  NSSPKIXGeneralNameChoice_registeredID = 8
};
typedef enum NSSPKIXGeneralNameChoiceEnum NSSPKIXGeneralNameChoice;

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

struct NSSPKIXGeneralNamesStr;
typedef struct NSSPKIXGeneralNamesStr NSSPKIXGeneralNames;

/*
 * SubjectAltName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectAltName ::= GeneralNames
 *
 */

typedef NSSPKIXGeneralNames NSSPKIXSubjectAltName;

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

struct NSSPKIXAnotherNameStr;
typedef struct NSSPKIXAnotherNameStr NSSPKIXAnotherName;

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

struct NSSPKIXEDIPartyNameStr;
typedef struct NSSPKIXEDIPartyNameStr NSSPKIXEDIPartyName;

/*
 * IssuerAltName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  IssuerAltName ::= GeneralNames
 *
 */

typedef NSSPKIXGeneralNames NSSPKIXIssuerAltName;

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

struct NSSPKIXSubjectDirectoryAttributesStr;
typedef struct NSSPKIXSubjectDirectoryAttributesStr NSSPKIXSubjectDirectoryAttributes;

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

struct NSSPKIXBasicConstraintsStr;
typedef struct NSSPKIXBasicConstraintsStr NSSPKIXBasicConstraints;

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

struct NSSPKIXNameConstraintsStr;
typedef struct NSSPKIXNameConstraintsStr NSSPKIXNameConstraints;

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

struct NSSPKIXGeneralSubtreesStr;
typedef struct NSSPKIXGeneralSubtreesStr NSSPKIXGeneralSubtrees;

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

struct NSSPKIXGeneralSubtreeStr;
typedef struct NSSPKIXGeneralSubtreeStr NSSPKIXGeneralSubtree;

/*
 * BaseDistance
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BaseDistance ::= INTEGER (0..MAX)
 *
 */

typedef PRInt32 NSSPKIXBaseDistance;

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

struct NSSPKIXPolicyConstraintsStr;
typedef NSSPKIXPolicyConstraintsStr NSSPKIXPolicyConstraints;

/*
 * SkipCerts
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SkipCerts ::= INTEGER (0..MAX)
 *
 */

typedef NSSItem NSSPKIXSkipCerts;

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

struct NSSPKIXCRLDistPointsSyntaxStr;
typedef struct NSSPKIXCRLDistPointsSyntaxStr NSSPKIXCRLDistPointsSyntax;

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

struct NSSPKIXDistributionPointStr;
typedef struct NSSPKIXDistributionPointStr NSSPKIXDistributionPoint;

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

struct NSSPKIXDistributionPointNameStr;
typedef struct NSSPKIXDistributionPointNameStr NSSPKIXDistributionPointName;

/*
 * DistributionPointNameChoice
 *
 * -- fgmr comments --
 *
 */

enum NSSPKIXDistributionPointNameChoiceEnum {
  NSSDistributionPointNameChoice_NSSinvalid = -1,
  NSSDistributionPointNameChoice_FullName = 0,
  NSSDistributionPointNameChoice_NameRelativeToCRLIssuer = 1
};
typedef enum NSSPKIXDistributionPointNameChoiceEnum NSSPKIXDistributionPointNameChoice;

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

struct NSSPKIXReasonFlagsStr;
typedef struct NSSPKIXReasonFlagsStr NSSPKIXReasonFlags;

/*
 * ReasonFlagsMask
 *
 * -- fgmr comments --
 *
 */

typedef PRInt32 NSSPKIXReasonFlagsMask;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_NSSinvalid          =  -1;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_KeyCompromise       =  0x02;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_CACompromise        =  0x04;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_AffiliationChanged  =  0x08;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_Superseded          =  0x10;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_CessationOfOperation=  0x20;
const NSSPKIXReasonFlagsMask NSSPKIXReasonFlagsMask_CertificateHold     =  0x40;

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

struct NSSPKIXExtKeyUsageSyntaxStr;
typedef struct NSSPKIXExtKeyUsageSyntaxStr NSSPKIXExtKeyUsageSyntax;

/*
 * KeyPurposeId
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  KeyPurposeId ::= OBJECT IDENTIFIER
 *
 */

typedef NSSOID NSSPKIXKeyPurposeId;

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

struct NSSPKIXAuthorityInfoAccessSyntaxStr;
typedef struct NSSPKIXAuthorityInfoAccessSyntaxStr NSSPKIXAuthorityInfoAccessSyntax;

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

struct NSSPKIXAccessDescriptionStr;
typedef struct NSSPKIXAccessDescriptionStr NSSPKIXAccessDescription;

/*
 * CRLNumber
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CRLNumber ::= INTEGER (0..MAX)
 *
 */

typedef NSSItem NSSPKIXCRLNumber;

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

struct NSSPKIXIssuingDistributionPointStr;
typedef struct NSSPKIXIssuingDistributionPointStr NSSPKIXIssuingDistributionPoint;

/*
 * BaseCRLNumber
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BaseCRLNumber ::= CRLNumber
 *
 */

typedef NSSPKIXCRLNumber NSSPKIXBaseCRLNumber;

/*
 * CRLReason
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CRLReason ::= ENUMERATED {
 *       unspecified             (0),
 *       keyCompromise           (1),
 *       cACompromise            (2),
 *       affiliationChanged      (3),
 *       superseded              (4),
 *       cessationOfOperation    (5),
 *       certificateHold         (6),
 *       removeFromCRL           (8) }
 * 
 */

enum NSSPKIXCRLReasonEnum {
  NSSPKIXCRLReasonEnum_NSSinvalid = -1,
  NSSPKIXCRLReasonEnum_unspecified = 0,
  NSSPKIXCRLReasonEnum_keyCompromise = 1,
  NSSPKIXCRLReasonEnum_cACompromise = 2,
  NSSPKIXCRLReasonEnum_affiliationChanged = 3,
  NSSPKIXCRLReasonEnum_superseded = 4,
  NSSPKIXCRLReasonEnum_cessationOfOperation = 5,
  NSSPKIXCRLReasonEnum_certificateHold = 6,
  NSSPKIXCRLReasonEnum_removeFromCRL = 8
};
typedef enum NSSPKIXCRLReasonEnum NSSPKIXCRLReason;

/*
 * CertificateIssuer
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificateIssuer ::= GeneralNames
 *
 */

typedef NSSPKIXGeneralNames NSSPKIXCertificateIssuer;

/*
 * HoldInstructionCode
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  HoldInstructionCode ::= OBJECT IDENTIFIER
 *
 */

typedef NSSOID NSSPKIXHoldInstructionCode;

/*
 * InvalidityDate
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  InvalidityDate ::=  GeneralizedTime
 *
 */

typedef PRTime NSSPKIXInvalidityDate;

PR_END_EXTERN_C

#endif /* NSSPKIXT_H */
