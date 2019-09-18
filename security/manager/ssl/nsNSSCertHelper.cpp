/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertHelper.h"

#include <algorithm>

#include "DateTimeFormat.h"
#include "ScopedNSSTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/NotNull.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Utf8.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsNSSASN1Object.h"
#include "nsNSSCertValidity.h"
#include "nsNSSCertificate.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prerror.h"
#include "prnetdb.h"
#include "secder.h"

using namespace mozilla;

// To avoid relying on localized strings in PSM, we hard-code the root module
// name internally. When we display it to the user in the list of modules in the
// front-end, we look up the localized value and display that instead of this.
const char* kRootModuleName = "Builtin Roots Module";
const size_t kRootModuleNameLen = strlen(kRootModuleName);

static nsresult GetPIPNSSBundle(nsIStringBundle** pipnssBundle) {
  nsCOMPtr<nsIStringBundleService> bundleService(
      do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (!bundleService) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return bundleService->CreateBundle("chrome://pipnss/locale/pipnss.properties",
                                     pipnssBundle);
}

nsresult GetPIPNSSBundleString(const char* stringName, nsAString& result) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  MOZ_ASSERT(stringName);
  if (!stringName) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIStringBundle> pipnssBundle;
  nsresult rv = GetPIPNSSBundle(getter_AddRefs(pipnssBundle));
  if (NS_FAILED(rv)) {
    return rv;
  }
  result.Truncate();
  return pipnssBundle->GetStringFromName(stringName, result);
}

nsresult GetPIPNSSBundleString(const char* stringName, nsACString& result) {
  nsAutoString tmp;
  nsresult rv = GetPIPNSSBundleString(stringName, tmp);
  if (NS_FAILED(rv)) {
    return rv;
  }
  result.Assign(NS_ConvertUTF16toUTF8(tmp));
  return NS_OK;
}

nsresult PIPBundleFormatStringFromName(const char* stringName,
                                       const nsTArray<nsString>& params,
                                       nsAString& result) {
  MOZ_ASSERT(stringName);
  MOZ_ASSERT(!params.IsEmpty());
  if (!stringName || params.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIStringBundle> pipnssBundle;
  nsresult rv = GetPIPNSSBundle(getter_AddRefs(pipnssBundle));
  if (NS_FAILED(rv)) {
    return rv;
  }
  result.Truncate();
  return pipnssBundle->FormatStringFromName(stringName, params, result);
}

static nsresult ProcessVersion(SECItem* versionItem,
                               nsIASN1PrintableItem** retItem) {
  nsAutoString text;
  GetPIPNSSBundleString("CertDumpVersion", text);
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  nsresult rv = printableItem->SetDisplayName(text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Now to figure out what version this certificate is.
  unsigned int version;
  if (versionItem->data) {
    // Filter out totally bogus version values/encodings.
    if (versionItem->len != 1) {
      return NS_ERROR_FAILURE;
    }
    version = *BitwiseCast<uint8_t*, unsigned char*>(versionItem->data);
  } else {
    // If there is no version present in the cert, then RFC 5280 says we
    // default to v1 (0).
    version = 0;
  }

  // A value of n actually corresponds to version n + 1
  AutoTArray<nsString, 1> params;
  params.AppendElement()->AppendInt(version + 1);
  rv = PIPBundleFormatStringFromName("CertDumpVersionValue", params, text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = printableItem->SetDisplayValue(text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  printableItem.forget(retItem);
  return NS_OK;
}

static nsresult ProcessSerialNumberDER(
    const SECItem& serialItem,
    /*out*/ nsCOMPtr<nsIASN1PrintableItem>& retItem) {
  nsAutoString text;
  nsresult rv = GetPIPNSSBundleString("CertDumpSerialNo", text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  rv = printableItem->SetDisplayName(text);
  if (NS_FAILED(rv)) {
    return rv;
  }

  UniquePORTString serialNumber(
      CERT_Hexify(const_cast<SECItem*>(&serialItem), 1));
  if (!serialNumber) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = printableItem->SetDisplayValue(
      NS_ConvertASCIItoUTF16(serialNumber.get()));
  if (NS_FAILED(rv)) {
    return rv;
  }

  retItem = printableItem.forget();
  return NS_OK;
}

static nsresult GetDefaultOIDFormat(SECItem* oid, nsAString& outString,
                                    char separator) {
  outString.Truncate();
  int invalidCount = 0;

  unsigned int i;
  unsigned long val = 0;
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
      if (val == 0 || (val >= (1 << (32 - 7))) || (i == oid->len - 1)) {
        invalid = true;
      }

      if (i < oid->len - 1) continue;
    }

    if (!invalid) {
      if (first) {
        unsigned long one = std::min(val / 40, 2UL);  // never > 2
        unsigned long two = val - (one * 40);

        outString.AppendPrintf("%lu%c%lu", one, separator, two);
      } else {
        outString.AppendPrintf("%c%lu", separator, val);
      }
    } else {
      if (!first) {
        outString.AppendPrintf("%c", separator);
      }
      nsAutoString unknownText;
      GetPIPNSSBundleString("CertUnknown", unknownText);
      outString.Append(unknownText);

      if (++invalidCount > 3) {
        // Allow only 3 occurences of Unknown in OID display string to
        // prevent bloat.
        break;
      }
    }

    val = 0;
    invalid = false;
    first = false;
  }

  return NS_OK;
}

static nsresult GetOIDText(SECItem* oid, nsAString& text) {
  nsresult rv;
  SECOidTag oidTag = SECOID_FindOIDTag(oid);
  const char* bundlekey = 0;

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
      break;
  }

  if (bundlekey) {
    rv = GetPIPNSSBundleString(bundlekey, text);
  } else {
    AutoTArray<nsString, 1> params;
    rv = GetDefaultOIDFormat(oid, *params.AppendElement(), ' ');
    if (NS_FAILED(rv)) return rv;

    rv = PIPBundleFormatStringFromName("CertDumpDefOID", params, text);
  }
  return rv;
}

#define SEPARATOR "\n"

static nsresult ProcessRawBytes(SECItem* data, nsAString& text,
                                bool wantHeader = true) {
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
    AutoTArray<nsString, 2> params;
    params.AppendElement()->AppendInt(data->len);      // bytelen
    params.AppendElement()->AppendInt(data->len * 8);  // bitlen

    nsresult rv =
        PIPBundleFormatStringFromName("CertDumpRawBytesHeader", params, text);
    if (NS_FAILED(rv)) return rv;

    text.AppendLiteral(SEPARATOR);
  }

  // This prints the value of the byte out into a
  // string that can later be displayed as a byte
  // string.  We place a new line after 24 bytes
  // to break up extermaly long sequence of bytes.

  uint32_t i;
  char buffer[5];
  for (i = 0; i < data->len; i++) {
    SprintfLiteral(buffer, "%02x ", data->data[i]);
    text.AppendASCII(buffer);
    if ((i + 1) % 16 == 0) {
      text.AppendLiteral(SEPARATOR);
    }
  }
  return NS_OK;
}

/**
 * Appends a pipnss bundle string to the given string.
 *
 * @param bundleKey Key for the string to append.
 * @param currentText The text to append to, using |SEPARATOR| as the separator.
 */
template <size_t N>
void AppendBundleString(const char (&bundleKey)[N],
                        /*in/out*/ nsAString& currentText) {
  nsAutoString bundleString;
  nsresult rv = GetPIPNSSBundleString(bundleKey, bundleString);
  if (NS_FAILED(rv)) {
    return;
  }

  currentText.Append(bundleString);
  currentText.AppendLiteral(SEPARATOR);
}

static nsresult ProcessKeyUsageExtension(SECItem* extData, nsAString& text) {
  MOZ_ASSERT(extData);
  NS_ENSURE_ARG(extData);

  ScopedAutoSECItem decoded;
  if (SEC_ASN1DecodeItem(nullptr, &decoded, SEC_ASN1_GET(SEC_BitStringTemplate),
                         extData) != SECSuccess) {
    AppendBundleString("CertDumpExtensionFailure", text);
    return NS_OK;
  }
  unsigned char keyUsage = 0;
  if (decoded.len) {
    keyUsage = decoded.data[0];
  }

  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    AppendBundleString("CertDumpKUSign", text);
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    AppendBundleString("CertDumpKUNonRep", text);
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    AppendBundleString("CertDumpKUEnc", text);
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    AppendBundleString("CertDumpKUDEnc", text);
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    AppendBundleString("CertDumpKUKA", text);
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    AppendBundleString("CertDumpKUCertSign", text);
  }
  if (keyUsage & KU_CRL_SIGN) {
    AppendBundleString("CertDumpKUCRLSigner", text);
  }

  return NS_OK;
}

static nsresult ProcessBasicConstraints(SECItem* extData, nsAString& text) {
  nsAutoString local;
  CERTBasicConstraints value;
  SECStatus rv;
  nsresult rv2;

  value.pathLenConstraint = -1;
  rv = CERT_DecodeBasicConstraintValue(&value, extData);
  if (rv != SECSuccess) {
    ProcessRawBytes(extData, text);
    return NS_OK;
  }
  if (value.isCA)
    rv2 = GetPIPNSSBundleString("CertDumpIsCA", local);
  else
    rv2 = GetPIPNSSBundleString("CertDumpIsNotCA", local);
  if (NS_FAILED(rv2)) return rv2;
  text.Append(local.get());
  if (value.pathLenConstraint != -1) {
    nsAutoString depth;
    if (value.pathLenConstraint == CERT_UNLIMITED_PATH_CONSTRAINT)
      GetPIPNSSBundleString("CertDumpPathLenUnlimited", depth);
    else
      depth.AppendInt(value.pathLenConstraint);
    AutoTArray<nsString, 1> params = {depth};
    rv2 = PIPBundleFormatStringFromName("CertDumpPathLen", params, local);
    if (NS_FAILED(rv2)) return rv2;
    text.AppendLiteral(SEPARATOR);
    text.Append(local.get());
  }
  return NS_OK;
}

static nsresult ProcessExtKeyUsage(SECItem* extData, nsAString& text) {
  nsAutoString local;
  SECItem** oids;
  SECItem* oid;
  nsresult rv;

  UniqueCERTOidSequence extKeyUsage(CERT_DecodeOidSequence(extData));
  if (!extKeyUsage) {
    return NS_ERROR_FAILURE;
  }

  oids = extKeyUsage->oids;
  while (oids && *oids) {
    // For each OID, try to find a bundle string
    // of the form CertDumpEKU_<underlined-OID>
    nsAutoString oidname;
    oid = *oids;
    rv = GetDefaultOIDFormat(oid, oidname, '_');
    if (NS_FAILED(rv)) return rv;
    nsAutoString bundlekey = NS_LITERAL_STRING("CertDumpEKU_") + oidname;
    NS_ConvertUTF16toUTF8 bk_ascii(bundlekey);

    rv = GetPIPNSSBundleString(bk_ascii.get(), local);
    nsresult rv2 = GetDefaultOIDFormat(oid, oidname, '.');
    if (NS_FAILED(rv2)) return rv2;
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

  return NS_OK;
}

void LossyUTF8ToUTF16(const char* str, uint32_t len,
                      /*out*/ nsAString& result) {
  auto span = MakeSpan(str, len);
  if (IsUtf8(span)) {
    CopyUTF8toUTF16(span, result);
  } else {
    // Actually Latin1 despite ASCII in the legacy name
    CopyASCIItoUTF16(span, result);
  }
}

static nsresult ProcessRDN(CERTRDN* rdn, nsAString& finalString) {
  CERTAVA** avas = rdn->avas;
  for (auto i = 0; avas[i]; i++) {
    CERTAVA* ava = avas[i];
    nsAutoString type;
    nsresult rv = GetOIDText(&ava->type, type);
    if (NS_FAILED(rv)) {
      return rv;
    }

    UniqueSECItem decodeItem(CERT_DecodeAVAValue(&ava->value));
    if (!decodeItem) {
      return NS_ERROR_FAILURE;
    }

    // We know we can fit buffer of this length. CERT_RFC1485_EscapeAndQuote
    // will fail if we provide smaller buffer then the result can fit to.
    int escapedValueCapacity = decodeItem->len * 3 + 3;
    UniquePtr<char[]> escapedValue = MakeUnique<char[]>(escapedValueCapacity);

    SECStatus status =
        CERT_RFC1485_EscapeAndQuote(escapedValue.get(), escapedValueCapacity,
                                    (char*)decodeItem->data, decodeItem->len);
    if (SECSuccess != status) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString avaValue;
    LossyUTF8ToUTF16(escapedValue.get(), strlen(escapedValue.get()), avaValue);

    AutoTArray<nsString, 2> params = {type, avaValue};
    nsAutoString temp;
    PIPBundleFormatStringFromName("AVATemplate", params, temp);
    finalString += temp + NS_LITERAL_STRING("\n");
  }
  return NS_OK;
}

static nsresult ProcessName(CERTName* name, char16_t** value) {
  CERTRDN** rdns;
  CERTRDN** rdn;
  nsString finalString;

  rdns = name->rdns;

  nsresult rv;
  CERTRDN** lastRdn;
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
    rv = ProcessRDN(*rdn, finalString);
    if (NS_FAILED(rv)) return rv;
  }
  *value = ToNewUnicode(finalString);
  return NS_OK;
}

static nsresult ProcessIA5String(const SECItem& extData,
                                 /*in/out*/ nsAString& text) {
  ScopedAutoSECItem item;
  if (SEC_ASN1DecodeItem(nullptr, &item, SEC_ASN1_GET(SEC_IA5StringTemplate),
                         &extData) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Yes this is supposed to be ASCII and not UTF8, but this is just for display
  // purposes.
  nsAutoString utf16;
  const char* str = BitwiseCast<char*, unsigned char*>(item.data);
  uint32_t len = AssertedCast<uint32_t>(item.len);
  LossyUTF8ToUTF16(str, len, utf16);
  text.Append(utf16);
  return NS_OK;
}

static nsresult AppendBMPtoUTF16(const UniquePLArenaPool& arena,
                                 unsigned char* data, unsigned int len,
                                 nsAString& text) {
  if (len % 2 != 0) {
    return NS_ERROR_FAILURE;
  }

  /* XXX instead of converting to and from UTF-8, it would
     be sufficient to just swap bytes, or do nothing */
  unsigned int utf8ValLen = len * 3 + 1;
  unsigned char* utf8Val =
      (unsigned char*)PORT_ArenaZAlloc(arena.get(), utf8ValLen);
  if (!PORT_UCS2_UTF8Conversion(false, data, len, utf8Val, utf8ValLen,
                                &utf8ValLen)) {
    return NS_ERROR_FAILURE;
  }
  AppendUTF8toUTF16(MakeSpan(reinterpret_cast<char*>(utf8Val), utf8ValLen),
                    text);
  return NS_OK;
}

static nsresult ProcessGeneralName(const UniquePLArenaPool& arena,
                                   CERTGeneralName* current, nsAString& text) {
  NS_ENSURE_ARG_POINTER(current);

  nsAutoString key;
  nsAutoString value;
  nsresult rv = NS_OK;

  switch (current->type) {
    case certOtherName:
      rv = GetDefaultOIDFormat(&current->name.OthName.oid, key, ' ');
      if (NS_FAILED(rv)) {
        return rv;
      }
      ProcessRawBytes(&current->name.OthName.name, value);
      break;
    case certRFC822Name: {
      GetPIPNSSBundleString("CertDumpRFC822Name", key);
      const char* str = reinterpret_cast<const char*>(current->name.other.data);
      uint32_t len = current->name.other.len;
      LossyUTF8ToUTF16(str, len, value);
      break;
    }
    case certDNSName: {
      GetPIPNSSBundleString("CertDumpDNSName", key);
      const char* str = reinterpret_cast<const char*>(current->name.other.data);
      uint32_t len = current->name.other.len;
      LossyUTF8ToUTF16(str, len, value);
      break;
    }
    case certX400Address:
      GetPIPNSSBundleString("CertDumpX400Address", key);
      ProcessRawBytes(&current->name.other, value);
      break;
    case certDirectoryName:
      GetPIPNSSBundleString("CertDumpDirectoryName", key);
      rv = ProcessName(&current->name.directoryName, getter_Copies(value));
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    case certEDIPartyName:
      GetPIPNSSBundleString("CertDumpEDIPartyName", key);
      ProcessRawBytes(&current->name.other, value);
      break;
    case certURI: {
      GetPIPNSSBundleString("CertDumpURI", key);
      const char* str = reinterpret_cast<const char*>(current->name.other.data);
      uint32_t len = current->name.other.len;
      LossyUTF8ToUTF16(str, len, value);
      break;
    }
    case certIPAddress: {
      char buf[INET6_ADDRSTRLEN];
      PRStatus status = PR_FAILURE;
      PRNetAddr addr;
      memset(&addr, 0, sizeof(addr));
      GetPIPNSSBundleString("CertDumpIPAddress", key);
      if (current->name.other.len == 4) {
        addr.inet.family = PR_AF_INET;
        memcpy(&addr.inet.ip, current->name.other.data,
               current->name.other.len);
        status = PR_NetAddrToString(&addr, buf, sizeof(buf));
      } else if (current->name.other.len == 16) {
        addr.ipv6.family = PR_AF_INET6;
        memcpy(&addr.ipv6.ip, current->name.other.data,
               current->name.other.len);
        status = PR_NetAddrToString(&addr, buf, sizeof(buf));
      }
      if (status == PR_SUCCESS) {
        value.AssignASCII(buf);
      } else {
        /* invalid IP address */
        ProcessRawBytes(&current->name.other, value);
      }
      break;
    }
    case certRegisterID:
      GetPIPNSSBundleString("CertDumpRegisterID", key);
      rv = GetDefaultOIDFormat(&current->name.other, value, '.');
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
  }
  text.Append(key);
  text.AppendLiteral(": ");
  text.Append(value);
  text.AppendLiteral(SEPARATOR);

  return rv;
}

static nsresult ProcessGeneralNames(const UniquePLArenaPool& arena,
                                    CERTGeneralName* nameList,
                                    nsAString& text) {
  CERTGeneralName* current = nameList;
  nsresult rv;

  do {
    rv = ProcessGeneralName(arena, current, text);
    if (NS_FAILED(rv)) {
      break;
    }
    current = CERT_GetNextGeneralName(current);
  } while (current != nameList);
  return rv;
}

static nsresult ProcessAltName(SECItem* extData, nsAString& text) {
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTGeneralName* nameList = CERT_DecodeAltNameExtension(arena.get(), extData);
  if (!nameList) {
    return NS_OK;
  }

  return ProcessGeneralNames(arena, nameList, text);
}

static nsresult ProcessSubjectKeyId(SECItem* extData, nsAString& text) {
  SECItem decoded;
  nsAutoString local;

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (SEC_QuickDERDecodeItem(arena.get(), &decoded,
                             SEC_ASN1_GET(SEC_OctetStringTemplate),
                             extData) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  GetPIPNSSBundleString("CertDumpKeyID", local);
  text.Append(local);
  text.AppendLiteral(": ");
  ProcessRawBytes(&decoded, text);

  return NS_OK;
}

static nsresult ProcessAuthKeyId(SECItem* extData, nsAString& text) {
  nsresult rv = NS_OK;
  nsAutoString local;

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTAuthKeyID* ret = CERT_DecodeAuthKeyID(arena.get(), extData);
  if (!ret) {
    return NS_ERROR_FAILURE;
  }

  if (ret->keyID.len > 0) {
    GetPIPNSSBundleString("CertDumpKeyID", local);
    text.Append(local);
    text.AppendLiteral(": ");
    ProcessRawBytes(&ret->keyID, text);
    text.AppendLiteral(SEPARATOR);
  }

  if (ret->authCertIssuer) {
    GetPIPNSSBundleString("CertDumpIssuer", local);
    text.Append(local);
    text.AppendLiteral(": ");
    rv = ProcessGeneralNames(arena, ret->authCertIssuer, text);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (ret->authCertSerialNumber.len > 0) {
    GetPIPNSSBundleString("CertDumpSerialNo", local);
    text.Append(local);
    text.AppendLiteral(": ");
    ProcessRawBytes(&ret->authCertSerialNumber, text);
  }

  return rv;
}

static nsresult ProcessUserNotice(SECItem* derNotice, nsAString& text) {
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  UniqueCERTUserNotice notice(CERT_DecodeUserNotice(derNotice));
  if (!notice) {
    ProcessRawBytes(derNotice, text);
    return NS_OK;
  }

  if (notice->noticeReference.organization.len != 0) {
    switch (notice->noticeReference.organization.type) {
      case siAsciiString:
      case siVisibleString:
      case siUTF8String: {
        const char* str = reinterpret_cast<const char*>(
            notice->noticeReference.organization.data);
        uint32_t len = notice->noticeReference.organization.len;
        nsAutoString utf16;
        LossyUTF8ToUTF16(str, len, utf16);
        text.Append(utf16);
        break;
      }
      case siBMPString:
        AppendBMPtoUTF16(arena, notice->noticeReference.organization.data,
                         notice->noticeReference.organization.len, text);
        break;
      default:
        break;
    }
    text.AppendLiteral(" - ");
    SECItem** itemList = notice->noticeReference.noticeNumbers;
    while (*itemList) {
      unsigned long number;
      char buffer[60];
      if (SEC_ASN1DecodeInteger(*itemList, &number) == SECSuccess) {
        SprintfLiteral(buffer, "#%lu", number);
        if (itemList != notice->noticeReference.noticeNumbers)
          text.AppendLiteral(", ");
        text.AppendASCII(buffer);
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
      case siUTF8String: {
        const char* str =
            reinterpret_cast<const char*>(notice->displayText.data);
        uint32_t len = notice->displayText.len;
        nsAutoString utf16;
        LossyUTF8ToUTF16(str, len, utf16);
        text.Append(utf16);
        break;
      }
      case siBMPString:
        AppendBMPtoUTF16(arena, notice->displayText.data,
                         notice->displayText.len, text);
        break;
      default:
        break;
    }
  }

  return NS_OK;
}

static nsresult ProcessCertificatePolicies(SECItem* extData, nsAString& text) {
  CERTPolicyInfo **policyInfos, *policyInfo;
  CERTPolicyQualifier **policyQualifiers, *policyQualifier;
  nsAutoString local;
  nsresult rv = NS_OK;

  UniqueCERTCertificatePolicies policies(
      CERT_DecodeCertificatePoliciesExtension(extData));
  if (!policies) {
    return NS_ERROR_FAILURE;
  }

  policyInfos = policies->policyInfos;
  while (*policyInfos) {
    policyInfo = *policyInfos++;
    switch (policyInfo->oid) {
      case SEC_OID_VERISIGN_USER_NOTICES:
        GetPIPNSSBundleString("CertDumpVerisignNotices", local);
        text.Append(local);
        break;
      default:
        GetDefaultOIDFormat(&policyInfo->policyID, local, '.');
        text.Append(local);
    }

    if (policyInfo->policyQualifiers) {
      /* Add all qualifiers on separate lines, indented */
      policyQualifiers = policyInfo->policyQualifiers;
      text.Append(':');
      text.AppendLiteral(SEPARATOR);
      while (*policyQualifiers) {
        text.AppendLiteral("  ");
        policyQualifier = *policyQualifiers++;
        switch (policyQualifier->oid) {
          case SEC_OID_PKIX_CPS_POINTER_QUALIFIER:
            GetPIPNSSBundleString("CertDumpCPSPointer", local);
            text.Append(local);
            text.Append(':');
            text.AppendLiteral(SEPARATOR);
            text.AppendLiteral("    ");
            /* The CPS pointer ought to be the cPSuri alternative
             of the Qualifier choice. */
            rv = ProcessIA5String(policyQualifier->qualifierValue, text);
            if (NS_FAILED(rv)) {
              return rv;
            }
            break;
          case SEC_OID_PKIX_USER_NOTICE_QUALIFIER:
            GetPIPNSSBundleString("CertDumpUserNotice", local);
            text.Append(local);
            text.AppendLiteral(": ");
            rv = ProcessUserNotice(&policyQualifier->qualifierValue, text);
            break;
          default:
            GetDefaultOIDFormat(&policyQualifier->qualifierID, local, '.');
            text.Append(local);
            text.AppendLiteral(": ");
            ProcessRawBytes(&policyQualifier->qualifierValue, text);
        }
        text.AppendLiteral(SEPARATOR);
      } /* while policyQualifiers */
    }   /* if policyQualifiers */
    text.AppendLiteral(SEPARATOR);
  }

  return rv;
}

static nsresult ProcessCrlDistPoints(SECItem* extData, nsAString& text) {
  nsresult rv = NS_OK;
  nsAutoString local;

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTCrlDistributionPoints* crldp =
      CERT_DecodeCRLDistributionPoints(arena.get(), extData);
  if (!crldp || !crldp->distPoints) {
    return NS_ERROR_FAILURE;
  }

  for (CRLDistributionPoint** points = crldp->distPoints; *points; points++) {
    CRLDistributionPoint* point = *points;
    switch (point->distPointType) {
      case generalName:
        rv = ProcessGeneralName(arena, point->distPoint.fullName, text);
        if (NS_FAILED(rv)) {
          return rv;
        }
        break;
      case relativeDistinguishedName:
        rv = ProcessRDN(&point->distPoint.relativeName, text);
        if (NS_FAILED(rv)) {
          return rv;
        }
        break;
    }
    if (point->reasons.len) {
      int reasons = point->reasons.data[0];
      text.Append(' ');
      bool comma = false;
      if (reasons & RF_UNUSED) {
        GetPIPNSSBundleString("CertDumpUnused", local);
        text.Append(local);
        comma = true;
      }
      if (reasons & RF_KEY_COMPROMISE) {
        if (comma) text.AppendLiteral(", ");
        GetPIPNSSBundleString("CertDumpKeyCompromise", local);
        text.Append(local);
        comma = true;
      }
      if (reasons & RF_CA_COMPROMISE) {
        if (comma) text.AppendLiteral(", ");
        GetPIPNSSBundleString("CertDumpCACompromise", local);
        text.Append(local);
        comma = true;
      }
      if (reasons & RF_AFFILIATION_CHANGED) {
        if (comma) text.AppendLiteral(", ");
        GetPIPNSSBundleString("CertDumpAffiliationChanged", local);
        text.Append(local);
        comma = true;
      }
      if (reasons & RF_SUPERSEDED) {
        if (comma) text.AppendLiteral(", ");
        GetPIPNSSBundleString("CertDumpSuperseded", local);
        text.Append(local);
        comma = true;
      }
      if (reasons & RF_CESSATION_OF_OPERATION) {
        if (comma) text.AppendLiteral(", ");
        GetPIPNSSBundleString("CertDumpCessation", local);
        text.Append(local);
        comma = true;
      }
      if (reasons & RF_CERTIFICATE_HOLD) {
        if (comma) text.AppendLiteral(", ");
        GetPIPNSSBundleString("CertDumpHold", local);
        text.Append(local);
        comma = true;
      }
      text.AppendLiteral(SEPARATOR);
    }
    if (point->crlIssuer) {
      GetPIPNSSBundleString("CertDumpIssuer", local);
      text.Append(local);
      text.AppendLiteral(": ");
      rv = ProcessGeneralNames(arena, point->crlIssuer, text);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}

static nsresult ProcessAuthInfoAccess(SECItem* extData, nsAString& text) {
  nsresult rv = NS_OK;
  nsAutoString local;

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CERTAuthInfoAccess** aia =
      CERT_DecodeAuthInfoAccessExtension(arena.get(), extData);
  if (!aia) {
    return NS_OK;
  }

  while (*aia) {
    CERTAuthInfoAccess* desc = *aia++;
    switch (SECOID_FindOIDTag(&desc->method)) {
      case SEC_OID_PKIX_OCSP:
        GetPIPNSSBundleString("CertDumpOCSPResponder", local);
        break;
      case SEC_OID_PKIX_CA_ISSUERS:
        GetPIPNSSBundleString("CertDumpCAIssuers", local);
        break;
      default:
        rv = GetDefaultOIDFormat(&desc->method, local, '.');
        if (NS_FAILED(rv)) {
          return rv;
        }
    }
    text.Append(local);
    text.AppendLiteral(": ");
    rv = ProcessGeneralName(arena, desc->location, text);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return rv;
}

static nsresult ProcessExtensionData(SECOidTag oidTag, SECItem* extData,
                                     nsAString& text) {
  nsresult rv;
  switch (oidTag) {
    case SEC_OID_X509_KEY_USAGE:
      rv = ProcessKeyUsageExtension(extData, text);
      break;
    case SEC_OID_X509_BASIC_CONSTRAINTS:
      rv = ProcessBasicConstraints(extData, text);
      break;
    case SEC_OID_X509_EXT_KEY_USAGE:
      rv = ProcessExtKeyUsage(extData, text);
      break;
    case SEC_OID_X509_ISSUER_ALT_NAME:
    case SEC_OID_X509_SUBJECT_ALT_NAME:
      rv = ProcessAltName(extData, text);
      break;
    case SEC_OID_X509_SUBJECT_KEY_ID:
      rv = ProcessSubjectKeyId(extData, text);
      break;
    case SEC_OID_X509_AUTH_KEY_ID:
      rv = ProcessAuthKeyId(extData, text);
      break;
    case SEC_OID_X509_CERTIFICATE_POLICIES:
      rv = ProcessCertificatePolicies(extData, text);
      break;
    case SEC_OID_X509_CRL_DIST_POINTS:
      rv = ProcessCrlDistPoints(extData, text);
      break;
    case SEC_OID_X509_AUTH_INFO_ACCESS:
      rv = ProcessAuthInfoAccess(extData, text);
      break;
    default:
      rv = ProcessRawBytes(extData, text);
      break;
  }
  return rv;
}

static nsresult ProcessSingleExtension(CERTCertExtension* extension,
                                       nsIASN1PrintableItem** retExtension) {
  nsAutoString text, extvalue;
  GetOIDText(&extension->id, text);
  nsCOMPtr<nsIASN1PrintableItem> extensionItem = new nsNSSASN1PrintableItem();

  extensionItem->SetDisplayName(text);
  SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);
  text.Truncate();
  if (extension->critical.data) {
    if (extension->critical.data[0]) {
      GetPIPNSSBundleString("CertDumpCritical", text);
    } else {
      GetPIPNSSBundleString("CertDumpNonCritical", text);
    }
  } else {
    GetPIPNSSBundleString("CertDumpNonCritical", text);
  }
  text.AppendLiteral(SEPARATOR);
  nsresult rv = ProcessExtensionData(oidTag, &extension->value, extvalue);
  if (NS_FAILED(rv)) {
    extvalue.Truncate();
    rv = ProcessRawBytes(&extension->value, extvalue, false);
  }
  text.Append(extvalue);

  extensionItem->SetDisplayValue(text);
  extensionItem.forget(retExtension);
  return NS_OK;
}

static nsresult ProcessSECAlgorithmID(SECAlgorithmID* algID,
                                      nsIASN1Sequence** retSequence) {
  SECOidTag algOIDTag = SECOID_FindOIDTag(&algID->algorithm);
  SECItem paramsOID = {siBuffer, nullptr, 0};
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();

  *retSequence = nullptr;
  nsString text;
  GetOIDText(&algID->algorithm, text);
  if (!algID->parameters.len ||
      algID->parameters.data[0] == nsIASN1Object::ASN1_NULL) {
    sequence->SetDisplayValue(text);
    sequence->SetIsValidContainer(false);
  } else {
    nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

    printableItem->SetDisplayValue(text);
    nsCOMPtr<nsIMutableArray> asn1Objects;
    sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
    asn1Objects->AppendElement(printableItem);
    GetPIPNSSBundleString("CertDumpAlgID", text);
    printableItem->SetDisplayName(text);

    printableItem = new nsNSSASN1PrintableItem();

    asn1Objects->AppendElement(printableItem);
    GetPIPNSSBundleString("CertDumpParams", text);
    printableItem->SetDisplayName(text);
    if ((algOIDTag == SEC_OID_ANSIX962_EC_PUBLIC_KEY) &&
        (algID->parameters.len > 2) &&
        (algID->parameters.data[0] == nsIASN1Object::ASN1_OBJECT_ID)) {
      paramsOID.len = algID->parameters.len - 2;
      paramsOID.data = algID->parameters.data + 2;
      GetOIDText(&paramsOID, text);
    } else {
      ProcessRawBytes(&algID->parameters, text);
    }
    printableItem->SetDisplayValue(text);
  }
  sequence.forget(retSequence);
  return NS_OK;
}

static nsresult ProcessTime(PRTime dispTime, const char16_t* displayName,
                            nsIASN1Sequence* parentSequence) {
  nsString text;
  nsString tempString;

  PRExplodedTime explodedTime;
  PR_ExplodeTime(dispTime, PR_LocalTimeParameters, &explodedTime);

  DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds,
                                       &explodedTime, tempString);

  text.Append(tempString);
  text.AppendLiteral("\n(");

  PRExplodedTime explodedTimeGMT;
  PR_ExplodeTime(dispTime, PR_GMTParameters, &explodedTimeGMT);

  DateTimeFormat::FormatPRExplodedTime(kDateFormatLong, kTimeFormatSeconds,
                                       &explodedTimeGMT, tempString);

  text.Append(tempString);
  // Append "GMT" if it's not already added by the formatter
  // since the OS pattern contained a timezone (Mac and Linux).
  if (tempString.Find(" GMT") == kNotFound) {
    text.AppendLiteral(" GMT)");
  } else {
    text.Append(')');
  }

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  printableItem->SetDisplayValue(text);
  printableItem->SetDisplayName(nsDependentString(displayName));
  nsCOMPtr<nsIMutableArray> asn1Objects;
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(printableItem);
  return NS_OK;
}

static nsresult ProcessSubjectPublicKeyInfo(CERTSubjectPublicKeyInfo* spki,
                                            nsIASN1Sequence* parentSequence) {
  nsCOMPtr<nsIASN1Sequence> spkiSequence = new nsNSSASN1Sequence();

  nsString text;
  GetPIPNSSBundleString("CertDumpSPKI", text);
  spkiSequence->SetDisplayName(text);

  GetPIPNSSBundleString("CertDumpSPKIAlg", text);
  nsCOMPtr<nsIASN1Sequence> sequenceItem;
  nsresult rv =
      ProcessSECAlgorithmID(&spki->algorithm, getter_AddRefs(sequenceItem));
  if (NS_FAILED(rv)) return rv;
  sequenceItem->SetDisplayName(text);
  nsCOMPtr<nsIMutableArray> asn1Objects;
  spkiSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(sequenceItem);

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  text.Truncate();

  UniqueSECKEYPublicKey key(SECKEY_ExtractPublicKey(spki));
  bool displayed = false;
  if (key) {
    switch (key->keyType) {
      case rsaKey: {
        displayed = true;
        nsAutoString length1, length2, data1, data2;
        length1.AppendInt(key->u.rsa.modulus.len * 8);
        length2.AppendInt(key->u.rsa.publicExponent.len * 8);
        ProcessRawBytes(&key->u.rsa.modulus, data1, false);
        ProcessRawBytes(&key->u.rsa.publicExponent, data2, false);
        AutoTArray<nsString, 4> params = {length1, data1, length2, data2};
        PIPBundleFormatStringFromName("CertDumpRSATemplate", params, text);
        break;
      }
      case ecKey: {
        displayed = true;
        SECKEYECPublicKey& ecpk = key->u.ec;
        int fieldSizeLenAsBits =
            SECKEY_ECParamsToKeySize(&ecpk.DEREncodedParams);
        int basePointOrderLenAsBits =
            SECKEY_ECParamsToBasePointOrderLen(&ecpk.DEREncodedParams);
        nsAutoString s_fsl, s_bpol, s_pv;
        s_fsl.AppendInt(fieldSizeLenAsBits);
        s_bpol.AppendInt(basePointOrderLenAsBits);

        if (ecpk.publicValue.len > 4) {
          ProcessRawBytes(&ecpk.publicValue, s_pv, false);
        } else {
          int i_pv = DER_GetInteger(&ecpk.publicValue);
          s_pv.AppendInt(i_pv);
        }
        AutoTArray<nsString, 3> params = {s_fsl, s_bpol, s_pv};
        PIPBundleFormatStringFromName("CertDumpECTemplate", params, text);
        break;
      }
      default:
        /* Algorithm unknown, or too rarely used to bother displaying it */
        break;
    }
  }
  if (!displayed) {
    // Algorithm unknown, display raw bytes
    // The subjectPublicKey field is encoded as a bit string.
    // ProcessRawBytes expects the length to be in bytes, so
    // let's convert the lenght into a temporary SECItem.
    SECItem data;
    data.data = spki->subjectPublicKey.data;
    data.len = spki->subjectPublicKey.len / 8;
    ProcessRawBytes(&data, text);
  }

  printableItem->SetDisplayValue(text);
  GetPIPNSSBundleString("CertDumpSubjPubKey", text);
  printableItem->SetDisplayName(text);
  asn1Objects->AppendElement(printableItem);

  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(spkiSequence);
  return NS_OK;
}

static nsresult ProcessExtensions(CERTCertExtension** extensions,
                                  nsIASN1Sequence* parentSequence) {
  nsCOMPtr<nsIASN1Sequence> extensionSequence = new nsNSSASN1Sequence;

  nsString text;
  GetPIPNSSBundleString("CertDumpExtensions", text);
  extensionSequence->SetDisplayName(text);
  int32_t i;
  nsresult rv;
  nsCOMPtr<nsIASN1PrintableItem> newExtension;
  nsCOMPtr<nsIMutableArray> asn1Objects;
  extensionSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  for (i = 0; extensions[i] != nullptr; i++) {
    rv = ProcessSingleExtension(extensions[i], getter_AddRefs(newExtension));
    if (NS_FAILED(rv)) return rv;

    asn1Objects->AppendElement(newExtension);
  }
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(extensionSequence);
  return NS_OK;
}

nsresult nsNSSCertificate::CreateTBSCertificateASN1Struct(
    nsIASN1Sequence** retSequence) {
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
  GetPIPNSSBundleString("CertDumpCertificate", text);
  sequence->SetDisplayName(text);
  nsCOMPtr<nsIASN1PrintableItem> printableItem;

  nsCOMPtr<nsIMutableArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));

  nsresult rv = ProcessVersion(&mCert->version, getter_AddRefs(printableItem));
  if (NS_FAILED(rv)) return rv;

  asn1Objects->AppendElement(printableItem);

  rv = ProcessSerialNumberDER(mCert->serialNumber, printableItem);
  if (NS_FAILED(rv)) return rv;
  asn1Objects->AppendElement(printableItem);

  nsCOMPtr<nsIASN1Sequence> algID;
  rv = ProcessSECAlgorithmID(&mCert->signature, getter_AddRefs(algID));
  if (NS_FAILED(rv)) return rv;

  GetPIPNSSBundleString("CertDumpSigAlg", text);
  algID->SetDisplayName(text);
  asn1Objects->AppendElement(algID);

  nsString value;
  ProcessName(&mCert->issuer, getter_Copies(value));

  printableItem = new nsNSSASN1PrintableItem();

  printableItem->SetDisplayValue(value);
  GetPIPNSSBundleString("CertDumpIssuer", text);
  printableItem->SetDisplayName(text);
  asn1Objects->AppendElement(printableItem);

  nsCOMPtr<nsIASN1Sequence> validitySequence = new nsNSSASN1Sequence();
  GetPIPNSSBundleString("CertDumpValidity", text);
  validitySequence->SetDisplayName(text);
  asn1Objects->AppendElement(validitySequence);
  GetPIPNSSBundleString("CertDumpNotBefore", text);
  nsCOMPtr<nsIX509CertValidity> validityData;
  GetValidity(getter_AddRefs(validityData));
  PRTime notBefore, notAfter;

  validityData->GetNotBefore(&notBefore);
  validityData->GetNotAfter(&notAfter);
  validityData = nullptr;
  rv = ProcessTime(notBefore, text.get(), validitySequence);
  if (NS_FAILED(rv)) return rv;

  GetPIPNSSBundleString("CertDumpNotAfter", text);
  rv = ProcessTime(notAfter, text.get(), validitySequence);
  if (NS_FAILED(rv)) return rv;

  GetPIPNSSBundleString("CertDumpSubject", text);

  printableItem = new nsNSSASN1PrintableItem();

  printableItem->SetDisplayName(text);
  ProcessName(&mCert->subject, getter_Copies(value));
  printableItem->SetDisplayValue(value);
  asn1Objects->AppendElement(printableItem);

  rv = ProcessSubjectPublicKeyInfo(&mCert->subjectPublicKeyInfo, sequence);
  if (NS_FAILED(rv)) return rv;

  SECItem data;
  // Is there an issuerUniqueID?
  if (mCert->issuerID.data) {
    // The issuerID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->issuerID.data;
    data.len = (mCert->issuerID.len + 7) / 8;

    ProcessRawBytes(&data, text);
    printableItem = new nsNSSASN1PrintableItem();

    printableItem->SetDisplayValue(text);
    GetPIPNSSBundleString("CertDumpIssuerUniqueID", text);
    printableItem->SetDisplayName(text);
    asn1Objects->AppendElement(printableItem);
  }

  if (mCert->subjectID.data) {
    // The subjectID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->subjectID.data;
    data.len = (mCert->subjectID.len + 7) / 8;

    ProcessRawBytes(&data, text);
    printableItem = new nsNSSASN1PrintableItem();

    printableItem->SetDisplayValue(text);
    GetPIPNSSBundleString("CertDumpSubjectUniqueID", text);
    printableItem->SetDisplayName(text);
    asn1Objects->AppendElement(printableItem);
  }
  if (mCert->extensions) {
    rv = ProcessExtensions(mCert->extensions, sequence);
    if (NS_FAILED(rv)) return rv;
  }
  sequence.forget(retSequence);
  return NS_OK;
}

nsresult nsNSSCertificate::CreateASN1Struct(nsIASN1Object** aRetVal) {
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();

  nsCOMPtr<nsIMutableArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));

  nsAutoString displayName;
  nsresult rv = GetDisplayName(displayName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = sequence->SetDisplayName(displayName);
  if (NS_FAILED(rv)) {
    return rv;
  }
  sequence.forget(aRetVal);

  // This sequence will be contain the tbsCertificate, signatureAlgorithm,
  // and signatureValue.
  rv = CreateTBSCertificateASN1Struct(getter_AddRefs(sequence));
  if (NS_FAILED(rv)) return rv;

  asn1Objects->AppendElement(sequence);
  nsCOMPtr<nsIASN1Sequence> algID;

  rv = ProcessSECAlgorithmID(&mCert->signatureWrap.signatureAlgorithm,
                             getter_AddRefs(algID));
  if (NS_FAILED(rv)) return rv;
  nsString text;
  GetPIPNSSBundleString("CertDumpSigAlg", text);
  algID->SetDisplayName(text);
  asn1Objects->AppendElement(algID);
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  GetPIPNSSBundleString("CertDumpCertSig", text);
  printableItem->SetDisplayName(text);
  // The signatureWrap is encoded as a bit string.
  // The function ProcessRawBytes expects the
  // length to be in bytes, so let's convert the
  // length in a temporary SECItem
  SECItem temp;
  temp.data = mCert->signatureWrap.signature.data;
  temp.len = mCert->signatureWrap.signature.len / 8;
  text.Truncate();
  ProcessRawBytes(&temp, text);
  printableItem->SetDisplayValue(text);
  asn1Objects->AppendElement(printableItem);
  return NS_OK;
}

nsresult GetCertFingerprintByOidTag(CERTCertificate* nsscert, SECOidTag aOidTag,
                                    nsCString& fp) {
  Digest digest;
  nsresult rv =
      digest.DigestBuf(aOidTag, nsscert->derCert.data, nsscert->derCert.len);
  NS_ENSURE_SUCCESS(rv, rv);

  UniquePORTString tmpstr(CERT_Hexify(const_cast<SECItem*>(&digest.get()), 1));
  NS_ENSURE_TRUE(tmpstr, NS_ERROR_OUT_OF_MEMORY);

  fp.Assign(tmpstr.get());
  return NS_OK;
}
