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
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
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
 *
 */

#include "prmem.h"
#include "prerror.h"
#include "prprf.h"

#include "nsNSSCertHelper.h"
#include "cert.h"
#include "nsCOMPtr.h"
#include "nsNSSASN1Object.h"
#include "nsNSSComponent.h"
#include "nsNSSCertTrust.h"


nsresult
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

nsresult
ProcessVersion(SECItem         *versionItem,
               nsINSSComponent *nssComponent,
               nsIASN1PrintableItem **retItem)
{
  nsresult rv;
  nsString text;
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
 
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion").get(),
                                      text);
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
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion1").get(),
                                             text);
    break;
  case 1:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion2").get(),
                                             text);
    break;
  case 2:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion3").get(),
                                             text);
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

nsresult 
ProcessSerialNumberDER(SECItem         *serialItem, 
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retItem)
{
  nsresult rv;
  nsString text;
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSerialNo").get(),
                                           text); 
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

nsresult
GetDefaultOIDFormat(SECItem *oid,
                    nsString &outString)
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

  outString = NS_ConvertASCIItoUCS2(buf).get();
  return NS_OK; 
}

nsresult
GetOIDText(SECItem *oid, nsINSSComponent *nssComponent, nsString &text)
{ 
  nsresult rv;
  SECOidTag oidTag = SECOID_FindOIDTag(oid);

  switch (oidTag) {
  case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpMD2WithRSA").get(),
                                             text);
    break;
  case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpMD5WithRSA").get(),
                                             text);
    break;
  case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSHA1WithRSA").get(),
                                             text);
    break;
  case SEC_OID_AVA_COUNTRY_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVACountry").get(),
                                             text);
    break;
  case SEC_OID_AVA_COMMON_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVACN").get(),
                                             text);
    break;
  case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVAOU").get(),
                                             text);
    break;
  case SEC_OID_AVA_ORGANIZATION_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVAOrg").get(),
                                             text);
    break;
  case SEC_OID_AVA_LOCALITY:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVALocality").get(),
                                             text);
    break;
  case SEC_OID_AVA_DN_QUALIFIER:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVADN").get(),
                                             text);
    break;
  case SEC_OID_AVA_DC:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVADC").get(),
                                             text);
    break;
  case SEC_OID_AVA_STATE_OR_PROVINCE:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVAState").get(),
                                             text);
    break;
  case SEC_OID_PKCS1_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpRSAEncr").get(),
                                             text);
    break;
  case SEC_OID_X509_KEY_USAGE:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKeyUsage").get(),
                                             text);
    break;
  case SEC_OID_NS_CERT_EXT_CERT_TYPE:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCertType").get(),
                                             text);
    break;
  case SEC_OID_X509_AUTH_KEY_ID:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAuthKeyID").get(),
                                             text);
    break;
  case SEC_OID_RFC1274_UID:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpUserID").get(),
                                             text);
    break;
  case SEC_OID_PKCS9_EMAIL_ADDRESS:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpPK9Email").get(),
                                             text);
    break;
  default:
    rv = GetDefaultOIDFormat(oid, text);
    if (NS_FAILED(rv))
      return rv;

    const PRUnichar *params[1] = {text.get()};
    nsXPIDLString text2;
    rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("CertDumpDefOID").get(),
                                                     params, 1,
                                                     getter_Copies(text2));
    text = text2;
    break;
  }
  return rv;  
}

#define SEPARATOR "\n"

nsresult
ProcessRawBytes(SECItem *data, nsString &text)
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
    text.Append(NS_ConvertASCIItoUCS2(buffer).get());
    if ((i+1)%16 == 0) {
      text.Append(NS_LITERAL_STRING(SEPARATOR).get());
    }
  }
  return NS_OK;
}    

nsresult
ProcessNSCertTypeExtensions(SECItem  *extData, 
                            nsString &text,
                            nsINSSComponent *nssComponent)
{
  nsAutoString local;
  SECItem decoded;
  decoded.data = nsnull;
  decoded.len  = 0;
  if (SECSuccess != SEC_ASN1DecodeItem(nsnull, &decoded, 
		SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpExtensionFailure").get(),
                                        local);
    text.Append(local.get());
    return NS_OK;
  }
  unsigned char nsCertType = decoded.data[0];
  nsMemory::Free(decoded.data);
  if (nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifySSLClient").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_SSL_SERVER) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifySSLServer").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_EMAIL) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCertTypeEmail").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifyObjSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_SSL_CA) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifySSLCA").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_EMAIL_CA) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpEmailCA").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING_CA) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifyObjSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  return NS_OK;
}

nsresult
ProcessKeyUsageExtension(SECItem *extData, nsString &text,
                         nsINSSComponent *nssComponent)
{
  nsAutoString local;
  SECItem decoded;
  decoded.data = nsnull;
  decoded.len  = 0;
  if (SECSuccess != SEC_ASN1DecodeItem(nsnull, &decoded, 
				SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpExtensionFailure").get(),
                                        local);
    text.Append(local.get());
    return NS_OK;
  }
  unsigned char keyUsage = decoded.data[0];
  nsMemory::Free(decoded.data);  
  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUNonRep").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUEnc").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUDEnc").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUKA").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUCertSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_CRL_SIGN) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUCRLSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }

  return NS_OK;
}

nsresult
ProcessExtensionData(SECOidTag oidTag, SECItem *extData, 
                     nsString &text, nsINSSComponent *nssComponent)
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

nsresult
ProcessSingleExtension(CERTCertExtension *extension,
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retExtension)
{
  nsString text;
  GetOIDText(&extension->id, nssComponent, text);
  nsCOMPtr<nsIASN1PrintableItem>extensionItem = new nsNSSASN1PrintableItem();
  if (extensionItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  extensionItem->SetDisplayName(text);
  SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);
  text.Truncate();
  if (extension->critical.data != nsnull) {
    if (extension->critical.data[0]) {
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCritical").get(),
                                          text);
    } else {
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNonCritical").get(),
                                         text);
    }
  } else {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNonCritical").get(),
                                        text);
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

PRUint32 
getCertType(CERTCertificate *cert)
{
  char *nick = cert->nickname;
  char *email = cert->emailAddr;
  nsNSSCertTrust trust(cert->trust);
  /*
fprintf(stderr, "====> nick: %s  email: %s  has-any-user: %d  hash-any-ca: %d  has-peer100: %d  has-peer001: %d\n",
  nick, email, (nick) ? trust.HasAnyUser() : 0, (nick) ? trust.HasAnyCA() : 0, (nick) ? trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE) : 0, 
  (email) ? trust.HasPeer(PR_FALSE, PR_TRUE, PR_FALSE) : 0 );
*/
  if (nick) {
    if (trust.HasAnyUser())
      return nsIX509Cert::USER_CERT;
    if (trust.HasAnyCA() || CERT_IsCACert(cert,NULL))
      return nsIX509Cert::CA_CERT;
    if (trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE))
      return nsIX509Cert::SERVER_CERT;
  }
  if (email && trust.HasPeer(PR_FALSE, PR_TRUE, PR_FALSE))
    return nsIX509Cert::EMAIL_CERT;
  return nsIX509Cert::UNKNOWN_CERT;
}
