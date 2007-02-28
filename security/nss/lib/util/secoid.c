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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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

#include "secoid.h"
#include "pkcs11t.h"
#include "secmodt.h"
#include "secitem.h"
#include "secerr.h"
#include "plhash.h"
#include "nssrwlk.h"

/* MISSI Mosaic Object ID space */
#define USGOV                   0x60, 0x86, 0x48, 0x01, 0x65
#define MISSI	                USGOV, 0x02, 0x01, 0x01
#define MISSI_OLD_KEA_DSS	MISSI, 0x0c
#define MISSI_OLD_DSS		MISSI, 0x02
#define MISSI_KEA_DSS		MISSI, 0x14
#define MISSI_DSS		MISSI, 0x13
#define MISSI_KEA               MISSI, 0x0a
#define MISSI_ALT_KEA           MISSI, 0x16

#define NISTALGS    USGOV, 3, 4
#define AES         NISTALGS, 1
#define SHAXXX      NISTALGS, 2

/**
 ** The Netscape OID space is allocated by Terry Hayes.  If you need
 ** a piece of the space, contact him at thayes@netscape.com.
 **/

/* Netscape Communications Corporation Object ID space */
/* { 2 16 840 1 113730 } */
#define NETSCAPE_OID	          0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42
#define NETSCAPE_CERT_EXT 	  NETSCAPE_OID, 0x01
#define NETSCAPE_DATA_TYPE 	  NETSCAPE_OID, 0x02
/* netscape directory oid - owned by Mark Smith (mcs@netscape.com) */
#define NETSCAPE_DIRECTORY 	  NETSCAPE_OID, 0x03
#define NETSCAPE_POLICY 	  NETSCAPE_OID, 0x04
#define NETSCAPE_CERT_SERVER 	  NETSCAPE_OID, 0x05
#define NETSCAPE_ALGS 		  NETSCAPE_OID, 0x06 /* algorithm OIDs */
#define NETSCAPE_NAME_COMPONENTS  NETSCAPE_OID, 0x07

#define NETSCAPE_CERT_EXT_AIA     NETSCAPE_CERT_EXT, 0x10
#define NETSCAPE_CERT_SERVER_CRMF NETSCAPE_CERT_SERVER, 0x01

/* these are old and should go away soon */
#define OLD_NETSCAPE		0x60, 0x86, 0x48, 0xd8, 0x6a
#define NS_CERT_EXT		OLD_NETSCAPE, 0x01
#define NS_FILE_TYPE		OLD_NETSCAPE, 0x02
#define NS_IMAGE_TYPE		OLD_NETSCAPE, 0x03

/* RSA OID name space */
#define RSADSI			0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d
#define PKCS			RSADSI, 0x01
#define DIGEST			RSADSI, 0x02
#define CIPHER			RSADSI, 0x03
#define PKCS1			PKCS, 0x01
#define PKCS5			PKCS, 0x05
#define PKCS7			PKCS, 0x07
#define PKCS9			PKCS, 0x09
#define PKCS12			PKCS, 0x0c

/* Fortezza algorithm OID space: { 2 16 840 1 101 2 1 1 } */
/* ### mwelch -- Is this just for algorithms, or all of Fortezza? */
#define FORTEZZA_ALG 0x60, 0x86, 0x48, 0x01, 0x65, 0x02, 0x01, 0x01

/* Other OID name spaces */
#define ALGORITHM		0x2b, 0x0e, 0x03, 0x02
#define X500			0x55
#define X520_ATTRIBUTE_TYPE	X500, 0x04
#define X500_ALG		X500, 0x08
#define X500_ALG_ENCRYPTION	X500_ALG, 0x01

/** X.509 v3 Extension OID 
 ** {joint-iso-ccitt (2) ds(5) 29}
 **/
#define	ID_CE_OID 		X500, 0x1d

#define RFC1274_ATTR_TYPE  0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x1
/* #define RFC2247_ATTR_TYPE  0x09, 0x92, 0x26, 0xf5, 0x98, 0x1e, 0x64, 0x1 this is WRONG! */

/* PKCS #12 name spaces */
#define PKCS12_MODE_IDS		PKCS12, 0x01
#define PKCS12_ESPVK_IDS	PKCS12, 0x02
#define PKCS12_BAG_IDS		PKCS12, 0x03
#define PKCS12_CERT_BAG_IDS	PKCS12, 0x04
#define PKCS12_OIDS		PKCS12, 0x05
#define PKCS12_PBE_IDS		PKCS12_OIDS, 0x01
#define PKCS12_ENVELOPING_IDS	PKCS12_OIDS, 0x02
#define PKCS12_SIGNATURE_IDS	PKCS12_OIDS, 0x03
#define PKCS12_V2_PBE_IDS	PKCS12, 0x01
#define PKCS9_CERT_TYPES	PKCS9, 0x16
#define PKCS9_CRL_TYPES		PKCS9, 0x17
#define PKCS9_SMIME_IDS		PKCS9, 0x10
#define PKCS9_SMIME_ATTRS	PKCS9_SMIME_IDS, 2
#define PKCS9_SMIME_ALGS	PKCS9_SMIME_IDS, 3
#define PKCS12_VERSION1		PKCS12, 0x0a
#define PKCS12_V1_BAG_IDS	PKCS12_VERSION1, 1

/* for DSA algorithm */
/* { iso(1) member-body(2) us(840) x9-57(10040) x9algorithm(4) } */
#define ANSI_X9_ALGORITHM  0x2a, 0x86, 0x48, 0xce, 0x38, 0x4

/* for DH algorithm */
/* { iso(1) member-body(2) us(840) x9-57(10046) number-type(2) } */
/* need real OID person to look at this, copied the above line
 * and added 6 to second to last value (and changed '4' to '2' */
#define ANSI_X942_ALGORITHM  0x2a, 0x86, 0x48, 0xce, 0x3e, 0x2

#define VERISIGN 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x45

#define PKIX 			0x2b, 0x06, 0x01, 0x05, 0x05, 0x07
#define PKIX_CERT_EXTENSIONS    PKIX, 1
#define PKIX_POLICY_QUALIFIERS  PKIX, 2
#define PKIX_KEY_USAGE 		PKIX, 3
#define PKIX_ACCESS_DESCRIPTION PKIX, 0x30
#define PKIX_OCSP 		PKIX_ACCESS_DESCRIPTION, 1
#define PKIX_CA_ISSUERS		PKIX_ACCESS_DESCRIPTION, 2

#define PKIX_ID_PKIP     	PKIX, 5
#define PKIX_ID_REGCTRL  	PKIX_ID_PKIP, 1 
#define PKIX_ID_REGINFO  	PKIX_ID_PKIP, 2

/* Microsoft Object ID space */
/* { 1.3.6.1.4.1.311 } */
#define MICROSOFT_OID 0x2b, 0x6, 0x1, 0x4, 0x1, 0x82, 0x37

#define CERTICOM_OID            0x2b, 0x81, 0x04
#define SECG_OID                CERTICOM_OID, 0x00

#define ANSI_X962_OID           0x2a, 0x86, 0x48, 0xce, 0x3d
#define ANSI_X962_CURVE_OID     ANSI_X962_OID, 0x03
#define ANSI_X962_GF2m_OID      ANSI_X962_CURVE_OID, 0x00
#define ANSI_X962_GFp_OID       ANSI_X962_CURVE_OID, 0x01
#define ANSI_X962_SIGNATURE_OID ANSI_X962_OID, 0x04
#define ANSI_X962_SPECIFY_OID   ANSI_X962_SIGNATURE_OID, 0x03

/* for Camellia: iso(1) member-body(2) jisc(392)
 *    mitsubishi(200011) isl(61) security(1) algorithm(1)
 */
#define MITSUBISHI_ALG 0x2a,0x83,0x08,0x8c,0x9a,0x4b,0x3d,0x01,0x01
#define CAMELLIA_ENCRYPT_OID MITSUBISHI_ALG,1
#define CAMELLIA_WRAP_OID    MITSUBISHI_ALG,3

#define CONST_OID static const unsigned char

CONST_OID md2[]        				= { DIGEST, 0x02 };
CONST_OID md4[]        				= { DIGEST, 0x04 };
CONST_OID md5[]        				= { DIGEST, 0x05 };

CONST_OID rc2cbc[]     				= { CIPHER, 0x02 };
CONST_OID rc4[]        				= { CIPHER, 0x04 };
CONST_OID desede3cbc[] 				= { CIPHER, 0x07 };
CONST_OID rc5cbcpad[]  				= { CIPHER, 0x09 };

CONST_OID desecb[]                           = { ALGORITHM, 0x06 };
CONST_OID descbc[]                           = { ALGORITHM, 0x07 };
CONST_OID desofb[]                           = { ALGORITHM, 0x08 };
CONST_OID descfb[]                           = { ALGORITHM, 0x09 };
CONST_OID desmac[]                           = { ALGORITHM, 0x0a };
CONST_OID sdn702DSASignature[]               = { ALGORITHM, 0x0c };
CONST_OID isoSHAWithRSASignature[]           = { ALGORITHM, 0x0f };
CONST_OID desede[]                           = { ALGORITHM, 0x11 };
CONST_OID sha1[]                             = { ALGORITHM, 0x1a };
CONST_OID bogusDSASignaturewithSHA1Digest[]  = { ALGORITHM, 0x1b };

CONST_OID pkcs1RSAEncryption[]         		= { PKCS1, 0x01 };
CONST_OID pkcs1MD2WithRSAEncryption[]  		= { PKCS1, 0x02 };
CONST_OID pkcs1MD4WithRSAEncryption[]  		= { PKCS1, 0x03 };
CONST_OID pkcs1MD5WithRSAEncryption[]  		= { PKCS1, 0x04 };
CONST_OID pkcs1SHA1WithRSAEncryption[] 		= { PKCS1, 0x05 };
CONST_OID pkcs1SHA256WithRSAEncryption[] 	= { PKCS1, 11 };
CONST_OID pkcs1SHA384WithRSAEncryption[] 	= { PKCS1, 12 };
CONST_OID pkcs1SHA512WithRSAEncryption[] 	= { PKCS1, 13 };

CONST_OID pkcs5PbeWithMD2AndDEScbc[]  		= { PKCS5, 0x01 };
CONST_OID pkcs5PbeWithMD5AndDEScbc[]  		= { PKCS5, 0x03 };
CONST_OID pkcs5PbeWithSha1AndDEScbc[] 		= { PKCS5, 0x0a };

CONST_OID pkcs7[]                     		= { PKCS7 };
CONST_OID pkcs7Data[]                 		= { PKCS7, 0x01 };
CONST_OID pkcs7SignedData[]           		= { PKCS7, 0x02 };
CONST_OID pkcs7EnvelopedData[]        		= { PKCS7, 0x03 };
CONST_OID pkcs7SignedEnvelopedData[]  		= { PKCS7, 0x04 };
CONST_OID pkcs7DigestedData[]         		= { PKCS7, 0x05 };
CONST_OID pkcs7EncryptedData[]        		= { PKCS7, 0x06 };

CONST_OID pkcs9EmailAddress[]                  = { PKCS9, 0x01 };
CONST_OID pkcs9UnstructuredName[]              = { PKCS9, 0x02 };
CONST_OID pkcs9ContentType[]                   = { PKCS9, 0x03 };
CONST_OID pkcs9MessageDigest[]                 = { PKCS9, 0x04 };
CONST_OID pkcs9SigningTime[]                   = { PKCS9, 0x05 };
CONST_OID pkcs9CounterSignature[]              = { PKCS9, 0x06 };
CONST_OID pkcs9ChallengePassword[]             = { PKCS9, 0x07 };
CONST_OID pkcs9UnstructuredAddress[]           = { PKCS9, 0x08 };
CONST_OID pkcs9ExtendedCertificateAttributes[] = { PKCS9, 0x09 };
CONST_OID pkcs9ExtensionRequest[]              = { PKCS9, 14 };
CONST_OID pkcs9SMIMECapabilities[]             = { PKCS9, 15 };
CONST_OID pkcs9FriendlyName[]                  = { PKCS9, 20 };
CONST_OID pkcs9LocalKeyID[]                    = { PKCS9, 21 };

CONST_OID pkcs9X509Certificate[]        	= { PKCS9_CERT_TYPES, 1 };
CONST_OID pkcs9SDSICertificate[]        	= { PKCS9_CERT_TYPES, 2 };
CONST_OID pkcs9X509CRL[]                	= { PKCS9_CRL_TYPES, 1 };

/* RFC2630 (CMS) OIDs */
CONST_OID cmsESDH[]     			= { PKCS9_SMIME_ALGS, 5 };
CONST_OID cms3DESwrap[] 			= { PKCS9_SMIME_ALGS, 6 };
CONST_OID cmsRC2wrap[]  			= { PKCS9_SMIME_ALGS, 7 };

/* RFC2633 SMIME message attributes */
CONST_OID smimeEncryptionKeyPreference[] 	= { PKCS9_SMIME_ATTRS, 11 };
CONST_OID ms_smimeEncryptionKeyPreference[] 	= { MICROSOFT_OID, 0x10, 0x4 };

CONST_OID x520CommonName[]                      = { X520_ATTRIBUTE_TYPE, 3 };
CONST_OID x520SurName[]                         = { X520_ATTRIBUTE_TYPE, 4 };
CONST_OID x520SerialNumber[]                    = { X520_ATTRIBUTE_TYPE, 5 };
CONST_OID x520CountryName[]                     = { X520_ATTRIBUTE_TYPE, 6 };
CONST_OID x520LocalityName[]                    = { X520_ATTRIBUTE_TYPE, 7 };
CONST_OID x520StateOrProvinceName[]             = { X520_ATTRIBUTE_TYPE, 8 };
CONST_OID x520StreetAddress[]                   = { X520_ATTRIBUTE_TYPE, 9 };
CONST_OID x520OrgName[]                         = { X520_ATTRIBUTE_TYPE, 10 };
CONST_OID x520OrgUnitName[]                     = { X520_ATTRIBUTE_TYPE, 11 };
CONST_OID x520Title[]                           = { X520_ATTRIBUTE_TYPE, 12 };
CONST_OID x520PostalAddress[]                   = { X520_ATTRIBUTE_TYPE, 16 };
CONST_OID x520PostalCode[]                      = { X520_ATTRIBUTE_TYPE, 17 };
CONST_OID x520PostOfficeBox[]                   = { X520_ATTRIBUTE_TYPE, 18 };
CONST_OID x520GivenName[]                       = { X520_ATTRIBUTE_TYPE, 42 };
CONST_OID x520Initials[]                        = { X520_ATTRIBUTE_TYPE, 43 };
CONST_OID x520GenerationQualifier[]             = { X520_ATTRIBUTE_TYPE, 44 };
CONST_OID x520DnQualifier[]                     = { X520_ATTRIBUTE_TYPE, 46 };
CONST_OID x520HouseIdentifier[]                 = { X520_ATTRIBUTE_TYPE, 51 };
CONST_OID x520Pseudonym[]                       = { X520_ATTRIBUTE_TYPE, 65 };

CONST_OID nsTypeGIF[]          			= { NETSCAPE_DATA_TYPE, 0x01 };
CONST_OID nsTypeJPEG[]         			= { NETSCAPE_DATA_TYPE, 0x02 };
CONST_OID nsTypeURL[]          			= { NETSCAPE_DATA_TYPE, 0x03 };
CONST_OID nsTypeHTML[]         			= { NETSCAPE_DATA_TYPE, 0x04 };
CONST_OID nsTypeCertSeq[]      			= { NETSCAPE_DATA_TYPE, 0x05 };

CONST_OID missiCertKEADSSOld[] 			= { MISSI_OLD_KEA_DSS };
CONST_OID missiCertDSSOld[]    			= { MISSI_OLD_DSS };
CONST_OID missiCertKEADSS[]    			= { MISSI_KEA_DSS };
CONST_OID missiCertDSS[]       			= { MISSI_DSS };
CONST_OID missiCertKEA[]       			= { MISSI_KEA };
CONST_OID missiCertAltKEA[]    			= { MISSI_ALT_KEA };
CONST_OID x500RSAEncryption[]  			= { X500_ALG_ENCRYPTION, 0x01 };

/* added for alg 1485 */
CONST_OID rfc1274Uid[]             		= { RFC1274_ATTR_TYPE, 1 };
CONST_OID rfc1274Mail[]            		= { RFC1274_ATTR_TYPE, 3 };
CONST_OID rfc2247DomainComponent[] 		= { RFC1274_ATTR_TYPE, 25 };

/* Netscape private certificate extensions */
CONST_OID nsCertExtNetscapeOK[]  		= { NS_CERT_EXT, 1 };
CONST_OID nsCertExtIssuerLogo[]  		= { NS_CERT_EXT, 2 };
CONST_OID nsCertExtSubjectLogo[] 		= { NS_CERT_EXT, 3 };
CONST_OID nsExtCertType[]        		= { NETSCAPE_CERT_EXT, 0x01 };
CONST_OID nsExtBaseURL[]         		= { NETSCAPE_CERT_EXT, 0x02 };
CONST_OID nsExtRevocationURL[]   		= { NETSCAPE_CERT_EXT, 0x03 };
CONST_OID nsExtCARevocationURL[] 		= { NETSCAPE_CERT_EXT, 0x04 };
CONST_OID nsExtCACRLURL[]        		= { NETSCAPE_CERT_EXT, 0x05 };
CONST_OID nsExtCACertURL[]       		= { NETSCAPE_CERT_EXT, 0x06 };
CONST_OID nsExtCertRenewalURL[]  		= { NETSCAPE_CERT_EXT, 0x07 };
CONST_OID nsExtCAPolicyURL[]     		= { NETSCAPE_CERT_EXT, 0x08 };
CONST_OID nsExtHomepageURL[]     		= { NETSCAPE_CERT_EXT, 0x09 };
CONST_OID nsExtEntityLogo[]      		= { NETSCAPE_CERT_EXT, 0x0a };
CONST_OID nsExtUserPicture[]     		= { NETSCAPE_CERT_EXT, 0x0b };
CONST_OID nsExtSSLServerName[]   		= { NETSCAPE_CERT_EXT, 0x0c };
CONST_OID nsExtComment[]         		= { NETSCAPE_CERT_EXT, 0x0d };

/* the following 2 extensions are defined for and used by Cartman(NSM) */
CONST_OID nsExtLostPasswordURL[] 		= { NETSCAPE_CERT_EXT, 0x0e };
CONST_OID nsExtCertRenewalTime[] 		= { NETSCAPE_CERT_EXT, 0x0f };

CONST_OID nsExtAIACertRenewal[]    	= { NETSCAPE_CERT_EXT_AIA, 0x01 };
CONST_OID nsExtCertScopeOfUse[]    	= { NETSCAPE_CERT_EXT, 0x11 };
/* Reserved Netscape (2 16 840 1 113730 1 18) = { NETSCAPE_CERT_EXT, 0x12 }; */

/* Netscape policy values */
CONST_OID nsKeyUsageGovtApproved[] 	= { NETSCAPE_POLICY, 0x01 };

/* Netscape other name types */
CONST_OID netscapeNickname[] 		= { NETSCAPE_NAME_COMPONENTS, 0x01 };
CONST_OID netscapeAOLScreenname[] 	= { NETSCAPE_NAME_COMPONENTS, 0x02 };

/* OIDs needed for cert server */
CONST_OID netscapeRecoveryRequest[] 	= { NETSCAPE_CERT_SERVER_CRMF, 0x01 };


/* Standard x.509 v3 Certificate Extensions */
CONST_OID x509SubjectDirectoryAttr[]  		= { ID_CE_OID,  9 };
CONST_OID x509SubjectKeyID[]          		= { ID_CE_OID, 14 };
CONST_OID x509KeyUsage[]              		= { ID_CE_OID, 15 };
CONST_OID x509PrivateKeyUsagePeriod[] 		= { ID_CE_OID, 16 };
CONST_OID x509SubjectAltName[]        		= { ID_CE_OID, 17 };
CONST_OID x509IssuerAltName[]         		= { ID_CE_OID, 18 };
CONST_OID x509BasicConstraints[]      		= { ID_CE_OID, 19 };
CONST_OID x509NameConstraints[]       		= { ID_CE_OID, 30 };
CONST_OID x509CRLDistPoints[]         		= { ID_CE_OID, 31 };
CONST_OID x509CertificatePolicies[]   		= { ID_CE_OID, 32 };
CONST_OID x509PolicyMappings[]        		= { ID_CE_OID, 33 };
CONST_OID x509AuthKeyID[]             		= { ID_CE_OID, 35 };
CONST_OID x509PolicyConstraints[]     		= { ID_CE_OID, 36 };
CONST_OID x509ExtKeyUsage[]           		= { ID_CE_OID, 37 };
CONST_OID x509AuthInfoAccess[]        		= { PKIX_CERT_EXTENSIONS, 1 };

/* Standard x.509 v3 CRL Extensions */
CONST_OID x509CrlNumber[]                    	= { ID_CE_OID, 20};
CONST_OID x509ReasonCode[]                   	= { ID_CE_OID, 21};
CONST_OID x509InvalidDate[]                  	= { ID_CE_OID, 24};

/* pkcs 12 additions */
CONST_OID pkcs12[]                           = { PKCS12 };
CONST_OID pkcs12ModeIDs[]                    = { PKCS12_MODE_IDS };
CONST_OID pkcs12ESPVKIDs[]                   = { PKCS12_ESPVK_IDS };
CONST_OID pkcs12BagIDs[]                     = { PKCS12_BAG_IDS };
CONST_OID pkcs12CertBagIDs[]                 = { PKCS12_CERT_BAG_IDS };
CONST_OID pkcs12OIDs[]                       = { PKCS12_OIDS };
CONST_OID pkcs12PBEIDs[]                     = { PKCS12_PBE_IDS };
CONST_OID pkcs12EnvelopingIDs[]              = { PKCS12_ENVELOPING_IDS };
CONST_OID pkcs12SignatureIDs[]               = { PKCS12_SIGNATURE_IDS };
CONST_OID pkcs12PKCS8KeyShrouding[]          = { PKCS12_ESPVK_IDS, 0x01 };
CONST_OID pkcs12KeyBagID[]                   = { PKCS12_BAG_IDS, 0x01 };
CONST_OID pkcs12CertAndCRLBagID[]            = { PKCS12_BAG_IDS, 0x02 };
CONST_OID pkcs12SecretBagID[]                = { PKCS12_BAG_IDS, 0x03 };
CONST_OID pkcs12X509CertCRLBag[]             = { PKCS12_CERT_BAG_IDS, 0x01 };
CONST_OID pkcs12SDSICertBag[]                = { PKCS12_CERT_BAG_IDS, 0x02 };
CONST_OID pkcs12PBEWithSha1And128BitRC4[]    = { PKCS12_PBE_IDS, 0x01 };
CONST_OID pkcs12PBEWithSha1And40BitRC4[]     = { PKCS12_PBE_IDS, 0x02 };
CONST_OID pkcs12PBEWithSha1AndTripleDESCBC[] = { PKCS12_PBE_IDS, 0x03 };
CONST_OID pkcs12PBEWithSha1And128BitRC2CBC[] = { PKCS12_PBE_IDS, 0x04 };
CONST_OID pkcs12PBEWithSha1And40BitRC2CBC[]  = { PKCS12_PBE_IDS, 0x05 };
CONST_OID pkcs12RSAEncryptionWith128BitRC4[] = { PKCS12_ENVELOPING_IDS, 0x01 };
CONST_OID pkcs12RSAEncryptionWith40BitRC4[]  = { PKCS12_ENVELOPING_IDS, 0x02 };
CONST_OID pkcs12RSAEncryptionWithTripleDES[] = { PKCS12_ENVELOPING_IDS, 0x03 }; 
CONST_OID pkcs12RSASignatureWithSHA1Digest[] = { PKCS12_SIGNATURE_IDS, 0x01 };

/* pkcs 12 version 1.0 ids */
CONST_OID pkcs12V2PBEWithSha1And128BitRC4[]       = { PKCS12_V2_PBE_IDS, 0x01 };
CONST_OID pkcs12V2PBEWithSha1And40BitRC4[]        = { PKCS12_V2_PBE_IDS, 0x02 };
CONST_OID pkcs12V2PBEWithSha1And3KeyTripleDEScbc[]= { PKCS12_V2_PBE_IDS, 0x03 };
CONST_OID pkcs12V2PBEWithSha1And2KeyTripleDEScbc[]= { PKCS12_V2_PBE_IDS, 0x04 };
CONST_OID pkcs12V2PBEWithSha1And128BitRC2cbc[]    = { PKCS12_V2_PBE_IDS, 0x05 };
CONST_OID pkcs12V2PBEWithSha1And40BitRC2cbc[]     = { PKCS12_V2_PBE_IDS, 0x06 };

CONST_OID pkcs12SafeContentsID[]                  = { PKCS12_BAG_IDS, 0x04 };
CONST_OID pkcs12PKCS8ShroudedKeyBagID[]           = { PKCS12_BAG_IDS, 0x05 };

CONST_OID pkcs12V1KeyBag[]              	= { PKCS12_V1_BAG_IDS, 0x01 };
CONST_OID pkcs12V1PKCS8ShroudedKeyBag[] 	= { PKCS12_V1_BAG_IDS, 0x02 };
CONST_OID pkcs12V1CertBag[]             	= { PKCS12_V1_BAG_IDS, 0x03 };
CONST_OID pkcs12V1CRLBag[]              	= { PKCS12_V1_BAG_IDS, 0x04 };
CONST_OID pkcs12V1SecretBag[]           	= { PKCS12_V1_BAG_IDS, 0x05 };
CONST_OID pkcs12V1SafeContentsBag[]     	= { PKCS12_V1_BAG_IDS, 0x06 };

CONST_OID pkcs12KeyUsageAttr[]          	= { 2, 5, 29, 15 };

CONST_OID ansix9DSASignature[]               	= { ANSI_X9_ALGORITHM, 0x01 };
CONST_OID ansix9DSASignaturewithSHA1Digest[] 	= { ANSI_X9_ALGORITHM, 0x03 };

/* verisign OIDs */
CONST_OID verisignUserNotices[]     		= { VERISIGN, 1, 7, 1, 1 };

/* pkix OIDs */
CONST_OID pkixCPSPointerQualifier[] 		= { PKIX_POLICY_QUALIFIERS, 1 };
CONST_OID pkixUserNoticeQualifier[] 		= { PKIX_POLICY_QUALIFIERS, 2 };

CONST_OID pkixOCSP[]				= { PKIX_OCSP };
CONST_OID pkixOCSPBasicResponse[]		= { PKIX_OCSP, 1 };
CONST_OID pkixOCSPNonce[]			= { PKIX_OCSP, 2 };
CONST_OID pkixOCSPCRL[] 			= { PKIX_OCSP, 3 };
CONST_OID pkixOCSPResponse[]			= { PKIX_OCSP, 4 };
CONST_OID pkixOCSPNoCheck[]			= { PKIX_OCSP, 5 };
CONST_OID pkixOCSPArchiveCutoff[]		= { PKIX_OCSP, 6 };
CONST_OID pkixOCSPServiceLocator[]		= { PKIX_OCSP, 7 };

CONST_OID pkixCAIssuers[]			= { PKIX_CA_ISSUERS };

CONST_OID pkixRegCtrlRegToken[]       		= { PKIX_ID_REGCTRL, 1};
CONST_OID pkixRegCtrlAuthenticator[]  		= { PKIX_ID_REGCTRL, 2};
CONST_OID pkixRegCtrlPKIPubInfo[]     		= { PKIX_ID_REGCTRL, 3};
CONST_OID pkixRegCtrlPKIArchOptions[] 		= { PKIX_ID_REGCTRL, 4};
CONST_OID pkixRegCtrlOldCertID[]      		= { PKIX_ID_REGCTRL, 5};
CONST_OID pkixRegCtrlProtEncKey[]     		= { PKIX_ID_REGCTRL, 6};
CONST_OID pkixRegInfoUTF8Pairs[]      		= { PKIX_ID_REGINFO, 1};
CONST_OID pkixRegInfoCertReq[]        		= { PKIX_ID_REGINFO, 2};

CONST_OID pkixExtendedKeyUsageServerAuth[]    	= { PKIX_KEY_USAGE, 1 };
CONST_OID pkixExtendedKeyUsageClientAuth[]    	= { PKIX_KEY_USAGE, 2 };
CONST_OID pkixExtendedKeyUsageCodeSign[]      	= { PKIX_KEY_USAGE, 3 };
CONST_OID pkixExtendedKeyUsageEMailProtect[]  	= { PKIX_KEY_USAGE, 4 };
CONST_OID pkixExtendedKeyUsageTimeStamp[]     	= { PKIX_KEY_USAGE, 8 };
CONST_OID pkixOCSPResponderExtendedKeyUsage[] 	= { PKIX_KEY_USAGE, 9 };

/* OIDs for Netscape defined algorithms */
CONST_OID netscapeSMimeKEA[] 			= { NETSCAPE_ALGS, 0x01 };

/* Fortezza algorithm OIDs */
CONST_OID skipjackCBC[] 			= { FORTEZZA_ALG, 0x04 };
CONST_OID dhPublicKey[] 			= { ANSI_X942_ALGORITHM, 0x1 };

CONST_OID aes128_ECB[] 				= { AES, 1 };
CONST_OID aes128_CBC[] 				= { AES, 2 };
#ifdef DEFINE_ALL_AES_CIPHERS
CONST_OID aes128_OFB[] 				= { AES, 3 };
CONST_OID aes128_CFB[] 				= { AES, 4 };
#endif
CONST_OID aes128_KEY_WRAP[]			= { AES, 5 };

CONST_OID aes192_ECB[] 				= { AES, 21 };
CONST_OID aes192_CBC[] 				= { AES, 22 };
#ifdef DEFINE_ALL_AES_CIPHERS
CONST_OID aes192_OFB[] 				= { AES, 23 };
CONST_OID aes192_CFB[] 				= { AES, 24 };
#endif
CONST_OID aes192_KEY_WRAP[]			= { AES, 25 };

CONST_OID aes256_ECB[] 				= { AES, 41 };
CONST_OID aes256_CBC[] 				= { AES, 42 };
#ifdef DEFINE_ALL_AES_CIPHERS
CONST_OID aes256_OFB[] 				= { AES, 43 };
CONST_OID aes256_CFB[] 				= { AES, 44 };
#endif
CONST_OID aes256_KEY_WRAP[]			= { AES, 45 };

CONST_OID camellia128_CBC[]			= { CAMELLIA_ENCRYPT_OID, 2};
CONST_OID camellia192_CBC[]			= { CAMELLIA_ENCRYPT_OID, 3};
CONST_OID camellia256_CBC[]			= { CAMELLIA_ENCRYPT_OID, 4};
CONST_OID camellia128_KEY_WRAP[]		= { CAMELLIA_WRAP_OID, 2};
CONST_OID camellia192_KEY_WRAP[]		= { CAMELLIA_WRAP_OID, 3};
CONST_OID camellia256_KEY_WRAP[]		= { CAMELLIA_WRAP_OID, 4};

CONST_OID sha256[]                              = { SHAXXX, 1 };
CONST_OID sha384[]                              = { SHAXXX, 2 };
CONST_OID sha512[]                              = { SHAXXX, 3 };

CONST_OID ansix962ECPublicKey[]             = { ANSI_X962_OID, 0x02, 0x01 };
CONST_OID ansix962SignaturewithSHA1Digest[] = { ANSI_X962_SIGNATURE_OID, 0x01 };
CONST_OID ansix962SignatureRecommended[]    = { ANSI_X962_SIGNATURE_OID, 0x02 };
CONST_OID ansix962SignatureSpecified[]      = { ANSI_X962_SPECIFY_OID };
CONST_OID ansix962SignaturewithSHA224Digest[] = { ANSI_X962_SPECIFY_OID, 0x01 };
CONST_OID ansix962SignaturewithSHA256Digest[] = { ANSI_X962_SPECIFY_OID, 0x02 };
CONST_OID ansix962SignaturewithSHA384Digest[] = { ANSI_X962_SPECIFY_OID, 0x03 };
CONST_OID ansix962SignaturewithSHA512Digest[] = { ANSI_X962_SPECIFY_OID, 0x04 };

/* ANSI X9.62 prime curve OIDs */
/* NOTE: prime192v1 is the same as secp192r1, prime256v1 is the
 * same as secp256r1
 */
CONST_OID ansiX962prime192v1[] = { ANSI_X962_GFp_OID, 0x01 };
CONST_OID ansiX962prime192v2[] = { ANSI_X962_GFp_OID, 0x02 };
CONST_OID ansiX962prime192v3[] = { ANSI_X962_GFp_OID, 0x03 };
CONST_OID ansiX962prime239v1[] = { ANSI_X962_GFp_OID, 0x04 };
CONST_OID ansiX962prime239v2[] = { ANSI_X962_GFp_OID, 0x05 };
CONST_OID ansiX962prime239v3[] = { ANSI_X962_GFp_OID, 0x06 };
CONST_OID ansiX962prime256v1[] = { ANSI_X962_GFp_OID, 0x07 };

/* SECG prime curve OIDs */
CONST_OID secgECsecp112r1[] = { SECG_OID, 0x06 };
CONST_OID secgECsecp112r2[] = { SECG_OID, 0x07 };
CONST_OID secgECsecp128r1[] = { SECG_OID, 0x1c };
CONST_OID secgECsecp128r2[] = { SECG_OID, 0x1d };
CONST_OID secgECsecp160k1[] = { SECG_OID, 0x09 };
CONST_OID secgECsecp160r1[] = { SECG_OID, 0x08 };
CONST_OID secgECsecp160r2[] = { SECG_OID, 0x1e };
CONST_OID secgECsecp192k1[] = { SECG_OID, 0x1f };
CONST_OID secgECsecp224k1[] = { SECG_OID, 0x20 };
CONST_OID secgECsecp224r1[] = { SECG_OID, 0x21 };
CONST_OID secgECsecp256k1[] = { SECG_OID, 0x0a };
CONST_OID secgECsecp384r1[] = { SECG_OID, 0x22 };
CONST_OID secgECsecp521r1[] = { SECG_OID, 0x23 };

/* ANSI X9.62 characteristic two curve OIDs */
CONST_OID ansiX962c2pnb163v1[] = { ANSI_X962_GF2m_OID, 0x01 };
CONST_OID ansiX962c2pnb163v2[] = { ANSI_X962_GF2m_OID, 0x02 };
CONST_OID ansiX962c2pnb163v3[] = { ANSI_X962_GF2m_OID, 0x03 };
CONST_OID ansiX962c2pnb176v1[] = { ANSI_X962_GF2m_OID, 0x04 };
CONST_OID ansiX962c2tnb191v1[] = { ANSI_X962_GF2m_OID, 0x05 };
CONST_OID ansiX962c2tnb191v2[] = { ANSI_X962_GF2m_OID, 0x06 };
CONST_OID ansiX962c2tnb191v3[] = { ANSI_X962_GF2m_OID, 0x07 };
CONST_OID ansiX962c2onb191v4[] = { ANSI_X962_GF2m_OID, 0x08 };
CONST_OID ansiX962c2onb191v5[] = { ANSI_X962_GF2m_OID, 0x09 };
CONST_OID ansiX962c2pnb208w1[] = { ANSI_X962_GF2m_OID, 0x0a };
CONST_OID ansiX962c2tnb239v1[] = { ANSI_X962_GF2m_OID, 0x0b };
CONST_OID ansiX962c2tnb239v2[] = { ANSI_X962_GF2m_OID, 0x0c };
CONST_OID ansiX962c2tnb239v3[] = { ANSI_X962_GF2m_OID, 0x0d };
CONST_OID ansiX962c2onb239v4[] = { ANSI_X962_GF2m_OID, 0x0e };
CONST_OID ansiX962c2onb239v5[] = { ANSI_X962_GF2m_OID, 0x0f };
CONST_OID ansiX962c2pnb272w1[] = { ANSI_X962_GF2m_OID, 0x10 };
CONST_OID ansiX962c2pnb304w1[] = { ANSI_X962_GF2m_OID, 0x11 };
CONST_OID ansiX962c2tnb359v1[] = { ANSI_X962_GF2m_OID, 0x12 };
CONST_OID ansiX962c2pnb368w1[] = { ANSI_X962_GF2m_OID, 0x13 };
CONST_OID ansiX962c2tnb431r1[] = { ANSI_X962_GF2m_OID, 0x14 };

/* SECG characterisitic two curve OIDs */
CONST_OID secgECsect113r1[] = {SECG_OID, 0x04 };
CONST_OID secgECsect113r2[] = {SECG_OID, 0x05 };
CONST_OID secgECsect131r1[] = {SECG_OID, 0x16 };
CONST_OID secgECsect131r2[] = {SECG_OID, 0x17 };
CONST_OID secgECsect163k1[] = {SECG_OID, 0x01 };
CONST_OID secgECsect163r1[] = {SECG_OID, 0x02 };
CONST_OID secgECsect163r2[] = {SECG_OID, 0x0f };
CONST_OID secgECsect193r1[] = {SECG_OID, 0x18 };
CONST_OID secgECsect193r2[] = {SECG_OID, 0x19 };
CONST_OID secgECsect233k1[] = {SECG_OID, 0x1a };
CONST_OID secgECsect233r1[] = {SECG_OID, 0x1b };
CONST_OID secgECsect239k1[] = {SECG_OID, 0x03 };
CONST_OID secgECsect283k1[] = {SECG_OID, 0x10 };
CONST_OID secgECsect283r1[] = {SECG_OID, 0x11 };
CONST_OID secgECsect409k1[] = {SECG_OID, 0x24 };
CONST_OID secgECsect409r1[] = {SECG_OID, 0x25 };
CONST_OID secgECsect571k1[] = {SECG_OID, 0x26 };
CONST_OID secgECsect571r1[] = {SECG_OID, 0x27 };

#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }
#ifndef SECOID_NO_STRINGS
#define OD(oid,tag,desc,mech,ext) { OI(oid), tag, desc, mech, ext }
#else
#define OD(oid,tag,desc,mech,ext) { OI(oid), tag, 0, mech, ext }
#endif

/*
 * NOTE: the order of these entries must mach the SECOidTag enum in secoidt.h!
 */
const static SECOidData oids[] = {
    { { siDEROID, NULL, 0 }, SEC_OID_UNKNOWN,
	"Unknown OID", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION },
    OD( md2, SEC_OID_MD2, "MD2", CKM_MD2, INVALID_CERT_EXTENSION ),
    OD( md4, SEC_OID_MD4,
	"MD4", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( md5, SEC_OID_MD5, "MD5", CKM_MD5, INVALID_CERT_EXTENSION ),
    OD( sha1, SEC_OID_SHA1, "SHA-1", CKM_SHA_1, INVALID_CERT_EXTENSION ),
    OD( rc2cbc, SEC_OID_RC2_CBC,
	"RC2-CBC", CKM_RC2_CBC, INVALID_CERT_EXTENSION ),
    OD( rc4, SEC_OID_RC4, "RC4", CKM_RC4, INVALID_CERT_EXTENSION ),
    OD( desede3cbc, SEC_OID_DES_EDE3_CBC,
	"DES-EDE3-CBC", CKM_DES3_CBC, INVALID_CERT_EXTENSION ),
    OD( rc5cbcpad, SEC_OID_RC5_CBC_PAD,
	"RC5-CBCPad", CKM_RC5_CBC, INVALID_CERT_EXTENSION ),
    OD( desecb, SEC_OID_DES_ECB,
	"DES-ECB", CKM_DES_ECB, INVALID_CERT_EXTENSION ),
    OD( descbc, SEC_OID_DES_CBC,
	"DES-CBC", CKM_DES_CBC, INVALID_CERT_EXTENSION ),
    OD( desofb, SEC_OID_DES_OFB,
	"DES-OFB", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( descfb, SEC_OID_DES_CFB,
	"DES-CFB", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( desmac, SEC_OID_DES_MAC,
	"DES-MAC", CKM_DES_MAC, INVALID_CERT_EXTENSION ),
    OD( desede, SEC_OID_DES_EDE,
	"DES-EDE", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( isoSHAWithRSASignature, SEC_OID_ISO_SHA_WITH_RSA_SIGNATURE,
	"ISO SHA with RSA Signature", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs1RSAEncryption, SEC_OID_PKCS1_RSA_ENCRYPTION,
	"PKCS #1 RSA Encryption", CKM_RSA_PKCS, INVALID_CERT_EXTENSION ),

    /* the following Signing mechanisms should get new CKM_ values when
     * values for CKM_RSA_WITH_MDX and CKM_RSA_WITH_SHA_1 get defined in
     * PKCS #11.
     */
    OD( pkcs1MD2WithRSAEncryption, SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
	"PKCS #1 MD2 With RSA Encryption", CKM_MD2_RSA_PKCS,
	INVALID_CERT_EXTENSION ),
    OD( pkcs1MD4WithRSAEncryption, SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION,
	"PKCS #1 MD4 With RSA Encryption", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs1MD5WithRSAEncryption, SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
	"PKCS #1 MD5 With RSA Encryption", CKM_MD5_RSA_PKCS,
	INVALID_CERT_EXTENSION ),
    OD( pkcs1SHA1WithRSAEncryption, SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION,
	"PKCS #1 SHA-1 With RSA Encryption", CKM_SHA1_RSA_PKCS,
	INVALID_CERT_EXTENSION ),

    OD( pkcs5PbeWithMD2AndDEScbc, SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC,
	"PKCS #5 Password Based Encryption with MD2 and DES CBC",
	CKM_PBE_MD2_DES_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs5PbeWithMD5AndDEScbc, SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC,
	"PKCS #5 Password Based Encryption with MD5 and DES CBC",
	CKM_PBE_MD5_DES_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs5PbeWithSha1AndDEScbc, SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC,
	"PKCS #5 Password Based Encryption with SHA1 and DES CBC", 
	CKM_NETSCAPE_PBE_SHA1_DES_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs7, SEC_OID_PKCS7,
	"PKCS #7", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs7Data, SEC_OID_PKCS7_DATA,
	"PKCS #7 Data", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs7SignedData, SEC_OID_PKCS7_SIGNED_DATA,
	"PKCS #7 Signed Data", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs7EnvelopedData, SEC_OID_PKCS7_ENVELOPED_DATA,
	"PKCS #7 Enveloped Data", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs7SignedEnvelopedData, SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA,
	"PKCS #7 Signed And Enveloped Data", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs7DigestedData, SEC_OID_PKCS7_DIGESTED_DATA,
	"PKCS #7 Digested Data", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs7EncryptedData, SEC_OID_PKCS7_ENCRYPTED_DATA,
	"PKCS #7 Encrypted Data", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9EmailAddress, SEC_OID_PKCS9_EMAIL_ADDRESS,
	"PKCS #9 Email Address", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9UnstructuredName, SEC_OID_PKCS9_UNSTRUCTURED_NAME,
	"PKCS #9 Unstructured Name", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9ContentType, SEC_OID_PKCS9_CONTENT_TYPE,
	"PKCS #9 Content Type", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9MessageDigest, SEC_OID_PKCS9_MESSAGE_DIGEST,
	"PKCS #9 Message Digest", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9SigningTime, SEC_OID_PKCS9_SIGNING_TIME,
	"PKCS #9 Signing Time", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9CounterSignature, SEC_OID_PKCS9_COUNTER_SIGNATURE,
	"PKCS #9 Counter Signature", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9ChallengePassword, SEC_OID_PKCS9_CHALLENGE_PASSWORD,
	"PKCS #9 Challenge Password", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9UnstructuredAddress, SEC_OID_PKCS9_UNSTRUCTURED_ADDRESS,
	"PKCS #9 Unstructured Address", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9ExtendedCertificateAttributes,
	SEC_OID_PKCS9_EXTENDED_CERTIFICATE_ATTRIBUTES,
	"PKCS #9 Extended Certificate Attributes", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9SMIMECapabilities, SEC_OID_PKCS9_SMIME_CAPABILITIES,
	"PKCS #9 S/MIME Capabilities", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520CommonName, SEC_OID_AVA_COMMON_NAME,
	"X520 Common Name", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520CountryName, SEC_OID_AVA_COUNTRY_NAME,
	"X520 Country Name", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520LocalityName, SEC_OID_AVA_LOCALITY,
	"X520 Locality Name", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520StateOrProvinceName, SEC_OID_AVA_STATE_OR_PROVINCE,
	"X520 State Or Province Name", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520OrgName, SEC_OID_AVA_ORGANIZATION_NAME,
	"X520 Organization Name", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520OrgUnitName, SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME,
	"X520 Organizational Unit Name", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520DnQualifier, SEC_OID_AVA_DN_QUALIFIER,
	"X520 DN Qualifier", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( rfc2247DomainComponent, SEC_OID_AVA_DC,
	"RFC 2247 Domain Component", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    OD( nsTypeGIF, SEC_OID_NS_TYPE_GIF,
	"GIF", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( nsTypeJPEG, SEC_OID_NS_TYPE_JPEG,
	"JPEG", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( nsTypeURL, SEC_OID_NS_TYPE_URL,
	"URL", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( nsTypeHTML, SEC_OID_NS_TYPE_HTML,
	"HTML", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( nsTypeCertSeq, SEC_OID_NS_TYPE_CERT_SEQUENCE,
	"Certificate Sequence", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( missiCertKEADSSOld, SEC_OID_MISSI_KEA_DSS_OLD, 
	"MISSI KEA and DSS Algorithm (Old)",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( missiCertDSSOld, SEC_OID_MISSI_DSS_OLD, 
	"MISSI DSS Algorithm (Old)",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( missiCertKEADSS, SEC_OID_MISSI_KEA_DSS, 
	"MISSI KEA and DSS Algorithm",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( missiCertDSS, SEC_OID_MISSI_DSS, 
	"MISSI DSS Algorithm",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( missiCertKEA, SEC_OID_MISSI_KEA, 
	"MISSI KEA Algorithm",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( missiCertAltKEA, SEC_OID_MISSI_ALT_KEA, 
	"MISSI Alternate KEA Algorithm",
          CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* Netscape private extensions */
    OD( nsCertExtNetscapeOK, SEC_OID_NS_CERT_EXT_NETSCAPE_OK,
	"Netscape says this cert is OK",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsCertExtIssuerLogo, SEC_OID_NS_CERT_EXT_ISSUER_LOGO,
	"Certificate Issuer Logo",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsCertExtSubjectLogo, SEC_OID_NS_CERT_EXT_SUBJECT_LOGO,
	"Certificate Subject Logo",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsExtCertType, SEC_OID_NS_CERT_EXT_CERT_TYPE,
	"Certificate Type",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtBaseURL, SEC_OID_NS_CERT_EXT_BASE_URL,
	"Certificate Extension Base URL",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtRevocationURL, SEC_OID_NS_CERT_EXT_REVOCATION_URL,
	"Certificate Revocation URL",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtCARevocationURL, SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL,
	"Certificate Authority Revocation URL",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtCACRLURL, SEC_OID_NS_CERT_EXT_CA_CRL_URL,
	"Certificate Authority CRL Download URL",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsExtCACertURL, SEC_OID_NS_CERT_EXT_CA_CERT_URL,
	"Certificate Authority Certificate Download URL",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsExtCertRenewalURL, SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL,
	"Certificate Renewal URL", 
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ), 
    OD( nsExtCAPolicyURL, SEC_OID_NS_CERT_EXT_CA_POLICY_URL,
	"Certificate Authority Policy URL",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtHomepageURL, SEC_OID_NS_CERT_EXT_HOMEPAGE_URL,
	"Certificate Homepage URL", 
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsExtEntityLogo, SEC_OID_NS_CERT_EXT_ENTITY_LOGO,
	"Certificate Entity Logo", 
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsExtUserPicture, SEC_OID_NS_CERT_EXT_USER_PICTURE,
	"Certificate User Picture", 
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( nsExtSSLServerName, SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME,
	"Certificate SSL Server Name", 
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtComment, SEC_OID_NS_CERT_EXT_COMMENT,
	"Certificate Comment", 
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtLostPasswordURL, SEC_OID_NS_CERT_EXT_LOST_PASSWORD_URL,
        "Lost Password URL", 
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsExtCertRenewalTime, SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME, 
	"Certificate Renewal Time", 
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( nsKeyUsageGovtApproved, SEC_OID_NS_KEY_USAGE_GOVT_APPROVED,
	"Strong Crypto Export Approved",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),


    /* x.509 v3 certificate extensions */
    OD( x509SubjectDirectoryAttr, SEC_OID_X509_SUBJECT_DIRECTORY_ATTR,
	"Certificate Subject Directory Attributes",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION),
    OD( x509SubjectKeyID, SEC_OID_X509_SUBJECT_KEY_ID, 
	"Certificate Subject Key ID",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509KeyUsage, SEC_OID_X509_KEY_USAGE, 
	"Certificate Key Usage",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509PrivateKeyUsagePeriod, SEC_OID_X509_PRIVATE_KEY_USAGE_PERIOD,
	"Certificate Private Key Usage Period",
        CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( x509SubjectAltName, SEC_OID_X509_SUBJECT_ALT_NAME, 
	"Certificate Subject Alt Name",
        CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509IssuerAltName, SEC_OID_X509_ISSUER_ALT_NAME, 
	"Certificate Issuer Alt Name",
        CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( x509BasicConstraints, SEC_OID_X509_BASIC_CONSTRAINTS, 
	"Certificate Basic Constraints",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509NameConstraints, SEC_OID_X509_NAME_CONSTRAINTS, 
	"Certificate Name Constraints",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509CRLDistPoints, SEC_OID_X509_CRL_DIST_POINTS, 
	"CRL Distribution Points",
	CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( x509CertificatePolicies, SEC_OID_X509_CERTIFICATE_POLICIES,
	"Certificate Policies",
        CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( x509PolicyMappings, SEC_OID_X509_POLICY_MAPPINGS, 
	"Certificate Policy Mappings",
        CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( x509PolicyConstraints, SEC_OID_X509_POLICY_CONSTRAINTS, 
	"Certificate Policy Constraints",
        CKM_INVALID_MECHANISM, UNSUPPORTED_CERT_EXTENSION ),
    OD( x509AuthKeyID, SEC_OID_X509_AUTH_KEY_ID, 
	"Certificate Authority Key Identifier",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509ExtKeyUsage, SEC_OID_X509_EXT_KEY_USAGE, 
	"Extended Key Usage",
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509AuthInfoAccess, SEC_OID_X509_AUTH_INFO_ACCESS, 
	"Authority Information Access",
        CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),

    /* x.509 v3 CRL extensions */
    OD( x509CrlNumber, SEC_OID_X509_CRL_NUMBER, 
	"CRL Number", CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509ReasonCode, SEC_OID_X509_REASON_CODE, 
	"CRL reason code", CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( x509InvalidDate, SEC_OID_X509_INVALID_DATE, 
	"Invalid Date", CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
	
    OD( x500RSAEncryption, SEC_OID_X500_RSA_ENCRYPTION,
	"X500 RSA Encryption", CKM_RSA_X_509, INVALID_CERT_EXTENSION ),

    /* added for alg 1485 */
    OD( rfc1274Uid, SEC_OID_RFC1274_UID,
	"RFC1274 User Id", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( rfc1274Mail, SEC_OID_RFC1274_MAIL,
	"RFC1274 E-mail Address", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* pkcs 12 additions */
    OD( pkcs12, SEC_OID_PKCS12,
	"PKCS #12", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12ModeIDs, SEC_OID_PKCS12_MODE_IDS,
	"PKCS #12 Mode IDs", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12ESPVKIDs, SEC_OID_PKCS12_ESPVK_IDS,
	"PKCS #12 ESPVK IDs", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12BagIDs, SEC_OID_PKCS12_BAG_IDS,
	"PKCS #12 Bag IDs", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12CertBagIDs, SEC_OID_PKCS12_CERT_BAG_IDS,
	"PKCS #12 Cert Bag IDs", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12OIDs, SEC_OID_PKCS12_OIDS,
	"PKCS #12 OIDs", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12PBEIDs, SEC_OID_PKCS12_PBE_IDS,
	"PKCS #12 PBE IDs", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12SignatureIDs, SEC_OID_PKCS12_SIGNATURE_IDS,
	"PKCS #12 Signature IDs", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12EnvelopingIDs, SEC_OID_PKCS12_ENVELOPING_IDS,
	"PKCS #12 Enveloping IDs", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12PKCS8KeyShrouding, SEC_OID_PKCS12_PKCS8_KEY_SHROUDING,
	"PKCS #12 Key Shrouding", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12KeyBagID, SEC_OID_PKCS12_KEY_BAG_ID,
	"PKCS #12 Key Bag ID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12CertAndCRLBagID, SEC_OID_PKCS12_CERT_AND_CRL_BAG_ID,
	"PKCS #12 Cert And CRL Bag ID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12SecretBagID, SEC_OID_PKCS12_SECRET_BAG_ID,
	"PKCS #12 Secret Bag ID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12X509CertCRLBag, SEC_OID_PKCS12_X509_CERT_CRL_BAG,
	"PKCS #12 X509 Cert CRL Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12SDSICertBag, SEC_OID_PKCS12_SDSI_CERT_BAG,
	"PKCS #12 SDSI Cert Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12PBEWithSha1And128BitRC4,
	SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4,
	"PKCS #12 PBE With Sha1 and 128 Bit RC4", 
	CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4, INVALID_CERT_EXTENSION ),
    OD( pkcs12PBEWithSha1And40BitRC4,
	SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4,
	"PKCS #12 PBE With Sha1 and 40 Bit RC4", 
	CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4, INVALID_CERT_EXTENSION ),
    OD( pkcs12PBEWithSha1AndTripleDESCBC,
	SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC,
	"PKCS #12 PBE With Sha1 and Triple DES CBC", 
	CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12PBEWithSha1And128BitRC2CBC,
	SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC,
	"PKCS #12 PBE With Sha1 and 128 Bit RC2 CBC", 
	CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12PBEWithSha1And40BitRC2CBC,
	SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC,
	"PKCS #12 PBE With Sha1 and 40 Bit RC2 CBC", 
	CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12RSAEncryptionWith128BitRC4,
	SEC_OID_PKCS12_RSA_ENCRYPTION_WITH_128_BIT_RC4,
	"PKCS #12 RSA Encryption with 128 Bit RC4",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12RSAEncryptionWith40BitRC4,
	SEC_OID_PKCS12_RSA_ENCRYPTION_WITH_40_BIT_RC4,
	"PKCS #12 RSA Encryption with 40 Bit RC4",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12RSAEncryptionWithTripleDES,
	SEC_OID_PKCS12_RSA_ENCRYPTION_WITH_TRIPLE_DES,
	"PKCS #12 RSA Encryption with Triple DES",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12RSASignatureWithSHA1Digest,
	SEC_OID_PKCS12_RSA_SIGNATURE_WITH_SHA1_DIGEST,
	"PKCS #12 RSA Encryption with Triple DES",
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* DSA signatures */
    OD( ansix9DSASignature, SEC_OID_ANSIX9_DSA_SIGNATURE,
	"ANSI X9.57 DSA Signature", CKM_DSA, INVALID_CERT_EXTENSION ),
    OD( ansix9DSASignaturewithSHA1Digest,
        SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST,
	"ANSI X9.57 DSA Signature with SHA1 Digest", 
	CKM_DSA_SHA1, INVALID_CERT_EXTENSION ),
    OD( bogusDSASignaturewithSHA1Digest,
        SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST,
	"FORTEZZA DSA Signature with SHA1 Digest", 
	CKM_DSA_SHA1, INVALID_CERT_EXTENSION ),

    /* verisign oids */
    OD( verisignUserNotices, SEC_OID_VERISIGN_USER_NOTICES,
	"Verisign User Notices", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* pkix oids */
    OD( pkixCPSPointerQualifier, SEC_OID_PKIX_CPS_POINTER_QUALIFIER,
	"PKIX CPS Pointer Qualifier", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixUserNoticeQualifier, SEC_OID_PKIX_USER_NOTICE_QUALIFIER,
	"PKIX User Notice Qualifier", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    OD( pkixOCSP, SEC_OID_PKIX_OCSP,
	"PKIX Online Certificate Status Protocol", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixOCSPBasicResponse, SEC_OID_PKIX_OCSP_BASIC_RESPONSE,
	"OCSP Basic Response", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixOCSPNonce, SEC_OID_PKIX_OCSP_NONCE,
	"OCSP Nonce Extension", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixOCSPCRL, SEC_OID_PKIX_OCSP_CRL,
	"OCSP CRL Reference Extension", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixOCSPResponse, SEC_OID_PKIX_OCSP_RESPONSE,
	"OCSP Response Types Extension", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixOCSPNoCheck, SEC_OID_PKIX_OCSP_NO_CHECK,
	"OCSP No Check Extension", 
	CKM_INVALID_MECHANISM, SUPPORTED_CERT_EXTENSION ),
    OD( pkixOCSPArchiveCutoff, SEC_OID_PKIX_OCSP_ARCHIVE_CUTOFF,
	"OCSP Archive Cutoff Extension", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixOCSPServiceLocator, SEC_OID_PKIX_OCSP_SERVICE_LOCATOR,
	"OCSP Service Locator Extension", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    OD( pkixRegCtrlRegToken, SEC_OID_PKIX_REGCTRL_REGTOKEN,
        "PKIX CRMF Registration Control, Registration Token", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixRegCtrlAuthenticator, SEC_OID_PKIX_REGCTRL_AUTHENTICATOR,
        "PKIX CRMF Registration Control, Registration Authenticator", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkixRegCtrlPKIPubInfo, SEC_OID_PKIX_REGCTRL_PKIPUBINFO,
        "PKIX CRMF Registration Control, PKI Publication Info", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixRegCtrlPKIArchOptions,
        SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS,
        "PKIX CRMF Registration Control, PKI Archive Options", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixRegCtrlOldCertID, SEC_OID_PKIX_REGCTRL_OLD_CERT_ID,
        "PKIX CRMF Registration Control, Old Certificate ID", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixRegCtrlProtEncKey, SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY,
        "PKIX CRMF Registration Control, Protocol Encryption Key", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixRegInfoUTF8Pairs, SEC_OID_PKIX_REGINFO_UTF8_PAIRS,
        "PKIX CRMF Registration Info, UTF8 Pairs", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixRegInfoCertReq, SEC_OID_PKIX_REGINFO_CERT_REQUEST,
        "PKIX CRMF Registration Info, Certificate Request", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixExtendedKeyUsageServerAuth,
        SEC_OID_EXT_KEY_USAGE_SERVER_AUTH,
        "TLS Web Server Authentication Certificate",
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixExtendedKeyUsageClientAuth,
        SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH,
        "TLS Web Client Authentication Certificate",
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixExtendedKeyUsageCodeSign, SEC_OID_EXT_KEY_USAGE_CODE_SIGN,
        "Code Signing Certificate",
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixExtendedKeyUsageEMailProtect,
        SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT,
        "E-Mail Protection Certificate",
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixExtendedKeyUsageTimeStamp,
        SEC_OID_EXT_KEY_USAGE_TIME_STAMP,
        "Time Stamping Certifcate",
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),
    OD( pkixOCSPResponderExtendedKeyUsage, SEC_OID_OCSP_RESPONDER,
          "OCSP Responder Certificate",
          CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION),

    /* Netscape Algorithm OIDs */

    OD( netscapeSMimeKEA, SEC_OID_NETSCAPE_SMIME_KEA,
	"Netscape S/MIME KEA", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

      /* Skipjack OID -- ### mwelch temporary */
    OD( skipjackCBC, SEC_OID_FORTEZZA_SKIPJACK,
	"Skipjack CBC64", CKM_SKIPJACK_CBC64, INVALID_CERT_EXTENSION ),

    /* pkcs12 v2 oids */
    OD( pkcs12V2PBEWithSha1And128BitRC4,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4,
	"PKCS12 V2 PBE With SHA1 And 128 Bit RC4", 
	CKM_PBE_SHA1_RC4_128, INVALID_CERT_EXTENSION ),
    OD( pkcs12V2PBEWithSha1And40BitRC4,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4,
	"PKCS12 V2 PBE With SHA1 And 40 Bit RC4", 
	CKM_PBE_SHA1_RC4_40, INVALID_CERT_EXTENSION ),
    OD( pkcs12V2PBEWithSha1And3KeyTripleDEScbc,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC,
	"PKCS12 V2 PBE With SHA1 And 3KEY Triple DES-cbc", 
	CKM_PBE_SHA1_DES3_EDE_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12V2PBEWithSha1And2KeyTripleDEScbc,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC,
	"PKCS12 V2 PBE With SHA1 And 2KEY Triple DES-cbc", 
	CKM_PBE_SHA1_DES2_EDE_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12V2PBEWithSha1And128BitRC2cbc,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC,
	"PKCS12 V2 PBE With SHA1 And 128 Bit RC2 CBC", 
	CKM_PBE_SHA1_RC2_128_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12V2PBEWithSha1And40BitRC2cbc,
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC,
	"PKCS12 V2 PBE With SHA1 And 40 Bit RC2 CBC", 
	CKM_PBE_SHA1_RC2_40_CBC, INVALID_CERT_EXTENSION ),
    OD( pkcs12SafeContentsID, SEC_OID_PKCS12_SAFE_CONTENTS_ID,
	"PKCS #12 Safe Contents ID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12PKCS8ShroudedKeyBagID,
	SEC_OID_PKCS12_PKCS8_SHROUDED_KEY_BAG_ID,
	"PKCS #12 Safe Contents ID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12V1KeyBag, SEC_OID_PKCS12_V1_KEY_BAG_ID,
	"PKCS #12 V1 Key Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12V1PKCS8ShroudedKeyBag,
	SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID,
	"PKCS #12 V1 PKCS8 Shrouded Key Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12V1CertBag, SEC_OID_PKCS12_V1_CERT_BAG_ID,
	"PKCS #12 V1 Cert Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12V1CRLBag, SEC_OID_PKCS12_V1_CRL_BAG_ID,
	"PKCS #12 V1 CRL Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12V1SecretBag, SEC_OID_PKCS12_V1_SECRET_BAG_ID,
	"PKCS #12 V1 Secret Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs12V1SafeContentsBag, SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID,
	"PKCS #12 V1 Safe Contents Bag", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    OD( pkcs9X509Certificate, SEC_OID_PKCS9_X509_CERT,
	"PKCS #9 X509 Certificate", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9SDSICertificate, SEC_OID_PKCS9_SDSI_CERT,
	"PKCS #9 SDSI Certificate", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9X509CRL, SEC_OID_PKCS9_X509_CRL,
	"PKCS #9 X509 CRL", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9FriendlyName, SEC_OID_PKCS9_FRIENDLY_NAME,
	"PKCS #9 Friendly Name", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9LocalKeyID, SEC_OID_PKCS9_LOCAL_KEY_ID,
	"PKCS #9 Local Key ID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ), 
    OD( pkcs12KeyUsageAttr, SEC_OID_PKCS12_KEY_USAGE,
	"PKCS 12 Key Usage", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( dhPublicKey, SEC_OID_X942_DIFFIE_HELMAN_KEY,
	"Diffie-Helman Public Key", CKM_DH_PKCS_DERIVE,
	INVALID_CERT_EXTENSION ),
    OD( netscapeNickname, SEC_OID_NETSCAPE_NICKNAME,
	"Netscape Nickname", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* Cert Server specific OIDs */
    OD( netscapeRecoveryRequest, SEC_OID_NETSCAPE_RECOVERY_REQUEST,
        "Recovery Request OID", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    OD( nsExtAIACertRenewal, SEC_OID_CERT_RENEWAL_LOCATOR,
        "Certificate Renewal Locator OID", CKM_INVALID_MECHANISM,
        INVALID_CERT_EXTENSION ), 

    OD( nsExtCertScopeOfUse, SEC_OID_NS_CERT_EXT_SCOPE_OF_USE,
        "Certificate Scope-of-Use Extension", CKM_INVALID_MECHANISM,
        SUPPORTED_CERT_EXTENSION ),

    /* CMS stuff */
    OD( cmsESDH, SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN,
        "Ephemeral-Static Diffie-Hellman", CKM_INVALID_MECHANISM /* XXX */,
        INVALID_CERT_EXTENSION ),
    OD( cms3DESwrap, SEC_OID_CMS_3DES_KEY_WRAP,
        "CMS 3DES Key Wrap", CKM_INVALID_MECHANISM /* XXX */,
        INVALID_CERT_EXTENSION ),
    OD( cmsRC2wrap, SEC_OID_CMS_RC2_KEY_WRAP,
        "CMS RC2 Key Wrap", CKM_INVALID_MECHANISM /* XXX */,
        INVALID_CERT_EXTENSION ),
    OD( smimeEncryptionKeyPreference, SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE,
	"S/MIME Encryption Key Preference", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* AES algorithm OIDs */
    OD( aes128_ECB, SEC_OID_AES_128_ECB,
	"AES-128-ECB", CKM_AES_ECB, INVALID_CERT_EXTENSION ),
    OD( aes128_CBC, SEC_OID_AES_128_CBC,
	"AES-128-CBC", CKM_AES_CBC, INVALID_CERT_EXTENSION ),
    OD( aes192_ECB, SEC_OID_AES_192_ECB,
	"AES-192-ECB", CKM_AES_ECB, INVALID_CERT_EXTENSION ),
    OD( aes192_CBC, SEC_OID_AES_192_CBC,
	"AES-192-CBC", CKM_AES_CBC, INVALID_CERT_EXTENSION ),
    OD( aes256_ECB, SEC_OID_AES_256_ECB,
	"AES-256-ECB", CKM_AES_ECB, INVALID_CERT_EXTENSION ),
    OD( aes256_CBC, SEC_OID_AES_256_CBC,
	"AES-256-CBC", CKM_AES_CBC, INVALID_CERT_EXTENSION ),

    /* More bogus DSA OIDs */
    OD( sdn702DSASignature, SEC_OID_SDN702_DSA_SIGNATURE, 
	"SDN.702 DSA Signature", CKM_DSA_SHA1, INVALID_CERT_EXTENSION ),

    OD( ms_smimeEncryptionKeyPreference, 
        SEC_OID_MS_SMIME_ENCRYPTION_KEY_PREFERENCE,
	"Microsoft S/MIME Encryption Key Preference", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    OD( sha256, SEC_OID_SHA256, "SHA-256", CKM_SHA256, INVALID_CERT_EXTENSION),
    OD( sha384, SEC_OID_SHA384, "SHA-384", CKM_SHA384, INVALID_CERT_EXTENSION),
    OD( sha512, SEC_OID_SHA512, "SHA-512", CKM_SHA512, INVALID_CERT_EXTENSION),

    OD( pkcs1SHA256WithRSAEncryption, SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
	"PKCS #1 SHA-256 With RSA Encryption", CKM_SHA256_RSA_PKCS,
	INVALID_CERT_EXTENSION ),
    OD( pkcs1SHA384WithRSAEncryption, SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION,
	"PKCS #1 SHA-384 With RSA Encryption", CKM_SHA384_RSA_PKCS,
	INVALID_CERT_EXTENSION ),
    OD( pkcs1SHA512WithRSAEncryption, SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION,
	"PKCS #1 SHA-512 With RSA Encryption", CKM_SHA512_RSA_PKCS,
	INVALID_CERT_EXTENSION ),

    OD( aes128_KEY_WRAP, SEC_OID_AES_128_KEY_WRAP,
	"AES-128 Key Wrap", CKM_NETSCAPE_AES_KEY_WRAP, INVALID_CERT_EXTENSION),
    OD( aes192_KEY_WRAP, SEC_OID_AES_192_KEY_WRAP,
	"AES-192 Key Wrap", CKM_NETSCAPE_AES_KEY_WRAP, INVALID_CERT_EXTENSION),
    OD( aes256_KEY_WRAP, SEC_OID_AES_256_KEY_WRAP,
	"AES-256 Key Wrap", CKM_NETSCAPE_AES_KEY_WRAP, INVALID_CERT_EXTENSION),

    /* Elliptic Curve Cryptography (ECC) OIDs */
    OD( ansix962ECPublicKey, SEC_OID_ANSIX962_EC_PUBLIC_KEY,
	"X9.62 elliptic curve public key", CKM_ECDH1_DERIVE,
	INVALID_CERT_EXTENSION ),
    OD( ansix962SignaturewithSHA1Digest, 
	SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE,
	"X9.62 ECDSA signature with SHA1", CKM_ECDSA_SHA1,
	INVALID_CERT_EXTENSION ),

    /* Named curves */

    /* ANSI X9.62 named elliptic curves (prime field) */
    OD( ansiX962prime192v1, SEC_OID_ANSIX962_EC_PRIME192V1,
	"ANSI X9.62 elliptic curve prime192v1 (aka secp192r1, NIST P-192)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962prime192v2, SEC_OID_ANSIX962_EC_PRIME192V2,
	"ANSI X9.62 elliptic curve prime192v2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962prime192v3, SEC_OID_ANSIX962_EC_PRIME192V3,
	"ANSI X9.62 elliptic curve prime192v3", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962prime239v1, SEC_OID_ANSIX962_EC_PRIME239V1,
	"ANSI X9.62 elliptic curve prime239v1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962prime239v2, SEC_OID_ANSIX962_EC_PRIME239V2,
	"ANSI X9.62 elliptic curve prime239v2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962prime239v3, SEC_OID_ANSIX962_EC_PRIME239V3,
	"ANSI X9.62 elliptic curve prime239v3", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962prime256v1, SEC_OID_ANSIX962_EC_PRIME256V1,
	"ANSI X9.62 elliptic curve prime256v1 (aka secp256r1, NIST P-256)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),

    /* SECG named elliptic curves (prime field) */
    OD( secgECsecp112r1, SEC_OID_SECG_EC_SECP112R1,
	"SECG elliptic curve secp112r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp112r2, SEC_OID_SECG_EC_SECP112R2,
	"SECG elliptic curve secp112r2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp128r1, SEC_OID_SECG_EC_SECP128R1,
	"SECG elliptic curve secp128r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp128r2, SEC_OID_SECG_EC_SECP128R2,
	"SECG elliptic curve secp128r2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp160k1, SEC_OID_SECG_EC_SECP160K1,
	"SECG elliptic curve secp160k1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp160r1, SEC_OID_SECG_EC_SECP160R1,
	"SECG elliptic curve secp160r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp160r2, SEC_OID_SECG_EC_SECP160R2,
	"SECG elliptic curve secp160r2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp192k1, SEC_OID_SECG_EC_SECP192K1,
	"SECG elliptic curve secp192k1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp224k1, SEC_OID_SECG_EC_SECP224K1,
	"SECG elliptic curve secp224k1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp224r1, SEC_OID_SECG_EC_SECP224R1,
	"SECG elliptic curve secp224r1 (aka NIST P-224)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp256k1, SEC_OID_SECG_EC_SECP256K1,
	"SECG elliptic curve secp256k1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp384r1, SEC_OID_SECG_EC_SECP384R1,
	"SECG elliptic curve secp384r1 (aka NIST P-384)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsecp521r1, SEC_OID_SECG_EC_SECP521R1,
	"SECG elliptic curve secp521r1 (aka NIST P-521)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),

    /* ANSI X9.62 named elliptic curves (characteristic two field) */
    OD( ansiX962c2pnb163v1, SEC_OID_ANSIX962_EC_C2PNB163V1,
	"ANSI X9.62 elliptic curve c2pnb163v1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb163v2, SEC_OID_ANSIX962_EC_C2PNB163V2,
	"ANSI X9.62 elliptic curve c2pnb163v2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb163v3, SEC_OID_ANSIX962_EC_C2PNB163V3,
	"ANSI X9.62 elliptic curve c2pnb163v3", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb176v1, SEC_OID_ANSIX962_EC_C2PNB176V1,
	"ANSI X9.62 elliptic curve c2pnb176v1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb191v1, SEC_OID_ANSIX962_EC_C2TNB191V1,
	"ANSI X9.62 elliptic curve c2tnb191v1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb191v2, SEC_OID_ANSIX962_EC_C2TNB191V2,
	"ANSI X9.62 elliptic curve c2tnb191v2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb191v3, SEC_OID_ANSIX962_EC_C2TNB191V3,
	"ANSI X9.62 elliptic curve c2tnb191v3", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2onb191v4, SEC_OID_ANSIX962_EC_C2ONB191V4,
	"ANSI X9.62 elliptic curve c2onb191v4", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2onb191v5, SEC_OID_ANSIX962_EC_C2ONB191V5,
	"ANSI X9.62 elliptic curve c2onb191v5", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb208w1, SEC_OID_ANSIX962_EC_C2PNB208W1,
	"ANSI X9.62 elliptic curve c2pnb208w1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb239v1, SEC_OID_ANSIX962_EC_C2TNB239V1,
	"ANSI X9.62 elliptic curve c2tnb239v1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb239v2, SEC_OID_ANSIX962_EC_C2TNB239V2,
	"ANSI X9.62 elliptic curve c2tnb239v2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb239v3, SEC_OID_ANSIX962_EC_C2TNB239V3,
	"ANSI X9.62 elliptic curve c2tnb239v3", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2onb239v4, SEC_OID_ANSIX962_EC_C2ONB239V4,
	"ANSI X9.62 elliptic curve c2onb239v4", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2onb239v5, SEC_OID_ANSIX962_EC_C2ONB239V5,
	"ANSI X9.62 elliptic curve c2onb239v5", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb272w1, SEC_OID_ANSIX962_EC_C2PNB272W1,
	"ANSI X9.62 elliptic curve c2pnb272w1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb304w1, SEC_OID_ANSIX962_EC_C2PNB304W1,
	"ANSI X9.62 elliptic curve c2pnb304w1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb359v1, SEC_OID_ANSIX962_EC_C2TNB359V1,
	"ANSI X9.62 elliptic curve c2tnb359v1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2pnb368w1, SEC_OID_ANSIX962_EC_C2PNB368W1,
	"ANSI X9.62 elliptic curve c2pnb368w1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansiX962c2tnb431r1, SEC_OID_ANSIX962_EC_C2TNB431R1,
	"ANSI X9.62 elliptic curve c2tnb431r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),

    /* SECG named elliptic curves (characterisitic two field) */
    OD( secgECsect113r1, SEC_OID_SECG_EC_SECT113R1,
	"SECG elliptic curve sect113r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect113r2, SEC_OID_SECG_EC_SECT113R2,
	"SECG elliptic curve sect113r2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect131r1, SEC_OID_SECG_EC_SECT131R1,
	"SECG elliptic curve sect131r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect131r2, SEC_OID_SECG_EC_SECT131R2,
	"SECG elliptic curve sect131r2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect163k1, SEC_OID_SECG_EC_SECT163K1,
	"SECG elliptic curve sect163k1 (aka NIST K-163)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect163r1, SEC_OID_SECG_EC_SECT163R1,
	"SECG elliptic curve sect163r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect163r2, SEC_OID_SECG_EC_SECT163R2,
	"SECG elliptic curve sect163r2 (aka NIST B-163)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect193r1, SEC_OID_SECG_EC_SECT193R1,
	"SECG elliptic curve sect193r1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect193r2, SEC_OID_SECG_EC_SECT193R2,
	"SECG elliptic curve sect193r2", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect233k1, SEC_OID_SECG_EC_SECT233K1,
	"SECG elliptic curve sect233k1 (aka NIST K-233)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect233r1, SEC_OID_SECG_EC_SECT233R1,
	"SECG elliptic curve sect233r1 (aka NIST B-233)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect239k1, SEC_OID_SECG_EC_SECT239K1,
	"SECG elliptic curve sect239k1", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect283k1, SEC_OID_SECG_EC_SECT283K1,
	"SECG elliptic curve sect283k1 (aka NIST K-283)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect283r1, SEC_OID_SECG_EC_SECT283R1,
	"SECG elliptic curve sect283r1 (aka NIST B-283)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect409k1, SEC_OID_SECG_EC_SECT409K1,
	"SECG elliptic curve sect409k1 (aka NIST K-409)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect409r1, SEC_OID_SECG_EC_SECT409R1,
	"SECG elliptic curve sect409r1 (aka NIST B-409)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect571k1, SEC_OID_SECG_EC_SECT571K1,
	"SECG elliptic curve sect571k1 (aka NIST K-571)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( secgECsect571r1, SEC_OID_SECG_EC_SECT571R1,
	"SECG elliptic curve sect571r1 (aka NIST B-571)", 
	CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),

    OD( netscapeAOLScreenname, SEC_OID_NETSCAPE_AOLSCREENNAME,
	"AOL Screenname", CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),

    OD( x520SurName, SEC_OID_AVA_SURNAME,
    	"X520 Title",         CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520SerialNumber, SEC_OID_AVA_SERIAL_NUMBER,
        "X520 Serial Number", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520StreetAddress, SEC_OID_AVA_STREET_ADDRESS,
        "X520 Street Address", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520Title, SEC_OID_AVA_TITLE, 
    	"X520 Title",         CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520PostalAddress, SEC_OID_AVA_POSTAL_ADDRESS,
    	"X520 Postal Address", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520PostalCode, SEC_OID_AVA_POSTAL_CODE,
    	"X520 Postal Code",   CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520PostOfficeBox, SEC_OID_AVA_POST_OFFICE_BOX,
    	"X520 Post Office Box", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520GivenName, SEC_OID_AVA_GIVEN_NAME,
    	"X520 Given Name",    CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520Initials, SEC_OID_AVA_INITIALS,
    	"X520 Initials",      CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520GenerationQualifier, SEC_OID_AVA_GENERATION_QUALIFIER,
    	"X520 Generation Qualifier", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520HouseIdentifier, SEC_OID_AVA_HOUSE_IDENTIFIER,
    	"X520 House Identifier", 
	CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( x520Pseudonym, SEC_OID_AVA_PSEUDONYM,
    	"X520 Pseudonym",     CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* More OIDs */
    OD( pkixCAIssuers, SEC_OID_PKIX_CA_ISSUERS,
        "PKIX CA issuers access method", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
    OD( pkcs9ExtensionRequest, SEC_OID_PKCS9_EXTENSION_REQUEST,
    	"PKCS #9 Extension Request",
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    /* more ECC Signature Oids */
    OD( ansix962SignatureRecommended,
	SEC_OID_ANSIX962_ECDSA_SIGNATURE_RECOMMENDED_DIGEST,
	"X9.62 ECDSA signature with recommended digest", CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansix962SignatureSpecified,
	SEC_OID_ANSIX962_ECDSA_SIGNATURE_SPECIFIED_DIGEST,
	"X9.62 ECDSA signature with specified digest", CKM_ECDSA,
	INVALID_CERT_EXTENSION ),
    OD( ansix962SignaturewithSHA224Digest,
	SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE,
	"X9.62 ECDSA signature with SHA224", CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansix962SignaturewithSHA256Digest,
	SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE,
	"X9.62 ECDSA signature with SHA256", CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansix962SignaturewithSHA384Digest,
	SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE,
	"X9.62 ECDSA signature with SHA384", CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),
    OD( ansix962SignaturewithSHA512Digest,
	SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE,
	"X9.62 ECDSA signature with SHA512", CKM_INVALID_MECHANISM,
	INVALID_CERT_EXTENSION ),

    /* Camellia algorithm OIDs */
    OD( camellia128_CBC, SEC_OID_CAMELLIA_128_CBC,
	"CAMELLIA-128-CBC", CKM_CAMELLIA_CBC, INVALID_CERT_EXTENSION ),
    OD( camellia192_CBC, SEC_OID_CAMELLIA_192_CBC,
	"CAMELLIA-192-CBC", CKM_CAMELLIA_CBC, INVALID_CERT_EXTENSION ),
    OD( camellia256_CBC, SEC_OID_CAMELLIA_256_CBC,
	"CAMELLIA-256-CBC", CKM_CAMELLIA_CBC, INVALID_CERT_EXTENSION ),
};

/*
 * now the dynamic table. The dynamic table gets build at init time.
 * and conceivably gets modified if the user loads new crypto modules.
 * All this static data, and the allocated data to which it points,
 * is protected by a global reader/writer lock.  
 * The c language guarantees that global and static data that is not 
 * explicitly initialized will be initialized with zeros.  If we 
 * initialize it with zeros, the data goes into the initialized data
 * secment, and increases the size of the library.  By leaving it 
 * uninitialized, it is allocated in BSS, and does NOT increase the 
 * library size. 
 */
static NSSRWLock   * dynOidLock;
static PLArenaPool * dynOidPool;
static PLHashTable * dynOidHash;
static SECOidData ** dynOidTable;	/* not in the pool */
static int           dynOidEntriesAllocated;
static int           dynOidEntriesUsed;

/* Creates NSSRWLock and dynOidPool, if they don't exist.
** This function MIGHT create the lock, but not the pool, so
** code should test for dynOidPool, not dynOidLock, when deciding
** whether or not to call this function.
*/
static SECStatus
secoid_InitDynOidData(void)
{
    SECStatus   rv = SECSuccess;
    NSSRWLock * lock;

    /* This function will create the lock if it doesn't exist,
    ** and will return the address of the lock, whether it was 
    ** previously created, or was created by the function.
    */
    lock = nssRWLock_AtomicCreate(&dynOidLock, 1, "dynamic OID data");
    if (!lock) {
    	return SECFailure; /* Error code should already be set. */
    }
    PORT_Assert(lock == dynOidLock);
    NSSRWLock_LockWrite(lock);
    if (!dynOidPool) {
    	dynOidPool = PORT_NewArena(2048);
	if (!dynOidPool) {
	    rv = SECFailure /* Error code should already be set. */;
	}
    }
    NSSRWLock_UnlockWrite(lock);
    return rv;
}

/* Add oidData to hash table.  Caller holds write lock dynOidLock. */
static SECStatus
secoid_HashDynamicOiddata(const SECOidData * oid)
{
    PLHashEntry *entry;

    if (!dynOidHash) {
        dynOidHash = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
			PL_CompareValues, NULL, NULL);
	if ( !dynOidHash ) {
	    return SECFailure;
	}
    }

    entry = PL_HashTableAdd( dynOidHash, &oid->oid, (void *)oid );
    return entry ? SECSuccess : SECFailure;
}


/*
 * Lookup a Dynamic OID. Dynamic OID's still change slowly, so it's
 * cheaper to rehash the table when it changes than it is to do the loop
 * each time. 
 */
static SECOidData *
secoid_FindDynamic(const SECItem *key) 
{
    SECOidData *ret = NULL;

    if (dynOidHash) {
	NSSRWLock_LockRead(dynOidLock);
	if (dynOidHash) { /* must check it again with lock held. */
	    ret = (SECOidData *)PL_HashTableLookup(dynOidHash, key);
	}
	NSSRWLock_UnlockRead(dynOidLock);
    }
    if (ret == NULL) {
	PORT_SetError(SEC_ERROR_UNRECOGNIZED_OID);
    }
    return ret;
}

static SECOidData *
secoid_FindDynamicByTag(SECOidTag tagnum)
{
    SECOidData *data = NULL;
    int tagNumDiff;

    if (tagnum < SEC_OID_TOTAL) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return NULL;
    }
    tagNumDiff = tagnum - SEC_OID_TOTAL;

    if (dynOidTable) {
	NSSRWLock_LockRead(dynOidLock);
	if (dynOidTable != NULL && /* must check it again with lock held. */
	    tagNumDiff < dynOidEntriesUsed) {
	    data = dynOidTable[tagNumDiff];
	}
	NSSRWLock_UnlockRead(dynOidLock);
    }
    if (data == NULL) {
	PORT_SetError(SEC_ERROR_UNRECOGNIZED_OID);
    }
    return data;
}

/*
 * This routine is thread safe now.
 */
SECOidTag
SECOID_AddEntry(const SECOidData * src)
{
    SECOidData * dst;
    SECOidData **table;
    SECOidTag    ret         = SEC_OID_UNKNOWN;
    SECStatus    rv;
    int          tableEntries;
    int          used;

    if (!src || !src->oid.data || !src->oid.len || \
        !src->desc || !strlen(src->desc)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return ret;
    }
    if (src->supportedExtension != INVALID_CERT_EXTENSION     &&
    	src->supportedExtension != UNSUPPORTED_CERT_EXTENSION &&
    	src->supportedExtension != SUPPORTED_CERT_EXTENSION     ) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return ret;
    }

    if (!dynOidPool && secoid_InitDynOidData() != SECSuccess) {
	/* Caller has set error code. */
    	return ret;
    }

    NSSRWLock_LockWrite(dynOidLock);

    /* We've just acquired the write lock, and now we call FindOIDTag
    ** which will acquire and release the read lock.  NSSRWLock has been
    ** designed to allow this very case without deadlock.  This approach 
    ** makes the test for the presence of the OID, and the subsequent 
    ** addition of the OID to the table a single atomic write operation.
    */
    ret = SECOID_FindOIDTag(&src->oid);
    if (ret != SEC_OID_UNKNOWN) {
    	/* we could return an error here, but I chose not to do that.
	** This way, if we add an OID to the shared library's built in
	** list of OIDs in some future release, and that OID is the same
	** as some OID that a program has been adding, the program will
	** not suddenly stop working.
	*/
	goto done;
    }

    table        = dynOidTable;
    tableEntries = dynOidEntriesAllocated;
    used         = dynOidEntriesUsed;

    if (used + 1 > tableEntries) {
	SECOidData **newTable;
	int          newTableEntries = tableEntries + 16;

	newTable = (SECOidData **)PORT_Realloc(table, 
				       newTableEntries * sizeof(SECOidData *));
	if (newTable == NULL) {
	    goto done;
	}
	dynOidTable            = table        = newTable;
	dynOidEntriesAllocated = tableEntries = newTableEntries;
    }

    /* copy oid structure */
    dst = PORT_ArenaNew(dynOidPool, SECOidData);
    if (!dst) {
    	goto done;
    }
    rv  = SECITEM_CopyItem(dynOidPool, &dst->oid, &src->oid);
    if (rv != SECSuccess) {
	goto done;
    }
    dst->desc = PORT_ArenaStrdup(dynOidPool, src->desc);
    if (!dst->desc) {
	goto done;
    }
    dst->offset             = (SECOidTag)(used + SEC_OID_TOTAL);
    dst->mechanism          = src->mechanism;
    dst->supportedExtension = src->supportedExtension;

    rv = secoid_HashDynamicOiddata(dst);
    if ( rv == SECSuccess ) {
	table[used++] = dst;
	dynOidEntriesUsed = used;
	ret = dst->offset;
    }
done:
    NSSRWLock_UnlockWrite(dynOidLock);
    return ret;
}


/* normal static table processing */
static PLHashTable *oidhash     = NULL;
static PLHashTable *oidmechhash = NULL;

static PLHashNumber
secoid_HashNumber(const void *key)
{
    return (PLHashNumber) key;
}


SECStatus
secoid_Init(void)
{
    PLHashEntry *entry;
    const SECOidData *oid;
    int i;

    if (!dynOidPool && secoid_InitDynOidData() != SECSuccess) {
    	return SECFailure;
    }

    if (oidhash) {
	return SECSuccess;
    }
    
    oidhash = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
			PL_CompareValues, NULL, NULL);
    oidmechhash = PL_NewHashTable(0, secoid_HashNumber, PL_CompareValues,
			PL_CompareValues, NULL, NULL);

    if ( !oidhash || !oidmechhash) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
 	PORT_Assert(0); /*This function should never fail. */
	return(SECFailure);
    }

    for ( i = 0; i < ( sizeof(oids) / sizeof(SECOidData) ); i++ ) {
	oid = &oids[i];

	PORT_Assert ( oid->offset == i );

	entry = PL_HashTableAdd( oidhash, &oid->oid, (void *)oid );
	if ( entry == NULL ) {
	    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            PORT_Assert(0); /*This function should never fail. */
	    return(SECFailure);
	}

	if ( oid->mechanism != CKM_INVALID_MECHANISM ) {
	    entry = PL_HashTableAdd( oidmechhash, 
					(void *)oid->mechanism, (void *)oid );
	    if ( entry == NULL ) {
	        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                PORT_Assert(0); /* This function should never fail. */
		return(SECFailure);
	    }
	}
    }

    PORT_Assert (i == SEC_OID_TOTAL);

    return(SECSuccess);
}

SECOidData *
SECOID_FindOIDByMechanism(unsigned long mechanism)
{
    SECOidData *ret;

    PR_ASSERT(oidhash != NULL);

    ret = PL_HashTableLookupConst ( oidmechhash, (void *)mechanism);
    if ( ret == NULL ) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    }

    return (ret);
}

SECOidData *
SECOID_FindOID(const SECItem *oid)
{
    SECOidData *ret;

    PR_ASSERT(oidhash != NULL);
    
    ret = PL_HashTableLookupConst ( oidhash, oid );
    if ( ret == NULL ) {
	ret  = secoid_FindDynamic(oid);
	if (ret == NULL) {
	    PORT_SetError(SEC_ERROR_UNRECOGNIZED_OID);
	}
    }

    return(ret);
}

SECOidTag
SECOID_FindOIDTag(const SECItem *oid)
{
    SECOidData *oiddata;

    oiddata = SECOID_FindOID (oid);
    if (oiddata == NULL)
	return SEC_OID_UNKNOWN;

    return oiddata->offset;
}

/* This really should return const. */
SECOidData *
SECOID_FindOIDByTag(SECOidTag tagnum)
{

    if (tagnum >= SEC_OID_TOTAL) {
	return secoid_FindDynamicByTag(tagnum);
    }

    PORT_Assert((unsigned int)tagnum < (sizeof(oids) / sizeof(SECOidData)));
    return (SECOidData *)(&oids[tagnum]);
}

PRBool SECOID_KnownCertExtenOID (SECItem *extenOid)
{
    SECOidData * oidData;

    oidData = SECOID_FindOID (extenOid);
    if (oidData == (SECOidData *)NULL)
	return (PR_FALSE);
    return ((oidData->supportedExtension == SUPPORTED_CERT_EXTENSION) ?
            PR_TRUE : PR_FALSE);
}


const char *
SECOID_FindOIDTagDescription(SECOidTag tagnum)
{
  const SECOidData *oidData = SECOID_FindOIDByTag(tagnum);
  return oidData ? oidData->desc : 0;
}

/*
 * free up the oid tables.
 */
SECStatus
SECOID_Shutdown(void)
{
    if (oidhash) {
	PL_HashTableDestroy(oidhash);
	oidhash = NULL;
    }
    if (oidmechhash) {
	PL_HashTableDestroy(oidmechhash);
	oidmechhash = NULL;
    }
    /* Have to handle the case where the lock was created, but
    ** the pool wasn't. 
    ** I'm not going to attempt to create the lock, just to protect
    ** the destruction of data that probably isn't initialized anyway.
    */
    if (dynOidLock) {
	NSSRWLock_LockWrite(dynOidLock);
	if (dynOidHash) {
	    PL_HashTableDestroy(dynOidHash);
	    dynOidHash = NULL;
	}
	if (dynOidPool) {
	    PORT_FreeArena(dynOidPool, PR_FALSE);
	    dynOidPool = NULL;
	}
	if (dynOidTable) {
	    PORT_Free(dynOidTable);
	    dynOidTable = NULL;
	}
	dynOidEntriesAllocated = 0;
	dynOidEntriesUsed = 0;

	NSSRWLock_UnlockWrite(dynOidLock);
	NSSRWLock_Destroy(dynOidLock);
	dynOidLock = NULL;
    } else {
    	/* Since dynOidLock doesn't exist, then all the data it protects
	** should be uninitialized.  We'll check that (in DEBUG builds),
	** and then make sure it is so, in case NSS is reinitialized.
	*/
	PORT_Assert(!dynOidHash && !dynOidPool && !dynOidTable && \
	            !dynOidEntriesAllocated && !dynOidEntriesUsed);
	dynOidHash = NULL;
	dynOidPool = NULL;
	dynOidTable = NULL;
	dynOidEntriesAllocated = 0;
	dynOidEntriesUsed = 0;
    }
    return SECSuccess;
}
