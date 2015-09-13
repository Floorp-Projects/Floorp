/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"

#include "prerror.h"
#include "prprf.h"

#include "nsNSSCertHelper.h"
#include "nsCOMPtr.h"
#include "nsNSSCertificate.h"
#include "secder.h"
#include "nsComponentManagerUtils.h"
#include "nsNSSCertValidity.h"
#include "nsNSSASN1Object.h"
#include "nsNSSComponent.h"
#include "nsNSSCertTrust.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsServiceManagerUtils.h"
#include <algorithm>

using namespace mozilla;
 
/* Object Identifier constants */
#define CONST_OID static const unsigned char
#define MICROSOFT_OID 0x2b, 0x6, 0x1, 0x4, 0x1, 0x82, 0x37
#define PKIX_OID 0x2b, 0x6, 0x01, 0x05, 0x05, 0x07
CONST_OID msCertExtCerttype[]      = { MICROSOFT_OID, 20, 2};
CONST_OID msNTPrincipalName[]      = { MICROSOFT_OID, 20, 2, 3 };
CONST_OID msCertsrvCAVersion[]     = { MICROSOFT_OID, 21, 1 };
CONST_OID msNTDSReplication[]      = { MICROSOFT_OID, 25, 1 };
CONST_OID pkixLogotype[]           = { PKIX_OID, 1, 12 };

#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }
#define OD(oid,desc,mech,ext) {OI(oid), SEC_OID_UNKNOWN, desc, mech, ext}
#define SEC_OID(tag) more_oids[tag].offset

static SECOidData more_oids[] = {
    /* Microsoft OIDs */
    #define MS_CERT_EXT_CERTTYPE 0
    OD( msCertExtCerttype,
        "Microsoft Certificate Template Name", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    #define MS_NT_PRINCIPAL_NAME 1
    OD( msNTPrincipalName,
        "Microsoft Principal Name", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    #define MS_CERTSERV_CA_VERSION 2
    OD( msCertsrvCAVersion,
        "Microsoft CA Version", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    #define MS_NTDS_REPLICATION 3
    OD( msNTDSReplication,
        "Microsoft Domain GUID", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),

    #define PKIX_LOGOTYPE 4
    OD( pkixLogotype,
        "Logotype", 
        CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION ),
};

static const unsigned int numOids = (sizeof more_oids) / (sizeof more_oids[0]);

static nsresult
GetIntValue(SECItem *versionItem, 
            unsigned long *version)
{
  SECStatus srv;

  srv = SEC_ASN1DecodeInteger(versionItem,version);
  if (srv != SECSuccess) {
    NS_ERROR("Could not decode version of cert");
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
    NS_ERROR("Bad value for cert version");
    rv = NS_ERROR_FAILURE;
  }
    
  if (NS_FAILED(rv))
    return rv;

  rv = printableItem->SetDisplayValue(text);
  if (NS_FAILED(rv))
    return rv;

  printableItem.forget(retItem);
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

  rv = nssComponent->GetPIPNSSBundleString("CertDumpSerialNo", text); 
  if (NS_FAILED(rv))
    return rv;

  rv = printableItem->SetDisplayName(text);
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString serialNumber;
  serialNumber.Adopt(CERT_Hexify(serialItem, 1));
  if (!serialNumber)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = printableItem->SetDisplayValue(NS_ConvertASCIItoUTF16(serialNumber));
  printableItem.forget(retItem);
  return rv;
}

static nsresult
GetDefaultOIDFormat(SECItem *oid,
                    nsINSSComponent *nssComponent,
                    nsAString &outString,
                    char separator)
{
  char buf[300];
  unsigned int len = 0;
  int written, invalidCount = 0;
    
  unsigned int i;
  unsigned long val  = 0;
  bool invalid = false;
  bool first = true;

  val = 0;
  for (i = 0; i < oid->len; ++i) {
    // In this loop, we have to parse a DER formatted 
    // If the first bit is a 1, then the integer is 
    // represented by more than one byte.  If the 
    // first bit is set then we continue on and add
    // the values of the later bytes until we get 
    // a byte without the first bit set.
    unsigned long j;

    j = oid->data[i];
    val = (val << 7) | (j & 0x7f);
    if (j & 0x80) {
      // - If val is 0 in this block, the OID number particle starts with 0x80
      // what is specified as an invalid formating.
      // - If val is larger then 2^32-7, on next left shift by 7 we will loose
      // the most significant bits, this OID number particle cannot be read
      // by our implementation.
      // - If the first bit is set while this is the last component of the OID
      // we are also in an invalid state.
      if (val == 0 || (val >= (1 << (32-7))) || (i == oid->len-1)) {
        invalid = true;
      }

      if (i < oid->len-1)
        continue;
    }

    if (!invalid) {
      if (first) {
        unsigned long one = std::min(val/40, 2UL); // never > 2
        unsigned long two = val - (one * 40);

        written = PR_snprintf(&buf[len], sizeof(buf)-len, "%lu%c%lu", 
			      one, separator, two);
      }
      else {
        written = PR_snprintf(&buf[len], sizeof(buf)-len, "%c%lu", 
			      separator, val);
      }
    }
    else {
      nsAutoString unknownText;
      nssComponent->GetPIPNSSBundleString("CertUnknown", 
                                          unknownText);
      if (first) {
        written = PR_snprintf(&buf[len], sizeof(buf)-len, "%s",
                              NS_ConvertUTF16toUTF8(unknownText).get());
      }
      else {
        written = PR_snprintf(&buf[len], sizeof(buf)-len, "%c%s",
                              separator, 
                              NS_ConvertUTF16toUTF8(unknownText).get());
      }

      if (++invalidCount > 3) {
        // Allow only 3 occurences of Unknown in OID display string to
        // prevent bloat.
        break;
      }
    }

    if (written < 0)
      return NS_ERROR_FAILURE;

    len += written;
    NS_ASSERTION(len < sizeof(buf), "OID data to big to display in 300 chars.");
    val = 0;      
    invalid = false;
    first = false;
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
  case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA256WithRSA";
    break;
  case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA384WithRSA";
    break;
  case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
    bundlekey = "CertDumpSHA512WithRSA";
    break;
  case SEC_OID_PKCS1_RSA_ENCRYPTION:
    bundlekey = "CertDumpRSAEncr";
    break;
  case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
    bundlekey = "CertDumpRSAPSSSignature";
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
  case SEC_OID_AVA_SURNAME:
    bundlekey = "CertDumpSurname";
    break;
  case SEC_OID_AVA_GIVEN_NAME:
    bundlekey = "CertDumpGivenName";
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
  case SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE:
    bundlekey = "CertDumpAnsiX962ECDsaSignatureWithSha1";
    break;
  case SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE:
    bundlekey = "CertDumpAnsiX962ECDsaSignatureWithSha224";
    break;
  case SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE:
    bundlekey = "CertDumpAnsiX962ECDsaSignatureWithSha256";
    break;
  case SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE:
    bundlekey = "CertDumpAnsiX962ECDsaSignatureWithSha384";
    break;
  case SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE:
    bundlekey = "CertDumpAnsiX962ECDsaSignatureWithSha512";
    break;
  case SEC_OID_RFC1274_UID:
    bundlekey = "CertDumpUserID";
    break;
  case SEC_OID_PKCS9_EMAIL_ADDRESS:
    bundlekey = "CertDumpPK9Email";
    break;
  case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
    bundlekey = "CertDumpECPublicKey";
    break;
  /* ANSI X9.62 named elliptic curves (prime field) */
  case SEC_OID_ANSIX962_EC_PRIME192V1:
    /* same as SEC_OID_SECG_EC_SECP192r1 */
    bundlekey = "CertDumpECprime192v1";
    break;
  case SEC_OID_ANSIX962_EC_PRIME192V2:
    bundlekey = "CertDumpECprime192v2";
    break;
  case SEC_OID_ANSIX962_EC_PRIME192V3:
    bundlekey = "CertDumpECprime192v3";
    break;
  case SEC_OID_ANSIX962_EC_PRIME239V1:
    bundlekey = "CertDumpECprime239v1";
    break;
  case SEC_OID_ANSIX962_EC_PRIME239V2:
    bundlekey = "CertDumpECprime239v2";
    break;
  case SEC_OID_ANSIX962_EC_PRIME239V3:
    bundlekey = "CertDumpECprime239v3";
    break;
  case SEC_OID_ANSIX962_EC_PRIME256V1:
    /* same as SEC_OID_SECG_EC_SECP256r1 */
    bundlekey = "CertDumpECprime256v1";
    break;
  /* SECG named elliptic curves (prime field) */
  case SEC_OID_SECG_EC_SECP112R1:
    bundlekey = "CertDumpECsecp112r1";
    break;
  case SEC_OID_SECG_EC_SECP112R2:
    bundlekey = "CertDumpECsecp112r2";
    break;
  case SEC_OID_SECG_EC_SECP128R1:
    bundlekey = "CertDumpECsecp128r1";
    break;
  case SEC_OID_SECG_EC_SECP128R2:
    bundlekey = "CertDumpECsecp128r2";
    break;
  case SEC_OID_SECG_EC_SECP160K1:
    bundlekey = "CertDumpECsecp160k1";
    break;
  case SEC_OID_SECG_EC_SECP160R1:
    bundlekey = "CertDumpECsecp160r1";
    break;
  case SEC_OID_SECG_EC_SECP160R2:
    bundlekey = "CertDumpECsecp160r2";
    break;
  case SEC_OID_SECG_EC_SECP192K1:
    bundlekey = "CertDumpECsecp192k1";
    break;
  case SEC_OID_SECG_EC_SECP224K1:
    bundlekey = "CertDumpECsecp224k1";
    break;
  case SEC_OID_SECG_EC_SECP224R1:
    bundlekey = "CertDumpECsecp224r1";
    break;
  case SEC_OID_SECG_EC_SECP256K1:
    bundlekey = "CertDumpECsecp256k1";
    break;
  case SEC_OID_SECG_EC_SECP384R1:
    bundlekey = "CertDumpECsecp384r1";
    break;

  case SEC_OID_SECG_EC_SECP521R1:
    bundlekey = "CertDumpECsecp521r1";
    break;
  /* ANSI X9.62 named elliptic curves (characteristic two field) */
  case SEC_OID_ANSIX962_EC_C2PNB163V1:
    bundlekey = "CertDumpECc2pnb163v1";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB163V2:
    bundlekey = "CertDumpECc2pnb163v2";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB163V3:
    bundlekey = "CertDumpECc2pnb163v3";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB176V1:
    bundlekey = "CertDumpECc2pnb176v1";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB191V1:
    bundlekey = "CertDumpECc2tnb191v1";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB191V2:
    bundlekey = "CertDumpECc2tnb191v2";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB191V3:
    bundlekey = "CertDumpECc2tnb191v3";
    break;
  case SEC_OID_ANSIX962_EC_C2ONB191V4:
    bundlekey = "CertDumpECc2onb191v4";
    break;
  case SEC_OID_ANSIX962_EC_C2ONB191V5:
    bundlekey = "CertDumpECc2onb191v5";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB208W1:
    bundlekey = "CertDumpECc2pnb208w1";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB239V1:
    bundlekey = "CertDumpECc2tnb239v1";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB239V2:
    bundlekey = "CertDumpECc2tnb239v2";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB239V3:
    bundlekey = "CertDumpECc2tnb239v3";
    break;
  case SEC_OID_ANSIX962_EC_C2ONB239V4:
    bundlekey = "CertDumpECc2onb239v4";
    break;
  case SEC_OID_ANSIX962_EC_C2ONB239V5:
    bundlekey = "CertDumpECc2onb239v5";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB272W1:
    bundlekey = "CertDumpECc2pnb272w1";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB304W1:
    bundlekey = "CertDumpECc2pnb304w1";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB359V1:
    bundlekey = "CertDumpECc2tnb359v1";
    break;
  case SEC_OID_ANSIX962_EC_C2PNB368W1:
    bundlekey = "CertDumpECc2pnb368w1";
    break;
  case SEC_OID_ANSIX962_EC_C2TNB431R1:
    bundlekey = "CertDumpECc2tnb431r1";
    break;
  /* SECG named elliptic curves (characteristic two field) */
  case SEC_OID_SECG_EC_SECT113R1:
    bundlekey = "CertDumpECsect113r1";
    break;
  case SEC_OID_SECG_EC_SECT113R2:
    bundlekey = "CertDumpECsect113r2";
    break;
  case SEC_OID_SECG_EC_SECT131R1:
    bundlekey = "CertDumpECsect131r1";
    break;
  case SEC_OID_SECG_EC_SECT131R2:
    bundlekey = "CertDumpECsect131r2";
    break;
  case SEC_OID_SECG_EC_SECT163K1:
    bundlekey = "CertDumpECsect163k1";
    break;
  case SEC_OID_SECG_EC_SECT163R1:
    bundlekey = "CertDumpECsect163r1";
    break;
  case SEC_OID_SECG_EC_SECT163R2:
    bundlekey = "CertDumpECsect163r2";
    break;
  case SEC_OID_SECG_EC_SECT193R1:
    bundlekey = "CertDumpECsect193r1";
    break;
  case SEC_OID_SECG_EC_SECT193R2:
    bundlekey = "CertDumpECsect193r2";
    break;
  case SEC_OID_SECG_EC_SECT233K1:
    bundlekey = "CertDumpECsect233k1";
    break;
  case SEC_OID_SECG_EC_SECT233R1:
    bundlekey = "CertDumpECsect233r1";
    break;
  case SEC_OID_SECG_EC_SECT239K1:
    bundlekey = "CertDumpECsect239k1";
    break;
  case SEC_OID_SECG_EC_SECT283K1:
    bundlekey = "CertDumpECsect283k1";
    break;
  case SEC_OID_SECG_EC_SECT283R1:
    bundlekey = "CertDumpECsect283r1";
    break;
  case SEC_OID_SECG_EC_SECT409K1:
    bundlekey = "CertDumpECsect409k1";
    break;
  case SEC_OID_SECG_EC_SECT409R1:
    bundlekey = "CertDumpECsect409r1";
    break;
  case SEC_OID_SECG_EC_SECT571K1:
    bundlekey = "CertDumpECsect571k1";
    break;
  case SEC_OID_SECG_EC_SECT571R1:
    bundlekey = "CertDumpECsect571r1";
    break;
  default: 
    if (oidTag == SEC_OID(MS_CERT_EXT_CERTTYPE)) {
      bundlekey = "CertDumpMSCerttype";
      break;
    }
    if (oidTag == SEC_OID(MS_CERTSERV_CA_VERSION)) {
      bundlekey = "CertDumpMSCAVersion";
      break;
    }
    if (oidTag == SEC_OID(PKIX_LOGOTYPE)) {
      bundlekey = "CertDumpLogotype";
      break;
    }
    /* fallthrough */
  }

  if (bundlekey) {
    rv = nssComponent->GetPIPNSSBundleString(bundlekey, text);
  } else {
    nsAutoString text2;
    rv = GetDefaultOIDFormat(oid, nssComponent, text2, ' ');
    if (NS_FAILED(rv))
      return rv;

    const char16_t *params[1] = {text2.get()};
    rv = nssComponent->PIPBundleFormatStringFromName("CertDumpDefOID",
                                                     params, 1, text);
  }
  return rv;  
}

#define SEPARATOR "\n"

static nsresult
ProcessRawBytes(nsINSSComponent *nssComponent, SECItem *data, 
                nsAString &text, bool wantHeader = true)
{
  // This function is used to display some DER bytes
  // that we have not added support for decoding.
  // If it's short, let's display as an integer, no size header.

  if (data->len <= 4) {
    int i_pv = DER_GetInteger(data);
    nsAutoString value;
    value.AppendInt(i_pv);
    text.Append(value);
    text.AppendLiteral(SEPARATOR);
    return NS_OK;
  }

  // Else produce a hex dump.

  if (wantHeader) {
    nsAutoString bytelen, bitlen;
    bytelen.AppendInt(data->len);
    bitlen.AppendInt(data->len*8);
  
    const char16_t *params[2] = {bytelen.get(), bitlen.get()};
    nsresult rv = nssComponent->PIPBundleFormatStringFromName("CertDumpRawBytesHeader",
                                                              params, 2, text);
    if (NS_FAILED(rv))
      return rv;

    text.AppendLiteral(SEPARATOR);
  }

  // This prints the value of the byte out into a 
  // string that can later be displayed as a byte
  // string.  We place a new line after 24 bytes
  // to break up extermaly long sequence of bytes.

  uint32_t i;
  char buffer[5];
  for (i=0; i<data->len; i++) {
    PR_snprintf(buffer, 5, "%02x ", data->data[i]);
    AppendASCIItoUTF16(buffer, text);
    if ((i+1)%16 == 0) {
      text.AppendLiteral(SEPARATOR);
    }
  }
  return NS_OK;
}    

static nsresult
ProcessKeyUsageExtension(SECItem *extData, nsAString &text,
                         nsINSSComponent *nssComponent)
{
  nsAutoString local;
  SECItem decoded;
  decoded.data = nullptr;
  decoded.len  = 0;
  if (SECSuccess != SEC_ASN1DecodeItem(nullptr, &decoded, 
				SEC_ASN1_GET(SEC_BitStringTemplate), extData)) {
    nssComponent->GetPIPNSSBundleString("CertDumpExtensionFailure", local);
    text.Append(local.get());
    return NS_OK;
  }
  unsigned char keyUsage = 0;
  if (decoded.len) {
    keyUsage = decoded.data[0];
  }
  free(decoded.data);
  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUSign", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUNonRep", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUEnc", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUDEnc", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUKA", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUCertSign", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }
  if (keyUsage & KU_CRL_SIGN) {
    nssComponent->GetPIPNSSBundleString("CertDumpKUCRLSigner", local);
    text.Append(local.get());
    text.AppendLiteral(SEPARATOR);
  }

  return NS_OK;
}

static nsresult
ProcessBasicConstraints(SECItem  *extData, 
                        nsAString &text,
                        nsINSSComponent *nssComponent)
{
  nsAutoString local;
  CERTBasicConstraints value;
  SECStatus rv;
  nsresult rv2;

  value.pathLenConstraint = -1;
  rv = CERT_DecodeBasicConstraintValue (&value, extData);
  if (rv != SECSuccess) {
    ProcessRawBytes(nssComponent, extData, text);
    return NS_OK;
  }
  if (value.isCA)
    rv2 = nssComponent->GetPIPNSSBundleString("CertDumpIsCA", local);
  else
    rv2 = nssComponent->GetPIPNSSBundleString("CertDumpIsNotCA", local);
  if (NS_FAILED(rv2))
    return rv2;
  text.Append(local.get());
  if (value.pathLenConstraint != -1) {
    nsAutoString depth;
    if (value.pathLenConstraint == CERT_UNLIMITED_PATH_CONSTRAINT)
      nssComponent->GetPIPNSSBundleString("CertDumpPathLenUnlimited", depth);
    else
      depth.AppendInt(value.pathLenConstraint);
    const char16_t *params[1] = {depth.get()};
    rv2 = nssComponent->PIPBundleFormatStringFromName("CertDumpPathLen",
                                                      params, 1, local);
    if (NS_FAILED(rv2))
      return rv2;
    text.AppendLiteral(SEPARATOR);
    text.Append(local.get());
  }
  return NS_OK;
}

static nsresult
ProcessExtKeyUsage(SECItem  *extData, 
                   nsAString &text,
                   nsINSSComponent *nssComponent)
{
  nsAutoString local;
  CERTOidSequence *extKeyUsage = nullptr;
  SECItem **oids;
  SECItem *oid;
  nsresult rv;
  
  extKeyUsage = CERT_DecodeOidSequence(extData);
  if (!extKeyUsage)
    return NS_ERROR_FAILURE;

  oids = extKeyUsage->oids;
  while (oids && *oids) {
    // For each OID, try to find a bundle string
    // of the form CertDumpEKU_<underlined-OID>
    nsAutoString oidname;
    oid = *oids;
    rv = GetDefaultOIDFormat(oid, nssComponent, oidname, '_');
    if (NS_FAILED(rv))
      return rv;
    nsAutoString bundlekey = NS_LITERAL_STRING("CertDumpEKU_")+ oidname;
    NS_ConvertUTF16toUTF8 bk_ascii(bundlekey);
    
    rv = nssComponent->GetPIPNSSBundleString(bk_ascii.get(), local);
    nsresult rv2 = GetDefaultOIDFormat(oid, nssComponent, oidname, '.');
    if (NS_FAILED(rv2))
      return rv2;
    if (NS_SUCCEEDED(rv)) {
      // display name and OID in parentheses
      text.Append(local);
      text.AppendLiteral(" (");
      text.Append(oidname);
      text.Append(')');
    } else
      // If there is no bundle string, just display the OID itself
      text.Append(oidname);

    text.AppendLiteral(SEPARATOR);
    oids++;
  }

  CERT_DestroyOidSequence(extKeyUsage);
  return NS_OK;
}

static nsresult
ProcessRDN(CERTRDN* rdn, nsAString &finalString, nsINSSComponent *nssComponent)
{
  nsresult rv;
  CERTAVA** avas;
  CERTAVA* ava;
  SECItem *decodeItem = nullptr;
  nsString avavalue;
  nsString type;
  nsAutoString temp;
  const char16_t *params[2];

  avas = rdn->avas;
  while ((ava = *avas++) != 0) {
    rv = GetOIDText(&ava->type, nssComponent, type);
    if (NS_FAILED(rv))
      return rv;
    
    //This function returns a string in UTF8 format.
    decodeItem = CERT_DecodeAVAValue(&ava->value);
    if(!decodeItem) {
      return NS_ERROR_FAILURE;
    }

    // We know we can fit buffer of this length. CERT_RFC1485_EscapeAndQuote
    // will fail if we provide smaller buffer then the result can fit to.
    int escapedValueCapacity = decodeItem->len * 3 + 3;
    UniquePtr<char[]> escapedValue = MakeUnique<char[]>(escapedValueCapacity);

    SECStatus status = CERT_RFC1485_EscapeAndQuote(
          escapedValue.get(),
          escapedValueCapacity, 
          (char*)decodeItem->data, 
          decodeItem->len);
    if (SECSuccess != status) {
      SECITEM_FreeItem(decodeItem, true);
      return NS_ERROR_FAILURE;
    }

    avavalue = NS_ConvertUTF8toUTF16(escapedValue.get());
    
    SECITEM_FreeItem(decodeItem, true);
    params[0] = type.get();
    params[1] = avavalue.get();
    nssComponent->PIPBundleFormatStringFromName("AVATemplate",
                                                  params, 2, temp);
    finalString += temp + NS_LITERAL_STRING("\n");
  }
  return NS_OK;
}

static nsresult
ProcessName(CERTName *name, nsINSSComponent *nssComponent, char16_t **value)
{
  CERTRDN** rdns;
  CERTRDN** rdn;
  nsString finalString;

  rdns = name->rdns;

  nsresult rv;
  CERTRDN **lastRdn;
  /* find last RDN */
  lastRdn = rdns;
  while (*lastRdn) lastRdn++;
  // The above whille loop will put us at the last member
  // of the array which is a nullptr pointer.  So let's back
  // up one spot so that we have the last non-nullptr entry in 
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
    rv = ProcessRDN(*rdn, finalString, nssComponent);
    if (NS_FAILED(rv))
      return rv;
  }
  *value = ToNewUnicode(finalString);    
  return NS_OK;
}

static nsresult
ProcessIA5String(SECItem  *extData, 
		 nsAString &text,
		 nsINSSComponent *nssComponent)
{
  SECItem item;
  nsAutoString local;
  if (SECSuccess != SEC_ASN1DecodeItem(nullptr, &item, 
				       SEC_ASN1_GET(SEC_IA5StringTemplate),
				       extData))
    return NS_ERROR_FAILURE;
  local.AssignASCII((char*)item.data, item.len);
  free(item.data);
  text.Append(local);
  return NS_OK;
}

static nsresult
AppendBMPtoUTF16(PLArenaPool *arena,
		 unsigned char* data, unsigned int len,
		 nsAString& text)
{
  unsigned int   utf8ValLen;
  unsigned char *utf8Val;

  if (len % 2 != 0)
    return NS_ERROR_FAILURE;

  /* XXX instead of converting to and from UTF-8, it would
     be sufficient to just swap bytes, or do nothing */
  utf8ValLen = len * 3 + 1;
  utf8Val = (unsigned char*)PORT_ArenaZAlloc(arena, utf8ValLen);
  if (!PORT_UCS2_UTF8Conversion(false, data, len,
				utf8Val, utf8ValLen, &utf8ValLen))
    return NS_ERROR_FAILURE;
  AppendUTF8toUTF16((char*)utf8Val, text);
  return NS_OK;
}

static nsresult
ProcessBMPString(SECItem  *extData, 
		 nsAString &text,
		 nsINSSComponent *nssComponent)
{
  SECItem item;
  PLArenaPool *arena;
  nsresult rv = NS_ERROR_FAILURE;
  
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_FAILURE;

  if (SECSuccess == SEC_ASN1DecodeItem(arena, &item, 
				       SEC_ASN1_GET(SEC_BMPStringTemplate),
				       extData))
    rv = AppendBMPtoUTF16(arena, item.data, item.len, text);
  PORT_FreeArena(arena, false);
  return rv;
}

static nsresult
ProcessGeneralName(PLArenaPool *arena,
		   CERTGeneralName *current,
		   nsAString &text,
		   nsINSSComponent *nssComponent)
{
  NS_ENSURE_ARG_POINTER(current);

  nsAutoString key;
  nsXPIDLString value;
  nsresult rv = NS_OK;

  switch (current->type) {
  case certOtherName: {
    SECOidTag oidTag = SECOID_FindOIDTag(&current->name.OthName.oid);
    if (oidTag == SEC_OID(MS_NT_PRINCIPAL_NAME)) {
	/* The type of this name is apparently nowhere explicitly
	   documented. However, in the generated templates, it is always
	   UTF-8. So try to decode this as UTF-8; if that fails, dump the
	   raw data. */
	SECItem decoded;
	nssComponent->GetPIPNSSBundleString("CertDumpMSNTPrincipal", key);
	if (SEC_ASN1DecodeItem(arena, &decoded, 
			       SEC_ASN1_GET(SEC_UTF8StringTemplate), 
			       &current->name.OthName.name) == SECSuccess) {
	  AppendUTF8toUTF16(nsAutoCString((char*)decoded.data, decoded.len),
			    value);
	} else {
	  ProcessRawBytes(nssComponent, &current->name.OthName.name, value);
	}
	break;
    } else if (oidTag == SEC_OID(MS_NTDS_REPLICATION)) {
	/* This should be a 16-byte GUID */
	SECItem guid;
	nssComponent->GetPIPNSSBundleString("CertDumpMSDomainGUID", key);
	if (SEC_ASN1DecodeItem(arena, &guid,
			       SEC_ASN1_GET(SEC_OctetStringTemplate),
			       &current->name.OthName.name) == SECSuccess
	    && guid.len == 16) {
	  char buf[40];
	  unsigned char *d = guid.data;
	  PR_snprintf(buf, sizeof(buf), 
		      "{%.2x%.2x%.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}",
		      d[3], d[2], d[1], d[0], d[5], d[4], d[7], d[6],
		      d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
	  value.AssignASCII(buf);
	} else {
	  ProcessRawBytes(nssComponent, &current->name.OthName.name, value);
	}
    } else {
      rv = GetDefaultOIDFormat(&current->name.OthName.oid, nssComponent, key, ' ');
      if (NS_FAILED(rv))
	goto finish;
      ProcessRawBytes(nssComponent, &current->name.OthName.name, value);
    }
    break;
  }
  case certRFC822Name:
    nssComponent->GetPIPNSSBundleString("CertDumpRFC822Name", key);
    value.AssignASCII((char*)current->name.other.data, current->name.other.len);
    break;
  case certDNSName:
    nssComponent->GetPIPNSSBundleString("CertDumpDNSName", key);
    value.AssignASCII((char*)current->name.other.data, current->name.other.len);
    break;
  case certX400Address:
    nssComponent->GetPIPNSSBundleString("CertDumpX400Address", key);
    ProcessRawBytes(nssComponent, &current->name.other, value);
    break;
  case certDirectoryName:
    nssComponent->GetPIPNSSBundleString("CertDumpDirectoryName", key);
    rv = ProcessName(&current->name.directoryName, nssComponent, 
		     getter_Copies(value));
    if (NS_FAILED(rv))
      goto finish;
    break;
  case certEDIPartyName:
    nssComponent->GetPIPNSSBundleString("CertDumpEDIPartyName", key);
    ProcessRawBytes(nssComponent, &current->name.other, value);
    break;
  case certURI:
    nssComponent->GetPIPNSSBundleString("CertDumpURI", key);
    value.AssignASCII((char*)current->name.other.data, current->name.other.len);
    break;
  case certIPAddress:
    {
      char buf[INET6_ADDRSTRLEN];
      PRStatus status = PR_FAILURE;
      PRNetAddr addr;
      memset(&addr, 0, sizeof(addr));
      nssComponent->GetPIPNSSBundleString("CertDumpIPAddress", key);
      if (current->name.other.len == 4) {
        addr.inet.family = PR_AF_INET;
        memcpy(&addr.inet.ip, current->name.other.data, current->name.other.len);
        status = PR_NetAddrToString(&addr, buf, sizeof(buf));
      } else if (current->name.other.len == 16) {
        addr.ipv6.family = PR_AF_INET6;
        memcpy(&addr.ipv6.ip, current->name.other.data, current->name.other.len);
        status = PR_NetAddrToString(&addr, buf, sizeof(buf));
      }
      if (status == PR_SUCCESS) {
        value.AssignASCII(buf);
      } else {
        /* invalid IP address */
        ProcessRawBytes(nssComponent, &current->name.other, value);
      }
      break;
    }
  case certRegisterID:
    nssComponent->GetPIPNSSBundleString("CertDumpRegisterID", key);
    rv = GetDefaultOIDFormat(&current->name.other, nssComponent, value, '.');
    if (NS_FAILED(rv))
      goto finish;
    break;
  }
  text.Append(key);
  text.AppendLiteral(": ");
  text.Append(value);
  text.AppendLiteral(SEPARATOR);
 finish:
    return rv;
}

static nsresult
ProcessGeneralNames(PLArenaPool *arena,
		    CERTGeneralName *nameList,
		    nsAString &text,
		    nsINSSComponent *nssComponent)
{
  CERTGeneralName *current = nameList;
  nsresult rv;

  do {
    rv = ProcessGeneralName(arena, current, text, nssComponent);
    if (NS_FAILED(rv))
      break;
    current = CERT_GetNextGeneralName(current);
  } while (current != nameList);
  return rv;
}

static nsresult
ProcessAltName(SECItem  *extData, 
	       nsAString &text,
	       nsINSSComponent *nssComponent)
{
  nsresult rv = NS_OK;
  PLArenaPool *arena;
  CERTGeneralName *nameList;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_FAILURE;

  nameList = CERT_DecodeAltNameExtension(arena, extData);
  if (!nameList)
    goto finish;

  rv = ProcessGeneralNames(arena, nameList, text, nssComponent);

 finish:
  PORT_FreeArena(arena, false);
  return rv;
}

static nsresult
ProcessSubjectKeyId(SECItem  *extData, 
		    nsAString &text,
		    nsINSSComponent *nssComponent)
{
  PLArenaPool *arena;
  nsresult rv = NS_OK;
  SECItem decoded;
  nsAutoString local;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_FAILURE;

  if (SEC_QuickDERDecodeItem(arena, &decoded, 
			     SEC_ASN1_GET(SEC_OctetStringTemplate), 
			     extData) != SECSuccess) {
    rv = NS_ERROR_FAILURE;
    goto finish;
  }
  
  nssComponent->GetPIPNSSBundleString("CertDumpKeyID", local);
  text.Append(local);
  text.AppendLiteral(": ");
  ProcessRawBytes(nssComponent, &decoded, text);

 finish:
  PORT_FreeArena(arena, false);
  return rv;
}

static nsresult
ProcessAuthKeyId(SECItem  *extData, 
		 nsAString &text,
		 nsINSSComponent *nssComponent)
{
  CERTAuthKeyID *ret;
  PLArenaPool *arena;
  nsresult rv = NS_OK;
  nsAutoString local;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  ret = CERT_DecodeAuthKeyID (arena, extData);
  if (!ret) {
    rv = NS_ERROR_FAILURE;
    goto finish;
  }

  if (ret->keyID.len > 0) {
    nssComponent->GetPIPNSSBundleString("CertDumpKeyID", local);
    text.Append(local);
    text.AppendLiteral(": ");
    ProcessRawBytes(nssComponent, &ret->keyID, text);
    text.AppendLiteral(SEPARATOR);
  }

  if (ret->authCertIssuer) {
    nssComponent->GetPIPNSSBundleString("CertDumpIssuer", local);
    text.Append(local);
    text.AppendLiteral(": ");
    rv = ProcessGeneralNames(arena, ret->authCertIssuer, text, nssComponent);
    if (NS_FAILED(rv))
      goto finish;
  }

  if (ret->authCertSerialNumber.len > 0) {
    nssComponent->GetPIPNSSBundleString("CertDumpSerialNo", local);
    text.Append(local);
    text.AppendLiteral(": ");
    ProcessRawBytes(nssComponent, &ret->authCertSerialNumber, text);
  }

 finish:
  PORT_FreeArena(arena, false);
  return rv;
}

static nsresult
ProcessUserNotice(SECItem *der_notice,
		  nsAString &text,
		  nsINSSComponent *nssComponent)
{
  CERTUserNotice *notice = nullptr;
  SECItem **itemList;
  PLArenaPool *arena;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_FAILURE;

  notice = CERT_DecodeUserNotice(der_notice);
  if (!notice) {
    ProcessRawBytes(nssComponent, der_notice, text);
    goto finish;
  }

  if (notice->noticeReference.organization.len != 0) {
    switch (notice->noticeReference.organization.type) {
    case siAsciiString:
    case siVisibleString:
    case siUTF8String:
      text.Append(NS_ConvertUTF8toUTF16(
                  (const char *)notice->noticeReference.organization.data,
                  notice->noticeReference.organization.len));
      break;
    case siBMPString:
      AppendBMPtoUTF16(arena, notice->noticeReference.organization.data,
                       notice->noticeReference.organization.len, text);
      break;
    default:
      break;
    }
    text.AppendLiteral(" - ");
    itemList = notice->noticeReference.noticeNumbers;
    while (*itemList) {
      unsigned long number;
      char buffer[60];
      if (SEC_ASN1DecodeInteger(*itemList, &number) == SECSuccess) {
        PR_snprintf(buffer, sizeof(buffer), "#%d", number);
        if (itemList != notice->noticeReference.noticeNumbers)
          text.AppendLiteral(", ");
        AppendASCIItoUTF16(buffer, text);
      }
      itemList++;
    }
  }
  if (notice->displayText.len != 0) {
    text.AppendLiteral(SEPARATOR);
    text.AppendLiteral("    ");
    switch (notice->displayText.type) {
    case siAsciiString:
    case siVisibleString:
    case siUTF8String:
      text.Append(NS_ConvertUTF8toUTF16((const char *)notice->displayText.data,
                                        notice->displayText.len));
      break;
    case siBMPString:
      AppendBMPtoUTF16(arena, notice->displayText.data, notice->displayText.len,
		       text);
      break;
    default:
      break;
    }
  }
 finish:
  if (notice)
    CERT_DestroyUserNotice(notice);
  PORT_FreeArena(arena, false);
  return NS_OK;
}

static nsresult
ProcessCertificatePolicies(SECItem  *extData, 
			   nsAString &text,
                           SECOidTag ev_oid_tag, // SEC_OID_UNKNOWN means: not EV
			   nsINSSComponent *nssComponent)
{
  CERTCertificatePolicies *policies;
  CERTPolicyInfo **policyInfos, *policyInfo;
  CERTPolicyQualifier **policyQualifiers, *policyQualifier;
  nsAutoString local;
  nsresult rv = NS_OK;

  policies = CERT_DecodeCertificatePoliciesExtension(extData);
  if (!policies)
    return NS_ERROR_FAILURE;

  policyInfos = policies->policyInfos;
  while (*policyInfos) {
    policyInfo = *policyInfos++;
    switch (policyInfo->oid) {
    case SEC_OID_VERISIGN_USER_NOTICES:
      nssComponent->GetPIPNSSBundleString("CertDumpVerisignNotices", local);
      text.Append(local);
      break;
    default:
      GetDefaultOIDFormat(&policyInfo->policyID, nssComponent, local, '.');
      text.Append(local);
    }

    bool needColon = true;
    if (ev_oid_tag != SEC_OID_UNKNOWN) {
      // This is an EV cert. Let's see if this oid is the EV oid,
      // because we want to display the EV information string
      // next to the correct OID.

      if (policyInfo->oid == ev_oid_tag) {
        text.Append(':');
        text.AppendLiteral(SEPARATOR);
        needColon = false;
        nssComponent->GetPIPNSSBundleString("CertDumpPolicyOidEV", local);
        text.Append(local);
      }
    }

    if (policyInfo->policyQualifiers) {
      /* Add all qualifiers on separate lines, indented */
      policyQualifiers = policyInfo->policyQualifiers;
      if (needColon)
        text.Append(':');
      text.AppendLiteral(SEPARATOR);
      while (*policyQualifiers) {
	text.AppendLiteral("  ");
	policyQualifier = *policyQualifiers++;
	switch(policyQualifier->oid) {
	case SEC_OID_PKIX_CPS_POINTER_QUALIFIER:
	  nssComponent->GetPIPNSSBundleString("CertDumpCPSPointer", local);
	  text.Append(local);
	  text.Append(':');
	  text.AppendLiteral(SEPARATOR);
	  text.AppendLiteral("    ");
	  /* The CPS pointer ought to be the cPSuri alternative
	     of the Qualifier choice. */
	  rv = ProcessIA5String(&policyQualifier->qualifierValue,
				text, nssComponent);
	  if (NS_FAILED(rv))
	    goto finish;
	  break;
	case SEC_OID_PKIX_USER_NOTICE_QUALIFIER:
	  nssComponent->GetPIPNSSBundleString("CertDumpUserNotice", local);
	  text.Append(local);
	  text.AppendLiteral(": ");
	  rv = ProcessUserNotice(&policyQualifier->qualifierValue,
				 text, nssComponent);
	  break;
	default:
	  GetDefaultOIDFormat(&policyQualifier->qualifierID, nssComponent, local, '.');
	  text.Append(local);
	  text.AppendLiteral(": ");
	  ProcessRawBytes(nssComponent, &policyQualifier->qualifierValue, text);
	}
	text.AppendLiteral(SEPARATOR);
      } /* while policyQualifiers */
    } /* if policyQualifiers */
    text.AppendLiteral(SEPARATOR);
  }

 finish:
  CERT_DestroyCertificatePoliciesExtension(policies);
  return rv;
}

static nsresult
ProcessCrlDistPoints(SECItem  *extData, 
		     nsAString &text,
		     nsINSSComponent *nssComponent)
{
  CERTCrlDistributionPoints *crldp;
  CRLDistributionPoint **points, *point;
  PLArenaPool *arena;
  nsresult rv = NS_OK;
  nsAutoString local;
  int reasons, comma;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_FAILURE;

  crldp = CERT_DecodeCRLDistributionPoints(arena, extData);
  if (!crldp || !crldp->distPoints) {
    rv = NS_ERROR_FAILURE;
    goto finish;
  }

  for(points = crldp->distPoints; *points; points++) {
    point = *points;
    switch (point->distPointType) {
    case generalName:
      rv = ProcessGeneralName(arena, point->distPoint.fullName,
			      text, nssComponent);
      if (NS_FAILED(rv))
	goto finish;
      break;
    case relativeDistinguishedName:
      rv = ProcessRDN(&point->distPoint.relativeName, 
		      text, nssComponent);
      if (NS_FAILED(rv))
	goto finish;
      break;
    }
    if (point->reasons.len) { 
      reasons = point->reasons.data[0];
      text.Append(' ');
      comma = 0;
      if (reasons & RF_UNUSED) {
	nssComponent->GetPIPNSSBundleString("CertDumpUnused", local);
	text.Append(local); comma = 1;
      }
      if (reasons & RF_KEY_COMPROMISE) {
	if (comma) text.AppendLiteral(", ");
	nssComponent->GetPIPNSSBundleString("CertDumpKeyCompromise", local);
	text.Append(local); comma = 1;
      }
      if (reasons & RF_CA_COMPROMISE) {
	if (comma) text.AppendLiteral(", ");
	nssComponent->GetPIPNSSBundleString("CertDumpCACompromise", local);
	text.Append(local); comma = 1;
      }
      if (reasons & RF_AFFILIATION_CHANGED) {
	if (comma) text.AppendLiteral(", ");
	nssComponent->GetPIPNSSBundleString("CertDumpAffiliationChanged", local);
	text.Append(local); comma = 1;
      }
      if (reasons & RF_SUPERSEDED) {
	if (comma) text.AppendLiteral(", ");
	nssComponent->GetPIPNSSBundleString("CertDumpSuperseded", local);
	text.Append(local); comma = 1;
      }
      if (reasons & RF_CESSATION_OF_OPERATION) {
	if (comma) text.AppendLiteral(", ");
	nssComponent->GetPIPNSSBundleString("CertDumpCessation", local);
	text.Append(local); comma = 1;
      }
      if (reasons & RF_CERTIFICATE_HOLD) {
	if (comma) text.AppendLiteral(", ");
	nssComponent->GetPIPNSSBundleString("CertDumpHold", local);
	text.Append(local); comma = 1;
      }
      text.AppendLiteral(SEPARATOR);
    }
    if (point->crlIssuer) {
      nssComponent->GetPIPNSSBundleString("CertDumpIssuer", local);
      text.Append(local);
      text.AppendLiteral(": ");
      rv = ProcessGeneralNames(arena, point->crlIssuer,
			       text, nssComponent);
      if (NS_FAILED(rv))
	goto finish;
    }
  }
  
 finish:
  PORT_FreeArena(arena, false);
  return NS_OK;
}

static nsresult
ProcessAuthInfoAccess(SECItem  *extData, 
		      nsAString &text,
		      nsINSSComponent *nssComponent)
{
  CERTAuthInfoAccess **aia, *desc;
  PLArenaPool *arena;
  nsresult rv = NS_OK;
  nsAutoString local;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_FAILURE;

  aia = CERT_DecodeAuthInfoAccessExtension(arena, extData);
  if (!aia)
    goto finish;

  while (*aia) {
    desc = *aia++;
    switch (SECOID_FindOIDTag(&desc->method)) {
    case SEC_OID_PKIX_OCSP:
      nssComponent->GetPIPNSSBundleString("CertDumpOCSPResponder", local);
      break;
    case SEC_OID_PKIX_CA_ISSUERS:
      nssComponent->GetPIPNSSBundleString("CertDumpCAIssuers", local);
      break;
    default:
      rv = GetDefaultOIDFormat(&desc->method, nssComponent, local, '.');
      if (NS_FAILED(rv))
	goto finish;
    }
    text.Append(local);
    text.AppendLiteral(": ");
    rv = ProcessGeneralName(arena, desc->location, text, nssComponent);
    if (NS_FAILED(rv))
      goto finish;
  }

 finish:
  PORT_FreeArena(arena, false);
  return rv;
}

static nsresult
ProcessMSCAVersion(SECItem  *extData, 
		   nsAString &text,
		   nsINSSComponent *nssComponent)
{
  unsigned long version;
  nsresult rv;
  char buf[50];
  SECItem decoded;

  if (SECSuccess != SEC_ASN1DecodeItem(nullptr, &decoded, 
				       SEC_ASN1_GET(SEC_IntegerTemplate), 
				       extData))
    /* This extension used to be an Integer when this code
       was written, but apparently isn't anymore. Display
       the raw bytes instead. */
    return ProcessRawBytes(nssComponent, extData, text);

  rv = GetIntValue(&decoded, &version);
  free(decoded.data);
  if (NS_FAILED(rv))
    /* Value out of range, display raw bytes */
    return ProcessRawBytes(nssComponent, extData, text);

  /* Apparently, the encoding is <minor><major>, with 16 bits each */
  PR_snprintf(buf, sizeof(buf), "%d.%d", version & 0xFFFF, version>>16);
  text.AppendASCII(buf);
  return NS_OK;
}

static nsresult
ProcessExtensionData(SECOidTag oidTag, SECItem *extData, 
                     nsAString &text, 
                     SECOidTag ev_oid_tag, // SEC_OID_UNKNOWN means: not EV
                     nsINSSComponent *nssComponent)
{
  nsresult rv;
  switch (oidTag) {
  case SEC_OID_X509_KEY_USAGE:
    rv = ProcessKeyUsageExtension(extData, text, nssComponent);
    break;
  case SEC_OID_X509_BASIC_CONSTRAINTS:
    rv = ProcessBasicConstraints(extData, text, nssComponent);
    break;
  case SEC_OID_X509_EXT_KEY_USAGE:
    rv = ProcessExtKeyUsage(extData, text, nssComponent);
    break;
  case SEC_OID_X509_ISSUER_ALT_NAME:
  case SEC_OID_X509_SUBJECT_ALT_NAME:
    rv = ProcessAltName(extData, text, nssComponent);
    break;
  case SEC_OID_X509_SUBJECT_KEY_ID:
    rv = ProcessSubjectKeyId(extData, text, nssComponent);
    break;
  case SEC_OID_X509_AUTH_KEY_ID:
    rv = ProcessAuthKeyId(extData, text, nssComponent);
    break;
  case SEC_OID_X509_CERTIFICATE_POLICIES:
    rv = ProcessCertificatePolicies(extData, text, ev_oid_tag, nssComponent);
    break;
  case SEC_OID_X509_CRL_DIST_POINTS:
    rv = ProcessCrlDistPoints(extData, text, nssComponent);
    break;
  case SEC_OID_X509_AUTH_INFO_ACCESS:
    rv = ProcessAuthInfoAccess(extData, text, nssComponent);
    break;
  default:
    if (oidTag == SEC_OID(MS_CERT_EXT_CERTTYPE)) {
      rv = ProcessBMPString(extData, text, nssComponent);
      break;
    }
    if (oidTag == SEC_OID(MS_CERTSERV_CA_VERSION)) {
      rv = ProcessMSCAVersion(extData, text, nssComponent);
      break;
    }
    rv = ProcessRawBytes(nssComponent, extData, text);
    break; 
  }
  return rv;
}

static nsresult
ProcessSingleExtension(CERTCertExtension *extension,
                       SECOidTag ev_oid_tag, // SEC_OID_UNKNOWN means: not EV
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retExtension)
{
  nsAutoString text, extvalue;
  GetOIDText(&extension->id, nssComponent, text);
  nsCOMPtr<nsIASN1PrintableItem>extensionItem = new nsNSSASN1PrintableItem();

  extensionItem->SetDisplayName(text);
  SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);
  text.Truncate();
  if (extension->critical.data) {
    if (extension->critical.data[0]) {
      nssComponent->GetPIPNSSBundleString("CertDumpCritical", text);
    } else {
      nssComponent->GetPIPNSSBundleString("CertDumpNonCritical", text);
    }
  } else {
    nssComponent->GetPIPNSSBundleString("CertDumpNonCritical", text);
  }
  text.AppendLiteral(SEPARATOR);
  nsresult rv = ProcessExtensionData(oidTag, &extension->value, extvalue, 
                                     ev_oid_tag, nssComponent);
  if (NS_FAILED(rv)) {
    extvalue.Truncate();
    rv = ProcessRawBytes(nssComponent, &extension->value, extvalue, false);
  }
  text.Append(extvalue);

  extensionItem->SetDisplayValue(text);
  extensionItem.forget(retExtension);
  return NS_OK;
}

static nsresult
ProcessSECAlgorithmID(SECAlgorithmID *algID,
                      nsINSSComponent *nssComponent,
                      nsIASN1Sequence **retSequence)
{
  SECOidTag algOIDTag = SECOID_FindOIDTag(&algID->algorithm);
  SECItem paramsOID = { siBuffer, nullptr, 0 };
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();

  *retSequence = nullptr;
  nsString text;
  GetOIDText(&algID->algorithm, nssComponent, text);
  if (!algID->parameters.len || algID->parameters.data[0] == nsIASN1Object::ASN1_NULL) {
    sequence->SetDisplayValue(text);
    sequence->SetIsValidContainer(false);
  } else {
    nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

    printableItem->SetDisplayValue(text);
    nsCOMPtr<nsIMutableArray> asn1Objects;
    sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
    asn1Objects->AppendElement(printableItem, false);
    nssComponent->GetPIPNSSBundleString("CertDumpAlgID", text);
    printableItem->SetDisplayName(text);

    printableItem = new nsNSSASN1PrintableItem();

    asn1Objects->AppendElement(printableItem, false);
    nssComponent->GetPIPNSSBundleString("CertDumpParams", text);
    printableItem->SetDisplayName(text);
    if ((algOIDTag == SEC_OID_ANSIX962_EC_PUBLIC_KEY) &&
        (algID->parameters.len > 2) && 
        (algID->parameters.data[0] == nsIASN1Object::ASN1_OBJECT_ID)) {
       paramsOID.len = algID->parameters.len - 2;
       paramsOID.data = algID->parameters.data + 2;
       GetOIDText(&paramsOID, nssComponent, text);
    } else {
       ProcessRawBytes(nssComponent, &algID->parameters,text);
    }
    printableItem->SetDisplayValue(text);
  }
  sequence.forget(retSequence);
  return NS_OK;
}

static nsresult
ProcessTime(PRTime dispTime, const char16_t* displayName,
            nsIASN1Sequence* parentSequence)
{
  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsString text;
  nsString tempString;

  PRExplodedTime explodedTime;
  PR_ExplodeTime(dispTime, PR_LocalTimeParameters, &explodedTime);

  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatLong,
                                      kTimeFormatSeconds, &explodedTime,
                                      tempString);

  text.Append(tempString);
  text.AppendLiteral("\n(");

  PRExplodedTime explodedTimeGMT;
  PR_ExplodeTime(dispTime, PR_GMTParameters, &explodedTimeGMT);

  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatLong,
                                      kTimeFormatSeconds, &explodedTimeGMT,
                                      tempString);

  text.Append(tempString);
  text.AppendLiteral(" GMT)");

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  printableItem->SetDisplayValue(text);
  printableItem->SetDisplayName(nsDependentString(displayName));
  nsCOMPtr<nsIMutableArray> asn1Objects;
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(printableItem, false);
  return NS_OK;
}

static nsresult
ProcessSubjectPublicKeyInfo(CERTSubjectPublicKeyInfo *spki, 
                            nsIASN1Sequence *parentSequence,
                            nsINSSComponent *nssComponent)
{
  nsCOMPtr<nsIASN1Sequence> spkiSequence = new nsNSSASN1Sequence();

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
  asn1Objects->AppendElement(sequenceItem, false);

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  text.Truncate();
 
  SECKEYPublicKey *key = SECKEY_ExtractPublicKey(spki);
  bool displayed = false;
  if (key) {
      switch (key->keyType) {
      case rsaKey: {
         displayed = true;
         nsAutoString length1, length2, data1, data2;
         length1.AppendInt(key->u.rsa.modulus.len * 8);
         length2.AppendInt(key->u.rsa.publicExponent.len * 8);
         ProcessRawBytes(nssComponent, &key->u.rsa.modulus, data1, 
                         false);
         ProcessRawBytes(nssComponent, &key->u.rsa.publicExponent, data2,
                         false);
         const char16_t *params[4] = {length1.get(), data1.get(), 
                                       length2.get(), data2.get()};
         nssComponent->PIPBundleFormatStringFromName("CertDumpRSATemplate",
                                                     params, 4, text);
         break;
      }
      case ecKey: {
        displayed = true;
        SECKEYECPublicKey &ecpk = key->u.ec;
        int fieldSizeLenAsBits = 
              SECKEY_ECParamsToKeySize(&ecpk.DEREncodedParams);
        int basePointOrderLenAsBits = 
              SECKEY_ECParamsToBasePointOrderLen(&ecpk.DEREncodedParams);
        nsAutoString s_fsl, s_bpol, s_pv;
        s_fsl.AppendInt(fieldSizeLenAsBits);
        s_bpol.AppendInt(basePointOrderLenAsBits);

        if (ecpk.publicValue.len > 4) {
          ProcessRawBytes(nssComponent, &ecpk.publicValue, s_pv, false);
        } else {
          int i_pv = DER_GetInteger(&ecpk.publicValue);
          s_pv.AppendInt(i_pv);
        }
        const char16_t *params[] = {s_fsl.get(), s_bpol.get(), s_pv.get()};
        nssComponent->PIPBundleFormatStringFromName("CertDumpECTemplate",
                                                    params, 3, text);
        break;
      }
      default:
         /* Algorithm unknown, or too rarely used to bother displaying it */
         break;
      }
      SECKEY_DestroyPublicKey (key);
  }
  if (!displayed) {
      // Algorithm unknown, display raw bytes
      // The subjectPublicKey field is encoded as a bit string.
      // ProcessRawBytes expects the length to be in bytes, so 
      // let's convert the lenght into a temporary SECItem.
      SECItem data;
      data.data = spki->subjectPublicKey.data;
      data.len  = spki->subjectPublicKey.len / 8;
      ProcessRawBytes(nssComponent, &data, text);
  
  }
 
  printableItem->SetDisplayValue(text);
  nssComponent->GetPIPNSSBundleString("CertDumpSubjPubKey", text);
  printableItem->SetDisplayName(text);
  asn1Objects->AppendElement(printableItem, false);
  
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(spkiSequence, false);
  return NS_OK;
}

static nsresult
ProcessExtensions(CERTCertExtension **extensions, 
                  nsIASN1Sequence *parentSequence,
                  SECOidTag ev_oid_tag, // SEC_OID_UNKNOWN means: not EV
                  nsINSSComponent *nssComponent)
{
  nsCOMPtr<nsIASN1Sequence> extensionSequence = new nsNSSASN1Sequence;

  nsString text;
  nssComponent->GetPIPNSSBundleString("CertDumpExtensions", text);
  extensionSequence->SetDisplayName(text);
  int32_t i;
  nsresult rv;
  nsCOMPtr<nsIASN1PrintableItem> newExtension;
  nsCOMPtr<nsIMutableArray> asn1Objects;
  extensionSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  for (i=0; extensions[i] != nullptr; i++) {
    rv = ProcessSingleExtension(extensions[i], 
                                ev_oid_tag,
                                nssComponent,
                                getter_AddRefs(newExtension));
    if (NS_FAILED(rv))
      return rv;

    asn1Objects->AppendElement(newExtension, false);
  }
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(extensionSequence, false);
  return NS_OK;
}

static bool registered;
static SECStatus RegisterDynamicOids()
{
  unsigned int i;
  SECStatus rv = SECSuccess;

  if (registered)
    return rv;

  for (i = 0; i < numOids; i++) {
    SECOidTag tag = SECOID_AddEntry(&more_oids[i]);
    if (tag == SEC_OID_UNKNOWN) {
      rv = SECFailure;
      continue;
    }
    more_oids[i].offset = tag;
  }
  registered = true;
  return rv;
}

nsresult
nsNSSCertificate::CreateTBSCertificateASN1Struct(nsIASN1Sequence **retSequence,
                                                 nsINSSComponent *nssComponent)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (RegisterDynamicOids() != SECSuccess)
    return NS_ERROR_FAILURE;

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

  asn1Objects->AppendElement(printableItem, false);
  
  rv = ProcessSerialNumberDER(&mCert->serialNumber, nssComponent,
                              getter_AddRefs(printableItem));

  if (NS_FAILED(rv))
    return rv;
  asn1Objects->AppendElement(printableItem, false);

  nsCOMPtr<nsIASN1Sequence> algID;
  rv = ProcessSECAlgorithmID(&mCert->signature,
                             nssComponent, getter_AddRefs(algID));
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString("CertDumpSigAlg", text);
  algID->SetDisplayName(text);
  asn1Objects->AppendElement(algID, false);

  nsXPIDLString value;
  ProcessName(&mCert->issuer, nssComponent, getter_Copies(value));

  printableItem = new nsNSSASN1PrintableItem();

  printableItem->SetDisplayValue(value);
  nssComponent->GetPIPNSSBundleString("CertDumpIssuer", text);
  printableItem->SetDisplayName(text);
  asn1Objects->AppendElement(printableItem, false);
  
  nsCOMPtr<nsIASN1Sequence> validitySequence = new nsNSSASN1Sequence();
  nssComponent->GetPIPNSSBundleString("CertDumpValidity", text);
  validitySequence->SetDisplayName(text);
  asn1Objects->AppendElement(validitySequence, false);
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

  printableItem->SetDisplayName(text);
  ProcessName(&mCert->subject, nssComponent,getter_Copies(value));
  printableItem->SetDisplayValue(value);
  asn1Objects->AppendElement(printableItem, false);

  rv = ProcessSubjectPublicKeyInfo(&mCert->subjectPublicKeyInfo, sequence,
                                   nssComponent); 
  if (NS_FAILED(rv))
    return rv;
 
  SECItem data; 
  // Is there an issuerUniqueID?
  if (mCert->issuerID.data) {
    // The issuerID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->issuerID.data;
    data.len  = (mCert->issuerID.len + 7) / 8;

    ProcessRawBytes(nssComponent, &data, text);
    printableItem = new nsNSSASN1PrintableItem();

    printableItem->SetDisplayValue(text);
    nssComponent->GetPIPNSSBundleString("CertDumpIssuerUniqueID", text);
    printableItem->SetDisplayName(text);
    asn1Objects->AppendElement(printableItem, false);
  }

  if (mCert->subjectID.data) {
    // The subjectID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->subjectID.data;
    data.len  = (mCert->subjectID.len + 7) / 8;

    ProcessRawBytes(nssComponent, &data, text);
    printableItem = new nsNSSASN1PrintableItem();

    printableItem->SetDisplayValue(text);
    nssComponent->GetPIPNSSBundleString("CertDumpSubjectUniqueID", text);
    printableItem->SetDisplayName(text);
    asn1Objects->AppendElement(printableItem, false);

  }
  if (mCert->extensions) {
    SECOidTag ev_oid_tag = SEC_OID_UNKNOWN;

#ifndef MOZ_NO_EV_CERTS
    bool validEV;
    rv = hasValidEVOidTag(ev_oid_tag, validEV);
    if (NS_FAILED(rv))
      return rv;

    if (!validEV)
      ev_oid_tag = SEC_OID_UNKNOWN;
#endif

    rv = ProcessExtensions(mCert->extensions, sequence, ev_oid_tag, nssComponent);
    if (NS_FAILED(rv))
      return rv;
  }
  sequence.forget(retSequence);
  return NS_OK;
}

nsresult
nsNSSCertificate::CreateASN1Struct(nsIASN1Object** aRetVal)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();

  nsCOMPtr<nsIMutableArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));

  nsAutoString title;
  nsresult rv = GetWindowTitle(title);
  if (NS_FAILED(rv)) {
    return rv;
  }

  sequence->SetDisplayName(title);
  sequence.forget(aRetVal);

  // This sequence will be contain the tbsCertificate, signatureAlgorithm,
  // and signatureValue.
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  rv = CreateTBSCertificateASN1Struct(getter_AddRefs(sequence),
                                      nssComponent);
  if (NS_FAILED(rv))
    return rv;

  asn1Objects->AppendElement(sequence, false);
  nsCOMPtr<nsIASN1Sequence> algID;

  rv = ProcessSECAlgorithmID(&mCert->signatureWrap.signatureAlgorithm, 
                             nssComponent, getter_AddRefs(algID));
  if (NS_FAILED(rv))
    return rv;
  nsString text;
  nssComponent->GetPIPNSSBundleString("CertDumpSigAlg", text);
  algID->SetDisplayName(text);
  asn1Objects->AppendElement(algID, false);
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
  ProcessRawBytes(nssComponent, &temp,text);
  printableItem->SetDisplayValue(text);
  asn1Objects->AppendElement(printableItem, false);
  return NS_OK;
}

uint32_t 
getCertType(CERTCertificate *cert)
{
  nsNSSCertTrust trust(cert->trust);
  if (cert->nickname && trust.HasAnyUser())
    return nsIX509Cert::USER_CERT;
  if (trust.HasAnyCA())
    return nsIX509Cert::CA_CERT;
  if (trust.HasPeer(true, false, false))
    return nsIX509Cert::SERVER_CERT;
  if (trust.HasPeer(false, true, false) && cert->emailAddr)
    return nsIX509Cert::EMAIL_CERT;
  if (CERT_IsCACert(cert, nullptr))
    return nsIX509Cert::CA_CERT;
  if (cert->emailAddr)
    return nsIX509Cert::EMAIL_CERT;
  return nsIX509Cert::UNKNOWN_CERT;
}

CERTCertNicknames *
getNSSCertNicknamesFromCertList(CERTCertList *certList)
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsresult rv;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return nullptr;

  nsAutoString expiredString, notYetValidString;
  nsAutoString expiredStringLeadingSpace, notYetValidStringLeadingSpace;

  nssComponent->GetPIPNSSBundleString("NicknameExpired", expiredString);
  nssComponent->GetPIPNSSBundleString("NicknameNotYetValid", notYetValidString);

  expiredStringLeadingSpace.Append(' ');
  expiredStringLeadingSpace.Append(expiredString);

  notYetValidStringLeadingSpace.Append(' ');
  notYetValidStringLeadingSpace.Append(notYetValidString);

  NS_ConvertUTF16toUTF8 aUtf8ExpiredString(expiredStringLeadingSpace);
  NS_ConvertUTF16toUTF8 aUtf8NotYetValidString(notYetValidStringLeadingSpace);

  return CERT_NicknameStringsFromCertList(certList,
                                          const_cast<char*>(aUtf8ExpiredString.get()),
                                          const_cast<char*>(aUtf8NotYetValidString.get()));
  
}

nsresult
GetCertFingerprintByOidTag(CERTCertificate* nsscert,
                           SECOidTag aOidTag, 
                           nsCString &fp)
{
  Digest digest;
  nsresult rv = digest.DigestBuf(aOidTag, nsscert->derCert.data,
                                 nsscert->derCert.len);
  NS_ENSURE_SUCCESS(rv, rv);

  char *tmpstr = CERT_Hexify(const_cast<SECItem*>(&digest.get()), 1);
  NS_ENSURE_TRUE(tmpstr, NS_ERROR_OUT_OF_MEMORY);

  fp.Assign(tmpstr);
  PORT_Free(tmpstr);
  return NS_OK;
}
