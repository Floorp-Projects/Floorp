/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EnterpriseRoots.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "mozpkix/Result.h"
#include "nsNSSCertHelper.h"
#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/EnterpriseRootsWrappers.h"
#endif  // MOZ_WIDGET_ANDROID

#ifdef XP_MACOSX
#  include <Security/Security.h>
#  include "KeychainSecret.h"
#endif

#ifdef XP_WIN
#  include <windows.h>
#  include <wincrypt.h>
#endif  // XP_WIN

extern mozilla::LazyLogModule gPIPNSSLog;

using namespace mozilla;

void EnterpriseCert::CopyBytes(nsTArray<uint8_t>& dest) const {
  dest.Assign(mDER);
}

pkix::Result EnterpriseCert::GetInput(pkix::Input& input) const {
  return input.Init(mDER.Elements(), mDER.Length());
}

bool EnterpriseCert::GetIsRoot() const { return mIsRoot; }

bool EnterpriseCert::IsKnownRoot(UniqueSECMODModule& rootsModule) {
  if (!rootsModule) {
    return false;
  }

  SECItem certItem = {siBuffer, mDER.Elements(),
                      static_cast<unsigned int>(mDER.Length())};
  AutoSECMODListReadLock lock;
  for (int i = 0; i < rootsModule->slotCount; i++) {
    PK11SlotInfo* slot = rootsModule->slots[i];
    if (PK11_FindEncodedCertInSlot(slot, &certItem, nullptr) !=
        CK_INVALID_HANDLE) {
      return true;
    }
  }
  return false;
}

#ifdef XP_WIN
const wchar_t* kWindowsDefaultRootStoreNames[] = {L"ROOT", L"CA"};

// Helper function to determine if the OS considers the given certificate to be
// a trust anchor for TLS server auth certificates. This is to be used in the
// context of importing what are presumed to be root certificates from the OS.
// If this function returns true but it turns out that the given certificate is
// in some way unsuitable to issue certificates, mozilla::pkix will never build
// a valid chain that includes the certificate, so importing it even if it
// isn't a valid CA poses no risk.
static void CertIsTrustAnchorForTLSServerAuth(PCCERT_CONTEXT certificate,
                                              bool& isTrusted, bool& isRoot) {
  isTrusted = false;
  isRoot = false;
  MOZ_ASSERT(certificate);
  if (!certificate) {
    return;
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
  // Disable anything that could result in network I/O.
  DWORD flags = CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY |
                CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL |
                CERT_CHAIN_DISABLE_AUTH_ROOT_AUTO_UPDATE |
                CERT_CHAIN_DISABLE_AIA;
  if (!CertGetCertificateChain(nullptr, certificate, nullptr, nullptr,
                               &chainPara, flags, nullptr, &pChainContext)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("CertGetCertificateChain failed"));
    return;
  }
  isTrusted = pChainContext->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR;
  if (isTrusted && pChainContext->cChain > 0) {
    // The so-called "final chain" is what we're after:
    // https://docs.microsoft.com/en-us/windows/desktop/api/wincrypt/ns-wincrypt-_cert_chain_context
    CERT_SIMPLE_CHAIN* finalChain =
        pChainContext->rgpChain[pChainContext->cChain - 1];
    // This is a root if the final chain consists of only one certificate (i.e.
    // this one).
    isRoot = finalChain->cElement == 1;
  }
  CertFreeCertificateChain(pChainContext);
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
//     (for HKLM\SOFTWARE\Policy\Microsoft\SystemCertificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE
//     (for HKLM\SOFTWARE\Microsoft\EnterpriseCertificates)
//   CERT_SYSTEM_STORE_CURRENT_USER
//     (for HKCU\SOFTWARE\Microsoft\SystemCertificates)
//   CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY
//     (for HKCU\SOFTWARE\Policy\Microsoft\SystemCertificates)
static void GatherEnterpriseCertsForLocation(DWORD locationFlag,
                                             nsTArray<EnterpriseCert>& certs,
                                             UniqueSECMODModule& rootsModule) {
  MOZ_ASSERT(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
                 locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
                 locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE ||
                 locationFlag == CERT_SYSTEM_STORE_CURRENT_USER ||
                 locationFlag == CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
             "unexpected locationFlag for GatherEnterpriseRootsForLocation");
  if (!(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE ||
        locationFlag == CERT_SYSTEM_STORE_CURRENT_USER ||
        locationFlag == CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY)) {
    return;
  }

  DWORD flags =
      locationFlag | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG;
  // The certificate store being opened should consist only of certificates
  // added by a user or administrator and not any certificates that are part
  // of Microsoft's root store program.
  // The 3rd parameter to CertOpenStore should be NULL according to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa376559%28v=vs.85%29.aspx
  for (auto name : kWindowsDefaultRootStoreNames) {
    ScopedCertStore enterpriseRootStore(
        CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, NULL, flags, name));
    if (!enterpriseRootStore.get()) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to open enterprise root store"));
      continue;
    }
    PCCERT_CONTEXT certificate = nullptr;
    uint32_t numImported = 0;
    while ((certificate = CertFindCertificateInStore(
                enterpriseRootStore.get(), X509_ASN_ENCODING, 0, CERT_FIND_ANY,
                nullptr, certificate))) {
      bool isTrusted;
      bool isRoot;
      CertIsTrustAnchorForTLSServerAuth(certificate, isTrusted, isRoot);
      if (!isTrusted) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("skipping cert not trusted for TLS server auth"));
        continue;
      }
      EnterpriseCert enterpriseCert(certificate->pbCertEncoded,
                                    certificate->cbCertEncoded, isRoot);
      if (!enterpriseCert.IsKnownRoot(rootsModule)) {
        certs.AppendElement(std::move(enterpriseCert));
        numImported++;
      } else {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping known root cert"));
      }
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("imported %u certs from %S", numImported, name));
  }
}

static void GatherEnterpriseCertsWindows(nsTArray<EnterpriseCert>& certs,
                                         UniqueSECMODModule& rootsModule) {
  GatherEnterpriseCertsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE, certs,
                                   rootsModule);
  GatherEnterpriseCertsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                                   certs, rootsModule);
  GatherEnterpriseCertsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                   certs, rootsModule);
  GatherEnterpriseCertsForLocation(CERT_SYSTEM_STORE_CURRENT_USER, certs,
                                   rootsModule);
  GatherEnterpriseCertsForLocation(CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
                                   certs, rootsModule);
}
#endif  // XP_WIN

#ifdef XP_MACOSX
enum class CertificateTrustResult {
  CanUseAsIntermediate,
  CanUseAsTrustAnchor,
  DoNotUse,
};

ScopedCFType<CFArrayRef> GetCertificateTrustSettingsInDomain(
    const SecCertificateRef certificate, SecTrustSettingsDomain domain) {
  CFArrayRef trustSettingsRaw;
  OSStatus rv =
      SecTrustSettingsCopyTrustSettings(certificate, domain, &trustSettingsRaw);
  if (rv != errSecSuccess || !trustSettingsRaw) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("  SecTrustSettingsCopyTrustSettings failed (or not found) for "
             "domain %" PRIu32,
             domain));
    return nullptr;
  }
  ScopedCFType<CFArrayRef> trustSettings(trustSettingsRaw);
  return trustSettings;
}

// This function processes trust settings returned by
// SecTrustSettingsCopyTrustSettings. See the documentation at
// https://developer.apple.com/documentation/security/1400261-sectrustsettingscopytrustsetting
// `trustSettings` is an array of CFDictionaryRef. Each dictionary may impose
// a constraint.
CertificateTrustResult ProcessCertificateTrustSettings(
    ScopedCFType<CFArrayRef>& trustSettings) {
  // If the array is empty, the certificate is a trust anchor.
  const CFIndex numTrustDictionaries = CFArrayGetCount(trustSettings.get());
  if (numTrustDictionaries == 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("  empty trust settings -> trust anchor"));
    return CertificateTrustResult::CanUseAsTrustAnchor;
  }
  CertificateTrustResult currentTrustSettings =
      CertificateTrustResult::CanUseAsIntermediate;
  for (CFIndex i = 0; i < numTrustDictionaries; i++) {
    CFDictionaryRef trustDictionary = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(trustSettings.get(), i));
    // kSecTrustSettingsApplication specifies an external application that
    // determines the certificate's trust settings.
    // kSecTrustSettingsPolicyString appears to be a mechanism like name
    // constraints.
    // These are not supported, so conservatively assume this certificate is
    // distrusted if either are present.
    if (CFDictionaryContainsKey(trustDictionary,
                                kSecTrustSettingsApplication) ||
        CFDictionaryContainsKey(trustDictionary,
                                kSecTrustSettingsPolicyString)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("  found unsupported policy -> assuming distrusted"));
      return CertificateTrustResult::DoNotUse;
    }

    // kSecTrustSettingsKeyUsage seems to be essentially the equivalent of the
    // x509 keyUsage extension. For parity, we allow
    // kSecTrustSettingsKeyUseSignature, kSecTrustSettingsKeyUseSignCert, and
    // kSecTrustSettingsKeyUseAny.
    if (CFDictionaryContainsKey(trustDictionary, kSecTrustSettingsKeyUsage)) {
      CFNumberRef keyUsage = (CFNumberRef)CFDictionaryGetValue(
          trustDictionary, kSecTrustSettingsKeyUsage);
      int32_t keyUsageValue;
      if (!keyUsage ||
          CFNumberGetValue(keyUsage, kCFNumberSInt32Type, &keyUsageValue) ||
          keyUsageValue < 0) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("  no trust settings key usage or couldn't get value"));
        return CertificateTrustResult::DoNotUse;
      }
      switch ((uint64_t)keyUsageValue) {
        case kSecTrustSettingsKeyUseSignature:  // fall-through
        case kSecTrustSettingsKeyUseSignCert:   // fall-through
        case kSecTrustSettingsKeyUseAny:
          break;
        default:
          return CertificateTrustResult::DoNotUse;
      }
    }

    // If there is a specific policy, ensure that it's for the
    // 'kSecPolicyAppleSSL' policy, which is the TLS server auth policy (i.e.
    // x509 + domain name checking).
    if (CFDictionaryContainsKey(trustDictionary, kSecTrustSettingsPolicy)) {
      SecPolicyRef policy = (SecPolicyRef)CFDictionaryGetValue(
          trustDictionary, kSecTrustSettingsPolicy);
      if (!policy) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("  kSecTrustSettingsPolicy present, but null?"));
        continue;
      }
      ScopedCFType<CFDictionaryRef> policyProperties(
          SecPolicyCopyProperties(policy));
      CFStringRef policyOid = (CFStringRef)CFDictionaryGetValue(
          policyProperties.get(), kSecPolicyOid);
      if (!CFEqual(policyOid, kSecPolicyAppleSSL)) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("  policy doesn't match"));
        continue;
      }
    }

    // By default, the trust setting result value is
    // kSecTrustSettingsResultTrustRoot.
    int32_t trustSettingsValue = kSecTrustSettingsResultTrustRoot;
    if (CFDictionaryContainsKey(trustDictionary, kSecTrustSettingsResult)) {
      CFNumberRef trustSetting = (CFNumberRef)CFDictionaryGetValue(
          trustDictionary, kSecTrustSettingsResult);
      if (!trustSetting || !CFNumberGetValue(trustSetting, kCFNumberSInt32Type,
                                             &trustSettingsValue)) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("  no trust settings result or couldn't get value"));
        continue;
      }
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("  trust setting: %d", trustSettingsValue));
    if (trustSettingsValue == kSecTrustSettingsResultDeny) {
      return CertificateTrustResult::DoNotUse;
    }
    if (trustSettingsValue == kSecTrustSettingsResultTrustRoot ||
        trustSettingsValue == kSecTrustSettingsResultTrustAsRoot) {
      currentTrustSettings = CertificateTrustResult::CanUseAsTrustAnchor;
    }
  }
  return currentTrustSettings;
}

CertificateTrustResult GetCertificateTrustResult(
    const SecCertificateRef certificate) {
  ScopedCFType<CFStringRef> subject(
      SecCertificateCopySubjectSummary(certificate));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("determining trust for '%s'",
           CFStringGetCStringPtr(subject.get(), kCFStringEncodingUTF8)));
  // There are three trust settings domains: kSecTrustSettingsDomainUser,
  // kSecTrustSettingsDomainAdmin, and kSecTrustSettingsDomainSystem. User
  // overrides admin and admin overrides system. However, if the given
  // certificate has trust settings in the system domain, it shipped with the
  // OS, so we don't want to use it.
  ScopedCFType<CFArrayRef> systemTrustSettings(
      GetCertificateTrustSettingsInDomain(certificate,
                                          kSecTrustSettingsDomainSystem));
  if (systemTrustSettings) {
    return CertificateTrustResult::DoNotUse;
  }

  // At this point, if there is no trust information regarding this
  // certificate, it can be used as an intermediate.
  CertificateTrustResult certificateTrustResult =
      CertificateTrustResult::CanUseAsIntermediate;

  // Process trust information in the user domain, if any.
  ScopedCFType<CFArrayRef> userTrustSettings(
      GetCertificateTrustSettingsInDomain(certificate,
                                          kSecTrustSettingsDomainUser));
  if (userTrustSettings) {
    certificateTrustResult = ProcessCertificateTrustSettings(userTrustSettings);
    // If there is definite information one way or another (either indicating
    // this is a trusted root or a distrusted certificate), use that
    // information.
    if (certificateTrustResult !=
        CertificateTrustResult::CanUseAsIntermediate) {
      return certificateTrustResult;
    }
  }

  // Process trust information in the admin domain, if any.
  ScopedCFType<CFArrayRef> adminTrustSettings(
      GetCertificateTrustSettingsInDomain(certificate,
                                          kSecTrustSettingsDomainAdmin));
  if (adminTrustSettings) {
    certificateTrustResult =
        ProcessCertificateTrustSettings(adminTrustSettings);
  }

  // Use whatever result we ended up with.
  return certificateTrustResult;
}

OSStatus GatherEnterpriseCertsMacOS(nsTArray<EnterpriseCert>& certs,
                                    UniqueSECMODModule& rootsModule) {
  // The following builds a search dictionary corresponding to:
  // { class: "certificate",
  //   match limit: "match all" }
  // This operates on items that have been added to the keychain and thus gives
  // us all 3rd party certificates. Unfortunately, if a root that shipped with
  // the OS has had its trust settings changed, it can also be returned from
  // this query. Further work (below) filters such certificates out.
  const CFStringRef keys[] = {kSecClass, kSecMatchLimit};
  const void* values[] = {kSecClassCertificate, kSecMatchLimitAll};
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
    // Because we asked for certificates, each CFTypeRef in the array is really
    // a SecCertificateRef.
    const SecCertificateRef certificate =
        (const SecCertificateRef)CFArrayGetValueAtIndex(arr.get(), i);
    CertificateTrustResult certificateTrustResult =
        GetCertificateTrustResult(certificate);
    if (certificateTrustResult == CertificateTrustResult::DoNotUse) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping distrusted cert"));
      continue;
    }
    ScopedCFType<CFDataRef> der(SecCertificateCopyData(certificate));
    if (!der) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("couldn't get bytes of certificate?"));
      continue;
    }
    bool isRoot =
        certificateTrustResult == CertificateTrustResult::CanUseAsTrustAnchor;
    EnterpriseCert enterpriseCert(CFDataGetBytePtr(der.get()),
                                  CFDataGetLength(der.get()), isRoot);
    if (!enterpriseCert.IsKnownRoot(rootsModule)) {
      certs.AppendElement(std::move(enterpriseCert));
      numImported++;
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("importing as %s", isRoot ? "root" : "intermediate"));
    } else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping known root cert"));
    }
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u certs", numImported));
  return errSecSuccess;
}
#endif  // XP_MACOSX

#ifdef MOZ_WIDGET_ANDROID
void GatherEnterpriseCertsAndroid(nsTArray<EnterpriseCert>& certs,
                                  UniqueSECMODModule& rootsModule) {
  if (!jni::IsAvailable()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("JNI not available"));
    return;
  }
  jni::ObjectArray::LocalRef roots =
      java::EnterpriseRoots::GatherEnterpriseRoots();
  uint32_t numImported = 0;
  for (size_t i = 0; i < roots->Length(); i++) {
    jni::ByteArray::LocalRef root = roots->GetElement(i);
    // Currently we treat all certificates gleaned from the Android
    // CA store as roots.
    EnterpriseCert enterpriseCert(
        reinterpret_cast<uint8_t*>(root->GetElements().Elements()),
        root->Length(), true);
    if (!enterpriseCert.IsKnownRoot(rootsModule)) {
      certs.AppendElement(std::move(enterpriseCert));
      numImported++;
    } else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping known root cert"));
    }
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u certs", numImported));
}
#endif  // MOZ_WIDGET_ANDROID

nsresult GatherEnterpriseCerts(nsTArray<EnterpriseCert>& certs) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  certs.Clear();
  UniqueSECMODModule rootsModule(SECMOD_FindModule(kRootModuleName));
#ifdef XP_WIN
  GatherEnterpriseCertsWindows(certs, rootsModule);
#endif  // XP_WIN
#ifdef XP_MACOSX
  OSStatus rv = GatherEnterpriseCertsMacOS(certs, rootsModule);
  if (rv != errSecSuccess) {
    return NS_ERROR_FAILURE;
  }
#endif  // XP_MACOSX
#ifdef MOZ_WIDGET_ANDROID
  GatherEnterpriseCertsAndroid(certs, rootsModule);
#endif  // MOZ_WIDGET_ANDROID
  return NS_OK;
}
