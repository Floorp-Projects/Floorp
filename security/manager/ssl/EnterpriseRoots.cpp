/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EnterpriseRoots.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "mozpkix/Result.h"
#include "nsCRT.h"
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
struct CertStoreLocation {
  const wchar_t* mName;
  const bool mIsRoot;

  CertStoreLocation(const wchar_t* name, bool isRoot)
      : mName(name), mIsRoot(isRoot) {}
};

// The documentation doesn't make this clear, but the certificate location
// identified by "ROOT" contains trusted root certificates. The certificate
// location identified by "CA" contains intermediate certificates.
const CertStoreLocation kCertStoreLocations[] = {
    CertStoreLocation(L"ROOT", true), CertStoreLocation(L"CA", false)};

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

// To determine if a certificate would be useful when verifying a server
// certificate for TLS server auth, Windows provides the function
// `CertGetEnhancedKeyUsage`, which combines the extended key usage extension
// with something called "enhanced key usage", which appears to be a Microsoft
// concept.
static bool CertCanBeUsedForTLSServerAuth(PCCERT_CONTEXT certificate) {
  DWORD usageSize = 0;
  if (!CertGetEnhancedKeyUsage(certificate, 0, NULL, &usageSize)) {
    return false;
  }
  nsTArray<uint8_t> usageBytes;
  usageBytes.SetLength(usageSize);
  PCERT_ENHKEY_USAGE usage(
      reinterpret_cast<PCERT_ENHKEY_USAGE>(usageBytes.Elements()));
  if (!CertGetEnhancedKeyUsage(certificate, 0, usage, &usageSize)) {
    return false;
  }
  // https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certgetenhancedkeyusage:
  // "If the cUsageIdentifier member is zero, the certificate might be valid
  // for all uses or the certificate might have no valid uses. The return from
  // a call to GetLastError can be used to determine whether the certificate is
  // good for all uses or for none. If GetLastError returns CRYPT_E_NOT_FOUND,
  // the certificate is good for all uses. If it returns zero, the certificate
  // has no valid uses."
  if (usage->cUsageIdentifier == 0) {
    return GetLastError() == static_cast<DWORD>(CRYPT_E_NOT_FOUND);
  }
  for (DWORD i = 0; i < usage->cUsageIdentifier; i++) {
    if (!nsCRT::strcmp(usage->rgpszUsageIdentifier[i],
                       szOID_PKIX_KP_SERVER_AUTH) ||
        !nsCRT::strcmp(usage->rgpszUsageIdentifier[i],
                       szOID_ANY_ENHANCED_KEY_USAGE)) {
      return true;
    }
  }
  return false;
}

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
  for (const auto& location : kCertStoreLocations) {
    ScopedCertStore certStore(CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W,
                                            0, NULL, flags, location.mName));
    if (!certStore.get()) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to open certificate store"));
      continue;
    }
    PCCERT_CONTEXT certificate = nullptr;
    uint32_t numImported = 0;
    while ((certificate = CertFindCertificateInStore(
                certStore.get(), X509_ASN_ENCODING, 0, CERT_FIND_ANY, nullptr,
                certificate))) {
      if (!CertCanBeUsedForTLSServerAuth(certificate)) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("skipping cert not relevant for TLS server auth"));
        continue;
      }
      EnterpriseCert enterpriseCert(certificate->pbCertEncoded,
                                    certificate->cbCertEncoded,
                                    location.mIsRoot);
      if (!enterpriseCert.IsKnownRoot(rootsModule)) {
        certs.AppendElement(std::move(enterpriseCert));
        numImported++;
      } else {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping known root cert"));
      }
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("imported %u certs from %S", numImported, location.mName));
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
