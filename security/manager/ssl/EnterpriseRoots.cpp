/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EnterpriseRoots.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"

#ifdef XP_MACOSX
#include <Security/Security.h>
#endif // XP_MACOSX

extern LazyLogModule gPIPNSSLog;

using namespace mozilla;

#ifdef XP_WIN
const wchar_t* kWindowsDefaultRootStoreName = L"ROOT";
NS_NAMED_LITERAL_CSTRING(kMicrosoftFamilySafetyCN, "Microsoft Family Safety");

// Helper function to determine if the OS considers the given certificate to be
// a trust anchor for TLS server auth certificates. This is to be used in the
// context of importing what are presumed to be root certificates from the OS.
// If this function returns true but it turns out that the given certificate is
// in some way unsuitable to issue certificates, mozilla::pkix will never build
// a valid chain that includes the certificate, so importing it even if it
// isn't a valid CA poses no risk.
static bool
CertIsTrustAnchorForTLSServerAuth(PCCERT_CONTEXT certificate)
{
  MOZ_ASSERT(certificate);
  if (!certificate) {
    return false;
  }

  PCCERT_CHAIN_CONTEXT pChainContext = nullptr;
  CERT_ENHKEY_USAGE enhkeyUsage;
  memset(&enhkeyUsage, 0, sizeof(CERT_ENHKEY_USAGE));
  LPSTR identifiers[] = {
    "1.3.6.1.5.5.7.3.1", // id-kp-serverAuth
  };
  enhkeyUsage.cUsageIdentifier = ArrayLength(identifiers);
  enhkeyUsage.rgpszUsageIdentifier = identifiers;
  CERT_USAGE_MATCH certUsage;
  memset(&certUsage, 0, sizeof(CERT_USAGE_MATCH));
  certUsage.dwType = USAGE_MATCH_TYPE_AND;
  certUsage.Usage = enhkeyUsage;
  CERT_CHAIN_PARA chainPara;
  memset(&chainPara, 0, sizeof(CERT_CHAIN_PARA));
  chainPara.cbSize = sizeof(CERT_CHAIN_PARA);
  chainPara.RequestedUsage = certUsage;

  if (!CertGetCertificateChain(nullptr, certificate, nullptr, nullptr,
                               &chainPara, 0, nullptr, &pChainContext)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("CertGetCertificateChain failed"));
    return false;
  }
  bool trusted = pChainContext->TrustStatus.dwErrorStatus ==
                 CERT_TRUST_NO_ERROR;
  bool isRoot = pChainContext->cChain == 1;
  CertFreeCertificateChain(pChainContext);
  if (trusted && isRoot) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("certificate is trust anchor for TLS server auth"));
    return true;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("certificate not trust anchor for TLS server auth"));
  return false;
}

// It would be convenient to just use nsIX509CertDB in the following code.
// However, since nsIX509CertDB depends on nsNSSComponent initialization (and
// since this code runs during that initialization), we can't use it. Instead,
// we can use NSS APIs directly (as long as we're called late enough in
// nsNSSComponent initialization such that those APIs are safe to use).

// Helper function to convert a PCCERT_CONTEXT (i.e. a certificate obtained via
// a Windows API) to a temporary CERTCertificate (i.e. a certificate for use
// with NSS APIs).
UniqueCERTCertificate
PCCERT_CONTEXTToCERTCertificate(PCCERT_CONTEXT pccert)
{
  MOZ_ASSERT(pccert);
  if (!pccert) {
    return nullptr;
  }

  SECItem derCert = {
    siBuffer,
    pccert->pbCertEncoded,
    pccert->cbCertEncoded
  };
  return UniqueCERTCertificate(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &derCert,
                            nullptr, // nickname unnecessary
                            false, // not permanent
                            true)); // copy DER
}

// Loads the enterprise roots at the registry location corresponding to the
// given location flag.
// Supported flags are:
//   CERT_SYSTEM_STORE_LOCAL_MACHINE
//     (for HKLM\SOFTWARE\Microsoft\SystemCertificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY
//     (for HKLM\SOFTWARE\Policies\Microsoft\SystemCertificates\Root\Certificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE
//     (for HKLM\SOFTWARE\Microsoft\EnterpriseCertificates\Root\Certificates)
static void
GatherEnterpriseRootsForLocation(DWORD locationFlag, UniqueCERTCertList& roots)
{
  MOZ_ASSERT(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
             locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
             locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
             "unexpected locationFlag for GatherEnterpriseRootsForLocation");
  if (!(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE)) {
    return;
  }
  MOZ_ASSERT(roots, "roots unexpectedly NULL?");
  if (!roots) {
    return;
  }

  DWORD flags = locationFlag |
                CERT_STORE_OPEN_EXISTING_FLAG |
                CERT_STORE_READONLY_FLAG;
  // The certificate store being opened should consist only of certificates
  // added by a user or administrator and not any certificates that are part
  // of Microsoft's root store program.
  // The 3rd parameter to CertOpenStore should be NULL according to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa376559%28v=vs.85%29.aspx
  ScopedCertStore enterpriseRootStore(CertOpenStore(
    CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, NULL, flags,
    kWindowsDefaultRootStoreName));
  if (!enterpriseRootStore.get()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to open enterprise root store"));
    return;
  }
  PCCERT_CONTEXT certificate = nullptr;
  uint32_t numImported = 0;
  while ((certificate = CertFindCertificateInStore(enterpriseRootStore.get(),
                                                   X509_ASN_ENCODING, 0,
                                                   CERT_FIND_ANY, nullptr,
                                                   certificate))) {
    if (!CertIsTrustAnchorForTLSServerAuth(certificate)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("skipping cert not trust anchor for TLS server auth"));
      continue;
    }
    UniqueCERTCertificate nssCertificate(
      PCCERT_CONTEXTToCERTCertificate(certificate));
    if (!nssCertificate) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't decode certificate"));
      continue;
    }
    // Don't import the Microsoft Family Safety root (this prevents the
    // Enterprise Roots feature from interacting poorly with the Family
    // Safety support).
    UniquePORTString subjectName(
      CERT_GetCommonName(&nssCertificate->subject));
    if (kMicrosoftFamilySafetyCN.Equals(subjectName.get())) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping Family Safety Root"));
      continue;
    }
    if (CERT_AddCertToListTail(roots.get(), nssCertificate.get())
          != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't add cert to list"));
      continue;
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Imported '%s'", subjectName.get()));
    numImported++;
    // now owned by roots
    Unused << nssCertificate.release();
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u roots", numImported));
}

static void
GatherEnterpriseRootsWindows(UniqueCERTCertList& roots)
{
  GatherEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE, roots);
  GatherEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                                   roots);
  GatherEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                   roots);
}
#endif // XP_WIN

#ifdef XP_MACOSX
template<typename T>
class ScopedCFType
{
public:
  explicit ScopedCFType(T value)
  : mValue(value)
  {
  }

  ~ScopedCFType() { CFRelease((CFTypeRef)mValue); }

  T get() { return mValue; }

private:
  T mValue;
};

OSStatus
GatherEnterpriseRootsOSX(UniqueCERTCertList& roots)
{
  MOZ_ASSERT(roots, "roots unexpectedly NULL?");
  if (!roots) {
    return errSecBadReq;
  }
  // The following builds a search dictionary corresponding to:
  // { class: "certificate",
  //   match limit: "match all",
  //   policy: "SSL (TLS)",
  //   only include trusted certificates: true }
  // This operates on items that have been added to the keychain and thus gives
  // us all 3rd party certificates that have been trusted for SSL (TLS), which
  // is what we want (thus we don't import built-in root certificates that ship
  // with the OS).
  const CFStringRef keys[] = { kSecClass,
                               kSecMatchLimit,
                               kSecMatchPolicy,
                               kSecMatchTrustedOnly };
  // https://developer.apple.com/documentation/security/1392592-secpolicycreatessl
  ScopedCFType<SecPolicyRef> sslPolicy(SecPolicyCreateSSL(true, nullptr));
  const void* values[] = { kSecClassCertificate,
                           kSecMatchLimitAll,
                           sslPolicy.get(),
                           kCFBooleanTrue };
  static_assert(ArrayLength(keys) == ArrayLength(values),
                "mismatched SecItemCopyMatching key/value array sizes");
  // https://developer.apple.com/documentation/corefoundation/1516782-cfdictionarycreate
  ScopedCFType<CFDictionaryRef> searchDictionary(
    CFDictionaryCreate(nullptr, (const void**)&keys, (const void**)&values,
                       ArrayLength(keys), &kCFTypeDictionaryKeyCallBacks,
                       &kCFTypeDictionaryValueCallBacks));
  CFTypeRef items;
  // https://developer.apple.com/documentation/security/1398306-secitemcopymatching
  OSStatus rv = SecItemCopyMatching(searchDictionary.get(), &items);
  if (rv != errSecSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("SecItemCopyMatching failed"));
    return rv;
  }
  // If given a match limit greater than 1 (which we did), SecItemCopyMatching
  // returns a CFArrayRef.
  ScopedCFType<CFArrayRef> arr(reinterpret_cast<CFArrayRef>(items));
  CFIndex count = CFArrayGetCount(arr.get());
  uint32_t numImported = 0;
  for (CFIndex i = 0; i < count; i++) {
    const CFTypeRef c = CFArrayGetValueAtIndex(arr.get(), i);
    // Because we asked for certificates, each CFTypeRef in the array is really
    // a SecCertificateRef.
    const SecCertificateRef s = (const SecCertificateRef)c;
    ScopedCFType<CFDataRef> der(SecCertificateCopyData(s));
    const unsigned char* ptr = CFDataGetBytePtr(der.get());
    unsigned int len = CFDataGetLength(der.get());
    SECItem derItem = {
      siBuffer,
      const_cast<unsigned char*>(ptr),
      len,
    };
    UniqueCERTCertificate cert(
      CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &derItem,
                              nullptr, // nickname unnecessary
                              false, // not permanent
                              true)); // copy DER
    if (!cert) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't decode 3rd party root certificate"));
      continue;
    }
    UniquePORTString subjectName(CERT_GetCommonName(&cert->subject));
    if (CERT_AddCertToListTail(roots.get(), cert.get()) != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't add cert to list"));
      continue;
    }
    numImported++;
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Imported '%s'", subjectName.get()));
    mozilla::Unused << cert.release(); // owned by roots
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u roots", numImported));
  return errSecSuccess;
}
#endif // XP_MACOSX

nsresult
GatherEnterpriseRoots(UniqueCERTCertList& result)
{
  UniqueCERTCertList roots(CERT_NewCertList());
  if (!roots) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
#ifdef XP_WIN
  GatherEnterpriseRootsWindows(roots);
#endif // XP_WIN
#ifdef XP_MACOSX
  OSStatus rv = GatherEnterpriseRootsOSX(roots);
  if (rv != errSecSuccess) {
    return NS_ERROR_FAILURE;
  }
#endif // XP_MACOSX
  result = std::move(roots);
  return NS_OK;
}
