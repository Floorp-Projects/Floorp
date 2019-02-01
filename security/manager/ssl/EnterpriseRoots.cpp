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
#  include <Security/Security.h>
#  include "KeychainSecret.h"  // for ScopedCFType
#endif                         // XP_MACOSX

extern LazyLogModule gPIPNSSLog;

using namespace mozilla;

#ifdef XP_WIN
const wchar_t* kWindowsDefaultRootStoreName = L"ROOT";

// Helper function to determine if the OS considers the given certificate to be
// a trust anchor for TLS server auth certificates. This is to be used in the
// context of importing what are presumed to be root certificates from the OS.
// If this function returns true but it turns out that the given certificate is
// in some way unsuitable to issue certificates, mozilla::pkix will never build
// a valid chain that includes the certificate, so importing it even if it
// isn't a valid CA poses no risk.
static bool CertIsTrustAnchorForTLSServerAuth(PCCERT_CONTEXT certificate) {
  MOZ_ASSERT(certificate);
  if (!certificate) {
    return false;
  }

  PCCERT_CHAIN_CONTEXT pChainContext = nullptr;
  CERT_ENHKEY_USAGE enhkeyUsage;
  memset(&enhkeyUsage, 0, sizeof(CERT_ENHKEY_USAGE));
  LPCSTR identifiers[] = {
      "1.3.6.1.5.5.7.3.1",  // id-kp-serverAuth
  };
  enhkeyUsage.cUsageIdentifier = ArrayLength(identifiers);
  enhkeyUsage.rgpszUsageIdentifier =
      const_cast<LPSTR*>(identifiers);  // -Wwritable-strings
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
  bool trusted =
      pChainContext->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR;
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

// Because HCERTSTORE is just a typedef void*, we can't use any of the nice
// scoped or unique pointer templates. To elaborate, any attempt would
// instantiate those templates with T = void. When T gets used in the context
// of T&, this results in void&, which isn't legal.
class ScopedCertStore final {
 public:
  explicit ScopedCertStore(HCERTSTORE certstore) : certstore(certstore) {}

  ~ScopedCertStore() { CertCloseStore(certstore, 0); }

  HCERTSTORE get() { return certstore; }

 private:
  ScopedCertStore(const ScopedCertStore&) = delete;
  ScopedCertStore& operator=(const ScopedCertStore&) = delete;
  HCERTSTORE certstore;
};

// Loads the enterprise roots at the registry location corresponding to the
// given location flag.
// Supported flags are:
//   CERT_SYSTEM_STORE_LOCAL_MACHINE
//     (for HKLM\SOFTWARE\Microsoft\SystemCertificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY
//     (for
//     HKLM\SOFTWARE\Policies\Microsoft\SystemCertificates\Root\Certificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE
//     (for HKLM\SOFTWARE\Microsoft\EnterpriseCertificates\Root\Certificates)
static void GatherEnterpriseRootsForLocation(DWORD locationFlag,
                                             Vector<Vector<uint8_t>>& roots) {
  MOZ_ASSERT(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
                 locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
                 locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
             "unexpected locationFlag for GatherEnterpriseRootsForLocation");
  if (!(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE)) {
    return;
  }

  DWORD flags =
      locationFlag | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG;
  // The certificate store being opened should consist only of certificates
  // added by a user or administrator and not any certificates that are part
  // of Microsoft's root store program.
  // The 3rd parameter to CertOpenStore should be NULL according to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa376559%28v=vs.85%29.aspx
  ScopedCertStore enterpriseRootStore(
      CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, NULL, flags,
                    kWindowsDefaultRootStoreName));
  if (!enterpriseRootStore.get()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to open enterprise root store"));
    return;
  }
  PCCERT_CONTEXT certificate = nullptr;
  uint32_t numImported = 0;
  while ((certificate = CertFindCertificateInStore(
              enterpriseRootStore.get(), X509_ASN_ENCODING, 0, CERT_FIND_ANY,
              nullptr, certificate))) {
    if (!CertIsTrustAnchorForTLSServerAuth(certificate)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("skipping cert not trust anchor for TLS server auth"));
      continue;
    }
    Vector<uint8_t> certBytes;
    if (!certBytes.append(certificate->pbCertEncoded,
                          certificate->cbCertEncoded)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
              ("couldn't copy cert bytes (out of memory?)"));
      continue;
    }
    if (!roots.append(std::move(certBytes))) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
              ("couldn't append cert bytes to roots list (out of memory?)"));
      continue;
    }
    numImported++;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u roots", numImported));
}

static void GatherEnterpriseRootsWindows(Vector<Vector<uint8_t>>& roots) {
  GatherEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE, roots);
  GatherEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                                   roots);
  GatherEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                   roots);
}
#endif  // XP_WIN

#ifdef XP_MACOSX
OSStatus GatherEnterpriseRootsOSX(Vector<Vector<uint8_t>>& roots) {
  // The following builds a search dictionary corresponding to:
  // { class: "certificate",
  //   match limit: "match all",
  //   policy: "SSL (TLS)",
  //   only include trusted certificates: true }
  // This operates on items that have been added to the keychain and thus gives
  // us all 3rd party certificates that have been trusted for SSL (TLS), which
  // is what we want (thus we don't import built-in root certificates that ship
  // with the OS).
  const CFStringRef keys[] = {kSecClass, kSecMatchLimit, kSecMatchPolicy,
                              kSecMatchTrustedOnly};
  // https://developer.apple.com/documentation/security/1392592-secpolicycreatessl
  ScopedCFType<SecPolicyRef> sslPolicy(SecPolicyCreateSSL(true, nullptr));
  const void* values[] = {kSecClassCertificate, kSecMatchLimitAll,
                          sslPolicy.get(), kCFBooleanTrue};
  static_assert(ArrayLength(keys) == ArrayLength(values),
                "mismatched SecItemCopyMatching key/value array sizes");
  // https://developer.apple.com/documentation/corefoundation/1516782-cfdictionarycreate
  ScopedCFType<CFDictionaryRef> searchDictionary(CFDictionaryCreate(
      nullptr, (const void**)&keys, (const void**)&values, ArrayLength(keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
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
    Vector<uint8_t> certBytes;
    if (!certBytes.append(ptr, len)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
              ("couldn't copy cert bytes (out of memory?)"));
      continue;
    }
    if (!roots.append(std::move(certBytes))) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
              ("couldn't append cert bytes to roots list (out of memory?)"));
      continue;
    }
    numImported++;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u roots", numImported));
  return errSecSuccess;
}
#endif  // XP_MACOSX

nsresult GatherEnterpriseRoots(Vector<Vector<uint8_t>>& roots) {
  roots.clear();
#ifdef XP_WIN
  GatherEnterpriseRootsWindows(roots);
#endif  // XP_WIN
#ifdef XP_MACOSX
  OSStatus rv = GatherEnterpriseRootsOSX(roots);
  if (rv != errSecSuccess) {
    return NS_ERROR_FAILURE;
  }
#endif  // XP_MACOSX
  return NS_OK;
}
