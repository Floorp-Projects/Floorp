/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSComponent.h"

#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "cert.h"
#include "certdb.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/Preferences.h"
#include "mozilla/PublicSSL.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsClientAuthRemember.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICertOverrideService.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIPrompt.h"
#include "nsIProperties.h"
#include "nsISiteSecurityService.h"
#include "nsITokenPasswordDialogs.h"
#include "nsIWindowWatcher.h"
#include "nsIXULRuntime.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSHelper.h"
#include "nsNSSShutDown.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nss.h"
#include "p12plcy.h"
#include "pkix/pkixnss.h"
#include "secerr.h"
#include "secmod.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#ifndef MOZ_NO_SMART_CARDS
#include "nsSmartCardMonitor.h"
#endif

#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#include "nsILocalFileWin.h"

#include "windows.h" // this needs to be before the following includes
#include "lmcons.h"
#include "sddl.h"
#include "wincrypt.h"
#include "nsIWindowsRegKey.h"
#endif

using namespace mozilla;
using namespace mozilla::psm;

LazyLogModule gPIPNSSLog("pipnss");

int nsNSSComponent::mInstanceCount = 0;

// This function can be called from chrome or content processes
// to ensure that NSS is initialized.
bool EnsureNSSInitializedChromeOrContent()
{
  nsresult rv;
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsISupports> nss = do_GetService(PSM_COMPONENT_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return false;
    }

    return true;
  }

  // If this is a content process and not the main thread (i.e. probably a
  // worker) then forward this call to the main thread.
  if (!NS_IsMainThread()) {
    static Atomic<bool> initialized(false);

    // Cache the result to dispatch to the main thread only once per worker.
    if (initialized) {
      return true;
    }

    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      return false;
    }

    // Forward to the main thread synchronously.
    mozilla::SyncRunnable::DispatchToThread(mainThread,
      new SyncRunnable(NS_NewRunnableFunction([]() {
        initialized = EnsureNSSInitializedChromeOrContent();
      }))
    );

    return initialized;
  }

  if (NSS_IsInitialized()) {
    return true;
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    return false;
  }

  if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
    return false;
  }

  mozilla::psm::DisableMD5();
  return true;
}

// We must ensure that the nsNSSComponent has been loaded before
// creating any other components.
bool EnsureNSSInitialized(EnsureNSSOperator op)
{
  if (GeckoProcessType_Default != XRE_GetProcessType())
  {
    if (op == nssEnsureOnChromeOnly)
    {
      // If the component needs PSM/NSS initialized only on the chrome process,
      // pretend we successfully initiated it but in reality we bypass it.
      // It's up to the programmer to check for process type in such components
      // and take care not to call anything that needs NSS/PSM initiated.
      return true;
    }

    NS_ERROR("Trying to initialize PSM/NSS in a non-chrome process!");
    return false;
  }

  static bool loading = false;
  static int32_t haveLoaded = 0;

  switch (op)
  {
    // In following 4 cases we are protected by monitor of XPCOM component
    // manager - we are inside of do_GetService call for nss component, so it is
    // safe to move with the flags here.
  case nssLoadingComponent:
    if (loading)
      return false; // We are reentered during nss component creation
    loading = true;
    return true;

  case nssInitSucceeded:
    NS_ASSERTION(loading, "Bad call to EnsureNSSInitialized(nssInitSucceeded)");
    loading = false;
    PR_AtomicSet(&haveLoaded, 1);
    return true;

  case nssInitFailed:
    NS_ASSERTION(loading, "Bad call to EnsureNSSInitialized(nssInitFailed)");
    loading = false;
    MOZ_FALLTHROUGH;

  case nssShutdown:
    PR_AtomicSet(&haveLoaded, 0);
    return false;

    // In this case we are called from a component to ensure nss initilization.
    // If the component has not yet been loaded and is not currently loading
    // call do_GetService for nss component to ensure it.
  case nssEnsure:
  case nssEnsureOnChromeOnly:
  case nssEnsureChromeOrContent:
    // We are reentered during nss component creation or nss component is already up
    if (PR_AtomicAdd(&haveLoaded, 0) || loading)
      return true;

    {
    nsCOMPtr<nsINSSComponent> nssComponent
      = do_GetService(PSM_COMPONENT_CONTRACTID);

    // Nss component failed to initialize, inform the caller of that fact.
    // Flags are appropriately set by component constructor itself.
    if (!nssComponent)
      return false;

    bool isInitialized;
    nsresult rv = nssComponent->IsNSSInitialized(&isInitialized);
    return NS_SUCCEEDED(rv) && isInitialized;
    }

  default:
    NS_ASSERTION(false, "Bad operator to EnsureNSSInitialized");
    return false;
  }
}

static void
GetRevocationBehaviorFromPrefs(/*out*/ CertVerifier::OcspDownloadConfig* odc,
                               /*out*/ CertVerifier::OcspStrictConfig* osc,
                               /*out*/ CertVerifier::OcspGetConfig* ogc,
                               /*out*/ uint32_t* certShortLifetimeInDays,
                               const MutexAutoLock& /*proofOfLock*/)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(odc);
  MOZ_ASSERT(osc);
  MOZ_ASSERT(ogc);
  MOZ_ASSERT(certShortLifetimeInDays);

  // 0 = disabled
  // 1 = enabled for everything (default)
  // 2 = enabled for EV certificates only
  int32_t ocspLevel = Preferences::GetInt("security.OCSP.enabled", 1);
  switch (ocspLevel) {
    case 0: *odc = CertVerifier::ocspOff; break;
    case 2: *odc = CertVerifier::ocspEVOnly; break;
    default: *odc = CertVerifier::ocspOn; break;
  }

  *osc = Preferences::GetBool("security.OCSP.require", false)
       ? CertVerifier::ocspStrict
       : CertVerifier::ocspRelaxed;

  // XXX: Always use POST for OCSP; see bug 871954 for undoing this.
  *ogc = Preferences::GetBool("security.OCSP.GET.enabled", false)
       ? CertVerifier::ocspGetEnabled
       : CertVerifier::ocspGetDisabled;

  // If we pass in just 0 as the second argument to Preferences::GetUint, there
  // are two function signatures that match (given that 0 can be intepreted as
  // a null pointer). Thus the compiler will complain without the cast.
  *certShortLifetimeInDays =
    Preferences::GetUint("security.pki.cert_short_lifetime_in_days",
                         static_cast<uint32_t>(0));

  SSL_ClearSessionCache();
}

nsNSSComponent::nsNSSComponent()
  : mutex("nsNSSComponent.mutex")
  , mNSSInitialized(false)
#ifndef MOZ_NO_SMART_CARDS
  , mThreadList(nullptr)
#endif
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::ctor\n"));
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  NS_ASSERTION( (0 == mInstanceCount), "nsNSSComponent is a singleton, but instantiated multiple times!");
  ++mInstanceCount;
}

nsNSSComponent::~nsNSSComponent()
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::dtor\n"));
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // All cleanup code requiring services needs to happen in xpcom_shutdown

  ShutdownNSS();
  SharedSSLState::GlobalCleanup();
  RememberCertErrorsTable::Cleanup();
  --mInstanceCount;
  nsNSSShutDownList::shutdown();

  // We are being freed, drop the haveLoaded flag to re-enable
  // potential nss initialization later.
  EnsureNSSInitialized(nssShutdown);

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::dtor finished\n"));
}

NS_IMETHODIMP
nsNSSComponent::PIPBundleFormatStringFromName(const char* name,
                                              const char16_t** params,
                                              uint32_t numParams,
                                              nsAString& outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mPIPNSSBundle && name) {
    nsXPIDLString result;
    rv = mPIPNSSBundle->FormatStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                             params, numParams,
                                             getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsNSSComponent::GetPIPNSSBundleString(const char* name, nsAString& outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  outString.SetLength(0);
  if (mPIPNSSBundle && name) {
    nsXPIDLString result;
    rv = mPIPNSSBundle->GetStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                          getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
      rv = NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNSSComponent::GetNSSBundleString(const char* name, nsAString& outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  outString.SetLength(0);
  if (mNSSErrorsBundle && name) {
    nsXPIDLString result;
    rv = mNSSErrorsBundle->GetStringFromName(NS_ConvertASCIItoUTF16(name).get(),
                                             getter_Copies(result));
    if (NS_SUCCEEDED(rv)) {
      outString = result;
      rv = NS_OK;
    }
  }

  return rv;
}

#ifndef MOZ_NO_SMART_CARDS
void
nsNSSComponent::LaunchSmartCardThreads()
{
  nsNSSShutDownPreventionLock locker;
  {
    SECMODModuleList* list;
    SECMODListLock* lock = SECMOD_GetDefaultModuleListLock();
    if (!lock) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Error,
               ("Couldn't get the module list lock, can't launch smart card threads\n"));
        return;
    }
    SECMOD_GetReadLock(lock);
    list = SECMOD_GetDefaultModuleList();

    while (list) {
      SECMODModule* module = list->module;
      LaunchSmartCardThread(module);
      list = list->next;
    }
    SECMOD_ReleaseReadLock(lock);
  }
}

NS_IMETHODIMP
nsNSSComponent::LaunchSmartCardThread(SECMODModule* module)
{
  SmartCardMonitoringThread* newThread;
  if (SECMOD_HasRemovableSlots(module)) {
    if (!mThreadList) {
      mThreadList = new SmartCardThreadList();
    }
    newThread = new SmartCardMonitoringThread(module);
    // newThread is adopted by the add.
    return mThreadList->Add(newThread);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::ShutdownSmartCardThread(SECMODModule* module)
{
  if (!mThreadList) {
    return NS_OK;
  }
  mThreadList->Remove(module);
  return NS_OK;
}

void
nsNSSComponent::ShutdownSmartCardThreads()
{
  delete mThreadList;
  mThreadList = nullptr;
}
#endif // MOZ_NO_SMART_CARDS

#ifdef XP_WIN
static bool
GetUserSid(nsAString& sidString)
{
  // UNLEN is the maximum user name length (see Lmcons.h). +1 for the null
  // terminator.
  WCHAR lpAccountName[UNLEN + 1];
  DWORD lcAccountName = sizeof(lpAccountName) / sizeof(lpAccountName[0]);
  BOOL success = GetUserName(lpAccountName, &lcAccountName);
  if (!success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("GetUserName failed"));
    return false;
  }
  char sid_buffer[SECURITY_MAX_SID_SIZE];
  SID* sid = BitwiseCast<SID*, char*>(sid_buffer);
  DWORD cbSid = ArrayLength(sid_buffer);
  SID_NAME_USE eUse;
  // There doesn't appear to be a defined maximum length for the domain name
  // here. To deal with this, we start with a reasonable buffer length and
  // see if that works. If it fails and the error indicates insufficient length,
  // we use the indicated required length and try again.
  DWORD cchReferencedDomainName = 128;
  auto ReferencedDomainName(MakeUnique<WCHAR[]>(cchReferencedDomainName));
  success = LookupAccountName(nullptr, lpAccountName, sid, &cbSid,
                              ReferencedDomainName.get(),
                              &cchReferencedDomainName, &eUse);
  if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("LookupAccountName failed"));
    return false;
  }
  if (!success) {
    ReferencedDomainName = MakeUnique<WCHAR[]>(cchReferencedDomainName);
    success = LookupAccountName(nullptr, lpAccountName, sid, &cbSid,
                                ReferencedDomainName.get(),
                                &cchReferencedDomainName, &eUse);
  }
  if (!success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("LookupAccountName failed"));
    return false;
  }
  LPTSTR StringSid;
  success = ConvertSidToStringSid(sid, &StringSid);
  if (!success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("ConvertSidToStringSid failed"));
    return false;
  }
  sidString.Assign(StringSid);
  LocalFree(StringSid);
  return true;
}

// This is a specialized helper function to read the value of a registry key
// that might not be present. If it is present, returns (via the output
// parameter) its value. Otherwise, returns the given default value.
// This function handles one level of nesting. That is, if the desired value
// is actually in a direct child of the given registry key (where the child
// and/or the value being sought may not actually be present), this function
// will handle that. In the normal case, though, optionalChildName will be
// null.
static nsresult
ReadRegKeyValueWithDefault(nsCOMPtr<nsIWindowsRegKey> regKey,
                           uint32_t flags,
                           wchar_t* optionalChildName,
                           wchar_t* valueName,
                           uint32_t defaultValue,
                           uint32_t& valueOut)
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("ReadRegKeyValueWithDefault"));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("attempting to read '%S%s%S' with default '%u'",
           optionalChildName ? optionalChildName : L"",
           optionalChildName ? "\\" : "", valueName, defaultValue));
  if (optionalChildName) {
    nsDependentString childNameString(optionalChildName);
    bool hasChild;
    nsresult rv = regKey->HasChild(childNameString, &hasChild);
    if (NS_FAILED(rv)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to determine if child key is present"));
      return rv;
    }
    if (!hasChild) {
      valueOut = defaultValue;
      return NS_OK;
    }
    nsCOMPtr<nsIWindowsRegKey> childRegKey;
    rv = regKey->OpenChild(childNameString, flags,
                           getter_AddRefs(childRegKey));
    if (NS_FAILED(rv)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't open child key"));
      return rv;
    }
    return ReadRegKeyValueWithDefault(childRegKey, flags, nullptr, valueName,
                                      defaultValue, valueOut);
  }
  nsDependentString valueNameString(valueName);
  bool hasValue;
  nsresult rv = regKey->HasValue(valueNameString, &hasValue);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to determine if value is present"));
    return rv;
  }
  if (!hasValue) {
    valueOut = defaultValue;
    return NS_OK;
  }
  rv = regKey->ReadIntValue(valueNameString, &valueOut);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to read value"));
    return rv;
  }
  return NS_OK;
}

static nsresult
AccountHasFamilySafetyEnabled(bool& enabled)
{
  enabled = false;
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("AccountHasFamilySafetyEnabled?"));
  nsCOMPtr<nsIWindowsRegKey> parentalControlsKey(
    do_CreateInstance("@mozilla.org/windows-registry-key;1"));
  if (!parentalControlsKey) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't create nsIWindowsRegKey"));
    return NS_ERROR_FAILURE;
  }
  uint32_t flags = nsIWindowsRegKey::ACCESS_READ | nsIWindowsRegKey::WOW64_64;
  NS_NAMED_LITERAL_STRING(familySafetyPath,
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Parental Controls");
  nsresult rv = parentalControlsKey->Open(
    nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, familySafetyPath, flags);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't open parentalControlsKey"));
    return rv;
  }
  NS_NAMED_LITERAL_STRING(usersString, "Users");
  bool hasUsers;
  rv = parentalControlsKey->HasChild(usersString, &hasUsers);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("HasChild(Users) failed"));
    return rv;
  }
  if (!hasUsers) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("Users subkey not present - Parental Controls not enabled"));
    return NS_OK;
  }
  nsCOMPtr<nsIWindowsRegKey> usersKey;
  rv = parentalControlsKey->OpenChild(usersString, flags,
                                      getter_AddRefs(usersKey));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("failed to open Users subkey"));
    return rv;
  }
  nsAutoString sid;
  if (!GetUserSid(sid)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't get sid"));
    return NS_ERROR_FAILURE;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("our sid is '%S'", sid.get()));
  bool hasSid;
  rv = usersKey->HasChild(sid, &hasSid);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("HasChild(sid) failed"));
    return rv;
  }
  if (!hasSid) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("sid not present in Family Safety Users"));
    return NS_OK;
  }
  nsCOMPtr<nsIWindowsRegKey> sidKey;
  rv = usersKey->OpenChild(sid, flags, getter_AddRefs(sidKey));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't open sid key"));
    return rv;
  }
  // There are three keys we're interested in: "Parental Controls On",
  // "Logging Required", and "Web\\Filter On". These keys will have value 0
  // or 1, indicating a particular feature is disabled or enabled,
  // respectively. So, if "Parental Controls On" is not 1, Family Safety is
  // disabled and we don't care about anything else. If both "Logging
  // Required" and "Web\\Filter On" are 0, the proxy will not be running,
  // so for our purposes we can consider Family Safety disabled in that
  // case.
  // By default, "Logging Required" is 1 and "Web\\Filter On" is 0,
  // reflecting the initial settings when Family Safety is enabled for an
  // account for the first time, However, these sub-keys are not created
  // unless they are switched away from the default value.
  uint32_t parentalControlsOn;
  rv = sidKey->ReadIntValue(NS_LITERAL_STRING("Parental Controls On"),
                            &parentalControlsOn);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't read Parental Controls On"));
    return rv;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("Parental Controls On: %u", parentalControlsOn));
  if (parentalControlsOn != 1) {
    return NS_OK;
  }
  uint32_t loggingRequired;
  rv = ReadRegKeyValueWithDefault(sidKey, flags, nullptr, L"Logging Required",
                                  1, loggingRequired);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to read value of Logging Required"));
    return rv;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("Logging Required: %u", loggingRequired));
  uint32_t webFilterOn;
  rv = ReadRegKeyValueWithDefault(sidKey, flags, L"Web", L"Filter On", 0,
                                  webFilterOn);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to read value of Web\\Filter On"));
    return rv;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Web\\Filter On: %u", webFilterOn));
  enabled = loggingRequired == 1 || webFilterOn == 1;
  return NS_OK;
}

// It would be convenient to just use nsIX509CertDB in the following code.
// However, since nsIX509CertDB depends on nsNSSComponent initialization (and
// since this code runs during that initialization), we can't use it. Instead,
// we can use NSS APIs directly (as long as we're called late enough in
// nsNSSComponent initialization such that those APIs are safe to use).

// Helper function to convert a PCCERT_CONTEXT (i.e. a certificate obtained via
// a Windows API) to a temporary CERTCertificate (i.e. a certificate for use
// with NSS APIs).
static UniqueCERTCertificate
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

static const char* kMicrosoftFamilySafetyCN = "Microsoft Family Safety";

nsresult
nsNSSComponent::MaybeImportFamilySafetyRoot(PCCERT_CONTEXT certificate,
                                            bool& wasFamilySafetyRoot)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("MaybeImportFamilySafetyRoot"));
  wasFamilySafetyRoot = false;

  UniqueCERTCertificate nssCertificate(
    PCCERT_CONTEXTToCERTCertificate(certificate));
  if (!nssCertificate) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't decode certificate"));
    return NS_ERROR_FAILURE;
  }
  // Looking for a certificate with the common name 'Microsoft Family Safety'
  UniquePORTString subjectName(CERT_GetCommonName(&nssCertificate->subject));
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("subject name is '%s'", subjectName.get()));
  if (nsCRT::strcmp(subjectName.get(), kMicrosoftFamilySafetyCN) == 0) {
    wasFamilySafetyRoot = true;
    CERTCertTrust trust = {
      CERTDB_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_USER,
      0,
      0
    };
    if (CERT_ChangeCertTrust(nullptr, nssCertificate.get(), &trust)
          != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("couldn't trust certificate for TLS server auth"));
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(!mFamilySafetyRoot);
    mFamilySafetyRoot = Move(nssCertificate);
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("added Family Safety root"));
  }
  return NS_OK;
}

// Because HCERTSTORE is just a typedef void*, we can't use any of the nice
// scoped or unique pointer templates. To elaborate, any attempt would
// instantiate those templates with T = void. When T gets used in the context
// of T&, this results in void&, which isn't legal.
class ScopedCertStore final
{
public:
  explicit ScopedCertStore(HCERTSTORE certstore) : certstore(certstore) {}

  ~ScopedCertStore()
  {
    CertCloseStore(certstore, 0);
  }

  HCERTSTORE get()
  {
    return certstore;
  }

private:
  ScopedCertStore(const ScopedCertStore&) = delete;
  ScopedCertStore& operator=(const ScopedCertStore&) = delete;
  HCERTSTORE certstore;
};

static const wchar_t* kWindowsDefaultRootStoreName = L"ROOT";

nsresult
nsNSSComponent::LoadFamilySafetyRoot()
{
  ScopedCertStore certstore(
    CertOpenSystemStore(0, kWindowsDefaultRootStoreName));
  if (!certstore.get()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't get certstore '%S'", kWindowsDefaultRootStoreName));
    return NS_ERROR_FAILURE;
  }
  // Any resources held by the certificate are released by the next call to
  // CertFindCertificateInStore.
  PCCERT_CONTEXT certificate = nullptr;
  while ((certificate = CertFindCertificateInStore(certstore.get(),
                                                   X509_ASN_ENCODING, 0,
                                                   CERT_FIND_ANY, nullptr,
                                                   certificate))) {
    bool wasFamilySafetyRoot = false;
    nsresult rv = MaybeImportFamilySafetyRoot(certificate,
                                              wasFamilySafetyRoot);
    if (NS_SUCCEEDED(rv) && wasFamilySafetyRoot) {
      return NS_OK; // We're done (we're only expecting one root).
    }
  }
  return NS_ERROR_FAILURE;
}

void
nsNSSComponent::UnloadFamilySafetyRoot()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("UnloadFamilySafetyRoot"));
  if (!mFamilySafetyRoot) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Family Safety Root wasn't present"));
    return;
  }
  // It would be intuitive to set the trust to { 0, 0, 0 } here. However, this
  // doesn't work for temporary certificates because CERT_ChangeCertTrust first
  // looks up the current trust settings in the permanent cert database, finds
  // that such trust doesn't exist, considers the current trust to be
  // { 0, 0, 0 }, and decides that it doesn't need to update the trust since
  // they're the same. To work around this, we set a non-zero flag to ensure
  // that the trust will get updated.
  CERTCertTrust trust = { CERTDB_USER, 0, 0 };
  if (CERT_ChangeCertTrust(nullptr, mFamilySafetyRoot.get(), &trust)
        != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("couldn't untrust certificate for TLS server auth"));
  }
  mFamilySafetyRoot = nullptr;
}

#endif // XP_WIN

// The supported values of this pref are:
// 0: disable detecting Family Safety mode and importing the root
// 1: only attempt to detect Family Safety mode (don't import the root)
// 2: detect Family Safety mode and import the root
const char* kFamilySafetyModePref = "security.family_safety.mode";

// The telemetry gathered by this function is as follows:
// 0-2: the value of the Family Safety mode pref
// 3: detecting Family Safety mode failed
// 4: Family Safety was not enabled
// 5: Family Safety was enabled
// 6: failed to import the Family Safety root
// 7: successfully imported the root
void
nsNSSComponent::MaybeEnableFamilySafetyCompatibility()
{
#ifdef XP_WIN
  UnloadFamilySafetyRoot();
  if (!(IsWin8Point1OrLater() && !IsWin10OrLater())) {
    return;
  }
  // Detect but don't import by default.
  uint32_t familySafetyMode = Preferences::GetUint(kFamilySafetyModePref, 1);
  if (familySafetyMode > 2) {
    familySafetyMode = 0;
  }
  Telemetry::Accumulate(Telemetry::FAMILY_SAFETY, familySafetyMode);
  if (familySafetyMode == 0) {
    return;
  }
  bool familySafetyEnabled;
  nsresult rv = AccountHasFamilySafetyEnabled(familySafetyEnabled);
  if (NS_FAILED(rv)) {
    Telemetry::Accumulate(Telemetry::FAMILY_SAFETY, 3);
    return;
  }
  if (!familySafetyEnabled) {
    Telemetry::Accumulate(Telemetry::FAMILY_SAFETY, 4);
    return;
  }
  Telemetry::Accumulate(Telemetry::FAMILY_SAFETY, 5);
  if (familySafetyMode == 2) {
    rv = LoadFamilySafetyRoot();
    if (NS_FAILED(rv)) {
      Telemetry::Accumulate(Telemetry::FAMILY_SAFETY, 6);
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("failed to load Family Safety root"));
    } else {
      Telemetry::Accumulate(Telemetry::FAMILY_SAFETY, 7);
    }
  }
#endif // XP_WIN
}

#ifdef XP_WIN
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

void
nsNSSComponent::UnloadEnterpriseRoots()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("UnloadEnterpriseRoots"));
  if (!mEnterpriseRoots) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("no enterprise roots were present"));
    return;
  }
  // It would be intuitive to set the trust to { 0, 0, 0 } here. However, this
  // doesn't work for temporary certificates because CERT_ChangeCertTrust first
  // looks up the current trust settings in the permanent cert database, finds
  // that such trust doesn't exist, considers the current trust to be
  // { 0, 0, 0 }, and decides that it doesn't need to update the trust since
  // they're the same. To work around this, we set a non-zero flag to ensure
  // that the trust will get updated.
  CERTCertTrust trust = { CERTDB_USER, 0, 0 };
  for (CERTCertListNode* n = CERT_LIST_HEAD(mEnterpriseRoots.get());
       !CERT_LIST_END(n, mEnterpriseRoots.get()); n = CERT_LIST_NEXT(n)) {
    if (!n || !n->cert) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("library failure: CERTCertListNode null or lacks cert"));
      continue;
    }
    if (CERT_ChangeCertTrust(nullptr, n->cert, &trust) != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("couldn't untrust certificate for TLS server auth"));
    }
  }
  mEnterpriseRoots = nullptr;
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("unloaded enterprise roots"));
}

NS_IMETHODIMP
nsNSSComponent::GetEnterpriseRoots(nsIX509CertList** enterpriseRoots)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  NS_ENSURE_ARG_POINTER(enterpriseRoots);

  nsNSSShutDownPreventionLock lock;
  // nsNSSComponent isn't a nsNSSShutDownObject, so we can't check
  // isAlreadyShutDown(). However, since mEnterpriseRoots is cleared when NSS
  // shuts down, we can use that as a proxy for checking for NSS shutdown.
  // (Of course, it may also be the case that no enterprise roots were imported,
  // so we should just return a null list and NS_OK in this case.)
  if (!mEnterpriseRoots) {
    *enterpriseRoots = nullptr;
    return NS_OK;
  }
  UniqueCERTCertList enterpriseRootsCopy(
    nsNSSCertList::DupCertList(mEnterpriseRoots, lock));
  if (!enterpriseRootsCopy) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIX509CertList> enterpriseRootsCertList(
    new nsNSSCertList(Move(enterpriseRootsCopy), lock));
  if (!enterpriseRootsCertList) {
    return NS_ERROR_FAILURE;
  }
  enterpriseRootsCertList.forget(enterpriseRoots);
  return NS_OK;
}
#endif // XP_WIN

static const char* kEnterpriseRootModePref = "security.enterprise_roots.enabled";

void
nsNSSComponent::MaybeImportEnterpriseRoots()
{
#ifdef XP_WIN
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  UnloadEnterpriseRoots();
  bool importEnterpriseRoots = Preferences::GetBool(kEnterpriseRootModePref,
                                                    false);
  if (!importEnterpriseRoots) {
    return;
  }

  MOZ_ASSERT(!mEnterpriseRoots);
  mEnterpriseRoots.reset(CERT_NewCertList());
  if (!mEnterpriseRoots) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("failed to allocate a new CERTCertList for mEnterpriseRoots"));
    return;
  }

  ImportEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE);
  ImportEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY);
  ImportEnterpriseRootsForLocation(CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE);
#endif // XP_WIN
}

#ifdef XP_WIN
// Loads the enterprise roots at the registry location corresponding to the
// given location flag.
// Supported flags are:
//   CERT_SYSTEM_STORE_LOCAL_MACHINE
//     (for HKLM\SOFTWARE\Microsoft\SystemCertificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY
//     (for HKLM\SOFTWARE\Policies\Microsoft\SystemCertificates\Root\Certificates)
//   CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE
//     (for HKLM\SOFTWARE\Microsoft\EnterpriseCertificates\Root\Certificates)
void
nsNSSComponent::ImportEnterpriseRootsForLocation(DWORD locationFlag)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  MOZ_ASSERT(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
             locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
             locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
             "unexpected locationFlag for ImportEnterpriseRootsForLocation");
  if (!(locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
        locationFlag == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE)) {
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
  CERTCertTrust trust = {
    CERTDB_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_USER,
    0,
    0
  };
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
    if (nsCRT::strcmp(subjectName.get(), kMicrosoftFamilySafetyCN) == 0) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("skipping Family Safety Root"));
      continue;
    }
    MOZ_ASSERT(mEnterpriseRoots, "mEnterpriseRoots unexpectedly NULL?");
    if (!mEnterpriseRoots) {
      return;
    }
    if (CERT_AddCertToListTail(mEnterpriseRoots.get(), nssCertificate.get())
          != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("couldn't add cert to list"));
      continue;
    }
    if (CERT_ChangeCertTrust(nullptr, nssCertificate.get(), &trust)
          != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("couldn't trust certificate for TLS server auth"));
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Imported '%s'", subjectName.get()));
    numImported++;
    // now owned by mEnterpriseRoots
    Unused << nssCertificate.release();
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("imported %u roots", numImported));
}
#endif // XP_WIN

void
nsNSSComponent::LoadLoadableRoots()
{
  nsNSSShutDownPreventionLock locker;
  SECMODModule* RootsModule = nullptr;

  // In the past we used SECMOD_AddNewModule to load our module containing
  // root CA certificates. This caused problems, refer to bug 176501.
  // On startup, we fix our database and clean any stored module reference,
  // and will use SECMOD_LoadUserModule to temporarily load it
  // for the session. (This approach requires to clean up
  // using SECMOD_UnloadUserModule at the end of the session.)

  {
    // Find module containing root certs

    SECMODModuleList* list;
    SECMODListLock* lock = SECMOD_GetDefaultModuleListLock();
    if (!lock) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Error,
               ("Couldn't get the module list lock, can't install loadable roots\n"));
        return;
    }
    SECMOD_GetReadLock(lock);
    list = SECMOD_GetDefaultModuleList();

    while (!RootsModule && list) {
      SECMODModule* module = list->module;

      for (int i=0; i < module->slotCount; i++) {
        PK11SlotInfo* slot = module->slots[i];
        if (PK11_IsPresent(slot)) {
          if (PK11_HasRootCerts(slot)) {
            RootsModule = SECMOD_ReferenceModule(module);
            break;
          }
        }
      }

      list = list->next;
    }
    SECMOD_ReleaseReadLock(lock);
  }

  if (RootsModule) {
    int32_t modType;
    SECMOD_DeleteModule(RootsModule->commonName, &modType);
    SECMOD_DestroyModule(RootsModule);
    RootsModule = nullptr;
  }

  // Find the best Roots module for our purposes.
  // Prefer the application's installation directory,
  // but also ensure the library is at least the version we expect.

  nsAutoString modName;
  nsresult rv = GetPIPNSSBundleString("RootCertModuleName", modName);
  if (NS_FAILED(rv)) {
    // When running Cpp unit tests on Android, this will fail because string
    // bundles aren't available (see bug 1311077, bug 1228175 comment 12, and
    // bug 929655). Because the module name is really only for display purposes,
    // we can just hard-code the value here. Furthermore, if we want to be able
    // to stop using string bundles in PSM in this way, we'll have to hard-code
    // the string and only use the localized version when displaying it to the
    // user, so this is a step in that direction anyway.
    modName.AssignLiteral("Builtin Roots Module");
  }

  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  if (!directoryService)
    return;

  static const char nss_lib[] = "nss3";
  const char* possible_ckbi_locations[] = {
    nss_lib, // This special value means: search for ckbi in the directory
             // where nss3 is.
    NS_XPCOM_CURRENT_PROCESS_DIR,
    NS_GRE_DIR,
    0 // This special value means:
      //   search for ckbi in the directories on the shared
      //   library/DLL search path
  };

  for (size_t il = 0; il < sizeof(possible_ckbi_locations)/sizeof(const char*); ++il) {
    nsAutoCString libDir;

    if (possible_ckbi_locations[il]) {
      nsCOMPtr<nsIFile> mozFile;
      if (possible_ckbi_locations[il] == nss_lib) {
        // Get the location of the nss3 library.
        char* nss_path = PR_GetLibraryFilePathname(DLL_PREFIX "nss3" DLL_SUFFIX,
                                                   (PRFuncPtr) NSS_Initialize);
        if (!nss_path) {
          continue;
        }
        // Get the directory containing the nss3 library.
        nsCOMPtr<nsIFile> nssLib(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) {
          rv = nssLib->InitWithNativePath(nsDependentCString(nss_path));
        }
        PR_Free(nss_path);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIFile> file;
          if (NS_SUCCEEDED(nssLib->GetParent(getter_AddRefs(file)))) {
            mozFile = do_QueryInterface(file);
          }
        }
      } else {
        directoryService->Get( possible_ckbi_locations[il],
                               NS_GET_IID(nsIFile),
                               getter_AddRefs(mozFile));
      }

      if (!mozFile) {
        continue;
      }

      if (NS_FAILED(mozFile->GetNativePath(libDir))) {
        continue;
      }
    }

    NS_ConvertUTF16toUTF8 modNameUTF8(modName);
    if (mozilla::psm::LoadLoadableRoots(
            libDir.Length() > 0 ? libDir.get() : nullptr,
            modNameUTF8.get()) == SECSuccess) {
      break;
    }
  }
}

void
nsNSSComponent::UnloadLoadableRoots()
{
  nsresult rv;
  nsAutoString modName;
  rv = GetPIPNSSBundleString("RootCertModuleName", modName);
  if (NS_FAILED(rv)) return;

  NS_ConvertUTF16toUTF8 modNameUTF8(modName);
  ::mozilla::psm::UnloadLoadableRoots(modNameUTF8.get());
}

nsresult
nsNSSComponent::ConfigureInternalPKCS11Token()
{
  nsNSSShutDownPreventionLock locker;
  nsAutoString manufacturerID;
  nsAutoString libraryDescription;
  nsAutoString tokenDescription;
  nsAutoString privateTokenDescription;
  nsAutoString slotDescription;
  nsAutoString privateSlotDescription;
  nsAutoString fips140SlotDescription;
  nsAutoString fips140TokenDescription;

  nsresult rv;
  rv = GetPIPNSSBundleString("ManufacturerID", manufacturerID);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("LibraryDescription", libraryDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("TokenDescription", tokenDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("PrivateTokenDescription", privateTokenDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("SlotDescription", slotDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("PrivateSlotDescription", privateSlotDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("Fips140SlotDescription", fips140SlotDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("Fips140TokenDescription", fips140TokenDescription);
  if (NS_FAILED(rv)) return rv;

  PK11_ConfigurePKCS11(NS_ConvertUTF16toUTF8(manufacturerID).get(),
                       NS_ConvertUTF16toUTF8(libraryDescription).get(),
                       NS_ConvertUTF16toUTF8(tokenDescription).get(),
                       NS_ConvertUTF16toUTF8(privateTokenDescription).get(),
                       NS_ConvertUTF16toUTF8(slotDescription).get(),
                       NS_ConvertUTF16toUTF8(privateSlotDescription).get(),
                       NS_ConvertUTF16toUTF8(fips140SlotDescription).get(),
                       NS_ConvertUTF16toUTF8(fips140TokenDescription).get(),
                       0, 0);
  return NS_OK;
}

nsresult
nsNSSComponent::InitializePIPNSSBundle()
{
  // Called during init only, no mutex required.

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(bundleService);
#endif
  if (NS_FAILED(rv) || !bundleService)
    return NS_ERROR_FAILURE;

  bundleService->CreateBundle("chrome://pipnss/locale/pipnss.properties",
                              getter_AddRefs(mPIPNSSBundle));
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(mPIPNSSBundle);
#endif
  if (!mPIPNSSBundle)
    rv = NS_ERROR_FAILURE;

  bundleService->CreateBundle("chrome://pipnss/locale/nsserrors.properties",
                              getter_AddRefs(mNSSErrorsBundle));
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(mNSSErrorsBundle);
#endif
  if (!mNSSErrorsBundle)
    rv = NS_ERROR_FAILURE;

  return rv;
}

// Table of pref names and SSL cipher ID
typedef struct {
  const char* pref;
  long id;
  bool enabledByDefault;
} CipherPref;

// Update the switch statement in AccumulateCipherSuite in nsNSSCallbacks.cpp
// when you add/remove cipher suites here.
static const CipherPref sCipherPrefs[] = {
 { "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
   TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, true },
 { "security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256",
   TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, true },

 { "security.ssl3.ecdhe_ecdsa_chacha20_poly1305_sha256",
   TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, true },
 { "security.ssl3.ecdhe_rsa_chacha20_poly1305_sha256",
   TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, true },

 { "security.ssl3.ecdhe_ecdsa_aes_256_gcm_sha384",
   TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, true },
 { "security.ssl3.ecdhe_rsa_aes_256_gcm_sha384",
   TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, true },

 { "security.ssl3.ecdhe_rsa_aes_128_sha",
   TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, true },
 { "security.ssl3.ecdhe_ecdsa_aes_128_sha",
   TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, true },

 { "security.ssl3.ecdhe_rsa_aes_256_sha",
   TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, true },
 { "security.ssl3.ecdhe_ecdsa_aes_256_sha",
   TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, true },

 { "security.ssl3.dhe_rsa_aes_128_sha",
   TLS_DHE_RSA_WITH_AES_128_CBC_SHA, true },

 { "security.ssl3.dhe_rsa_aes_256_sha",
   TLS_DHE_RSA_WITH_AES_256_CBC_SHA, true },

 { "security.tls13.aes_128_gcm_sha256",
   TLS_AES_128_GCM_SHA256, true },
 { "security.tls13.chacha20_poly1305_sha256",
   TLS_CHACHA20_POLY1305_SHA256, true },
 { "security.tls13.aes_256_gcm_sha384",
   TLS_AES_256_GCM_SHA384, true },

 { "security.ssl3.rsa_aes_128_sha",
   TLS_RSA_WITH_AES_128_CBC_SHA, true }, // deprecated (RSA key exchange)
 { "security.ssl3.rsa_aes_256_sha",
   TLS_RSA_WITH_AES_256_CBC_SHA, true }, // deprecated (RSA key exchange)
 { "security.ssl3.rsa_des_ede3_sha",
   TLS_RSA_WITH_3DES_EDE_CBC_SHA, true }, // deprecated (RSA key exchange, 3DES)

 // All the rest are disabled

 { nullptr, 0 } // end marker
};

// This function will convert from pref values like 1, 2, ...
// to the internal values of SSL_LIBRARY_VERSION_TLS_1_0,
// SSL_LIBRARY_VERSION_TLS_1_1, ...
/*static*/ void
nsNSSComponent::FillTLSVersionRange(SSLVersionRange& rangeOut,
                                    uint32_t minFromPrefs,
                                    uint32_t maxFromPrefs,
                                    SSLVersionRange defaults)
{
  rangeOut = defaults;
  // determine what versions are supported
  SSLVersionRange supported;
  if (SSL_VersionRangeGetSupported(ssl_variant_stream, &supported)
        != SECSuccess) {
    return;
  }

  // Clip the defaults by what NSS actually supports to enable
  // working with a system NSS with different ranges.
  rangeOut.min = std::max(rangeOut.min, supported.min);
  rangeOut.max = std::min(rangeOut.max, supported.max);

  // convert min/maxFromPrefs to the internal representation
  minFromPrefs += SSL_LIBRARY_VERSION_3_0;
  maxFromPrefs += SSL_LIBRARY_VERSION_3_0;
  // if min/maxFromPrefs are invalid, use defaults
  if (minFromPrefs > maxFromPrefs ||
      minFromPrefs < supported.min || maxFromPrefs > supported.max ||
      minFromPrefs < SSL_LIBRARY_VERSION_TLS_1_0) {
    return;
  }

  // fill out rangeOut
  rangeOut.min = (uint16_t) minFromPrefs;
  rangeOut.max = (uint16_t) maxFromPrefs;
}

static const int32_t OCSP_ENABLED_DEFAULT = 1;
static const bool REQUIRE_SAFE_NEGOTIATION_DEFAULT = false;
static const bool FALSE_START_ENABLED_DEFAULT = true;
static const bool ALPN_ENABLED_DEFAULT = false;
static const bool ENABLED_0RTT_DATA_DEFAULT = false;

static void
ConfigureTLSSessionIdentifiers()
{
  bool disableSessionIdentifiers =
    Preferences::GetBool("security.ssl.disable_session_identifiers", false);
  SSL_OptionSetDefault(SSL_ENABLE_SESSION_TICKETS, !disableSessionIdentifiers);
  SSL_OptionSetDefault(SSL_NO_CACHE, disableSessionIdentifiers);
}

namespace {

class CipherSuiteChangeObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsresult StartObserve();

protected:
  virtual ~CipherSuiteChangeObserver() {}

private:
  static StaticRefPtr<CipherSuiteChangeObserver> sObserver;
  CipherSuiteChangeObserver() {}
};

NS_IMPL_ISUPPORTS(CipherSuiteChangeObserver, nsIObserver)

// static
StaticRefPtr<CipherSuiteChangeObserver> CipherSuiteChangeObserver::sObserver;

// static
nsresult
CipherSuiteChangeObserver::StartObserve()
{
  NS_ASSERTION(NS_IsMainThread(), "CipherSuiteChangeObserver::StartObserve() can only be accessed in main thread");
  if (!sObserver) {
    RefPtr<CipherSuiteChangeObserver> observer = new CipherSuiteChangeObserver();
    nsresult rv = Preferences::AddStrongObserver(observer.get(), "security.");
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
    if (NS_FAILED(rv)) {
      sObserver = nullptr;
      return rv;
    }

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                 false);

    sObserver = observer;
  }
  return NS_OK;
}

nsresult
CipherSuiteChangeObserver::Observe(nsISupports* aSubject,
                                   const char* aTopic,
                                   const char16_t* someData)
{
  NS_ASSERTION(NS_IsMainThread(), "CipherSuiteChangeObserver::Observe can only be accessed in main thread");
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    NS_ConvertUTF16toUTF8  prefName(someData);
    // Look through the cipher table and set according to pref setting
    const CipherPref* const cp = sCipherPrefs;
    for (size_t i = 0; cp[i].pref; ++i) {
      if (prefName.Equals(cp[i].pref)) {
        bool cipherEnabled = Preferences::GetBool(cp[i].pref,
                                                  cp[i].enabledByDefault);
        SSL_CipherPrefSetDefault(cp[i].id, cipherEnabled);
        SSL_ClearSessionCache();
        break;
      }
    }
  } else if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Preferences::RemoveObserver(this, "security.");
    MOZ_ASSERT(sObserver.get() == this);
    sObserver = nullptr;
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
  return NS_OK;
}

} // namespace

// Caller must hold a lock on nsNSSComponent::mutex when calling this function
void nsNSSComponent::setValidationOptions(bool isInitialSetting,
                                          const MutexAutoLock& lock)
{
  // This preference controls whether we do OCSP fetching and does not affect
  // OCSP stapling.
  // 0 = disabled, 1 = enabled
  int32_t ocspEnabled = Preferences::GetInt("security.OCSP.enabled",
                                            OCSP_ENABLED_DEFAULT);

  bool ocspRequired = ocspEnabled &&
    Preferences::GetBool("security.OCSP.require", false);

  // We measure the setting of the pref at startup only to minimize noise by
  // addons that may muck with the settings, though it probably doesn't matter.
  if (isInitialSetting) {
    Telemetry::Accumulate(Telemetry::CERT_OCSP_ENABLED, ocspEnabled);
    Telemetry::Accumulate(Telemetry::CERT_OCSP_REQUIRED, ocspRequired);
  }

  bool ocspStaplingEnabled = Preferences::GetBool("security.ssl.enable_ocsp_stapling",
                                                  true);
  PublicSSLState()->SetOCSPStaplingEnabled(ocspStaplingEnabled);
  PrivateSSLState()->SetOCSPStaplingEnabled(ocspStaplingEnabled);

  bool ocspMustStapleEnabled = Preferences::GetBool("security.ssl.enable_ocsp_must_staple",
                                                    true);
  PublicSSLState()->SetOCSPMustStapleEnabled(ocspMustStapleEnabled);
  PrivateSSLState()->SetOCSPMustStapleEnabled(ocspMustStapleEnabled);

  const CertVerifier::CertificateTransparencyMode defaultCTMode =
    CertVerifier::CertificateTransparencyMode::TelemetryOnly;
  CertVerifier::CertificateTransparencyMode ctMode =
    static_cast<CertVerifier::CertificateTransparencyMode>
      (Preferences::GetInt("security.pki.certificate_transparency.mode",
                           static_cast<int32_t>(defaultCTMode)));
  switch (ctMode) {
    case CertVerifier::CertificateTransparencyMode::Disabled:
    case CertVerifier::CertificateTransparencyMode::TelemetryOnly:
      break;
    default:
      ctMode = defaultCTMode;
      break;
  }
  bool sctsEnabled =
    ctMode != CertVerifier::CertificateTransparencyMode::Disabled;
  PublicSSLState()->SetSignedCertTimestampsEnabled(sctsEnabled);
  PrivateSSLState()->SetSignedCertTimestampsEnabled(sctsEnabled);

  CertVerifier::PinningMode pinningMode =
    static_cast<CertVerifier::PinningMode>
      (Preferences::GetInt("security.cert_pinning.enforcement_level",
                           CertVerifier::pinningDisabled));
  if (pinningMode > CertVerifier::pinningEnforceTestMode) {
    pinningMode = CertVerifier::pinningDisabled;
  }

  CertVerifier::SHA1Mode sha1Mode = static_cast<CertVerifier::SHA1Mode>
      (Preferences::GetInt("security.pki.sha1_enforcement_level",
                           static_cast<int32_t>(CertVerifier::SHA1Mode::Allowed)));
  switch (sha1Mode) {
    case CertVerifier::SHA1Mode::Allowed:
    case CertVerifier::SHA1Mode::Forbidden:
    case CertVerifier::SHA1Mode::UsedToBeBefore2016ButNowIsForbidden:
    case CertVerifier::SHA1Mode::ImportedRoot:
    case CertVerifier::SHA1Mode::ImportedRootOrBefore2016:
      break;
    default:
      sha1Mode = CertVerifier::SHA1Mode::Allowed;
      break;
  }

  // Convert a previously-available setting to a safe one.
  if (sha1Mode == CertVerifier::SHA1Mode::UsedToBeBefore2016ButNowIsForbidden) {
    sha1Mode = CertVerifier::SHA1Mode::Forbidden;
  }

  BRNameMatchingPolicy::Mode nameMatchingMode =
    static_cast<BRNameMatchingPolicy::Mode>
      (Preferences::GetInt("security.pki.name_matching_mode",
                           static_cast<int32_t>(BRNameMatchingPolicy::Mode::DoNotEnforce)));
  switch (nameMatchingMode) {
    case BRNameMatchingPolicy::Mode::Enforce:
    case BRNameMatchingPolicy::Mode::EnforceAfter23August2015:
    case BRNameMatchingPolicy::Mode::EnforceAfter23August2016:
    case BRNameMatchingPolicy::Mode::DoNotEnforce:
      break;
    default:
      nameMatchingMode = BRNameMatchingPolicy::Mode::DoNotEnforce;
      break;
  }

  NetscapeStepUpPolicy netscapeStepUpPolicy =
    static_cast<NetscapeStepUpPolicy>
      (Preferences::GetUint("security.pki.netscape_step_up_policy",
                            static_cast<uint32_t>(NetscapeStepUpPolicy::AlwaysMatch)));
  switch (netscapeStepUpPolicy) {
    case NetscapeStepUpPolicy::AlwaysMatch:
    case NetscapeStepUpPolicy::MatchBefore23August2016:
    case NetscapeStepUpPolicy::MatchBefore23August2015:
    case NetscapeStepUpPolicy::NeverMatch:
      break;
    default:
      netscapeStepUpPolicy = NetscapeStepUpPolicy::AlwaysMatch;
      break;
  }

  CertVerifier::OcspDownloadConfig odc;
  CertVerifier::OcspStrictConfig osc;
  CertVerifier::OcspGetConfig ogc;
  uint32_t certShortLifetimeInDays;

  GetRevocationBehaviorFromPrefs(&odc, &osc, &ogc, &certShortLifetimeInDays,
                                 lock);
  mDefaultCertVerifier = new SharedCertVerifier(odc, osc, ogc,
                                                certShortLifetimeInDays,
                                                pinningMode, sha1Mode,
                                                nameMatchingMode,
                                                netscapeStepUpPolicy,
                                                ctMode);
}

// Enable the TLS versions given in the prefs, defaulting to TLS 1.0 (min) and
// TLS 1.2 (max) when the prefs aren't set or set to invalid values.
nsresult
nsNSSComponent::setEnabledTLSVersions()
{
  // keep these values in sync with security-prefs.js
  // 1 means TLS 1.0, 2 means TLS 1.1, etc.
  static const uint32_t PSM_DEFAULT_MIN_TLS_VERSION = 1;
  static const uint32_t PSM_DEFAULT_MAX_TLS_VERSION = 4;

  uint32_t minFromPrefs = Preferences::GetUint("security.tls.version.min",
                                               PSM_DEFAULT_MIN_TLS_VERSION);
  uint32_t maxFromPrefs = Preferences::GetUint("security.tls.version.max",
                                               PSM_DEFAULT_MAX_TLS_VERSION);

  SSLVersionRange defaults = {
    SSL_LIBRARY_VERSION_3_0 + PSM_DEFAULT_MIN_TLS_VERSION,
    SSL_LIBRARY_VERSION_3_0 + PSM_DEFAULT_MAX_TLS_VERSION
  };
  SSLVersionRange filledInRange;
  FillTLSVersionRange(filledInRange, minFromPrefs, maxFromPrefs, defaults);

  SECStatus srv =
    SSL_VersionRangeSetDefault(ssl_variant_stream, &filledInRange);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
GetNSSProfilePath(nsAutoCString& aProfilePath)
{
  aProfilePath.Truncate();
  const char* dbDirOverride = getenv("MOZPSM_NSSDBDIR_OVERRIDE");
  if (dbDirOverride && strlen(dbDirOverride) > 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("Using specified MOZPSM_NSSDBDIR_OVERRIDE as NSS DB dir: %s\n",
            dbDirOverride));
    aProfilePath.Assign(dbDirOverride);
    return NS_OK;
  }

  nsCOMPtr<nsIFile> profileFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileFile));
  if (NS_FAILED(rv)) {
    NS_WARNING("NSS will be initialized without a profile directory. "
               "Some things may not work as expected.");
    return NS_OK;
  }

#if defined(XP_WIN)
  // Native path will drop Unicode characters that cannot be mapped to system's
  // codepage, using short (canonical) path as workaround.
  nsCOMPtr<nsILocalFileWin> profileFileWin(do_QueryInterface(profileFile));
  if (!profileFileWin) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
           ("Could not get nsILocalFileWin for profile directory.\n"));
    return NS_ERROR_FAILURE;
  }
  rv = profileFileWin->GetNativeCanonicalPath(aProfilePath);
#else
  rv = profileFile->GetNativePath(aProfilePath);
#endif
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
           ("Could not get native path for profile directory.\n"));
    return rv;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("NSS profile at '%s'\n", aProfilePath.get()));
  return NS_OK;
}

nsresult
nsNSSComponent::InitializeNSS()
{
  // Can be called both during init and profile change.
  // Needs mutex protection.

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::InitializeNSS\n"));

  static_assert(nsINSSErrorsService::NSS_SEC_ERROR_BASE == SEC_ERROR_BASE &&
                nsINSSErrorsService::NSS_SEC_ERROR_LIMIT == SEC_ERROR_LIMIT &&
                nsINSSErrorsService::NSS_SSL_ERROR_BASE == SSL_ERROR_BASE &&
                nsINSSErrorsService::NSS_SSL_ERROR_LIMIT == SSL_ERROR_LIMIT,
                "You must update the values in nsINSSErrorsService.idl");

  MutexAutoLock lock(mutex);

#ifdef ANDROID
  MOZ_RELEASE_ASSERT(!mNSSInitialized);
#endif
  if (mNSSInitialized) {
    // We should never try to initialize NSS more than once in a process.
    MOZ_ASSERT_UNREACHABLE("Trying to initialize NSS twice");
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS Initialization beginning\n"));

  // The call to ConfigureInternalPKCS11Token needs to be done before NSS is initialized,
  // but affects only static data.
  // If we could assume i18n will not change between profiles, one call per application
  // run were sufficient. As I can't predict what happens in the future, let's repeat
  // this call for every re-init of NSS.

  ConfigureInternalPKCS11Token();

  nsAutoCString profileStr;
  nsresult rv = GetNSSProfilePath(profileStr);
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SECStatus init_rv = SECFailure;
  bool nocertdb = Preferences::GetBool("security.nocertdb", false);
  bool inSafeMode = true;
  nsCOMPtr<nsIXULRuntime> runtime(do_GetService("@mozilla.org/xre/runtime;1"));
  // There might not be an nsIXULRuntime in embedded situations. This will
  // default to assuming we are in safe mode (as a result, no external PKCS11
  // modules will be loaded).
  if (runtime) {
    rv = runtime->GetInSafeMode(&inSafeMode);
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("inSafeMode: %u\n", inSafeMode));

  if (!nocertdb && !profileStr.IsEmpty()) {
    // First try to initialize the NSS DB in read/write mode.
    // Only load PKCS11 modules if we're not in safe mode.
    init_rv = ::mozilla::psm::InitializeNSS(profileStr.get(), false, !inSafeMode);
    // If that fails, attempt read-only mode.
    if (init_rv != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("could not init NSS r/w in %s\n", profileStr.get()));
      init_rv = ::mozilla::psm::InitializeNSS(profileStr.get(), true, !inSafeMode);
    }
    if (init_rv != SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("could not init in r/o either\n"));
    }
  }
  // If we haven't succeeded in initializing the DB in our profile
  // directory or we don't have a profile at all, or the "security.nocertdb"
  // pref has been set to "true", attempt to initialize with no DB.
  if (nocertdb || init_rv != SECSuccess) {
    init_rv = NSS_NoDB_Init(nullptr);
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(init_rv == SECSuccess);
#endif
  }
  if (init_rv != SECSuccess) {
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(false);
#endif
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("could not initialize NSS - panicking\n"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // ensure we have an initial value for the content signer root
  mContentSigningRootHash =
    Preferences::GetString("security.content.signature.root_hash");

  mNSSInitialized = true;

  PK11_SetPasswordFunc(PK11PasswordPrompt);

  SharedSSLState::GlobalInit();

  // Register an observer so we can inform NSS when these prefs change
  Preferences::AddStrongObserver(this, "security.");

  SSL_OptionSetDefault(SSL_ENABLE_SSL2, false);
  SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, false);

  rv = setEnabledTLSVersions();
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
  if (NS_FAILED(rv)) {
    return NS_ERROR_UNEXPECTED;
  }

  DisableMD5();
  LoadLoadableRoots();

  rv = LoadExtendedValidationInfo();
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("failed to load EV info"));
    return rv;
  }

  MaybeEnableFamilySafetyCompatibility();
  MaybeImportEnterpriseRoots();

  ConfigureTLSSessionIdentifiers();

  bool requireSafeNegotiation =
    Preferences::GetBool("security.ssl.require_safe_negotiation",
                         REQUIRE_SAFE_NEGOTIATION_DEFAULT);
  SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, requireSafeNegotiation);

  SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_REQUIRES_XTN);

  SSL_OptionSetDefault(SSL_ENABLE_EXTENDED_MASTER_SECRET, true);

  SSL_OptionSetDefault(SSL_ENABLE_FALSE_START,
                       Preferences::GetBool("security.ssl.enable_false_start",
                                            FALSE_START_ENABLED_DEFAULT));

  // SSL_ENABLE_ALPN also requires calling SSL_SetNextProtoNego in order for
  // the extensions to be negotiated.
  // WebRTC does not do that so it will not use ALPN even when this preference
  // is true.
  SSL_OptionSetDefault(SSL_ENABLE_ALPN,
                       Preferences::GetBool("security.ssl.enable_alpn",
                                            ALPN_ENABLED_DEFAULT));

  SSL_OptionSetDefault(SSL_ENABLE_0RTT_DATA,
                       Preferences::GetBool("security.tls.enable_0rtt_data",
                                            ENABLED_0RTT_DATA_DEFAULT));

  if (NS_FAILED(InitializeCipherSuite())) {
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(false);
#endif
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("Unable to initialize cipher suite settings\n"));
    return NS_ERROR_FAILURE;
  }

  // TLSServerSocket may be run with the session cache enabled. It is necessary
  // to call this once before that can happen. This specifies a maximum of 1000
  // cache entries (the default number of cache entries is 10000, which seems a
  // little excessive as there probably won't be that many clients connecting to
  // any TLSServerSockets the browser runs.)
  // Note that this must occur before any calls to SSL_ClearSessionCache
  // (otherwise memory will leak).
  if (SSL_ConfigServerSessionIDCache(1000, 0, 0, nullptr) != SECSuccess) {
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(false);
#endif
    return NS_ERROR_FAILURE;
  }

  // ensure the CertBlocklist is initialised
  nsCOMPtr<nsICertBlocklist> certList = do_GetService(NS_CERTBLOCKLIST_CONTRACTID);
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(certList);
#endif
  if (!certList) {
    return NS_ERROR_FAILURE;
  }

  // dynamic options from prefs
  setValidationOptions(true, lock);

#ifndef MOZ_NO_SMART_CARDS
  LaunchSmartCardThreads();
#endif

  mozilla::pkix::RegisterErrorTable();

  // Initialize the site security service
  nsCOMPtr<nsISiteSecurityService> sssService =
    do_GetService(NS_SSSERVICE_CONTRACTID);
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(sssService);
#endif
  if (!sssService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Cannot initialize site security service\n"));
    return NS_ERROR_FAILURE;
  }

  // Initialize the cert override service
  nsCOMPtr<nsICertOverrideService> coService =
    do_GetService(NS_CERTOVERRIDE_CONTRACTID);
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(coService);
#endif
  if (!coService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Cannot initialize cert override service\n"));
    return NS_ERROR_FAILURE;
  }

  if (PK11_IsFIPS()) {
    Telemetry::Accumulate(Telemetry::FIPS_ENABLED, true);
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS Initialization done\n"));
  return NS_OK;
}

void
nsNSSComponent::ShutdownNSS()
{
  // Can be called both during init and profile change,
  // needs mutex protection.

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::ShutdownNSS\n"));
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mutex);

  if (mNSSInitialized) {
    mNSSInitialized = false;

    PK11_SetPasswordFunc((PK11PasswordFunc)nullptr);

    Preferences::RemoveObserver(this, "security.");

#ifdef XP_WIN
    mFamilySafetyRoot = nullptr;
    mEnterpriseRoots = nullptr;
#endif

#ifndef MOZ_NO_SMART_CARDS
    ShutdownSmartCardThreads();
#endif
    SSL_ClearSessionCache();
    // TLSServerSocket may be run with the session cache enabled. This ensures
    // those resources are cleaned up.
    Unused << SSL_ShutdownServerSessionIDCache();

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("evaporating psm resources"));
    if (NS_FAILED(nsNSSShutDownList::evaporateAllNSSResources())) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("failed to evaporate resources"));
      return;
    }
    UnloadLoadableRoots();
    EnsureNSSInitialized(nssShutdown);
    if (SECSuccess != ::NSS_Shutdown()) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("NSS SHUTDOWN FAILURE"));
    } else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS shutdown =====>> OK <<====="));
    }
  }
}

nsresult
nsNSSComponent::Init()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv = NS_OK;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Beginning NSS initialization\n"));

  rv = InitializePIPNSSBundle();
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("Unable to create pipnss bundle.\n"));
    return rv;
  }

  // Access our string bundles now, this prevents assertions from I/O
  // - nsStandardURL not thread-safe
  // - wrong thread: 'NS_IsMainThread()' in nsIOService.cpp
  // when loading error strings on the SSL threads.
  {
    NS_NAMED_LITERAL_STRING(dummy_name, "dummy");
    nsXPIDLString result;
    mPIPNSSBundle->GetStringFromName(dummy_name.get(),
                                     getter_Copies(result));
    mNSSErrorsBundle->GetStringFromName(dummy_name.get(),
                                        getter_Copies(result));
  }


  rv = InitializeNSS();
#ifdef ANDROID
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
#endif
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("nsNSSComponent::InitializeNSS() failed\n"));
    return rv;
  }

  RememberCertErrorsTable::Init();

  return RegisterObservers();
}

// nsISupports Implementation for the class
NS_IMPL_ISUPPORTS(nsNSSComponent,
                  nsINSSComponent,
                  nsIObserver)

static const char* const PROFILE_BEFORE_CHANGE_TOPIC = "profile-before-change";

NS_IMETHODIMP
nsNSSComponent::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* someData)
{
  if (nsCRT::strcmp(aTopic, PROFILE_BEFORE_CHANGE_TOPIC) == 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("receiving profile change topic\n"));
    DoProfileBeforeChange();
  } else if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    nsNSSShutDownPreventionLock locker;
    bool clearSessionCache = true;
    NS_ConvertUTF16toUTF8  prefName(someData);

    if (prefName.EqualsLiteral("security.tls.version.min") ||
        prefName.EqualsLiteral("security.tls.version.max")) {
      (void) setEnabledTLSVersions();
    } else if (prefName.EqualsLiteral("security.ssl.require_safe_negotiation")) {
      bool requireSafeNegotiation =
        Preferences::GetBool("security.ssl.require_safe_negotiation",
                             REQUIRE_SAFE_NEGOTIATION_DEFAULT);
      SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, requireSafeNegotiation);
    } else if (prefName.EqualsLiteral("security.ssl.enable_false_start")) {
      SSL_OptionSetDefault(SSL_ENABLE_FALSE_START,
                           Preferences::GetBool("security.ssl.enable_false_start",
                                                FALSE_START_ENABLED_DEFAULT));
    } else if (prefName.EqualsLiteral("security.ssl.enable_alpn")) {
      SSL_OptionSetDefault(SSL_ENABLE_ALPN,
                           Preferences::GetBool("security.ssl.enable_alpn",
                                                ALPN_ENABLED_DEFAULT));
    } else if (prefName.EqualsLiteral("security.tls.enable_0rtt_data")) {
      SSL_OptionSetDefault(SSL_ENABLE_0RTT_DATA,
                           Preferences::GetBool("security.tls.enable_0rtt_data",
                                                ENABLED_0RTT_DATA_DEFAULT));
    } else if (prefName.Equals("security.ssl.disable_session_identifiers")) {
      ConfigureTLSSessionIdentifiers();
    } else if (prefName.EqualsLiteral("security.OCSP.enabled") ||
               prefName.EqualsLiteral("security.OCSP.require") ||
               prefName.EqualsLiteral("security.OCSP.GET.enabled") ||
               prefName.EqualsLiteral("security.pki.cert_short_lifetime_in_days") ||
               prefName.EqualsLiteral("security.ssl.enable_ocsp_stapling") ||
               prefName.EqualsLiteral("security.ssl.enable_ocsp_must_staple") ||
               prefName.EqualsLiteral("security.pki.certificate_transparency.mode") ||
               prefName.EqualsLiteral("security.cert_pinning.enforcement_level") ||
               prefName.EqualsLiteral("security.pki.sha1_enforcement_level") ||
               prefName.EqualsLiteral("security.pki.name_matching_mode") ||
               prefName.EqualsLiteral("security.pki.netscape_step_up_policy")) {
      MutexAutoLock lock(mutex);
      setValidationOptions(false, lock);
#ifdef DEBUG
    } else if (prefName.EqualsLiteral("security.test.built_in_root_hash")) {
      MutexAutoLock lock(mutex);
      mTestBuiltInRootHash = Preferences::GetString("security.test.built_in_root_hash");
#endif // DEBUG
    } else if (prefName.Equals(kFamilySafetyModePref)) {
      MaybeEnableFamilySafetyCompatibility();
    } else if (prefName.EqualsLiteral("security.content.signature.root_hash")) {
      MutexAutoLock lock(mutex);
      mContentSigningRootHash =
        Preferences::GetString("security.content.signature.root_hash");
    } else if (prefName.Equals(kEnterpriseRootModePref)) {
      MaybeImportEnterpriseRoots();
    } else {
      clearSessionCache = false;
    }
    if (clearSessionCache)
      SSL_ClearSessionCache();
  }

  return NS_OK;
}

/*static*/ nsresult
nsNSSComponent::GetNewPrompter(nsIPrompt** result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("nsSDRContext::GetNewPrompter called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = wwatch->GetNewPrompter(0, result);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

/*static*/ nsresult
nsNSSComponent::ShowAlertWithConstructedString(const nsString& message)
{
  nsCOMPtr<nsIPrompt> prompter;
  nsresult rv = GetNewPrompter(getter_AddRefs(prompter));
  if (prompter) {
    rv = prompter->Alert(nullptr, message.get());
  }
  return rv;
}

NS_IMETHODIMP
nsNSSComponent::ShowAlertFromStringBundle(const char* messageID)
{
  nsString message;
  nsresult rv;

  rv = GetPIPNSSBundleString(messageID, message);
  if (NS_FAILED(rv)) {
    NS_ERROR("GetPIPNSSBundleString failed");
    return rv;
  }

  return ShowAlertWithConstructedString(message);
}

nsresult nsNSSComponent::LogoutAuthenticatedPK11()
{
  nsCOMPtr<nsICertOverrideService> icos =
    do_GetService("@mozilla.org/security/certoverride;1");
  if (icos) {
    icos->ClearValidityOverride(
            NS_LITERAL_CSTRING("all:temporary-certificates"),
            0);
  }

  nsClientAuthRememberService::ClearAllRememberedDecisions();

  return nsNSSShutDownList::doPK11Logout();
}

nsresult
nsNSSComponent::RegisterObservers()
{
  // Happens once during init only, no mutex protection.

  nsCOMPtr<nsIObserverService> observerService(
    do_GetService("@mozilla.org/observer-service;1"));
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(observerService);
#endif
  if (!observerService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSComponent: couldn't get observer service\n"));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent: adding observers\n"));
  // Using false for the ownsweak parameter means the observer service will
  // keep a strong reference to this component. As a result, this will live at
  // least as long as the observer service.
  observerService->AddObserver(this, PROFILE_BEFORE_CHANGE_TOPIC, false);

  return NS_OK;
}

void
nsNSSComponent::DoProfileBeforeChange()
{
  bool needsCleanup = true;

  {
    MutexAutoLock lock(mutex);

    if (!mNSSInitialized) {
      // Make sure we don't try to cleanup if we have already done so.
      // This makes sure we behave safely, in case we are notified
      // multiple times.
      needsCleanup = false;
    }
  }

  if (needsCleanup) {
    ShutdownNSS();
  }
}

NS_IMETHODIMP
nsNSSComponent::IsNSSInitialized(bool* initialized)
{
  MutexAutoLock lock(mutex);
  *initialized = mNSSInitialized;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsNSSComponent::IsCertTestBuiltInRoot(CERTCertificate* cert, bool& result)
{
  MutexAutoLock lock(mutex);
  MOZ_ASSERT(mNSSInitialized);

  result = false;

  if (mTestBuiltInRootHash.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(cert);
  if (!nsc) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString certHash;
  nsresult rv = nsc->GetSha256Fingerprint(certHash);
  if (NS_FAILED(rv)) {
    return rv;
  }

  result = mTestBuiltInRootHash.Equals(certHash);
  return NS_OK;
}
#endif // DEBUG

NS_IMETHODIMP
nsNSSComponent::IsCertContentSigningRoot(CERTCertificate* cert, bool& result)
{
  MutexAutoLock lock(mutex);
  MOZ_ASSERT(mNSSInitialized);

  result = false;

  if (mContentSigningRootHash.IsEmpty()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("mContentSigningRootHash is empty"));
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(cert);
  if (!nsc) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("creating nsNSSCertificate failed"));
    return NS_ERROR_FAILURE;
  }
  nsAutoString certHash;
  nsresult rv = nsc->GetSha256Fingerprint(certHash);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("getting cert fingerprint failed"));
    return rv;
  }

  result = mContentSigningRootHash.Equals(certHash);
  return NS_OK;
}

SharedCertVerifier::~SharedCertVerifier() { }

already_AddRefed<SharedCertVerifier>
nsNSSComponent::GetDefaultCertVerifier()
{
  MutexAutoLock lock(mutex);
  MOZ_ASSERT(mNSSInitialized);
  RefPtr<SharedCertVerifier> certVerifier(mDefaultCertVerifier);
  return certVerifier.forget();
}

namespace mozilla { namespace psm {

already_AddRefed<SharedCertVerifier>
GetDefaultCertVerifier()
{
  static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID));
  if (nssComponent) {
    return nssComponent->GetDefaultCertVerifier();
  }

  return nullptr;
}

} } // namespace mozilla::psm

NS_IMPL_ISUPPORTS(PipUIContext, nsIInterfaceRequestor)

PipUIContext::PipUIContext()
{
}

PipUIContext::~PipUIContext()
{
}

NS_IMETHODIMP
PipUIContext::GetInterface(const nsIID& uuid, void** result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nullptr;

  if (!NS_IsMainThread()) {
    NS_ERROR("PipUIContext::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (!uuid.Equals(NS_GET_IID(nsIPrompt)))
    return NS_ERROR_NO_INTERFACE;

  nsIPrompt* prompt = nullptr;
  nsresult rv = nsNSSComponent::GetNewPrompter(&prompt);
  *result = prompt;
  return rv;
}

nsresult
getNSSDialogs(void** _result, REFNSIID aIID, const char* contract)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("getNSSDialogs called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv;

  nsCOMPtr<nsISupports> svc = do_GetService(contract, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = svc->QueryInterface(aIID, _result);

  return rv;
}

nsresult
setPassword(PK11SlotInfo* slot, nsIInterfaceRequestor* ctx,
            nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  MOZ_ASSERT(slot);
  MOZ_ASSERT(ctx);
  NS_ENSURE_ARG_POINTER(slot);
  NS_ENSURE_ARG_POINTER(ctx);

  if (PK11_NeedUserInit(slot)) {
    nsCOMPtr<nsITokenPasswordDialogs> dialogs;
    nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                                NS_GET_IID(nsITokenPasswordDialogs),
                                NS_TOKENPASSWORDSDIALOG_CONTRACTID);
    if (NS_FAILED(rv)) {
      return rv;
    }

    bool canceled;
    NS_ConvertUTF8toUTF16 tokenName(PK11_GetTokenName(slot));
    rv = dialogs->SetPassword(ctx, tokenName.get(), &canceled);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (canceled) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  return NS_OK;
}

namespace mozilla {
namespace psm {

nsresult
InitializeCipherSuite()
{
  NS_ASSERTION(NS_IsMainThread(), "InitializeCipherSuite() can only be accessed in main thread");

  if (NSS_SetDomesticPolicy() != SECSuccess) {
#ifdef ANDROID
    MOZ_RELEASE_ASSERT(false);
#endif
    return NS_ERROR_FAILURE;
  }

  // Disable any ciphers that NSS might have enabled by default
  for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    uint16_t cipher_id = SSL_ImplementedCiphers[i];
    SSL_CipherPrefSetDefault(cipher_id, false);
  }

  // Now only set SSL/TLS ciphers we knew about at compile time
  const CipherPref* const cp = sCipherPrefs;
  for (size_t i = 0; cp[i].pref; ++i) {
    bool cipherEnabled = Preferences::GetBool(cp[i].pref,
                                              cp[i].enabledByDefault);
    SSL_CipherPrefSetDefault(cp[i].id, cipherEnabled);
  }

  // Enable ciphers for PKCS#12
  SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
  SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
  PORT_SetUCS2_ASCIIConversionFunction(pip_ucs2_ascii_conversion_fn);

  // PSM enforces a minimum RSA key size of 1024 bits, which is overridable.
  // NSS has its own minimum, which is not overridable (the default is 1023
  // bits). This sets the NSS minimum to 512 bits so users can still connect to
  // devices like wifi routers with woefully small keys (they would have to add
  // an override to do so, but they already do for such devices).
  NSS_OptionSet(NSS_RSA_MIN_KEY_SIZE, 512);

  // Observe preference change around cipher suite setting.
  return CipherSuiteChangeObserver::StartObserve();
}

} // namespace psm
} // namespace mozilla
