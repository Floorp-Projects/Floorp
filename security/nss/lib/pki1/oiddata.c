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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: oiddata.c,v $ $Revision: 1.6 $ $Date: 2005/04/11 21:27:01 $ ; @(#) $RCSfile: oiddata.c,v $ $Revision: 1.6 $ $Date: 2005/04/11 21:27:01 $";
#endif /* DEBUG */

#ifndef PKI1T_H
#include "pki1t.h"
#endif /* PKI1T_H */

const NSSOID nss_builtin_oids[] = {
  {
#ifdef DEBUG
    "ccitt",
    "ITU-T",
#endif /* DEBUG */
    { "\x80\x00", 2 }
  },
  {
#ifdef DEBUG
    "recommendation",
    "ITU-T Recommendation",
#endif /* DEBUG */
    { "\x00", 1 }
  },
  {
#ifdef DEBUG
    "question",
    "ITU-T Question",
#endif /* DEBUG */
    { "\x01", 1 }
  },
  {
#ifdef DEBUG
    "administration",
    "ITU-T Administration",
#endif /* DEBUG */
    { "\x02", 1 }
  },
  {
#ifdef DEBUG
    "network-operator",
    "ITU-T Network Operator",
#endif /* DEBUG */
    { "\x03", 1 }
  },
  {
#ifdef DEBUG
    "identified-organization",
    "ITU-T Identified Organization",
#endif /* DEBUG */
    { "\x04", 1 }
  },
  {
#ifdef DEBUG
    "data",
    "RFC Data",
#endif /* DEBUG */
    { "\x09", 1 }
  },
  {
#ifdef DEBUG
    "pss",
    "PSS British Telecom X.25 Network",
#endif /* DEBUG */
    { "\x09\x92\x26", 3 }
  },
  {
#ifdef DEBUG
    "ucl",
    "RFC 1274 UCL Data networks",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c", 7 }
  },
  {
#ifdef DEBUG
    "pilot",
    "RFC 1274 pilot",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64", 8 }
  },
  {
#ifdef DEBUG
    "attributeType",
    "RFC 1274 Attribute Type",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x01", 9 }
  },
  {
#ifdef DEBUG
    "uid",
    "RFC 1274 User Id",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x01\x01", 10 }
  },
  {
#ifdef DEBUG
    "mail",
    "RFC 1274 E-mail Addres",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x01\x03", 10 }
  },
  {
#ifdef DEBUG
    "dc",
    "RFC 2247 Domain Component",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x01\x19", 10 }
  },
  {
#ifdef DEBUG
    "attributeSyntax",
    "RFC 1274 Attribute Syntax",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x03", 9 }
  },
  {
#ifdef DEBUG
    "iA5StringSyntax",
    "RFC 1274 IA5 String Attribute Syntax",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x03\x04", 10 }
  },
  {
#ifdef DEBUG
    "caseIgnoreIA5StringSyntax",
    "RFC 1274 Case-Ignore IA5 String Attribute Syntax",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x03\x05", 10 }
  },
  {
#ifdef DEBUG
    "objectClass",
    "RFC 1274 Object Class",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x04", 9 }
  },
  {
#ifdef DEBUG
    "groups",
    "RFC 1274 Groups",
#endif /* DEBUG */
    { "\x09\x92\x26\x89\x93\xf2\x2c\x64\x0a", 9 }
  },
  {
#ifdef DEBUG
    "ucl",
    "RFC 1327 ucl",
#endif /* DEBUG */
    { "\x09\x92\x26\x86\xe8\xc4\xb5\xbe\x2c", 9 }
  },
  {
#ifdef DEBUG
    "iso",
    "ISO",
#endif /* DEBUG */
    { "\x80\x01", 2 }
  },
  {
#ifdef DEBUG
    "standard",
    "ISO Standard",
#endif /* DEBUG */
    { "\x28", 1 }
  },
  {
#ifdef DEBUG
    "registration-authority",
    "ISO Registration Authority",
#endif /* DEBUG */
    { "\x29", 1 }
  },
  {
#ifdef DEBUG
    "member-body",
    "ISO Member Body",
#endif /* DEBUG */
    { "\x2a", 1 }
  },
  {
#ifdef DEBUG
    "australia",
    "Australia (ISO)",
#endif /* DEBUG */
    { "\x2a\x24", 2 }
  },
  {
#ifdef DEBUG
    "taiwan",
    "Taiwan (ISO)",
#endif /* DEBUG */
    { "\x2a\x81\x1e", 3 }
  },
  {
#ifdef DEBUG
    "ireland",
    "Ireland (ISO)",
#endif /* DEBUG */
    { "\x2a\x82\x74", 3 }
  },
  {
#ifdef DEBUG
    "norway",
    "Norway (ISO)",
#endif /* DEBUG */
    { "\x2a\x84\x42", 3 }
  },
  {
#ifdef DEBUG
    "sweden",
    "Sweden (ISO)",
#endif /* DEBUG */
    { "\x2a\x85\x70", 3 }
  },
  {
#ifdef DEBUG
    "great-britain",
    "Great Britain (ISO)",
#endif /* DEBUG */
    { "\x2a\x86\x3a", 3 }
  },
  {
#ifdef DEBUG
    "us",
    "United States (ISO)",
#endif /* DEBUG */
    { "\x2a\x86\x48", 3 }
  },
  {
#ifdef DEBUG
    "organization",
    "US (ISO) organization",
#endif /* DEBUG */
    { "\x2a\x86\x48\x01", 4 }
  },
  {
#ifdef DEBUG
    "ansi-z30-50",
    "ANSI Z39.50",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x13", 5 }
  },
  {
#ifdef DEBUG
    "dicom",
    "DICOM",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x18", 5 }
  },
  {
#ifdef DEBUG
    "ieee-1224",
    "IEEE 1224",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x21", 5 }
  },
  {
#ifdef DEBUG
    "ieee-802-10",
    "IEEE 802.10",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x26", 5 }
  },
  {
#ifdef DEBUG
    "ieee-802-11",
    "IEEE 802.11",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x34", 5 }
  },
  {
#ifdef DEBUG
    "x9-57",
    "ANSI X9.57",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38", 5 }
  },
  {
#ifdef DEBUG
    "holdInstruction",
    "ANSI X9.57 Hold Instruction",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x02", 6 }
  },
  {
#ifdef DEBUG
    "id-holdinstruction-none",
    "ANSI X9.57 Hold Instruction: None",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x02\x01", 7 }
  },
  {
#ifdef DEBUG
    "id-holdinstruction-callissuer",
    "ANSI X9.57 Hold Instruction: Call Issuer",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x02\x02", 7 }
  },
  {
#ifdef DEBUG
    "id-holdinstruction-reject",
    "ANSI X9.57 Hold Instruction: Reject",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x02\x03", 7 }
  },
  {
#ifdef DEBUG
    "x9algorithm",
    "ANSI X9.57 Algorithm",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x04", 6 }
  },
  {
#ifdef DEBUG
    "id-dsa",
    "ANSI X9.57 DSA Signature",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x04\x01", 7 }
  },
  {
#ifdef DEBUG
    "id-dsa-with-sha1",
    "ANSI X9.57 Algorithm DSA Signature with SHA-1 Digest",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x38\x04\x03", 7 }
  },
  {
#ifdef DEBUG
    "x942",
    "ANSI X9.42",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x3e", 5 }
  },
  {
#ifdef DEBUG
    "algorithm",
    "ANSI X9.42 Algorithm",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x3e\x02", 6 }
  },
  {
#ifdef DEBUG
    "dhpublicnumber",
    "Diffie-Hellman Public Key Algorithm",
#endif /* DEBUG */
    { "\x2a\x86\x48\xce\x3e\x02\x01", 7 }
  },
  {
#ifdef DEBUG
    "entrust",
    "Entrust Technologies",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf6\x7d", 6 }
  },
  {
#ifdef DEBUG
    "rsadsi",
    "RSA Data Security Inc.",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d", 6 }
  },
  {
#ifdef DEBUG
    "pkcs",
    "PKCS",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01", 7 }
  },
  {
#ifdef DEBUG
    "pkcs-1",
    "PKCS #1",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x01", 8 }
  },
  {
#ifdef DEBUG
    "rsaEncryption",
    "PKCS #1 RSA Encryption",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01", 9 }
  },
  {
#ifdef DEBUG
    "md2WithRSAEncryption",
    "PKCS #1 MD2 With RSA Encryption",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x02", 9 }
  },
  {
#ifdef DEBUG
    "md4WithRSAEncryption",
    "PKCS #1 MD4 With RSA Encryption",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x03", 9 }
  },
  {
#ifdef DEBUG
    "md5WithRSAEncryption",
    "PKCS #1 MD5 With RSA Encryption",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x04", 9 }
  },
  {
#ifdef DEBUG
    "sha1WithRSAEncryption",
    "PKCS #1 SHA-1 With RSA Encryption",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05", 9 }
  },
  {
#ifdef DEBUG
    "pkcs-5",
    "PKCS #5",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x05", 8 }
  },
  {
#ifdef DEBUG
    "pbeWithMD2AndDES-CBC",
    "PKCS #5 Password Based Encryption With MD2 and DES-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x01", 9 }
  },
  {
#ifdef DEBUG
    "pbeWithMD5AndDES-CBC",
    "PKCS #5 Password Based Encryption With MD5 and DES-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x03", 9 }
  },
  {
#ifdef DEBUG
    "pbeWithSha1AndDES-CBC",
    "PKCS #5 Password Based Encryption With SHA-1 and DES-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x05\x0a", 9 }
  },
  {
#ifdef DEBUG
    "pkcs-7",
    "PKCS #7",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07", 8 }
  },
  {
#ifdef DEBUG
    "data",
    "PKCS #7 Data",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07\x01", 9 }
  },
  {
#ifdef DEBUG
    "signedData",
    "PKCS #7 Signed Data",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07\x02", 9 }
  },
  {
#ifdef DEBUG
    "envelopedData",
    "PKCS #7 Enveloped Data",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07\x03", 9 }
  },
  {
#ifdef DEBUG
    "signedAndEnvelopedData",
    "PKCS #7 Signed and Enveloped Data",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07\x04", 9 }
  },
  {
#ifdef DEBUG
    "digestedData",
    "PKCS #7 Digested Data",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07\x05", 9 }
  },
  {
#ifdef DEBUG
    "encryptedData",
    "PKCS #7 Encrypted Data",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x07\x06", 9 }
  },
  {
#ifdef DEBUG
    "pkcs-9",
    "PKCS #9",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09", 8 }
  },
  {
#ifdef DEBUG
    "emailAddress",
    "PKCS #9 Email Address",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01", 9 }
  },
  {
#ifdef DEBUG
    "unstructuredName",
    "PKCS #9 Unstructured Name",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x02", 9 }
  },
  {
#ifdef DEBUG
    "contentType",
    "PKCS #9 Content Type",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x03", 9 }
  },
  {
#ifdef DEBUG
    "messageDigest",
    "PKCS #9 Message Digest",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x04", 9 }
  },
  {
#ifdef DEBUG
    "signingTime",
    "PKCS #9 Signing Time",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x05", 9 }
  },
  {
#ifdef DEBUG
    "counterSignature",
    "PKCS #9 Counter Signature",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x06", 9 }
  },
  {
#ifdef DEBUG
    "challengePassword",
    "PKCS #9 Challenge Password",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x07", 9 }
  },
  {
#ifdef DEBUG
    "unstructuredAddress",
    "PKCS #9 Unstructured Address",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x08", 9 }
  },
  {
#ifdef DEBUG
    "extendedCertificateAttributes",
    "PKCS #9 Extended Certificate Attributes",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x09", 9 }
  },
  {
#ifdef DEBUG
    "sMIMECapabilities",
    "PKCS #9 S/MIME Capabilities",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x0f", 9 }
  },
  {
#ifdef DEBUG
    "friendlyName",
    "PKCS #9 Friendly Name",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x14", 9 }
  },
  {
#ifdef DEBUG
    "localKeyID",
    "PKCS #9 Local Key ID",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x15", 9 }
  },
  {
#ifdef DEBUG
    "certTypes",
    "PKCS #9 Certificate Types",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x16", 9 }
  },
  {
#ifdef DEBUG
    "x509Certificate",
    "PKCS #9 Certificate Type = X.509",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x16\x01", 10 }
  },
  {
#ifdef DEBUG
    "sdsiCertificate",
    "PKCS #9 Certificate Type = SDSI",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x16\x02", 10 }
  },
  {
#ifdef DEBUG
    "crlTypes",
    "PKCS #9 Certificate Revocation List Types",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x17", 9 }
  },
  {
#ifdef DEBUG
    "x509Crl",
    "PKCS #9 CRL Type = X.509",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x17\x01", 10 }
  },
  {
#ifdef DEBUG
    "pkcs-12",
    "PKCS #12",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c", 8 }
  },
  {
#ifdef DEBUG
    "pkcs-12PbeIds",
    "PKCS #12 Password Based Encryption IDs",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01", 9 }
  },
  {
#ifdef DEBUG
    "pbeWithSHA1And128BitRC4",
    "PKCS #12 Password Based Encryption With SHA-1 and 128-bit RC4",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x01", 10 }
  },
  {
#ifdef DEBUG
    "pbeWithSHA1And40BitRC4",
    "PKCS #12 Password Based Encryption With SHA-1 and 40-bit RC4",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x02", 10 }
  },
  {
#ifdef DEBUG
    "pbeWithSHA1And3-KeyTripleDES-CBC",
    "PKCS #12 Password Based Encryption With SHA-1 and 3-key Triple DES-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x03", 10 }
  },
  {
#ifdef DEBUG
    "pbeWithSHA1And2-KeyTripleDES-CBC",
    "PKCS #12 Password Based Encryption With SHA-1 and 2-key Triple DES-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x04", 10 }
  },
  {
#ifdef DEBUG
    "pbeWithSHA1And128BitRC2-CBC",
    "PKCS #12 Password Based Encryption With SHA-1 and 128-bit RC2-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x05", 10 }
  },
  {
#ifdef DEBUG
    "pbeWithSHA1And40BitRC2-CBC",
    "PKCS #12 Password Based Encryption With SHA-1 and 40-bit RC2-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x01\x06", 10 }
  },
  {
#ifdef DEBUG
    "pkcs-12EspvkIds",
    "PKCS #12 ESPVK IDs",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x02", 9 }
  },
  {
#ifdef DEBUG
    "pkcs8-key-shrouding",
    "PKCS #12 Key Shrouding",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x02\x01", 10 }
  },
  {
#ifdef DEBUG
    "draft1Pkcs-12Bag-ids",
    "Draft 1.0 PKCS #12 Bag IDs",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x03", 9 }
  },
  {
#ifdef DEBUG
    "keyBag",
    "Draft 1.0 PKCS #12 Key Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x03\x01", 10 }
  },
  {
#ifdef DEBUG
    "certAndCRLBagId",
    "Draft 1.0 PKCS #12 Cert and CRL Bag ID",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x03\x02", 10 }
  },
  {
#ifdef DEBUG
    "secretBagId",
    "Draft 1.0 PKCS #12 Secret Bag ID",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x03\x03", 10 }
  },
  {
#ifdef DEBUG
    "safeContentsId",
    "Draft 1.0 PKCS #12 Safe Contents Bag ID",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x03\x04", 10 }
  },
  {
#ifdef DEBUG
    "pkcs-8ShroudedKeyBagId",
    "Draft 1.0 PKCS #12 PKCS #8-shrouded Key Bag ID",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x03\x05", 10 }
  },
  {
#ifdef DEBUG
    "pkcs-12CertBagIds",
    "PKCS #12 Certificate Bag IDs",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x04", 9 }
  },
  {
#ifdef DEBUG
    "x509CertCRLBagId",
    "PKCS #12 X.509 Certificate and CRL Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x04\x01", 10 }
  },
  {
#ifdef DEBUG
    "SDSICertBagID",
    "PKCS #12 SDSI Certificate Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x04\x02", 10 }
  },
  {
#ifdef DEBUG
    "pkcs-12Oids",
    "PKCS #12 OIDs (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05", 9 }
  },
  {
#ifdef DEBUG
    "pkcs-12PbeIds",
    "PKCS #12 OIDs PBE IDs (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x01", 10 }
  },
  {
#ifdef DEBUG
    "pbeWithSha1And128BitRC4",
    "PKCS #12 OIDs PBE with SHA-1 and 128-bit RC4 (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x01\x01", 11 }
  },
  {
#ifdef DEBUG
    "pbeWithSha1And40BitRC4",
    "PKCS #12 OIDs PBE with SHA-1 and 40-bit RC4 (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x01\x02", 11 }
  },
  {
#ifdef DEBUG
    "pbeWithSha1AndTripleDES-CBC",
    "PKCS #12 OIDs PBE with SHA-1 and Triple DES-CBC (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x01\x03", 11 }
  },
  {
#ifdef DEBUG
    "pbeWithSha1And128BitRC2-CBC",
    "PKCS #12 OIDs PBE with SHA-1 and 128-bit RC2-CBC (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x01\x04", 11 }
  },
  {
#ifdef DEBUG
    "pbeWithSha1And40BitRC2-CBC",
    "PKCS #12 OIDs PBE with SHA-1 and 40-bit RC2-CBC (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x01\x05", 11 }
  },
  {
#ifdef DEBUG
    "pkcs-12EnvelopingIds",
    "PKCS #12 OIDs Enveloping IDs (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x02", 10 }
  },
  {
#ifdef DEBUG
    "rsaEncryptionWith128BitRC4",
    "PKCS #12 OIDs Enveloping RSA Encryption with 128-bit RC4",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x02\x01", 11 }
  },
  {
#ifdef DEBUG
    "rsaEncryptionWith40BitRC4",
    "PKCS #12 OIDs Enveloping RSA Encryption with 40-bit RC4",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x02\x02", 11 }
  },
  {
#ifdef DEBUG
    "rsaEncryptionWithTripleDES",
    "PKCS #12 OIDs Enveloping RSA Encryption with Triple DES",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x02\x03", 11 }
  },
  {
#ifdef DEBUG
    "pkcs-12SignatureIds",
    "PKCS #12 OIDs Signature IDs (XXX)",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x03", 10 }
  },
  {
#ifdef DEBUG
    "rsaSignatureWithSHA1Digest",
    "PKCS #12 OIDs RSA Signature with SHA-1 Digest",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x05\x03\x01", 11 }
  },
  {
#ifdef DEBUG
    "pkcs-12Version1",
    "PKCS #12 Version 1",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a", 9 }
  },
  {
#ifdef DEBUG
    "pkcs-12BagIds",
    "PKCS #12 Bag IDs",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01", 10 }
  },
  {
#ifdef DEBUG
    "keyBag",
    "PKCS #12 Key Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01\x01", 11 }
  },
  {
#ifdef DEBUG
    "pkcs-8ShroudedKeyBag",
    "PKCS #12 PKCS #8-shrouded Key Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01\x02", 11 }
  },
  {
#ifdef DEBUG
    "certBag",
    "PKCS #12 Certificate Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01\x03", 11 }
  },
  {
#ifdef DEBUG
    "crlBag",
    "PKCS #12 CRL Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01\x04", 11 }
  },
  {
#ifdef DEBUG
    "secretBag",
    "PKCS #12 Secret Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01\x05", 11 }
  },
  {
#ifdef DEBUG
    "safeContentsBag",
    "PKCS #12 Safe Contents Bag",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x01\x0c\x0a\x01\x06", 11 }
  },
  {
#ifdef DEBUG
    "digest",
    "RSA digest algorithm",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x02", 7 }
  },
  {
#ifdef DEBUG
    "md2",
    "MD2",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x02\x02", 8 }
  },
  {
#ifdef DEBUG
    "md4",
    "MD4",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x02\x04", 8 }
  },
  {
#ifdef DEBUG
    "md5",
    "MD5",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x02\x05", 8 }
  },
  {
#ifdef DEBUG
    "cipher",
    "RSA cipher algorithm",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x03", 7 }
  },
  {
#ifdef DEBUG
    "rc2cbc",
    "RC2-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x03\x02", 8 }
  },
  {
#ifdef DEBUG
    "rc4",
    "RC4",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x03\x04", 8 }
  },
  {
#ifdef DEBUG
    "desede3cbc",
    "DES-EDE3-CBC",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x03\x07", 8 }
  },
  {
#ifdef DEBUG
    "rc5cbcpad",
    "RC5-CBCPad",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x0d\x03\x09", 8 }
  },
  {
#ifdef DEBUG
    "microsoft",
    "Microsoft",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x14", 6 }
  },
  {
#ifdef DEBUG
    "columbia-university",
    "Columbia University",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x18", 6 }
  },
  {
#ifdef DEBUG
    "unisys",
    "Unisys",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x24", 6 }
  },
  {
#ifdef DEBUG
    "xapia",
    "XAPIA",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf7\x7a", 6 }
  },
  {
#ifdef DEBUG
    "wordperfect",
    "WordPerfect",
#endif /* DEBUG */
    { "\x2a\x86\x48\x86\xf8\x23", 6 }
  },
  {
#ifdef DEBUG
    "identified-organization",
    "ISO identified organizations",
#endif /* DEBUG */
    { "\x2b", 1 }
  },
  {
#ifdef DEBUG
    "us-dod",
    "United States Department of Defense",
#endif /* DEBUG */
    { "\x2b\x06", 2 }
  },
  {
#ifdef DEBUG
    "internet",
    "The Internet",
#endif /* DEBUG */
    { "\x2b\x06\x01", 3 }
  },
  {
#ifdef DEBUG
    "directory",
    "Internet: Directory",
#endif /* DEBUG */
    { "\x2b\x06\x01\x01", 4 }
  },
  {
#ifdef DEBUG
    "management",
    "Internet: Management",
#endif /* DEBUG */
    { "\x2b\x06\x01\x02", 4 }
  },
  {
#ifdef DEBUG
    "experimental",
    "Internet: Experimental",
#endif /* DEBUG */
    { "\x2b\x06\x01\x03", 4 }
  },
  {
#ifdef DEBUG
    "private",
    "Internet: Private",
#endif /* DEBUG */
    { "\x2b\x06\x01\x04", 4 }
  },
  {
#ifdef DEBUG
    "security",
    "Internet: Security",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05", 4 }
  },
  {
#ifdef DEBUG
    "",
    "",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05", 5 }
  },
  {
#ifdef DEBUG
    "id-pkix",
    "Public Key Infrastructure",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07", 6 }
  },
  {
#ifdef DEBUG
    "PKIX1Explicit88",
    "RFC 2459 Explicitly Tagged Module, 1988 Syntax",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x00\x01", 8 }
  },
  {
#ifdef DEBUG
    "PKIXImplicit88",
    "RFC 2459 Implicitly Tagged Module, 1988 Syntax",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x00\x02", 8 }
  },
  {
#ifdef DEBUG
    "PKIXExplicit93",
    "RFC 2459 Explicitly Tagged Module, 1993 Syntax",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x00\x03", 8 }
  },
  {
#ifdef DEBUG
    "id-pe",
    "PKIX Private Certificate Extensions",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x01", 7 }
  },
  {
#ifdef DEBUG
    "id-pe-authorityInfoAccess",
    "Certificate Authority Information Access",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x01\x01", 8 }
  },
  {
#ifdef DEBUG
    "id-qt",
    "PKIX Policy Qualifier Types",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x02", 7 }
  },
  {
#ifdef DEBUG
    "id-qt-cps",
    "PKIX CPS Pointer Qualifier",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x02\x01", 8 }
  },
  {
#ifdef DEBUG
    "id-qt-unotice",
    "PKIX User Notice Qualifier",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x02\x02", 8 }
  },
  {
#ifdef DEBUG
    "id-kp",
    "PKIX Key Purpose",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03", 7 }
  },
  {
#ifdef DEBUG
    "id-kp-serverAuth",
    "TLS Web Server Authentication Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x01", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-clientAuth",
    "TLS Web Client Authentication Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x02", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-codeSigning",
    "Code Signing Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x03", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-emailProtection",
    "E-Mail Protection Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x04", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-ipsecEndSystem",
    "IPSEC End System Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x05", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-ipsecTunnel",
    "IPSEC Tunnel Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x06", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-ipsecUser",
    "IPSEC User Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x07", 8 }
  },
  {
#ifdef DEBUG
    "id-kp-timeStamping",
    "Time Stamping Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x08", 8 }
  },
  {
#ifdef DEBUG
    "ocsp-responder",
    "OCSP Responder Certificate",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x03\x09", 8 }
  },
  {
#ifdef DEBUG
    "pkix-id-pkix",
    "",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07", 7 }
  },
  {
#ifdef DEBUG
    "pkix-id-pkip",
    "",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05", 8 }
  },
  {
#ifdef DEBUG
    "pkix-id-regctrl",
    "CRMF Registration Control",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01", 9 }
  },
  {
#ifdef DEBUG
    "regtoken",
    "CRMF Registration Control, Registration Token",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01\x01", 10 }
  },
  {
#ifdef DEBUG
    "authenticator",
    "CRMF Registration Control, Registration Authenticator",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01\x02", 10 }
  },
  {
#ifdef DEBUG
    "pkipubinfo",
    "CRMF Registration Control, PKI Publication Info",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01\x03", 10 }
  },
  {
#ifdef DEBUG
    "pki-arch-options",
    "CRMF Registration Control, PKI Archive Options",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01\x04", 10 }
  },
  {
#ifdef DEBUG
    "old-cert-id",
    "CRMF Registration Control, Old Certificate ID",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01\x05", 10 }
  },
  {
#ifdef DEBUG
    "protocol-encryption-key",
    "CRMF Registration Control, Protocol Encryption Key",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x01\x06", 10 }
  },
  {
#ifdef DEBUG
    "pkix-id-reginfo",
    "CRMF Registration Info",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x02", 9 }
  },
  {
#ifdef DEBUG
    "utf8-pairs",
    "CRMF Registration Info, UTF8 Pairs",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x02\x01", 10 }
  },
  {
#ifdef DEBUG
    "cert-request",
    "CRMF Registration Info, Certificate Request",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x07\x05\x02\x02", 10 }
  },
  {
#ifdef DEBUG
    "id-ad",
    "PKIX Access Descriptors",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30", 7 }
  },
  {
#ifdef DEBUG
    "id-ad-ocsp",
    "PKIX Online Certificate Status Protocol",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01", 8 }
  },
  {
#ifdef DEBUG
    "basic-response",
    "OCSP Basic Response",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x01", 9 }
  },
  {
#ifdef DEBUG
    "nonce-extension",
    "OCSP Nonce Extension",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x02", 9 }
  },
  {
#ifdef DEBUG
    "response",
    "OCSP Response Types Extension",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x03", 9 }
  },
  {
#ifdef DEBUG
    "crl",
    "OCSP CRL Reference Extension",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x04", 9 }
  },
  {
#ifdef DEBUG
    "no-check",
    "OCSP No Check Extension",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x05", 9 }
  },
  {
#ifdef DEBUG
    "archive-cutoff",
    "OCSP Archive Cutoff Extension",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x06", 9 }
  },
  {
#ifdef DEBUG
    "service-locator",
    "OCSP Service Locator Extension",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x01\x07", 9 }
  },
  {
#ifdef DEBUG
    "id-ad-caIssuers",
    "Certificate Authority Issuers",
#endif /* DEBUG */
    { "\x2b\x06\x01\x05\x05\x07\x30\x02", 8 }
  },
  {
#ifdef DEBUG
    "snmpv2",
    "Internet: SNMPv2",
#endif /* DEBUG */
    { "\x2b\x06\x01\x06", 4 }
  },
  {
#ifdef DEBUG
    "mail",
    "Internet: mail",
#endif /* DEBUG */
    { "\x2b\x06\x01\x07", 4 }
  },
  {
#ifdef DEBUG
    "mime-mhs",
    "Internet: mail MIME mhs",
#endif /* DEBUG */
    { "\x2b\x06\x01\x07\x01", 5 }
  },
  {
#ifdef DEBUG
    "ecma",
    "European Computers Manufacturing Association",
#endif /* DEBUG */
    { "\x2b\x0c", 2 }
  },
  {
#ifdef DEBUG
    "oiw",
    "Open Systems Implementors Workshop",
#endif /* DEBUG */
    { "\x2b\x0e", 2 }
  },
  {
#ifdef DEBUG
    "secsig",
    "Open Systems Implementors Workshop Security Special Interest Group",
#endif /* DEBUG */
    { "\x2b\x0e\x03", 3 }
  },
  {
#ifdef DEBUG
    "oIWSECSIGAlgorithmObjectIdentifiers",
    "OIW SECSIG Algorithm OIDs",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x01", 4 }
  },
  {
#ifdef DEBUG
    "algorithm",
    "OIW SECSIG Algorithm",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02", 4 }
  },
  {
#ifdef DEBUG
    "desecb",
    "DES-ECB",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x06", 5 }
  },
  {
#ifdef DEBUG
    "descbc",
    "DES-CBC",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x07", 5 }
  },
  {
#ifdef DEBUG
    "desofb",
    "DES-OFB",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x08", 5 }
  },
  {
#ifdef DEBUG
    "descfb",
    "DES-CFB",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x09", 5 }
  },
  {
#ifdef DEBUG
    "desmac",
    "DES-MAC",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x0a", 5 }
  },
  {
#ifdef DEBUG
    "isoSHAWithRSASignature",
    "ISO SHA with RSA Signature",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x0f", 5 }
  },
  {
#ifdef DEBUG
    "desede",
    "DES-EDE",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x11", 5 }
  },
  {
#ifdef DEBUG
    "sha1",
    "SHA-1",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x1a", 5 }
  },
  {
#ifdef DEBUG
    "bogusDSASignatureWithSHA1Digest",
    "Forgezza DSA Signature with SHA-1 Digest",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x02\x1b", 5 }
  },
  {
#ifdef DEBUG
    "authentication-mechanism",
    "OIW SECSIG Authentication Mechanisms",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x03", 4 }
  },
  {
#ifdef DEBUG
    "security-attribute",
    "OIW SECSIG Security Attributes",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x04", 4 }
  },
  {
#ifdef DEBUG
    "document-definition",
    "OIW SECSIG Document Definitions used in security",
#endif /* DEBUG */
    { "\x2b\x0e\x03\x05", 4 }
  },
  {
#ifdef DEBUG
    "directory-services-sig",
    "OIW directory services sig",
#endif /* DEBUG */
    { "\x2b\x0e\x07", 3 }
  },
  {
#ifdef DEBUG
    "ewos",
    "European Workshop on Open Systems",
#endif /* DEBUG */
    { "\x2b\x10", 2 }
  },
  {
#ifdef DEBUG
    "osf",
    "Open Software Foundation",
#endif /* DEBUG */
    { "\x2b\x16", 2 }
  },
  {
#ifdef DEBUG
    "nordunet",
    "Nordunet",
#endif /* DEBUG */
    { "\x2b\x17", 2 }
  },
  {
#ifdef DEBUG
    "nato-id-org",
    "NATO identified organisation",
#endif /* DEBUG */
    { "\x2b\x1a", 2 }
  },
  {
#ifdef DEBUG
    "teletrust",
    "Teletrust",
#endif /* DEBUG */
    { "\x2b\x24", 2 }
  },
  {
#ifdef DEBUG
    "smpte",
    "Society of Motion Picture and Television Engineers",
#endif /* DEBUG */
    { "\x2b\x34", 2 }
  },
  {
#ifdef DEBUG
    "sita",
    "Societe Internationale de Telecommunications Aeronautiques",
#endif /* DEBUG */
    { "\x2b\x45", 2 }
  },
  {
#ifdef DEBUG
    "iana",
    "Internet Assigned Numbers Authority",
#endif /* DEBUG */
    { "\x2b\x5a", 2 }
  },
  {
#ifdef DEBUG
    "thawte",
    "Thawte",
#endif /* DEBUG */
    { "\x2b\x65", 2 }
  },
  {
#ifdef DEBUG
    "joint-iso-ccitt",
    "Joint ISO/ITU-T assignment",
#endif /* DEBUG */
    { "\x80\x02", 2 }
  },
  {
#ifdef DEBUG
    "presentation",
    "Joint ISO/ITU-T Presentation",
#endif /* DEBUG */
    { "\x50", 1 }
  },
  {
#ifdef DEBUG
    "asn-1",
    "Abstract Syntax Notation One",
#endif /* DEBUG */
    { "\x51", 1 }
  },
  {
#ifdef DEBUG
    "acse",
    "Association Control",
#endif /* DEBUG */
    { "\x52", 1 }
  },
  {
#ifdef DEBUG
    "rtse",
    "Reliable Transfer",
#endif /* DEBUG */
    { "\x53", 1 }
  },
  {
#ifdef DEBUG
    "rose",
    "Remote Operations",
#endif /* DEBUG */
    { "\x54", 1 }
  },
  {
#ifdef DEBUG
    "x500",
    "Directory",
#endif /* DEBUG */
    { "\x55", 1 }
  },
  {
#ifdef DEBUG
    "modules",
    "X.500 modules",
#endif /* DEBUG */
    { "\x55\x01", 2 }
  },
  {
#ifdef DEBUG
    "service-environment",
    "X.500 service environment",
#endif /* DEBUG */
    { "\x55\x02", 2 }
  },
  {
#ifdef DEBUG
    "application-context",
    "X.500 application context",
#endif /* DEBUG */
    { "\x55\x03", 2 }
  },
  {
#ifdef DEBUG
    "id-at",
    "X.520 attribute types",
#endif /* DEBUG */
    { "\x55\x04", 2 }
  },
  {
#ifdef DEBUG
    "id-at-commonName",
    "X.520 Common Name",
#endif /* DEBUG */
    { "\x55\x04\x03", 3 }
  },
  {
#ifdef DEBUG
    "id-at-surname",
    "X.520 Surname",
#endif /* DEBUG */
    { "\x55\x04\x04", 3 }
  },
  {
#ifdef DEBUG
    "id-at-countryName",
    "X.520 Country Name",
#endif /* DEBUG */
    { "\x55\x04\x06", 3 }
  },
  {
#ifdef DEBUG
    "id-at-localityName",
    "X.520 Locality Name",
#endif /* DEBUG */
    { "\x55\x04\x07", 3 }
  },
  {
#ifdef DEBUG
    "id-at-stateOrProvinceName",
    "X.520 State or Province Name",
#endif /* DEBUG */
    { "\x55\x04\x08", 3 }
  },
  {
#ifdef DEBUG
    "id-at-organizationName",
    "X.520 Organization Name",
#endif /* DEBUG */
    { "\x55\x04\x0a", 3 }
  },
  {
#ifdef DEBUG
    "id-at-organizationalUnitName",
    "X.520 Organizational Unit Name",
#endif /* DEBUG */
    { "\x55\x04\x0b", 3 }
  },
  {
#ifdef DEBUG
    "id-at-title",
    "X.520 Title",
#endif /* DEBUG */
    { "\x55\x04\x0c", 3 }
  },
  {
#ifdef DEBUG
    "id-at-name",
    "X.520 Name",
#endif /* DEBUG */
    { "\x55\x04\x29", 3 }
  },
  {
#ifdef DEBUG
    "id-at-givenName",
    "X.520 Given Name",
#endif /* DEBUG */
    { "\x55\x04\x2a", 3 }
  },
  {
#ifdef DEBUG
    "id-at-initials",
    "X.520 Initials",
#endif /* DEBUG */
    { "\x55\x04\x2b", 3 }
  },
  {
#ifdef DEBUG
    "id-at-generationQualifier",
    "X.520 Generation Qualifier",
#endif /* DEBUG */
    { "\x55\x04\x2c", 3 }
  },
  {
#ifdef DEBUG
    "id-at-dnQualifier",
    "X.520 DN Qualifier",
#endif /* DEBUG */
    { "\x55\x04\x2e", 3 }
  },
  {
#ifdef DEBUG
    "attribute-syntax",
    "X.500 attribute syntaxes",
#endif /* DEBUG */
    { "\x55\x05", 2 }
  },
  {
#ifdef DEBUG
    "object-classes",
    "X.500 standard object classes",
#endif /* DEBUG */
    { "\x55\x06", 2 }
  },
  {
#ifdef DEBUG
    "attribute-set",
    "X.500 attribute sets",
#endif /* DEBUG */
    { "\x55\x07", 2 }
  },
  {
#ifdef DEBUG
    "algorithms",
    "X.500-defined algorithms",
#endif /* DEBUG */
    { "\x55\x08", 2 }
  },
  {
#ifdef DEBUG
    "encryption",
    "X.500-defined encryption algorithms",
#endif /* DEBUG */
    { "\x55\x08\x01", 3 }
  },
  {
#ifdef DEBUG
    "rsa",
    "RSA Encryption Algorithm",
#endif /* DEBUG */
    { "\x55\x08\x01\x01", 4 }
  },
  {
#ifdef DEBUG
    "abstract-syntax",
    "X.500 abstract syntaxes",
#endif /* DEBUG */
    { "\x55\x09", 2 }
  },
  {
#ifdef DEBUG
    "operational-attribute",
    "DSA Operational Attributes",
#endif /* DEBUG */
    { "\x55\x0c", 2 }
  },
  {
#ifdef DEBUG
    "matching-rule",
    "Matching Rule",
#endif /* DEBUG */
    { "\x55\x0d", 2 }
  },
  {
#ifdef DEBUG
    "knowledge-matching-rule",
    "X.500 knowledge Matching Rules",
#endif /* DEBUG */
    { "\x55\x0e", 2 }
  },
  {
#ifdef DEBUG
    "name-form",
    "X.500 name forms",
#endif /* DEBUG */
    { "\x55\x0f", 2 }
  },
  {
#ifdef DEBUG
    "group",
    "X.500 groups",
#endif /* DEBUG */
    { "\x55\x10", 2 }
  },
  {
#ifdef DEBUG
    "subentry",
    "X.500 subentry",
#endif /* DEBUG */
    { "\x55\x11", 2 }
  },
  {
#ifdef DEBUG
    "operational-attribute-type",
    "X.500 operational attribute type",
#endif /* DEBUG */
    { "\x55\x12", 2 }
  },
  {
#ifdef DEBUG
    "operational-binding",
    "X.500 operational binding",
#endif /* DEBUG */
    { "\x55\x13", 2 }
  },
  {
#ifdef DEBUG
    "schema-object-class",
    "X.500 schema Object class",
#endif /* DEBUG */
    { "\x55\x14", 2 }
  },
  {
#ifdef DEBUG
    "schema-operational-attribute",
    "X.500 schema operational attributes",
#endif /* DEBUG */
    { "\x55\x15", 2 }
  },
  {
#ifdef DEBUG
    "administrative-role",
    "X.500 administrative roles",
#endif /* DEBUG */
    { "\x55\x17", 2 }
  },
  {
#ifdef DEBUG
    "access-control-attribute",
    "X.500 access control attribute",
#endif /* DEBUG */
    { "\x55\x18", 2 }
  },
  {
#ifdef DEBUG
    "ros",
    "X.500 ros object",
#endif /* DEBUG */
    { "\x55\x19", 2 }
  },
  {
#ifdef DEBUG
    "contract",
    "X.500 contract",
#endif /* DEBUG */
    { "\x55\x1a", 2 }
  },
  {
#ifdef DEBUG
    "package",
    "X.500 package",
#endif /* DEBUG */
    { "\x55\x1b", 2 }
  },
  {
#ifdef DEBUG
    "access-control-schema",
    "X.500 access control schema",
#endif /* DEBUG */
    { "\x55\x1c", 2 }
  },
  {
#ifdef DEBUG
    "id-ce",
    "X.500 Certificate Extension",
#endif /* DEBUG */
    { "\x55\x1d", 2 }
  },
  {
#ifdef DEBUG
    "subject-directory-attributes",
    "Certificate Subject Directory Attributes",
#endif /* DEBUG */
    { "\x55\x1d\x05", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-subjectDirectoryAttributes",
    "Certificate Subject Directory Attributes",
#endif /* DEBUG */
    { "\x55\x1d\x09", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-subjectKeyIdentifier",
    "Certificate Subject Key ID",
#endif /* DEBUG */
    { "\x55\x1d\x0e", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-keyUsage",
    "Certificate Key Usage",
#endif /* DEBUG */
    { "\x55\x1d\x0f", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-privateKeyUsagePeriod",
    "Certificate Private Key Usage Period",
#endif /* DEBUG */
    { "\x55\x1d\x10", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-subjectAltName",
    "Certificate Subject Alternate Name",
#endif /* DEBUG */
    { "\x55\x1d\x11", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-issuerAltName",
    "Certificate Issuer Alternate Name",
#endif /* DEBUG */
    { "\x55\x1d\x12", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-basicConstraints",
    "Certificate Basic Constraints",
#endif /* DEBUG */
    { "\x55\x1d\x13", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-cRLNumber",
    "CRL Number",
#endif /* DEBUG */
    { "\x55\x1d\x14", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-cRLReasons",
    "CRL Reason Code",
#endif /* DEBUG */
    { "\x55\x1d\x15", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-holdInstructionCode",
    "Hold Instruction Code",
#endif /* DEBUG */
    { "\x55\x1d\x17", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-invalidityDate",
    "Invalidity Date",
#endif /* DEBUG */
    { "\x55\x1d\x18", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-deltaCRLIndicator",
    "Delta CRL Indicator",
#endif /* DEBUG */
    { "\x55\x1d\x1b", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-issuingDistributionPoint",
    "Issuing Distribution Point",
#endif /* DEBUG */
    { "\x55\x1d\x1c", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-certificateIssuer",
    "Certificate Issuer",
#endif /* DEBUG */
    { "\x55\x1d\x1d", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-nameConstraints",
    "Certificate Name Constraints",
#endif /* DEBUG */
    { "\x55\x1d\x1e", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-cRLDistributionPoints",
    "CRL Distribution Points",
#endif /* DEBUG */
    { "\x55\x1d\x1f", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-certificatePolicies",
    "Certificate Policies",
#endif /* DEBUG */
    { "\x55\x1d\x20", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-policyMappings",
    "Certificate Policy Mappings",
#endif /* DEBUG */
    { "\x55\x1d\x21", 3 }
  },
  {
#ifdef DEBUG
    "policy-constraints",
    "Certificate Policy Constraints (old)",
#endif /* DEBUG */
    { "\x55\x1d\x22", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-authorityKeyIdentifier",
    "Certificate Authority Key Identifier",
#endif /* DEBUG */
    { "\x55\x1d\x23", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-policyConstraints",
    "Certificate Policy Constraints",
#endif /* DEBUG */
    { "\x55\x1d\x24", 3 }
  },
  {
#ifdef DEBUG
    "id-ce-extKeyUsage",
    "Extended Key Usage",
#endif /* DEBUG */
    { "\x55\x1d\x25", 3 }
  },
  {
#ifdef DEBUG
    "id-mgt",
    "X.500 Management Object",
#endif /* DEBUG */
    { "\x55\x1e", 2 }
  },
  {
#ifdef DEBUG
    "x400",
    "X.400 MHS",
#endif /* DEBUG */
    { "\x56", 1 }
  },
  {
#ifdef DEBUG
    "ccr",
    "Committment, Concurrency and Recovery",
#endif /* DEBUG */
    { "\x57", 1 }
  },
  {
#ifdef DEBUG
    "oda",
    "Office Document Architecture",
#endif /* DEBUG */
    { "\x58", 1 }
  },
  {
#ifdef DEBUG
    "osi-management",
    "OSI management",
#endif /* DEBUG */
    { "\x59", 1 }
  },
  {
#ifdef DEBUG
    "tp",
    "Transaction Processing",
#endif /* DEBUG */
    { "\x5a", 1 }
  },
  {
#ifdef DEBUG
    "dor",
    "Distinguished Object Reference",
#endif /* DEBUG */
    { "\x5b", 1 }
  },
  {
#ifdef DEBUG
    "rdt",
    "Referenced Data Transfer",
#endif /* DEBUG */
    { "\x5c", 1 }
  },
  {
#ifdef DEBUG
    "nlm",
    "Network Layer Management",
#endif /* DEBUG */
    { "\x5d", 1 }
  },
  {
#ifdef DEBUG
    "tlm",
    "Transport Layer Management",
#endif /* DEBUG */
    { "\x5e", 1 }
  },
  {
#ifdef DEBUG
    "llm",
    "Link Layer Management",
#endif /* DEBUG */
    { "\x5f", 1 }
  },
  {
#ifdef DEBUG
    "country",
    "Country Assignments",
#endif /* DEBUG */
    { "\x60", 1 }
  },
  {
#ifdef DEBUG
    "canada",
    "Canada",
#endif /* DEBUG */
    { "\x60\x7c", 2 }
  },
  {
#ifdef DEBUG
    "taiwan",
    "Taiwan",
#endif /* DEBUG */
    { "\x60\x81\x1e", 3 }
  },
  {
#ifdef DEBUG
    "norway",
    "Norway",
#endif /* DEBUG */
    { "\x60\x84\x42", 3 }
  },
  {
#ifdef DEBUG
    "switzerland",
    "Switzerland",
#endif /* DEBUG */
    { "\x60\x85\x74", 3 }
  },
  {
#ifdef DEBUG
    "us",
    "United States",
#endif /* DEBUG */
    { "\x60\x86\x48", 3 }
  },
  {
#ifdef DEBUG
    "us-company",
    "United States Company",
#endif /* DEBUG */
    { "\x60\x86\x48\x01", 4 }
  },
  {
#ifdef DEBUG
    "us-government",
    "United States Government (1.101)",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65", 5 }
  },
  {
#ifdef DEBUG
    "us-dod",
    "United States Department of Defense",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02", 6 }
  },
  {
#ifdef DEBUG
    "id-infosec",
    "US DOD Infosec",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01", 7 }
  },
  {
#ifdef DEBUG
    "id-modules",
    "US DOD Infosec modules",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x00", 8 }
  },
  {
#ifdef DEBUG
    "id-algorithms",
    "US DOD Infosec algorithms (MISSI)",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01", 8 }
  },
  {
#ifdef DEBUG
    "old-dss",
    "MISSI DSS Algorithm (Old)",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x02", 9 }
  },
  {
#ifdef DEBUG
    "skipjack-cbc-64",
    "Skipjack CBC64",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x04", 9 }
  },
  {
#ifdef DEBUG
    "kea",
    "MISSI KEA Algorithm",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x0a", 9 }
  },
  {
#ifdef DEBUG
    "old-kea-dss",
    "MISSI KEA and DSS Algorithm (Old)",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x0c", 9 }
  },
  {
#ifdef DEBUG
    "dss",
    "MISSI DSS Algorithm",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x13", 9 }
  },
  {
#ifdef DEBUG
    "kea-dss",
    "MISSI KEA and DSS Algorithm",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x14", 9 }
  },
  {
#ifdef DEBUG
    "alt-kea",
    "MISSI Alternate KEA Algorithm",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x01\x16", 9 }
  },
  {
#ifdef DEBUG
    "id-formats",
    "US DOD Infosec formats",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x02", 8 }
  },
  {
#ifdef DEBUG
    "id-policy",
    "US DOD Infosec policy",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x03", 8 }
  },
  {
#ifdef DEBUG
    "id-object-classes",
    "US DOD Infosec object classes",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x04", 8 }
  },
  {
#ifdef DEBUG
    "id-attributes",
    "US DOD Infosec attributes",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x05", 8 }
  },
  {
#ifdef DEBUG
    "id-attribute-syntax",
    "US DOD Infosec attribute syntax",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x65\x02\x01\x06", 8 }
  },
  {
#ifdef DEBUG
    "netscape",
    "Netscape Communications Corp.",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42", 7 }
  },
  {
#ifdef DEBUG
    "cert-ext",
    "Netscape Cert Extensions",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01", 8 }
  },
  {
#ifdef DEBUG
    "cert-type",
    "Certificate Type",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x01", 9 }
  },
  {
#ifdef DEBUG
    "base-url",
    "Certificate Extension Base URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x02", 9 }
  },
  {
#ifdef DEBUG
    "revocation-url",
    "Certificate Revocation URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x03", 9 }
  },
  {
#ifdef DEBUG
    "ca-revocation-url",
    "Certificate Authority Revocation URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x04", 9 }
  },
  {
#ifdef DEBUG
    "ca-crl-download-url",
    "Certificate Authority CRL Download URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x05", 9 }
  },
  {
#ifdef DEBUG
    "ca-cert-url",
    "Certificate Authority Certificate Download URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x06", 9 }
  },
  {
#ifdef DEBUG
    "renewal-url",
    "Certificate Renewal URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x07", 9 }
  },
  {
#ifdef DEBUG
    "ca-policy-url",
    "Certificate Authority Policy URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x08", 9 }
  },
  {
#ifdef DEBUG
    "homepage-url",
    "Certificate Homepage URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x09", 9 }
  },
  {
#ifdef DEBUG
    "entity-logo",
    "Certificate Entity Logo",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x0a", 9 }
  },
  {
#ifdef DEBUG
    "user-picture",
    "Certificate User Picture",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x0b", 9 }
  },
  {
#ifdef DEBUG
    "ssl-server-name",
    "Certificate SSL Server Name",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x0c", 9 }
  },
  {
#ifdef DEBUG
    "comment",
    "Certificate Comment",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x0d", 9 }
  },
  {
#ifdef DEBUG
    "thayes",
    "",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x01\x0e", 9 }
  },
  {
#ifdef DEBUG
    "data-type",
    "Netscape Data Types",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x02", 8 }
  },
  {
#ifdef DEBUG
    "gif",
    "image/gif",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x02\x01", 9 }
  },
  {
#ifdef DEBUG
    "jpeg",
    "image/jpeg",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x02\x02", 9 }
  },
  {
#ifdef DEBUG
    "url",
    "URL",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x02\x03", 9 }
  },
  {
#ifdef DEBUG
    "html",
    "text/html",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x02\x04", 9 }
  },
  {
#ifdef DEBUG
    "cert-sequence",
    "Certificate Sequence",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x02\x05", 9 }
  },
  {
#ifdef DEBUG
    "directory",
    "Netscape Directory",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x03", 8 }
  },
  {
#ifdef DEBUG
    "policy",
    "Netscape Policy Type OIDs",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x04", 8 }
  },
  {
#ifdef DEBUG
    "export-approved",
    "Strong Crypto Export Approved",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x04\x01", 9 }
  },
  {
#ifdef DEBUG
    "cert-server",
    "Netscape Certificate Server",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x05", 8 }
  },
  {
#ifdef DEBUG
    "",
    "",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x05\x01", 9 }
  },
  {
#ifdef DEBUG
    "recovery-request",
    "Netscape Cert Server Recovery Request",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x05\x01\x01", 10 }
  },
  {
#ifdef DEBUG
    "algs",
    "Netscape algorithm OIDs",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x06", 8 }
  },
  {
#ifdef DEBUG
    "smime-kea",
    "Netscape S/MIME KEA",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x06\x01", 9 }
  },
  {
#ifdef DEBUG
    "name-components",
    "Netscape Name Components",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x07", 8 }
  },
  {
#ifdef DEBUG
    "nickname",
    "Netscape Nickname",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x42\x07\x01", 9 }
  },
  {
#ifdef DEBUG
    "verisign",
    "Verisign",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x45", 7 }
  },
  {
#ifdef DEBUG
    "",
    "",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x45\x01", 8 }
  },
  {
#ifdef DEBUG
    "",
    "",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x45\x01\x07", 9 }
  },
  {
#ifdef DEBUG
    "",
    "",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x45\x01\x07\x01", 10 }
  },
  {
#ifdef DEBUG
    "verisign-user-notices",
    "Verisign User Notices",
#endif /* DEBUG */
    { "\x60\x86\x48\x01\x86\xf8\x45\x01\x07\x01\x01", 11 }
  },
  {
#ifdef DEBUG
    "us-government",
    "US Government (101)",
#endif /* DEBUG */
    { "\x60\x86\x48\x65", 4 }
  },
  {
#ifdef DEBUG
    "us-government2",
    "US Government (102)",
#endif /* DEBUG */
    { "\x60\x86\x48\x66", 4 }
  },
  {
#ifdef DEBUG
    "old-netscape",
    "Netscape Communications Corp. (Old)",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a", 5 }
  },
  {
#ifdef DEBUG
    "ns-cert-ext",
    "Netscape Cert Extensions (Old NS)",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a\x01", 6 }
  },
  {
#ifdef DEBUG
    "netscape-ok",
    "Netscape says this cert is ok (Old NS)",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a\x01\x01", 7 }
  },
  {
#ifdef DEBUG
    "issuer-logo",
    "Certificate Issuer Logo (Old NS)",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a\x01\x02", 7 }
  },
  {
#ifdef DEBUG
    "subject-logo",
    "Certificate Subject Logo (Old NS)",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a\x01\x03", 7 }
  },
  {
#ifdef DEBUG
    "ns-file-type",
    "Netscape File Type",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a\x02", 6 }
  },
  {
#ifdef DEBUG
    "ns-image-type",
    "Netscape Image Type",
#endif /* DEBUG */
    { "\x60\x86\x48\xd8\x6a\x03", 6 }
  },
  {
#ifdef DEBUG
    "registration-procedures",
    "Registration procedures",
#endif /* DEBUG */
    { "\x61", 1 }
  },
  {
#ifdef DEBUG
    "physical-layer-management",
    "Physical layer Management",
#endif /* DEBUG */
    { "\x62", 1 }
  },
  {
#ifdef DEBUG
    "mheg",
    "MHEG",
#endif /* DEBUG */
    { "\x63", 1 }
  },
  {
#ifdef DEBUG
    "guls",
    "Generic Upper Layer Security",
#endif /* DEBUG */
    { "\x64", 1 }
  },
  {
#ifdef DEBUG
    "tls",
    "Transport Layer Security Protocol",
#endif /* DEBUG */
    { "\x65", 1 }
  },
  {
#ifdef DEBUG
    "nls",
    "Network Layer Security Protocol",
#endif /* DEBUG */
    { "\x66", 1 }
  },
  {
#ifdef DEBUG
    "organization",
    "International organizations",
#endif /* DEBUG */
    { "\x67", 1 }
  }
};

const PRUint32 nss_builtin_oid_count = 379;

const NSSOID *NSS_OID_RFC1274_UID = (NSSOID *)&nss_builtin_oids[11];
const NSSOID *NSS_OID_RFC1274_EMAIL = (NSSOID *)&nss_builtin_oids[12];
const NSSOID *NSS_OID_RFC2247_DC = (NSSOID *)&nss_builtin_oids[13];
const NSSOID *NSS_OID_ANSIX9_DSA_SIGNATURE = (NSSOID *)&nss_builtin_oids[43];
const NSSOID *NSS_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST = (NSSOID *)&nss_builtin_oids[44];
const NSSOID *NSS_OID_X942_DIFFIE_HELMAN_KEY = (NSSOID *)&nss_builtin_oids[47];
const NSSOID *NSS_OID_PKCS1_RSA_ENCRYPTION = (NSSOID *)&nss_builtin_oids[52];
const NSSOID *NSS_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION = (NSSOID *)&nss_builtin_oids[53];
const NSSOID *NSS_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION = (NSSOID *)&nss_builtin_oids[54];
const NSSOID *NSS_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION = (NSSOID *)&nss_builtin_oids[55];
const NSSOID *NSS_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION = (NSSOID *)&nss_builtin_oids[56];
const NSSOID *NSS_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC = (NSSOID *)&nss_builtin_oids[58];
const NSSOID *NSS_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC = (NSSOID *)&nss_builtin_oids[59];
const NSSOID *NSS_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC = (NSSOID *)&nss_builtin_oids[60];
const NSSOID *NSS_OID_PKCS7 = (NSSOID *)&nss_builtin_oids[61];
const NSSOID *NSS_OID_PKCS7_DATA = (NSSOID *)&nss_builtin_oids[62];
const NSSOID *NSS_OID_PKCS7_SIGNED_DATA = (NSSOID *)&nss_builtin_oids[63];
const NSSOID *NSS_OID_PKCS7_ENVELOPED_DATA = (NSSOID *)&nss_builtin_oids[64];
const NSSOID *NSS_OID_PKCS7_SIGNED_ENVELOPED_DATA = (NSSOID *)&nss_builtin_oids[65];
const NSSOID *NSS_OID_PKCS7_DIGESTED_DATA = (NSSOID *)&nss_builtin_oids[66];
const NSSOID *NSS_OID_PKCS7_ENCRYPTED_DATA = (NSSOID *)&nss_builtin_oids[67];
const NSSOID *NSS_OID_PKCS9_EMAIL_ADDRESS = (NSSOID *)&nss_builtin_oids[69];
const NSSOID *NSS_OID_PKCS9_UNSTRUCTURED_NAME = (NSSOID *)&nss_builtin_oids[70];
const NSSOID *NSS_OID_PKCS9_CONTENT_TYPE = (NSSOID *)&nss_builtin_oids[71];
const NSSOID *NSS_OID_PKCS9_MESSAGE_DIGEST = (NSSOID *)&nss_builtin_oids[72];
const NSSOID *NSS_OID_PKCS9_SIGNING_TIME = (NSSOID *)&nss_builtin_oids[73];
const NSSOID *NSS_OID_PKCS9_COUNTER_SIGNATURE = (NSSOID *)&nss_builtin_oids[74];
const NSSOID *NSS_OID_PKCS9_CHALLENGE_PASSWORD = (NSSOID *)&nss_builtin_oids[75];
const NSSOID *NSS_OID_PKCS9_UNSTRUCTURED_ADDRESS = (NSSOID *)&nss_builtin_oids[76];
const NSSOID *NSS_OID_PKCS9_EXTENDED_CERTIFICATE_ATTRIBUTES = (NSSOID *)&nss_builtin_oids[77];
const NSSOID *NSS_OID_PKCS9_SMIME_CAPABILITIES = (NSSOID *)&nss_builtin_oids[78];
const NSSOID *NSS_OID_PKCS9_FRIENDLY_NAME = (NSSOID *)&nss_builtin_oids[79];
const NSSOID *NSS_OID_PKCS9_LOCAL_KEY_ID = (NSSOID *)&nss_builtin_oids[80];
const NSSOID *NSS_OID_PKCS9_X509_CERT = (NSSOID *)&nss_builtin_oids[82];
const NSSOID *NSS_OID_PKCS9_SDSI_CERT = (NSSOID *)&nss_builtin_oids[83];
const NSSOID *NSS_OID_PKCS9_X509_CRL = (NSSOID *)&nss_builtin_oids[85];
const NSSOID *NSS_OID_PKCS12 = (NSSOID *)&nss_builtin_oids[86];
const NSSOID *NSS_OID_PKCS12_PBE_IDS = (NSSOID *)&nss_builtin_oids[87];
const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4 = (NSSOID *)&nss_builtin_oids[88];
const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4 = (NSSOID *)&nss_builtin_oids[89];
const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_3_KEY_TRIPLE_DES_CBC = (NSSOID *)&nss_builtin_oids[90];
const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_2_KEY_TRIPLE_DES_CBC = (NSSOID *)&nss_builtin_oids[91];
const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC = (NSSOID *)&nss_builtin_oids[92];
const NSSOID *NSS_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC = (NSSOID *)&nss_builtin_oids[93];
const NSSOID *NSS_OID_PKCS12_KEY_BAG = (NSSOID *)&nss_builtin_oids[120];
const NSSOID *NSS_OID_PKCS12_PKCS8_SHROUDED_KEY_BAG = (NSSOID *)&nss_builtin_oids[121];
const NSSOID *NSS_OID_PKCS12_CERT_BAG = (NSSOID *)&nss_builtin_oids[122];
const NSSOID *NSS_OID_PKCS12_CRL_BAG = (NSSOID *)&nss_builtin_oids[123];
const NSSOID *NSS_OID_PKCS12_SECRET_BAG = (NSSOID *)&nss_builtin_oids[124];
const NSSOID *NSS_OID_PKCS12_SAFE_CONTENTS_BAG = (NSSOID *)&nss_builtin_oids[125];
const NSSOID *NSS_OID_MD2 = (NSSOID *)&nss_builtin_oids[127];
const NSSOID *NSS_OID_MD4 = (NSSOID *)&nss_builtin_oids[128];
const NSSOID *NSS_OID_MD5 = (NSSOID *)&nss_builtin_oids[129];
const NSSOID *NSS_OID_RC2_CBC = (NSSOID *)&nss_builtin_oids[131];
const NSSOID *NSS_OID_RC4 = (NSSOID *)&nss_builtin_oids[132];
const NSSOID *NSS_OID_DES_EDE3_CBC = (NSSOID *)&nss_builtin_oids[133];
const NSSOID *NSS_OID_RC5_CBC_PAD = (NSSOID *)&nss_builtin_oids[134];
const NSSOID *NSS_OID_X509_AUTH_INFO_ACCESS = (NSSOID *)&nss_builtin_oids[154];
const NSSOID *NSS_OID_PKIX_CPS_POINTER_QUALIFIER = (NSSOID *)&nss_builtin_oids[156];
const NSSOID *NSS_OID_PKIX_USER_NOTICE_QUALIFIER = (NSSOID *)&nss_builtin_oids[157];
const NSSOID *NSS_OID_EXT_KEY_USAGE_SERVER_AUTH = (NSSOID *)&nss_builtin_oids[159];
const NSSOID *NSS_OID_EXT_KEY_USAGE_CLIENT_AUTH = (NSSOID *)&nss_builtin_oids[160];
const NSSOID *NSS_OID_EXT_KEY_USAGE_CODE_SIGN = (NSSOID *)&nss_builtin_oids[161];
const NSSOID *NSS_OID_EXT_KEY_USAGE_EMAIL_PROTECTION = (NSSOID *)&nss_builtin_oids[162];
const NSSOID *NSS_OID_EXT_KEY_USAGE_IPSEC_END_SYSTEM = (NSSOID *)&nss_builtin_oids[163];
const NSSOID *NSS_OID_EXT_KEY_USAGE_IPSEC_TUNNEL = (NSSOID *)&nss_builtin_oids[164];
const NSSOID *NSS_OID_EXT_KEY_USAGE_IPSEC_USER = (NSSOID *)&nss_builtin_oids[165];
const NSSOID *NSS_OID_EXT_KEY_USAGE_TIME_STAMP = (NSSOID *)&nss_builtin_oids[166];
const NSSOID *NSS_OID_OCSP_RESPONDER = (NSSOID *)&nss_builtin_oids[167];
const NSSOID *NSS_OID_PKIX_REGCTRL_REGTOKEN = (NSSOID *)&nss_builtin_oids[171];
const NSSOID *NSS_OID_PKIX_REGCTRL_AUTHENTICATOR = (NSSOID *)&nss_builtin_oids[172];
const NSSOID *NSS_OID_PKIX_REGCTRL_PKIPUBINFO = (NSSOID *)&nss_builtin_oids[173];
const NSSOID *NSS_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS = (NSSOID *)&nss_builtin_oids[174];
const NSSOID *NSS_OID_PKIX_REGCTRL_OLD_CERT_ID = (NSSOID *)&nss_builtin_oids[175];
const NSSOID *NSS_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY = (NSSOID *)&nss_builtin_oids[176];
const NSSOID *NSS_OID_PKIX_REGINFO_UTF8_PAIRS = (NSSOID *)&nss_builtin_oids[178];
const NSSOID *NSS_OID_PKIX_REGINFO_CERT_REQUEST = (NSSOID *)&nss_builtin_oids[179];
const NSSOID *NSS_OID_OID_PKIX_OCSP = (NSSOID *)&nss_builtin_oids[181];
const NSSOID *NSS_OID_PKIX_OCSP_BASIC_RESPONSE = (NSSOID *)&nss_builtin_oids[182];
const NSSOID *NSS_OID_PKIX_OCSP_NONCE = (NSSOID *)&nss_builtin_oids[183];
const NSSOID *NSS_OID_PKIX_OCSP_RESPONSE = (NSSOID *)&nss_builtin_oids[184];
const NSSOID *NSS_OID_PKIX_OCSP_CRL = (NSSOID *)&nss_builtin_oids[185];
const NSSOID *NSS_OID_X509_OCSP_NO_CHECK = (NSSOID *)&nss_builtin_oids[186];
const NSSOID *NSS_OID_PKIX_OCSP_ARCHIVE_CUTOFF = (NSSOID *)&nss_builtin_oids[187];
const NSSOID *NSS_OID_PKIX_OCSP_SERVICE_LOCATOR = (NSSOID *)&nss_builtin_oids[188];
const NSSOID *NSS_OID_DES_ECB = (NSSOID *)&nss_builtin_oids[198];
const NSSOID *NSS_OID_DES_CBC = (NSSOID *)&nss_builtin_oids[199];
const NSSOID *NSS_OID_DES_OFB = (NSSOID *)&nss_builtin_oids[200];
const NSSOID *NSS_OID_DES_CFB = (NSSOID *)&nss_builtin_oids[201];
const NSSOID *NSS_OID_DES_MAC = (NSSOID *)&nss_builtin_oids[202];
const NSSOID *NSS_OID_ISO_SHA_WITH_RSA_SIGNATURE = (NSSOID *)&nss_builtin_oids[203];
const NSSOID *NSS_OID_DES_EDE = (NSSOID *)&nss_builtin_oids[204];
const NSSOID *NSS_OID_SHA1 = (NSSOID *)&nss_builtin_oids[205];
const NSSOID *NSS_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST = (NSSOID *)&nss_builtin_oids[206];
const NSSOID *NSS_OID_X520_COMMON_NAME = (NSSOID *)&nss_builtin_oids[231];
const NSSOID *NSS_OID_X520_SURNAME = (NSSOID *)&nss_builtin_oids[232];
const NSSOID *NSS_OID_X520_COUNTRY_NAME = (NSSOID *)&nss_builtin_oids[233];
const NSSOID *NSS_OID_X520_LOCALITY_NAME = (NSSOID *)&nss_builtin_oids[234];
const NSSOID *NSS_OID_X520_STATE_OR_PROVINCE_NAME = (NSSOID *)&nss_builtin_oids[235];
const NSSOID *NSS_OID_X520_ORGANIZATION_NAME = (NSSOID *)&nss_builtin_oids[236];
const NSSOID *NSS_OID_X520_ORGANIZATIONAL_UNIT_NAME = (NSSOID *)&nss_builtin_oids[237];
const NSSOID *NSS_OID_X520_TITLE = (NSSOID *)&nss_builtin_oids[238];
const NSSOID *NSS_OID_X520_NAME = (NSSOID *)&nss_builtin_oids[239];
const NSSOID *NSS_OID_X520_GIVEN_NAME = (NSSOID *)&nss_builtin_oids[240];
const NSSOID *NSS_OID_X520_INITIALS = (NSSOID *)&nss_builtin_oids[241];
const NSSOID *NSS_OID_X520_GENERATION_QUALIFIER = (NSSOID *)&nss_builtin_oids[242];
const NSSOID *NSS_OID_X520_DN_QUALIFIER = (NSSOID *)&nss_builtin_oids[243];
const NSSOID *NSS_OID_X500_RSA_ENCRYPTION = (NSSOID *)&nss_builtin_oids[249];
const NSSOID *NSS_OID_X509_SUBJECT_DIRECTORY_ATTR = (NSSOID *)&nss_builtin_oids[268];
const NSSOID *NSS_OID_X509_SUBJECT_DIRECTORY_ATTRIBUTES = (NSSOID *)&nss_builtin_oids[269];
const NSSOID *NSS_OID_X509_SUBJECT_KEY_ID = (NSSOID *)&nss_builtin_oids[270];
const NSSOID *NSS_OID_X509_KEY_USAGE = (NSSOID *)&nss_builtin_oids[271];
const NSSOID *NSS_OID_X509_PRIVATE_KEY_USAGE_PERIOD = (NSSOID *)&nss_builtin_oids[272];
const NSSOID *NSS_OID_X509_SUBJECT_ALT_NAME = (NSSOID *)&nss_builtin_oids[273];
const NSSOID *NSS_OID_X509_ISSUER_ALT_NAME = (NSSOID *)&nss_builtin_oids[274];
const NSSOID *NSS_OID_X509_BASIC_CONSTRAINTS = (NSSOID *)&nss_builtin_oids[275];
const NSSOID *NSS_OID_X509_CRL_NUMBER = (NSSOID *)&nss_builtin_oids[276];
const NSSOID *NSS_OID_X509_REASON_CODE = (NSSOID *)&nss_builtin_oids[277];
const NSSOID *NSS_OID_X509_HOLD_INSTRUCTION_CODE = (NSSOID *)&nss_builtin_oids[278];
const NSSOID *NSS_OID_X509_INVALID_DATE = (NSSOID *)&nss_builtin_oids[279];
const NSSOID *NSS_OID_X509_DELTA_CRL_INDICATOR = (NSSOID *)&nss_builtin_oids[280];
const NSSOID *NSS_OID_X509_ISSUING_DISTRIBUTION_POINT = (NSSOID *)&nss_builtin_oids[281];
const NSSOID *NSS_OID_X509_CERTIFICATE_ISSUER = (NSSOID *)&nss_builtin_oids[282];
const NSSOID *NSS_OID_X509_NAME_CONSTRAINTS = (NSSOID *)&nss_builtin_oids[283];
const NSSOID *NSS_OID_X509_CRL_DIST_POINTS = (NSSOID *)&nss_builtin_oids[284];
const NSSOID *NSS_OID_X509_CERTIFICATE_POLICIES = (NSSOID *)&nss_builtin_oids[285];
const NSSOID *NSS_OID_X509_POLICY_MAPPINGS = (NSSOID *)&nss_builtin_oids[286];
const NSSOID *NSS_OID_X509_AUTH_KEY_ID = (NSSOID *)&nss_builtin_oids[288];
const NSSOID *NSS_OID_X509_POLICY_CONSTRAINTS = (NSSOID *)&nss_builtin_oids[289];
const NSSOID *NSS_OID_X509_EXT_KEY_USAGE = (NSSOID *)&nss_builtin_oids[290];
const NSSOID *NSS_OID_MISSI_DSS_OLD = (NSSOID *)&nss_builtin_oids[314];
const NSSOID *NSS_OID_FORTEZZA_SKIPJACK = (NSSOID *)&nss_builtin_oids[315];
const NSSOID *NSS_OID_MISSI_KEA = (NSSOID *)&nss_builtin_oids[316];
const NSSOID *NSS_OID_MISSI_KEA_DSS_OLD = (NSSOID *)&nss_builtin_oids[317];
const NSSOID *NSS_OID_MISSI_DSS = (NSSOID *)&nss_builtin_oids[318];
const NSSOID *NSS_OID_MISSI_KEA_DSS = (NSSOID *)&nss_builtin_oids[319];
const NSSOID *NSS_OID_MISSI_ALT_KEY = (NSSOID *)&nss_builtin_oids[320];
const NSSOID *NSS_OID_NS_CERT_EXT_CERT_TYPE = (NSSOID *)&nss_builtin_oids[328];
const NSSOID *NSS_OID_NS_CERT_EXT_BASE_URL = (NSSOID *)&nss_builtin_oids[329];
const NSSOID *NSS_OID_NS_CERT_EXT_REVOCATION_URL = (NSSOID *)&nss_builtin_oids[330];
const NSSOID *NSS_OID_NS_CERT_EXT_CA_REVOCATION_URL = (NSSOID *)&nss_builtin_oids[331];
const NSSOID *NSS_OID_NS_CERT_EXT_CA_CRL_URL = (NSSOID *)&nss_builtin_oids[332];
const NSSOID *NSS_OID_NS_CERT_EXT_CA_CERT_URL = (NSSOID *)&nss_builtin_oids[333];
const NSSOID *NSS_OID_NS_CERT_EXT_CERT_RENEWAL_URL = (NSSOID *)&nss_builtin_oids[334];
const NSSOID *NSS_OID_NS_CERT_EXT_CA_POLICY_URL = (NSSOID *)&nss_builtin_oids[335];
const NSSOID *NSS_OID_NS_CERT_EXT_HOMEPAGE_URL = (NSSOID *)&nss_builtin_oids[336];
const NSSOID *NSS_OID_NS_CERT_EXT_ENTITY_LOGO = (NSSOID *)&nss_builtin_oids[337];
const NSSOID *NSS_OID_NS_CERT_EXT_USER_PICTURE = (NSSOID *)&nss_builtin_oids[338];
const NSSOID *NSS_OID_NS_CERT_EXT_SSL_SERVER_NAME = (NSSOID *)&nss_builtin_oids[339];
const NSSOID *NSS_OID_NS_CERT_EXT_COMMENT = (NSSOID *)&nss_builtin_oids[340];
const NSSOID *NSS_OID_NS_CERT_EXT_THAYES = (NSSOID *)&nss_builtin_oids[341];
const NSSOID *NSS_OID_NS_TYPE_GIF = (NSSOID *)&nss_builtin_oids[343];
const NSSOID *NSS_OID_NS_TYPE_JPEG = (NSSOID *)&nss_builtin_oids[344];
const NSSOID *NSS_OID_NS_TYPE_URL = (NSSOID *)&nss_builtin_oids[345];
const NSSOID *NSS_OID_NS_TYPE_HTML = (NSSOID *)&nss_builtin_oids[346];
const NSSOID *NSS_OID_NS_TYPE_CERT_SEQUENCE = (NSSOID *)&nss_builtin_oids[347];
const NSSOID *NSS_OID_NS_KEY_USAGE_GOVT_APPROVED = (NSSOID *)&nss_builtin_oids[350];
const NSSOID *NSS_OID_NETSCAPE_RECOVERY_REQUEST = (NSSOID *)&nss_builtin_oids[353];
const NSSOID *NSS_OID_NETSCAPE_SMIME_KEA = (NSSOID *)&nss_builtin_oids[355];
const NSSOID *NSS_OID_NETSCAPE_NICKNAME = (NSSOID *)&nss_builtin_oids[357];
const NSSOID *NSS_OID_VERISIGN_USER_NOTICES = (NSSOID *)&nss_builtin_oids[362];
const NSSOID *NSS_OID_NS_CERT_EXT_NETSCAPE_OK = (NSSOID *)&nss_builtin_oids[367];
const NSSOID *NSS_OID_NS_CERT_EXT_ISSUER_LOGO = (NSSOID *)&nss_builtin_oids[368];
const NSSOID *NSS_OID_NS_CERT_EXT_SUBJECT_LOGO = (NSSOID *)&nss_builtin_oids[369];

const nssAttributeTypeAliasTable nss_attribute_type_aliases[] = {
  {
    "uid",
    &NSS_OID_RFC1274_UID
  },
  {
    "mail",
    &NSS_OID_RFC1274_EMAIL
  },
  {
    "dc",
    &NSS_OID_RFC2247_DC
  },
  {
    "cn",
    &NSS_OID_X520_COMMON_NAME
  },
  {
    "sn",
    &NSS_OID_X520_SURNAME
  },
  {
    "c",
    &NSS_OID_X520_COUNTRY_NAME
  },
  {
    "l",
    &NSS_OID_X520_LOCALITY_NAME
  },
  {
    "s",
    &NSS_OID_X520_STATE_OR_PROVINCE_NAME
  },
  {
    "o",
    &NSS_OID_X520_ORGANIZATION_NAME
  },
  {
    "ou",
    &NSS_OID_X520_ORGANIZATIONAL_UNIT_NAME
  },
  {
    "title",
    &NSS_OID_X520_TITLE
  },
  {
    "name",
    &NSS_OID_X520_NAME
  },
  {
    "givenName",
    &NSS_OID_X520_GIVEN_NAME
  },
  {
    "initials",
    &NSS_OID_X520_INITIALS
  },
  {
    "generationQualifier",
    &NSS_OID_X520_GENERATION_QUALIFIER
  },
  {
    "dnQualifier",
    &NSS_OID_X520_DN_QUALIFIER
  }
};

const PRUint32 nss_attribute_type_alias_count = 16;

