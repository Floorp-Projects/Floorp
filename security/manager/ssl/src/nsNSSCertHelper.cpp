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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   John Gardiner Myers <jgmyers@speakeasy.net>
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

#include "prmem.h"
#include "prerror.h"
#include "prprf.h"

#include "nsNSSCertHelper.h"
#include "nsCOMPtr.h"
#include "nsNSSCertificate.h"
#include "cert.h"
#include "nsNSSCertValidity.h"
#include "nsNSSASN1Object.h"
#include "nsNSSComponent.h"
#include "nsNSSCertTrust.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
 
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

static nsresult
GetIntValue(SECItem *versionItem, 
            unsigned long *version)
{
  SECStatus srv;

  srv = SEC_ASN1DecodeInteger(versionItem,version);
  if (srv != SECSuccess) {
    NS_ASSERTION(0,"Could not decode version of cert");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static nsresult
ProcessVersion(SECItem         *versionItem,
               nsINSSComponent *nssComponent,
               nsIASN1PrintableItem **retItem)
{
  nsresult rv;
  nsAutoString text;
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
 
  nssComponent->GetPIPNSSBundleString("CertDumpVersion", text);
  rv = printableItem->SetDisplayName(text);
  if (NS_FAILED(rv))
    return rv;

  // Now to figure out what version this certificate is.
  unsigned long version;

  if (versionItem->data) {
    rv = GetIntValue(versionItem, &version);
    if (NS_FAILED(rv))
      return rv;
  } else {
    // If there is no version present in the cert, then rfc2459
    // says we default to v1 (0)
    version = 0;
  }

  switch (version){
  case 0:
    rv = nssComponent->GetPIPNSSBundleString("CertDumpVersion1", text);
    break;
  case 1:
    rv = nssComponent->GetPIPNSSBundleString("CertDumpVersion2", text);
    break;
  case 2:
    rv = nssComponent->GetPIPNSSBundleString("CertDumpVersion3", text);
    break;
  default:
    NS_ASSERTION(0,"Bad value for cert version");
    rv = NS_ERROR_FAILURE;
  }
    
  if (NS_FAILED(rv))
    return rv;

  rv = printableItem->SetDisplayValue(text);
  if (NS_FAILED(rv))
    return rv;

  *retItem = printableItem;
  NS_ADDREF(*retItem);
  return NS_OK;
}

static nsresult 
ProcessSerialNumberDER(SECItem         *serialItem, 
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retItem)
{
  nsresult rv;
  nsAutoString text;
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = nssComponent->GetPIPNSSBundleString("CertDumpSerialNo", text); 
  if (NS_FAILED(rv))
    return rv;

  rv = printableItem->SetDisplayName(text);
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString serialNumber;
  serialNumber.Adopt(CERT_Hexify(serialItem, 1));
  if (serialNumber == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = printableItem->SetDisplayValue(NS_ConvertASCIItoUCS2(serialNumber));
  *retItem = printableItem;
  NS_ADDREF(*retItem);
  return rv;
}

static nsresult
GetDefaultOIDFormat(SECItem *oid,
                    nsAString &outString)
{
  char buf[300];
  unsigned int len;
  int written;
    
  unsigned long val  = oid->data[0];
  unsigned int  i    = val % 40;
  val /= 40;
  written = PR_snprintf(buf, 300, "%lu %u ", val, i);
  if (written < 0)
    return NS_ERROR_FAILURE;	
  len = written;

  val = 0;
  for (i = 1; i < oid->len; ++i) {
    // In this loop, we have to parse a DER formatted 
    // If the first bit is a 1, then the integer is 
    // represented by more than one byte.  If the 
    // first bit is set then we continue on and add
    // the values of the later bytes until we get 
    // a byte without the first bit set.
    unsigned long j;

    j = oid->data[i];
    val = (val << 7) | (j & 0x7f);
    if (j & 0x80)
      continue;
    written = PR_snprintf(&buf[len], sizeof(buf)-len, "%lu ", val);
    if (written < 0)
      return NS_ERROR_FAILURE;

    len += written;
    NS_ASSERTION(len < sizeof(buf), "OID data to big to display in 300 chars.");
    val = 0;      
  }

  CopyASCIItoUTF16(buf, outString);
  return NS_OK; 
}

static nsresult
GetOIDText(SECItem *oid, nsINSSComponent *nssComponent, nsAString &text)
{ 
  nsresult rv;
  SECOidTag oidTag = SECOID_FindOIDTag(oid);
  const char *bundlekey = 0;

  switch (oidTag) {
  case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpMD2WithRSA";
    break;
  case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpMD5WithRSA";
    break;
  case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA1WithRSA";
    break;
  case SEC_OID_PKCS1_RSA_ENCRYPTION:
    bundlekey = "CertDumpRSAEncr";
    break;
  case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA256WithRSA";
    break;
  case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA384WithRSA";
    break;
  case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA512WithRSA";
    break;
  case SEC_OID_NS_CERT_EXT_CERT_TYPE:
    bundlekey = "CertDumpCertType";
    break;
  case SEC_OID_NS_CERT_EXT_BASE_URL:
    bundlekey = "CertDumpNSCertExtBaseUrl";
    break;
  case SEC_OID_NS_CERT_EXT_REVOCATION_URL:
    bundlekey = "CertDumpNSCertExtRevocationUrl";
    break;
  case SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL:
    bundlekey = "CertDumpNSCertExtCARevocationUrl";
    break;
  case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL:
    bundlekey = "CertDumpNSCertExtCertRenewalUrl";
    break;
  case SEC_OID_NS_CERT_EXT_CA_POLICY_URL:
    bundlekey = "CertDumpNSCertExtCAPolicyUrl";
    break;
  case SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME:
    bundlekey = "CertDumpNSCertExtSslServerName";
    break;
  case SEC_OID_NS_CERT_EXT_COMMENT:
    bundlekey = "CertDumpNSCertExtComment";
    break;
  case SEC_OID_NS_CERT_EXT_LOST_PASSWORD_URL:
    bundlekey = "CertDumpNSCertExtLostPasswordUrl";
    break;
  case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME:
    bundlekey = "CertDumpNSCertExtCertRenewalTime";
    break;
  case SEC_OID_NETSCAPE_AOLSCREENNAME:
    bundlekey = "CertDumpNetscapeAolScreenname";
    break;
  case SEC_OID_AVA_COUNTRY_NAME:
    bundlekey = "CertDumpAVACountry";
    break;
  case SEC_OID_AVA_COMMON_NAME:
    bundlekey = "CertDumpAVACN";
    break;
  case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
    bundlekey = "CertDumpAVAOU";
    break;
  case SEC_OID_AVA_ORGANIZATION_NAME:
    bundlekey = "CertDumpAVAOrg";
    break;
  case SEC_OID_AVA_LOCALITY:
    bundlekey = "CertDumpAVALocality";
    break;
  case SEC_OID_AVA_DN_QUALIFIER:
    bundlekey = "CertDumpAVADN";
    break;
  case SEC_OID_AVA_DC:
    bundlekey = "CertDumpAVADC";
    break;
  case SEC_OID_AVA_STATE_OR_PROVINCE:
    bundlekey = "CertDumpAVAState";
    break;
  case SEC_OID_X509_SUBJECT_DIRECTORY_ATTR:
    bundlekey = "CertDumpSubjectDirectoryAttr";
    break;
  case SEC_OID_X509_SUBJECT_KEY_ID:
    bundlekey = "CertDumpSubjectKeyID";
    break;
  case SEC_OID_X509_KEY_USAGE:
    bundlekey = "CertDumpKeyUsage";
    break;
  case SEC_OID_X509_SUBJECT_ALT_NAME:
    bundlekey = "CertDumpSubjectAltName";
    break;
  case SEC_OID_X509_ISSUER_ALT_NAME:
    bundlekey = "CertDumpIssuerAltName";
    break;
  case SEC_OID_X509_BASIC_CONSTRAINTS:
    bundlekey = "CertDumpBasicConstraints";
    break;
  case SEC_OID_X509_NAME_CONSTRAINTS:
    bundlekey = "CertDumpNameConstraints";
    break;
  case SEC_OID_X509_CRL_DIST_POINTS:
    bundlekey = "CertDumpCrlDistPoints";
    break;
  case SEC_OID_X509_CERTIFICATE_POLICIES:
    bundlekey = "CertDumpCertPolicies";
    break;
  case SEC_OID_X509_POLICY_MAPPINGS:
    bundlekey = "CertDumpPolicyMappings";
    break;
  case SEC_OID_X509_POLICY_CONSTRAINTS:
    bundlekey = "CertDumpPolicyConstraints";
    break;
  case SEC_OID_X509_AUTH_KEY_ID:
    bundlekey = "CertDumpAuthKeyID";
    break;
  case SEC_OID_X509_EXT_KEY_USAGE:
    bundlekey = "CertDumpExtKeyUsage";
    break;
  case SEC_OID_X509_AUTH_INFO_ACCESS:
    bundlekey = "CertDumpAuthInfoAccess";
    break;
  case SEC_OID_ANSIX9_DSA_SIGNATURE:
    bundlekey = "CertDumpAnsiX9DsaSignature";
    break;
  case SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST:
    bundlekey = "CertDumpAnsiX9DsaSignatureWithSha1";
    break;
  case SEC_OID_ANSIX962_ECDSA_SIGNATURE_WITH_SHA1_DIGEST:
    bundlekey = "CertDumpAnsiX962ECDsaSignatureWithSha1";
    break;
  case SEC_OID_RFC1274_UID:
    bundlekey = "CertDumpUserID";
    break;
  case SEC_OID_PKCS9_EMAIL_ADDRESS:
    bundlekey = "CertDumpPK9Email";
    break;
  default: ;
  }

  if (bundlekey) {
    rv = nssComponent->GetPIPNSSBundleString(bundlekey, text);
  } else {
    nsAutoString text2;
    rv = GetDefaultOIDFormat(oid, text2);
    if (NS_FAILED(rv))
      return rv;

    const PRUnichar *params[1] = {text2.get()};
    rv = nssComponent->PIPBundleFormatStringFromName("CertDumpDefOID",
                                                     params, 1, text);
  }
  return rv;  
}

#define SEPARATOR "\n"

static nsresult
ProcessRawBytes(SECItem *data, nsAString &text)
{
  // This function is used to display some DER bytes
  // that we have not added support for decoding.
  // It prints the value of the byte out into a 
  // string that can later be displayed as a byte
  // string.  We place a new line after 24 bytes
  // to break up extermaly long sequence of bytes.
  PRUint32 i;
  char buffer[5];
  for (i=0; i<data->len; i++) {
    PR_snprintf(buffer, 5, "%02x ", data->data[i]);
    AppendASCIItoUTF16(buffer, text);
    if ((i+1)%16 == 0) {
      text.Append(NS_LITERAL_STRING(SEPARATOR).get());
    }
  }
  return NS_OK;
}    

static nsresult
ProcessNSCertTypeExtensions(SECItem  *extData, 
                            nsAString &text,
                            nsINSSComponent *nssComponent)
{
  nsAutoString local;
  SECItem decoded;
  decoded.data = nsnull;
  decoded.len  = 0;
  if (SECSuccess != SEC_ASN1DecodeItem(nsnull, &decoded, 
		SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
    nssComponent->GetPIPNSSBundleString("CertDumpExtensionFailure", local);
    text.Append(local.get());
    return NS_OK;
  }
  unsigned char nsCertType = decoded.data[0];
  nsMemory::Free(decoded.data);
  if (nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
    nssComponent->GetPIPNSSBundleString("VerifySSLClient", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_SSL_SERVER) {
    nssComponent->GetPIPNSSBundleString("VerifySSLServer", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_EMAIL) {
    nssComponent->GetPIPNSSBundleString("CertDumpCertTypeEmail", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING) {
    nssComponent->GetPIPNSSBundleString("VerifyObjSign", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_SSL_CA) {
    nssComponent->GetPIPNSSBundleString("VerifySSLCA", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_EMAIL_CA) {
    nssComponent->GetPIPNSSBundleString("CertDumpEmailCA", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING_CA) {
    nssComponent->GetPIPNSSBundleString("VerifyObjSign", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  return NS_OK;
}

static nsresult
ProcessKeyUsageExtension(SECItem *extData, nsAString &text,
                         nsINSSComponent *nssComponent)
{
  nsAutoString local;
  SECItem decoded;
  decoded.data = nsnull;
  decoded.len  = 0;
  if (SECSuccess != SEC_ASN1DecodeItem(nsnull, &decoded, 
				SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
    nssComponent->GetPIPNSSBundleString("CertDumpExtensionFailure", local);
    text.Append(local.get());
    return NS_OK;
  }
  unsigned char keyUsage = decoded.data[0];
  nsMemory::Free(decoded.data);  
  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUSign", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUNonRep", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUEnc", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUDEnc", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUKA", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUCertSign", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_CRL_SIGN) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUCRLSign", local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }

  return NS_OK;
}

static nsresult
ProcessName(CERTName *name, nsINSSComponent *nssComponent, PRUnichar **value)
{
  CERTRDN** rdns;
  CERTRDN** rdn;
  CERTAVA** avas;
  CERTAVA* ava;
  SECItem *decodeItem = nsnull;
  nsString finalString;

  rdns = name->rdns;

  nsString type;
  nsresult rv;
  const PRUnichar *params[2];
  nsString avavalue;
  nsAutoString temp;
  CERTRDN **lastRdn;
  lastRdn = rdns;


  /* find last RDN */
  lastRdn = rdns;
  while (*lastRdn) lastRdn++;
  // The above whille loop will put us at the last member
  // of the array which is a NULL pointer.  So let's back
  // up one spot so that we have the last non-NULL entry in 
  // the array in preparation for traversing the 
  // RDN's (Relative Distinguished Name) in reverse oder.
  lastRdn--;
   
  /*
   * Loop over name contents in _reverse_ RDN order appending to string
   * When building the Ascii string, NSS loops over these entries in 
   * reverse order, so I will as well.  The difference is that NSS
   * will always place them in a one line string separated by commas,
   * where I want each entry on a single line.  I can't just use a comma
   * as my delimitter because it is a valid character to have in the 
   * value portion of the AVA and could cause trouble when parsing.
   */
  for (rdn = lastRdn; rdn >= rdns; rdn--) {
    avas = (*rdn)->avas;
    while ((ava = *avas++) != 0) {
      rv = GetOIDText(&ava->type, nssComponent, type);
      if (NS_FAILED(rv))
        return rv;

      //This function returns a string in UTF8 format.
      decodeItem = CERT_DecodeAVAValue(&ava->value);
      if(!decodeItem) {
         return NS_ERROR_FAILURE;
      }
      avavalue = NS_ConvertUTF8toUTF16((char*)decodeItem->data, decodeItem->len);

      SECITEM_FreeItem(decodeItem, PR_TRUE);
      params[0] = type.get();
      params[1] = avavalue.get();
      nssComponent->PIPBundleFormatStringFromName("AVATemplate",
                                                  params, 2, temp);
      finalString += temp + NS_LITERAL_STRING("\n");
    }
  }
  *value = ToNewUnicode(finalString);    
  return NS_OK;
}

static nsresult
ProcessExtensionData(SECOidTag oidTag, SECItem *extData, 
                     nsAString &text, nsINSSComponent *nssComponent)
{
  nsresult rv;
  switch (oidTag) {
  case SEC_OID_NS_CERT_EXT_CERT_TYPE:
    rv = ProcessNSCertTypeExtensions(extData, text, nssComponent);
    break;
  case SEC_OID_X509_KEY_USAGE:
    rv = ProcessKeyUsageExtension(extData, text, nssComponent);
    break;
  default:
    rv = ProcessRawBytes(extData, text);
    break; 
  }
  return rv;
}

static nsresult
ProcessSingleExtension(CERTCertExtension *extension,
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retExtension)
{
  nsAutoString text;
  GetOIDText(&extension->id, nssComponent, text);
  nsCOMPtr<nsIASN1PrintableItem>extensionItem = new nsNSSASN1PrintableItem();
  if (extensionItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  extensionItem->SetDisplayName(text);
  SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);
  text.Truncate();
  if (extension->critical.data != nsnull) {
    if (extension->critical.data[0]) {
      nssComponent->GetPIPNSSBundleString("CertDumpCritical", text);
    } else {
      nssComponent->GetPIPNSSBundleString("CertDumpNonCritical", text);
    }
  } else {
    nssComponent->GetPIPNSSBundleString("CertDumpNonCritical", text);
  }
  text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  nsresult rv = ProcessExtensionData(oidTag, &extension->value, text, 
                                     nssComponent);
  if (NS_FAILED(rv))
    return rv;

  extensionItem->SetDisplayValue(text);
  *retExtension = extensionItem;
  NS_ADDREF(*retExtension);
  return NS_OK;
}

static nsresult
ProcessSECAlgorithmID(SECAlgorithmID *algID,
                      nsINSSComponent *nssComponent,
                      nsIASN1Sequence **retSequence)
{
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();
  if (sequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  *retSequence = nsnull;
  nsString text;
  GetOIDText(&algID->algorithm, nssComponent, text);
  if (!algID->parameters.len || algID->parameters.data[0] == nsIASN1Object::ASN1_NULL) {
    sequence->SetDisplayValue(text);
    sequence->SetIsValidContainer(PR_FALSE);
  } else {
    nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
    printableItem->SetDisplayValue(text);
    nsCOMPtr<nsIMutableArray> asn1Objects;
    sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
    asn1Objects->AppendElement(printableItem, PR_FALSE);
    nssComponent->GetPIPNSSBundleString("CertDumpAlgID", text);
    printableItem->SetDisplayName(text);
    printableItem = new nsNSSASN1PrintableItem();
    asn1Objects->AppendElement(printableItem, PR_FALSE);
    nssComponent->GetPIPNSSBundleString("CertDumpParams", text);
    printableItem->SetDisplayName(text); 
    ProcessRawBytes(&algID->parameters,text);
    printableItem->SetDisplayValue(text);
  }
  *retSequence = sequence;
  NS_ADDREF(*retSequence);
  return NS_OK;
}

static nsresult
ProcessTime(PRTime dispTime, const PRUnichar *displayName, 
            nsIASN1Sequence *parentSequence)
{
  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  nsString text;
  nsString tempString;

  PRExplodedTime explodedTime;
  PR_ExplodeTime(dispTime, PR_LocalTimeParameters, &explodedTime);

  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                              &explodedTime, tempString);

  text.Append(tempString);
  text.AppendLiteral("\n(");

  PRExplodedTime explodedTimeGMT;
  PR_ExplodeTime(dispTime, PR_GMTParameters, &explodedTimeGMT);

  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                              &explodedTimeGMT, tempString);

  text.Append(tempString);
  text.Append(NS_LITERAL_STRING(" GMT)"));

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayValue(text);
  printableItem->SetDisplayName(nsDependentString(displayName));
  nsCOMPtr<nsIMutableArray> asn1Objects;
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(printableItem, PR_FALSE);
  return NS_OK;
}

static nsresult
ProcessSubjectPublicKeyInfo(CERTSubjectPublicKeyInfo *spki, 
                            nsIASN1Sequence *parentSequence,
                            nsINSSComponent *nssComponent)
{
  nsCOMPtr<nsIASN1Sequence> spkiSequence = new nsNSSASN1Sequence();

  if (spkiSequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsString text;
  nssComponent->GetPIPNSSBundleString("CertDumpSPKI", text);
  spkiSequence->SetDisplayName(text);

  nssComponent->GetPIPNSSBundleString("CertDumpSPKIAlg", text);
  nsCOMPtr<nsIASN1Sequence> sequenceItem;
  nsresult rv = ProcessSECAlgorithmID(&spki->algorithm, nssComponent,
                                      getter_AddRefs(sequenceItem));
  if (NS_FAILED(rv))
    return rv;
  sequenceItem->SetDisplayName(text);
  nsCOMPtr<nsIMutableArray> asn1Objects;
  spkiSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(sequenceItem, PR_FALSE);

  // The subjectPublicKey field is encoded as a bit string.
  // ProcessRawBytes expects the lenght to be in bytes, so 
  // let's convert the lenght into a temporary SECItem.
  SECItem data;
  data.data = spki->subjectPublicKey.data;
  data.len  = spki->subjectPublicKey.len / 8;
  text.Truncate();
  ProcessRawBytes(&data, text);
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayValue(text);
  nssComponent->GetPIPNSSBundleString("CertDumpSubjPubKey", text);
  printableItem->SetDisplayName(text);
  asn1Objects->AppendElement(printableItem, PR_FALSE);
  
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(spkiSequence, PR_FALSE);
  return NS_OK;
}

static nsresult
ProcessExtensions(CERTCertExtension **extensions, 
                  nsIASN1Sequence *parentSequence, 
                  nsINSSComponent *nssComponent)
{
  nsCOMPtr<nsIASN1Sequence> extensionSequence = new nsNSSASN1Sequence;
  if (extensionSequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsString text;
  nssComponent->GetPIPNSSBundleString("CertDumpExtensions", text);
  extensionSequence->SetDisplayName(text);
  PRInt32 i;
  nsresult rv;
  nsCOMPtr<nsIASN1PrintableItem> newExtension;
  nsCOMPtr<nsIMutableArray> asn1Objects;
  extensionSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  for (i=0; extensions[i] != nsnull; i++) {
    rv = ProcessSingleExtension(extensions[i], nssComponent,
                                getter_AddRefs(newExtension));
    if (NS_FAILED(rv))
      return rv;

    asn1Objects->AppendElement(newExtension, PR_FALSE);
  }
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(extensionSequence, PR_FALSE);
  return NS_OK;
}

nsresult
nsNSSCertificate::CreateTBSCertificateASN1Struct(nsIASN1Sequence **retSequence,
                                                 nsINSSComponent *nssComponent)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  //
  //   TBSCertificate  ::=  SEQUENCE  {
  //        version         [0]  EXPLICIT Version DEFAULT v1,
  //        serialNumber         CertificateSerialNumber,
  //        signature            AlgorithmIdentifier,
  //        issuer               Name,
  //        validity             Validity,
  //        subject              Name,
  //        subjectPublicKeyInfo SubjectPublicKeyInfo,
  //        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version shall be v2 or v3
  //        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version shall be v2 or v3
  //        extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                            -- If present, version shall be v3
  //        }
  //
  // This is the ASN1 structure we should be dealing with at this point.
  // The code in this method will assert this is the structure we're dealing
  // and then add more user friendly text for that field.
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();
  if (sequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsString text;
  nssComponent->GetPIPNSSBundleString("CertDumpCertificate", text);
  sequence->SetDisplayName(text);
  nsCOMPtr<nsIASN1PrintableItem> printableItem;
  
  nsCOMPtr<nsIMutableArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));

  nsresult rv = ProcessVersion(&mCert->version, nssComponent,
                               getter_AddRefs(printableItem));
  if (NS_FAILED(rv))
    return rv;

  asn1Objects->AppendElement(printableItem, PR_FALSE);
  
  rv = ProcessSerialNumberDER(&mCert->serialNumber, nssComponent,
                              getter_AddRefs(printableItem));

  if (NS_FAILED(rv))
    return rv;
  asn1Objects->AppendElement(printableItem, PR_FALSE);

  nsCOMPtr<nsIASN1Sequence> algID;
  rv = ProcessSECAlgorithmID(&mCert->signature,
                             nssComponent, getter_AddRefs(algID));
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString("CertDumpSigAlg", text);
  algID->SetDisplayName(text);
  asn1Objects->AppendElement(algID, PR_FALSE);

  nsXPIDLString value;
  ProcessName(&mCert->issuer, nssComponent, getter_Copies(value));

  printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayValue(value);
  nssComponent->GetPIPNSSBundleString("CertDumpIssuer", text);
  printableItem->SetDisplayName(text);
  asn1Objects->AppendElement(printableItem, PR_FALSE);
  
  nsCOMPtr<nsIASN1Sequence> validitySequence = new nsNSSASN1Sequence();
  nssComponent->GetPIPNSSBundleString("CertDumpValidity", text);
  validitySequence->SetDisplayName(text);
  asn1Objects->AppendElement(validitySequence, PR_FALSE);
  nssComponent->GetPIPNSSBundleString("CertDumpNotBefore", text);
  nsCOMPtr<nsIX509CertValidity> validityData;
  GetValidity(getter_AddRefs(validityData));
  PRTime notBefore, notAfter;

  validityData->GetNotBefore(&notBefore);
  validityData->GetNotAfter(&notAfter);
  validityData = 0;
  rv = ProcessTime(notBefore, text.get(), validitySequence);
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString("CertDumpNotAfter", text);
  rv = ProcessTime(notAfter, text.get(), validitySequence);
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString("CertDumpSubject", text);

  printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayName(text);
  ProcessName(&mCert->subject, nssComponent,getter_Copies(value));
  printableItem->SetDisplayValue(value);
  asn1Objects->AppendElement(printableItem, PR_FALSE);

  rv = ProcessSubjectPublicKeyInfo(&mCert->subjectPublicKeyInfo, sequence,
                                   nssComponent); 
  if (NS_FAILED(rv))
    return rv;
 
  SECItem data; 
  // Is there an issuerUniqueID?
  if (mCert->issuerID.data != nsnull) {
    // The issuerID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->issuerID.data;
    data.len  = mCert->issuerID.len / 8;

    ProcessRawBytes(&data, text);
    printableItem = new nsNSSASN1PrintableItem();
    if (printableItem == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    printableItem->SetDisplayValue(text);
    nssComponent->GetPIPNSSBundleString("CertDumpIssuerUniqueID", text);
    printableItem->SetDisplayName(text);
    asn1Objects->AppendElement(printableItem, PR_FALSE);
  }

  if (mCert->subjectID.data) {
    // The subjectID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->issuerID.data;
    data.len  = mCert->issuerID.len / 8;

    ProcessRawBytes(&data, text);
    printableItem = new nsNSSASN1PrintableItem();
    if (printableItem == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    printableItem->SetDisplayValue(text);
    nssComponent->GetPIPNSSBundleString("CertDumpSubjectUniqueID", text);
    printableItem->SetDisplayName(text);
    asn1Objects->AppendElement(printableItem, PR_FALSE);

  }
  if (mCert->extensions) {
    rv = ProcessExtensions(mCert->extensions, sequence, nssComponent);
    if (NS_FAILED(rv))
      return rv;
  }
  *retSequence = sequence;
  NS_ADDREF(*retSequence);  
  return NS_OK;
}

nsresult
nsNSSCertificate::CreateASN1Struct()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();

  mASN1Structure = sequence; 
  if (mASN1Structure == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIMutableArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  nsXPIDLCString title;
  GetWindowTitle(getter_Copies(title));
  
  mASN1Structure->SetDisplayName(NS_ConvertUTF8toUCS2(title));
  // This sequence will be contain the tbsCertificate, signatureAlgorithm,
  // and signatureValue.
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  rv = CreateTBSCertificateASN1Struct(getter_AddRefs(sequence),
                                      nssComponent);
  if (NS_FAILED(rv))
    return rv;

  asn1Objects->AppendElement(sequence, PR_FALSE);
  nsCOMPtr<nsIASN1Sequence> algID;

  rv = ProcessSECAlgorithmID(&mCert->signatureWrap.signatureAlgorithm, 
                             nssComponent, getter_AddRefs(algID));
  if (NS_FAILED(rv))
    return rv;
  nsString text;
  nssComponent->GetPIPNSSBundleString("CertDumpSigAlg", text);
  algID->SetDisplayName(text);
  asn1Objects->AppendElement(algID, PR_FALSE);
  nsCOMPtr<nsIASN1PrintableItem>printableItem = new nsNSSASN1PrintableItem();
  nssComponent->GetPIPNSSBundleString("CertDumpCertSig", text);
  printableItem->SetDisplayName(text);
  // The signatureWrap is encoded as a bit string.
  // The function ProcessRawBytes expects the
  // length to be in bytes, so let's convert the
  // length in a temporary SECItem
  SECItem temp;
  temp.data = mCert->signatureWrap.signature.data;
  temp.len  = mCert->signatureWrap.signature.len / 8;
  text.Truncate();
  ProcessRawBytes(&temp,text);
  printableItem->SetDisplayValue(text);
  asn1Objects->AppendElement(printableItem, PR_FALSE);
  return NS_OK;
}

PRUint32 
getCertType(CERTCertificate *cert)
{
  nsNSSCertTrust trust(cert->trust);
  if (cert->nickname && trust.HasAnyUser())
    return nsIX509Cert::USER_CERT;
  if (trust.HasAnyCA())
    return nsIX509Cert::CA_CERT;
  if (trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE))
    return nsIX509Cert::SERVER_CERT;
  if (trust.HasPeer(PR_FALSE, PR_TRUE, PR_FALSE) && cert->emailAddr)
    return nsIX509Cert::EMAIL_CERT;
  if (CERT_IsCACert(cert,NULL))
    return nsIX509Cert::CA_CERT;
  if (cert->emailAddr)
    return nsIX509Cert::EMAIL_CERT;
  return nsIX509Cert::SERVER_CERT;
}
