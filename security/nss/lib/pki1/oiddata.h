/* THIS IS A GENERATED FILE */
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

#ifndef OIDDATA_H
#define OIDDATA_H

#ifdef DEBUG
static const char OIDDATA_CVS_ID[] = "@(#) $RCSfile: oiddata.h,v $ $Revision: 1.4 $ $Date: 2005/03/14 18:02:00 $ ; @(#) $RCSfile: oiddata.h,v $ $Revision: 1.4 $ $Date: 2005/03/14 18:02:00 $";
#endif /* DEBUG */

#ifndef NSSPKI1T_H
#include "nsspki1t.h"
#endif /* NSSPKI1T_H */

extern const NSSOID *NSS_OID_RFC1274_UID;
extern const NSSOID *NSS_OID_RFC1274_EMAIL;
extern const NSSOID *NSS_OID_RFC2247_DC;
extern const NSSOID *NSS_OID_ANSIX9_DSA_SIGNATURE;
extern const NSSOID *NSS_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
extern const NSSOID *NSS_OID_X942_DIFFIE_HELMAN_KEY;
extern const NSSOID *NSS_OID_PKCS1_RSA_ENCRYPTION;
extern const NSSOID *NSS_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
extern const NSSOID *NSS_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION;
extern const NSSOID *NSS_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
extern const NSSOID *NSS_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
extern const NSSOID *NSS_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC;
extern const NSSOID *NSS_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC;
extern const NSSOID *NSS_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC;
extern const NSSOID *NSS_OID_PKCS7;
extern const NSSOID *NSS_OID_PKCS7_DATA;
extern const NSSOID *NSS_OID_PKCS7_SIGNED_DATA;
extern const NSSOID *NSS_OID_PKCS7_ENVELOPED_DATA;
extern const NSSOID *NSS_OID_PKCS7_SIGNED_ENVELOPED_DATA;
extern const NSSOID *NSS_OID_PKCS7_DIGESTED_DATA;
extern const NSSOID *NSS_OID_PKCS7_ENCRYPTED_DATA;
extern const NSSOID *NSS_OID_PKCS9_EMAIL_ADDRESS;
extern const NSSOID *NSS_OID_PKCS9_UNSTRUCTURED_NAME;
extern const NSSOID *NSS_OID_PKCS9_CONTENT_TYPE;
extern const NSSOID *NSS_OID_PKCS9_MESSAGE_DIGEST;
extern const NSSOID *NSS_OID_PKCS9_SIGNING_TIME;
extern const NSSOID *NSS_OID_PKCS9_COUNTER_SIGNATURE;
extern const NSSOID *NSS_OID_PKCS9_CHALLENGE_PASSWORD;
extern const NSSOID *NSS_OID_PKCS9_UNSTRUCTURED_ADDRESS;
extern const NSSOID *NSS_OID_PKCS9_EXTENDED_CERTIFICATE_ATTRIBUTES;
extern const NSSOID *NSS_OID_PKCS9_SMIME_CAPABILITIES;
extern const NSSOID *NSS_OID_PKCS9_FRIENDLY_NAME;
extern const NSSOID *NSS_OID_PKCS9_LOCAL_KEY_ID;
extern const NSSOID *NSS_OID_PKCS9_X509_CERT;
extern const NSSOID *NSS_OID_PKCS9_SDSI_CERT;
extern const NSSOID *NSS_OID_PKCS9_X509_CRL;
extern const NSSOID *NSS_OID_PKCS12;
extern const NSSOID *NSS_OID_PKCS12_PBE_IDS;
extern const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4;
extern const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4;
extern const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_3_KEY_TRIPLE_DES_CBC;
extern const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_2_KEY_TRIPLE_DES_CBC;
extern const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
extern const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
extern const NSSOID *NSS_OID_PKCS12_KEY_BAG;
extern const NSSOID *NSS_OID_PKCS12_PKCS8_SHROUDED_KEY_BAG;
extern const NSSOID *NSS_OID_PKCS12_CERT_BAG;
extern const NSSOID *NSS_OID_PKCS12_CRL_BAG;
extern const NSSOID *NSS_OID_PKCS12_SECRET_BAG;
extern const NSSOID *NSS_OID_PKCS12_SAFE_CONTENTS_BAG;
extern const NSSOID *NSS_OID_MD2;
extern const NSSOID *NSS_OID_MD4;
extern const NSSOID *NSS_OID_MD5;
extern const NSSOID *NSS_OID_RC2_CBC;
extern const NSSOID *NSS_OID_RC4;
extern const NSSOID *NSS_OID_DES_EDE3_CBC;
extern const NSSOID *NSS_OID_RC5_CBC_PAD;
extern const NSSOID *NSS_OID_X509_AUTH_INFO_ACCESS;
extern const NSSOID *NSS_OID_PKIX_CPS_POINTER_QUALIFIER;
extern const NSSOID *NSS_OID_PKIX_USER_NOTICE_QUALIFIER;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_SERVER_AUTH;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_CLIENT_AUTH;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_CODE_SIGN;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_EMAIL_PROTECTION;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_IPSEC_END_SYSTEM;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_IPSEC_TUNNEL;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_IPSEC_USER;
extern const NSSOID *NSS_OID_EXT_KEY_USAGE_TIME_STAMP;
extern const NSSOID *NSS_OID_OCSP_RESPONDER;
extern const NSSOID *NSS_OID_PKIX_REGCTRL_REGTOKEN;
extern const NSSOID *NSS_OID_PKIX_REGCTRL_AUTHENTICATOR;
extern const NSSOID *NSS_OID_PKIX_REGCTRL_PKIPUBINFO;
extern const NSSOID *NSS_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS;
extern const NSSOID *NSS_OID_PKIX_REGCTRL_OLD_CERT_ID;
extern const NSSOID *NSS_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY;
extern const NSSOID *NSS_OID_PKIX_REGINFO_UTF8_PAIRS;
extern const NSSOID *NSS_OID_PKIX_REGINFO_CERT_REQUEST;
extern const NSSOID *NSS_OID_OID_PKIX_OCSP;
extern const NSSOID *NSS_OID_PKIX_OCSP_BASIC_RESPONSE;
extern const NSSOID *NSS_OID_PKIX_OCSP_NONCE;
extern const NSSOID *NSS_OID_PKIX_OCSP_RESPONSE;
extern const NSSOID *NSS_OID_PKIX_OCSP_CRL;
extern const NSSOID *NSS_OID_X509_OCSP_NO_CHECK;
extern const NSSOID *NSS_OID_PKIX_OCSP_ARCHIVE_CUTOFF;
extern const NSSOID *NSS_OID_PKIX_OCSP_SERVICE_LOCATOR;
extern const NSSOID *NSS_OID_DES_ECB;
extern const NSSOID *NSS_OID_DES_CBC;
extern const NSSOID *NSS_OID_DES_OFB;
extern const NSSOID *NSS_OID_DES_CFB;
extern const NSSOID *NSS_OID_DES_MAC;
extern const NSSOID *NSS_OID_ISO_SHA_WITH_RSA_SIGNATURE;
extern const NSSOID *NSS_OID_DES_EDE;
extern const NSSOID *NSS_OID_SHA1;
extern const NSSOID *NSS_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST;
extern const NSSOID *NSS_OID_X520_COMMON_NAME;
extern const NSSOID *NSS_OID_X520_SURNAME;
extern const NSSOID *NSS_OID_X520_COUNTRY_NAME;
extern const NSSOID *NSS_OID_X520_LOCALITY_NAME;
extern const NSSOID *NSS_OID_X520_STATE_OR_PROVINCE_NAME;
extern const NSSOID *NSS_OID_X520_ORGANIZATION_NAME;
extern const NSSOID *NSS_OID_X520_ORGANIZATIONAL_UNIT_NAME;
extern const NSSOID *NSS_OID_X520_TITLE;
extern const NSSOID *NSS_OID_X520_NAME;
extern const NSSOID *NSS_OID_X520_GIVEN_NAME;
extern const NSSOID *NSS_OID_X520_INITIALS;
extern const NSSOID *NSS_OID_X520_GENERATION_QUALIFIER;
extern const NSSOID *NSS_OID_X520_DN_QUALIFIER;
extern const NSSOID *NSS_OID_X500_RSA_ENCRYPTION;
extern const NSSOID *NSS_OID_X509_SUBJECT_DIRECTORY_ATTR;
extern const NSSOID *NSS_OID_X509_SUBJECT_DIRECTORY_ATTRIBUTES;
extern const NSSOID *NSS_OID_X509_SUBJECT_KEY_ID;
extern const NSSOID *NSS_OID_X509_KEY_USAGE;
extern const NSSOID *NSS_OID_X509_PRIVATE_KEY_USAGE_PERIOD;
extern const NSSOID *NSS_OID_X509_SUBJECT_ALT_NAME;
extern const NSSOID *NSS_OID_X509_ISSUER_ALT_NAME;
extern const NSSOID *NSS_OID_X509_BASIC_CONSTRAINTS;
extern const NSSOID *NSS_OID_X509_CRL_NUMBER;
extern const NSSOID *NSS_OID_X509_REASON_CODE;
extern const NSSOID *NSS_OID_X509_HOLD_INSTRUCTION_CODE;
extern const NSSOID *NSS_OID_X509_INVALID_DATE;
extern const NSSOID *NSS_OID_X509_DELTA_CRL_INDICATOR;
extern const NSSOID *NSS_OID_X509_ISSUING_DISTRIBUTION_POINT;
extern const NSSOID *NSS_OID_X509_CERTIFICATE_ISSUER;
extern const NSSOID *NSS_OID_X509_NAME_CONSTRAINTS;
extern const NSSOID *NSS_OID_X509_CRL_DIST_POINTS;
extern const NSSOID *NSS_OID_X509_CERTIFICATE_POLICIES;
extern const NSSOID *NSS_OID_X509_POLICY_MAPPINGS;
extern const NSSOID *NSS_OID_X509_AUTH_KEY_ID;
extern const NSSOID *NSS_OID_X509_POLICY_CONSTRAINTS;
extern const NSSOID *NSS_OID_X509_EXT_KEY_USAGE;
extern const NSSOID *NSS_OID_MISSI_DSS_OLD;
extern const NSSOID *NSS_OID_FORTEZZA_SKIPJACK;
extern const NSSOID *NSS_OID_MISSI_KEA;
extern const NSSOID *NSS_OID_MISSI_KEA_DSS_OLD;
extern const NSSOID *NSS_OID_MISSI_DSS;
extern const NSSOID *NSS_OID_MISSI_KEA_DSS;
extern const NSSOID *NSS_OID_MISSI_ALT_KEY;
extern const NSSOID *NSS_OID_NS_CERT_EXT_CERT_TYPE;
extern const NSSOID *NSS_OID_NS_CERT_EXT_BASE_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_REVOCATION_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_CA_REVOCATION_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_CA_CRL_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_CA_CERT_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_CERT_RENEWAL_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_CA_POLICY_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_HOMEPAGE_URL;
extern const NSSOID *NSS_OID_NS_CERT_EXT_ENTITY_LOGO;
extern const NSSOID *NSS_OID_NS_CERT_EXT_USER_PICTURE;
extern const NSSOID *NSS_OID_NS_CERT_EXT_SSL_SERVER_NAME;
extern const NSSOID *NSS_OID_NS_CERT_EXT_COMMENT;
extern const NSSOID *NSS_OID_NS_CERT_EXT_THAYES;
extern const NSSOID *NSS_OID_NS_TYPE_GIF;
extern const NSSOID *NSS_OID_NS_TYPE_JPEG;
extern const NSSOID *NSS_OID_NS_TYPE_URL;
extern const NSSOID *NSS_OID_NS_TYPE_HTML;
extern const NSSOID *NSS_OID_NS_TYPE_CERT_SEQUENCE;
extern const NSSOID *NSS_OID_NS_KEY_USAGE_GOVT_APPROVED;
extern const NSSOID *NSS_OID_NETSCAPE_RECOVERY_REQUEST;
extern const NSSOID *NSS_OID_NETSCAPE_SMIME_KEA;
extern const NSSOID *NSS_OID_NETSCAPE_NICKNAME;
extern const NSSOID *NSS_OID_VERISIGN_USER_NOTICES;
extern const NSSOID *NSS_OID_NS_CERT_EXT_NETSCAPE_OK;
extern const NSSOID *NSS_OID_NS_CERT_EXT_ISSUER_LOGO;
extern const NSSOID *NSS_OID_NS_CERT_EXT_SUBJECT_LOGO;

#endif /* OIDDATA_H */
