/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSComponent.h"

#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "SharedSSLState.h"
#include "mozilla/Preferences.h"
#include "mozilla/PublicSSL.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsCertVerificationThread.h"
#include "nsClientAuthRemember.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIBufEntropyCollector.h"
#include "nsICertOverrideService.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIPrompt.h"
#include "nsIProperties.h"
#include "nsISiteSecurityService.h"
#include "nsITokenPasswordDialogs.h"
#include "nsIWindowWatcher.h"
#include "nsIXULRuntime.h"
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
#include "nsILocalFileWin.h"
#endif

using namespace mozilla;
using namespace mozilla::psm;

PRLogModuleInfo* gPIPNSSLog = nullptr;

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

  if (!NS_IsMainThread()) {
    return false;
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
    // no break

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
  :mutex("nsNSSComponent.mutex"),
   mNSSInitialized(false),
#ifndef MOZ_NO_SMART_CARDS
   mThreadList(nullptr),
#endif
   mCertVerificationThread(nullptr)
{
  if (!gPIPNSSLog)
    gPIPNSSLog = PR_NewLogModule("pipnss");
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::ctor\n"));

  NS_ASSERTION( (0 == mInstanceCount), "nsNSSComponent is a singleton, but instantiated multiple times!");
  ++mInstanceCount;
  mShutdownObjectList = nsNSSShutDownList::construct();
  mIsNetworkDown = false;
}

void
nsNSSComponent::deleteBackgroundThreads()
{
  if (mCertVerificationThread)
  {
    mCertVerificationThread->requestExit();
    delete mCertVerificationThread;
    mCertVerificationThread = nullptr;
  }
}

void
nsNSSComponent::createBackgroundThreads()
{
  NS_ASSERTION(!mCertVerificationThread,
               "Cert verification thread already created.");

  mCertVerificationThread = new nsCertVerificationThread;
  nsresult rv = mCertVerificationThread->startThread(
    NS_LITERAL_CSTRING("Cert Verify"));

  if (NS_FAILED(rv)) {
    delete mCertVerificationThread;
    mCertVerificationThread = nullptr;
  }
}

nsNSSComponent::~nsNSSComponent()
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent::dtor\n"));
  NS_ASSERTION(!mCertVerificationThread,
               "Cert verification thread should have been cleaned up.");

  deleteBackgroundThreads();

  // All cleanup code requiring services needs to happen in xpcom_shutdown

  ShutdownNSS();
  SharedSSLState::GlobalCleanup();
  RememberCertErrorsTable::Cleanup();
  --mInstanceCount;
  delete mShutdownObjectList;

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

  nsresult rv;
  nsAutoString modName;
  rv = GetPIPNSSBundleString("RootCertModuleName", modName);
  if (NS_FAILED(rv)) return;

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
  nsAutoString fips140TokenDescription;
  nsAutoString fips140SlotDescription;

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

  rv = GetPIPNSSBundleString("Fips140TokenDescription", fips140TokenDescription);
  if (NS_FAILED(rv)) return rv;

  rv = GetPIPNSSBundleString("Fips140SlotDescription", fips140SlotDescription);
  if (NS_FAILED(rv)) return rv;

  PK11_ConfigurePKCS11(NS_ConvertUTF16toUTF8(manufacturerID).get(),
                       NS_ConvertUTF16toUTF8(libraryDescription).get(),
                       NS_ConvertUTF16toUTF8(tokenDescription).get(),
                       NS_ConvertUTF16toUTF8(privateTokenDescription).get(),
                       NS_ConvertUTF16toUTF8(slotDescription).get(),
                       NS_ConvertUTF16toUTF8(privateSlotDescription).get(),
                       NS_ConvertUTF16toUTF8(fips140TokenDescription).get(),
                       NS_ConvertUTF16toUTF8(fips140SlotDescription).get(),
                       0, 0);
  return NS_OK;
}

nsresult
nsNSSComponent::InitializePIPNSSBundle()
{
  // Called during init only, no mutex required.

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv) || !bundleService)
    return NS_ERROR_FAILURE;

  bundleService->CreateBundle("chrome://pipnss/locale/pipnss.properties",
                              getter_AddRefs(mPIPNSSBundle));
  if (!mPIPNSSBundle)
    rv = NS_ERROR_FAILURE;

  bundleService->CreateBundle("chrome://pipnss/locale/nsserrors.properties",
                              getter_AddRefs(mNSSErrorsBundle));
  if (!mNSSErrorsBundle)
    rv = NS_ERROR_FAILURE;

  return rv;
}

// Table of pref names and SSL cipher ID
typedef struct {
  const char* pref;
  long id;
  bool enabledByDefault;
  bool weak;
} CipherPref;

// Update the switch statement in HandshakeCallback in nsNSSCallbacks.cpp when
// you add/remove cipher suites here.
static const CipherPref sCipherPrefs[] = {
 { "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
   TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, true },
 { "security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256",
   TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, true },
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

 { "security.ssl3.ecdhe_rsa_rc4_128_sha",
   TLS_ECDHE_RSA_WITH_RC4_128_SHA, true, true }, // deprecated (RC4)
 { "security.ssl3.ecdhe_ecdsa_rc4_128_sha",
   TLS_ECDHE_ECDSA_WITH_RC4_128_SHA, true, true }, // deprecated (RC4)

 { "security.ssl3.rsa_aes_128_sha",
   TLS_RSA_WITH_AES_128_CBC_SHA, true }, // deprecated (RSA key exchange)
 { "security.ssl3.rsa_aes_256_sha",
   TLS_RSA_WITH_AES_256_CBC_SHA, true }, // deprecated (RSA key exchange)
 { "security.ssl3.rsa_des_ede3_sha",
   TLS_RSA_WITH_3DES_EDE_CBC_SHA, true }, // deprecated (RSA key exchange, 3DES)

 { "security.ssl3.rsa_rc4_128_sha",
   TLS_RSA_WITH_RC4_128_SHA, true, true }, // deprecated (RSA key exchange, RC4)
 { "security.ssl3.rsa_rc4_128_md5",
   TLS_RSA_WITH_RC4_128_MD5, true, true }, // deprecated (RSA key exchange, RC4, HMAC-MD5)

 // All the rest are disabled by default

 { nullptr, 0 } // end marker
};

// Bit flags indicating what weak ciphers are enabled.
// The bit index will correspond to the index in sCipherPrefs.
// Wrtten by the main thread, read from any threads.
static Atomic<uint32_t> sEnabledWeakCiphers;
static_assert(MOZ_ARRAY_LENGTH(sCipherPrefs) - 1 <= sizeof(uint32_t) * CHAR_BIT,
              "too many cipher suites");

/*static*/ bool
nsNSSComponent::AreAnyWeakCiphersEnabled()
{
  return !!sEnabledWeakCiphers;
}

/*static*/ void
nsNSSComponent::UseWeakCiphersOnSocket(PRFileDesc* fd)
{
  const uint32_t enabledWeakCiphers = sEnabledWeakCiphers;
  const CipherPref* const cp = sCipherPrefs;
  for (size_t i = 0; cp[i].pref; ++i) {
    if (enabledWeakCiphers & ((uint32_t)1 << i)) {
      SSL_CipherPrefSet(fd, cp[i].id, true);
    }
  }
}

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
static const bool NPN_ENABLED_DEFAULT = true;
static const bool ALPN_ENABLED_DEFAULT = false;

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
        if (cp[i].weak) {
          // Weak ciphers will not be used by default even if they
          // are enabled in prefs. They are only used on specific
          // sockets as a part of a fallback mechanism.
          // Only the main thread will change sEnabledWeakCiphers.
          uint32_t enabledWeakCiphers = sEnabledWeakCiphers;
          if (cipherEnabled) {
            enabledWeakCiphers |= ((uint32_t)1 << i);
          } else {
            enabledWeakCiphers &= ~((uint32_t)1 << i);
          }
          sEnabledWeakCiphers = enabledWeakCiphers;
        } else {
          SSL_CipherPrefSetDefault(cp[i].id, cipherEnabled);
          SSL_ClearSessionCache();
        }
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
                                                    false);
  PublicSSLState()->SetOCSPMustStapleEnabled(ocspMustStapleEnabled);
  PrivateSSLState()->SetOCSPMustStapleEnabled(ocspMustStapleEnabled);

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
  if (sha1Mode > CertVerifier::SHA1Mode::OnlyBefore2016) {
    sha1Mode = CertVerifier::SHA1Mode::Allowed;
  }

  CertVerifier::OcspDownloadConfig odc;
  CertVerifier::OcspStrictConfig osc;
  CertVerifier::OcspGetConfig ogc;
  uint32_t certShortLifetimeInDays;

  GetRevocationBehaviorFromPrefs(&odc, &osc, &ogc, &certShortLifetimeInDays,
                                 lock);
  mDefaultCertVerifier = new SharedCertVerifier(odc, osc, ogc,
                                                certShortLifetimeInDays,
                                                pinningMode, sha1Mode);
}

// Enable the TLS versions given in the prefs, defaulting to TLS 1.0 (min) and
// TLS 1.2 (max) when the prefs aren't set or set to invalid values.
nsresult
nsNSSComponent::setEnabledTLSVersions()
{
  // keep these values in sync with security-prefs.js
  // 1 means TLS 1.0, 2 means TLS 1.1, etc.
  static const uint32_t PSM_DEFAULT_MIN_TLS_VERSION = 1;
  static const uint32_t PSM_DEFAULT_MAX_TLS_VERSION = 3;

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
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
           ("Unable to get profile directory - continuing with no NSS DB\n"));
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
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
           ("Could not get native path for profile directory.\n"));
    return rv;
  }

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

  if (mNSSInitialized) {
    PR_ASSERT(!"Trying to initialize NSS twice"); // We should never try to
                                                  // initialize NSS more than
                                                  // once in a process.
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
  }
  if (init_rv != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("could not initialize NSS - panicking\n"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  mNSSInitialized = true;

  PK11_SetPasswordFunc(PK11PasswordPrompt);

  SharedSSLState::GlobalInit();

  // Register an observer so we can inform NSS when these prefs change
  Preferences::AddStrongObserver(this, "security.");

  SSL_OptionSetDefault(SSL_ENABLE_SSL2, false);
  SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, false);

  rv = setEnabledTLSVersions();
  if (NS_FAILED(rv)) {
    return NS_ERROR_UNEXPECTED;
  }

  DisableMD5();
  // Initialize the certverifier log before calling any functions that library.
  InitCertVerifierLog();
  LoadLoadableRoots();

  ConfigureTLSSessionIdentifiers();

  bool requireSafeNegotiation =
    Preferences::GetBool("security.ssl.require_safe_negotiation",
                         REQUIRE_SAFE_NEGOTIATION_DEFAULT);
  SSL_OptionSetDefault(SSL_REQUIRE_SAFE_NEGOTIATION, requireSafeNegotiation);

  SSL_OptionSetDefault(SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_REQUIRES_XTN);

  SSL_OptionSetDefault(SSL_ENABLE_FALSE_START,
                       Preferences::GetBool("security.ssl.enable_false_start",
                                            FALSE_START_ENABLED_DEFAULT));

  // SSL_ENABLE_NPN and SSL_ENABLE_ALPN also require calling
  // SSL_SetNextProtoNego in order for the extensions to be negotiated.
  // WebRTC does not do that so it will not use NPN or ALPN even when these
  // preferences are true.
  SSL_OptionSetDefault(SSL_ENABLE_NPN,
                       Preferences::GetBool("security.ssl.enable_npn",
                                            NPN_ENABLED_DEFAULT));
  SSL_OptionSetDefault(SSL_ENABLE_ALPN,
                       Preferences::GetBool("security.ssl.enable_alpn",
                                            ALPN_ENABLED_DEFAULT));

  if (NS_FAILED(InitializeCipherSuite())) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("Unable to initialize cipher suite settings\n"));
    return NS_ERROR_FAILURE;
  }

  // ensure the CertBlocklist is initialised
  nsCOMPtr<nsICertBlocklist> certList = do_GetService(NS_CERTBLOCKLIST_CONTRACTID);
  if (!certList) {
    return NS_ERROR_FAILURE;
  }

  // dynamic options from prefs
  setValidationOptions(true, lock);

  mHttpForNSS.initTable();

#ifndef MOZ_NO_SMART_CARDS
  LaunchSmartCardThreads();
#endif

  mozilla::pkix::RegisterErrorTable();

  // Initialize the site security service
  nsCOMPtr<nsISiteSecurityService> sssService =
    do_GetService(NS_SSSERVICE_CONTRACTID);
  if (!sssService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Cannot initialize site security service\n"));
    return NS_ERROR_FAILURE;
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

  MutexAutoLock lock(mutex);

  if (mNSSInitialized) {
    mNSSInitialized = false;

    PK11_SetPasswordFunc((PK11PasswordFunc)nullptr);

    Preferences::RemoveObserver(this, "security.");

#ifndef MOZ_NO_SMART_CARDS
    ShutdownSmartCardThreads();
#endif
    SSL_ClearSessionCache();
    UnloadLoadableRoots();
#ifndef MOZ_NO_EV_CERTS
    CleanupIdentityInfo();
#endif
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("evaporating psm resources\n"));
    mShutdownObjectList->evaporateAllNSSResources();
    EnsureNSSInitialized(nssShutdown);
    if (SECSuccess != ::NSS_Shutdown()) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("NSS SHUTDOWN FAILURE\n"));
    }
    else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS shutdown =====>> OK <<=====\n"));
    }
  }
}

nsresult
nsNSSComponent::Init()
{
  // No mutex protection.
  // Assume Init happens before any concurrency on "this" can start.

  nsresult rv = NS_OK;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Beginning NSS initialization\n"));

  if (!mShutdownObjectList)
  {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("NSS init, out of memory in constructor\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = InitializePIPNSSBundle();
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
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("nsNSSComponent::InitializeNSS() failed\n"));
    return rv;
  }

  RememberCertErrorsTable::Init();

  createBackgroundThreads();
  if (!mCertVerificationThread) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSComponent::createBackgroundThreads() failed\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIEntropyCollector> ec(
    do_GetService(NS_ENTROPYCOLLECTOR_CONTRACTID));
  if (!ec) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIBufEntropyCollector> bec(do_QueryInterface(ec));
  if (!bec) {
    return NS_ERROR_FAILURE;
  }
  bec->ForwardTo(this);

  return RegisterObservers();
}

// nsISupports Implementation for the class
NS_IMPL_ISUPPORTS(nsNSSComponent,
                  nsIEntropyCollector,
                  nsINSSComponent,
                  nsIObserver)

NS_IMETHODIMP
nsNSSComponent::RandomUpdate(void* entropy, int32_t bufLen)
{
  nsNSSShutDownPreventionLock locker;

  // Asynchronous event happening often,
  // must not interfere with initialization or profile switch.

  MutexAutoLock lock(mutex);

  if (!mNSSInitialized)
      return NS_ERROR_NOT_INITIALIZED;

  PK11_RandomUpdate(entropy, bufLen);
  return NS_OK;
}

static const char* const PROFILE_CHANGE_NET_TEARDOWN_TOPIC
  = "profile-change-net-teardown";
static const char* const PROFILE_CHANGE_NET_RESTORE_TOPIC
  = "profile-change-net-restore";
static const char* const PROFILE_BEFORE_CHANGE_TOPIC = "profile-before-change";
static const char* const PROFILE_DO_CHANGE_TOPIC = "profile-do-change";

NS_IMETHODIMP
nsNSSComponent::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* someData)
{
  if (nsCRT::strcmp(aTopic, PROFILE_BEFORE_CHANGE_TOPIC) == 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("receiving profile change topic\n"));
    DoProfileBeforeChange(aSubject);
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_DO_CHANGE_TOPIC) == 0) {
    if (someData && NS_LITERAL_STRING("startup").Equals(someData)) {
      // The application is initializing against a known profile directory for
      // the first time during process execution.
      // However, earlier code execution might have already triggered NSS init.
      // We must ensure that NSS gets shut down prior to any attempt to init
      // it again. We use the same cleanup functionality used when switching
      // profiles. The order of function calls must correspond to the order
      // of notifications sent by Profile Manager (nsProfile).
      DoProfileChangeNetTeardown();
      DoProfileBeforeChange(aSubject);
      DoProfileChangeNetRestore();
    }

    bool needsInit = true;

    {
      MutexAutoLock lock(mutex);

      if (mNSSInitialized) {
        // We have already initialized NSS before the profile came up,
        // no need to do it again
        needsInit = false;
      }
    }

    if (needsInit) {
      if (NS_FAILED(InitializeNSS())) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("Unable to Initialize NSS after profile switch.\n"));
      }
    }
  }
  else if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent: XPCom shutdown observed\n"));

    // Cleanup code that requires services, it's too late in destructor.

    nsCOMPtr<nsIEntropyCollector> ec
        = do_GetService(NS_ENTROPYCOLLECTOR_CONTRACTID);

    if (ec) {
      nsCOMPtr<nsIBufEntropyCollector> bec
        = do_QueryInterface(ec);
      if (bec) {
        bec->DontForward();
      }
    }

    deleteBackgroundThreads();
  }
  else if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
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
    } else if (prefName.EqualsLiteral("security.ssl.enable_npn")) {
      SSL_OptionSetDefault(SSL_ENABLE_NPN,
                           Preferences::GetBool("security.ssl.enable_npn",
                                                NPN_ENABLED_DEFAULT));
    } else if (prefName.EqualsLiteral("security.ssl.enable_alpn")) {
      SSL_OptionSetDefault(SSL_ENABLE_ALPN,
                           Preferences::GetBool("security.ssl.enable_alpn",
                                                ALPN_ENABLED_DEFAULT));
    } else if (prefName.Equals("security.ssl.disable_session_identifiers")) {
      ConfigureTLSSessionIdentifiers();
    } else if (prefName.EqualsLiteral("security.OCSP.enabled") ||
               prefName.EqualsLiteral("security.OCSP.require") ||
               prefName.EqualsLiteral("security.OCSP.GET.enabled") ||
               prefName.EqualsLiteral("security.pki.cert_short_lifetime_in_days") ||
               prefName.EqualsLiteral("security.ssl.enable_ocsp_stapling") ||
               prefName.EqualsLiteral("security.ssl.enable_ocsp_must_staple") ||
               prefName.EqualsLiteral("security.cert_pinning.enforcement_level") ||
               prefName.EqualsLiteral("security.pki.sha1_enforcement_level")) {
      MutexAutoLock lock(mutex);
      setValidationOptions(false, lock);
    } else {
      clearSessionCache = false;
    }
    if (clearSessionCache)
      SSL_ClearSessionCache();
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_NET_TEARDOWN_TOPIC) == 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("receiving network teardown topic\n"));
    DoProfileChangeNetTeardown();
  }
  else if (nsCRT::strcmp(aTopic, PROFILE_CHANGE_NET_RESTORE_TOPIC) == 0) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("receiving network restore topic\n"));
    DoProfileChangeNetRestore();
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

  return mShutdownObjectList->doPK11Logout();
}

nsresult
nsNSSComponent::RegisterObservers()
{
  // Happens once during init only, no mutex protection.

  nsCOMPtr<nsIObserverService> observerService(
    do_GetService("@mozilla.org/observer-service;1"));
  if (!observerService) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("nsNSSComponent: couldn't get observer service\n"));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("nsNSSComponent: adding observers\n"));
  // Using false for the ownsweak parameter means the observer service will
  // keep a strong reference to this component. As a result, this will live at
  // least as long as the observer service.
  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  observerService->AddObserver(this, PROFILE_BEFORE_CHANGE_TOPIC, false);
  observerService->AddObserver(this, PROFILE_DO_CHANGE_TOPIC, false);
  observerService->AddObserver(this, PROFILE_CHANGE_NET_TEARDOWN_TOPIC, false);
  observerService->AddObserver(this, PROFILE_CHANGE_NET_RESTORE_TOPIC, false);

  return NS_OK;
}

void
nsNSSComponent::DoProfileChangeNetTeardown()
{
  if (mCertVerificationThread)
    mCertVerificationThread->requestExit();
  mIsNetworkDown = true;
}

void
nsNSSComponent::DoProfileBeforeChange(nsISupports* aSubject)
{
  NS_ASSERTION(mIsNetworkDown, "nsNSSComponent relies on profile manager to wait for synchronous shutdown of all network activity");

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

void
nsNSSComponent::DoProfileChangeNetRestore()
{
  // XXX this doesn't work well, since nothing expects null pointers
  deleteBackgroundThreads();
  createBackgroundThreads();
  mIsNetworkDown = false;
}

NS_IMETHODIMP
nsNSSComponent::IsNSSInitialized(bool* initialized)
{
  MutexAutoLock lock(mutex);
  *initialized = mNSSInitialized;
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
  RefPtr<SharedCertVerifier> certVerifier;
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
  nsresult rv = NS_OK;

  if (PK11_NeedUserInit(slot)) {
    nsITokenPasswordDialogs* dialogs;
    bool canceled;
    NS_ConvertUTF8toUTF16 tokenName(PK11_GetTokenName(slot));

    rv = getNSSDialogs((void**)&dialogs,
                       NS_GET_IID(nsITokenPasswordDialogs),
                       NS_TOKENPASSWORDSDIALOG_CONTRACTID);

    if (NS_FAILED(rv)) goto loser;

    rv = dialogs->SetPassword(ctx, tokenName.get(), &canceled);

    NS_RELEASE(dialogs);
    if (NS_FAILED(rv)) goto loser;

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
  }
 loser:
  return rv;
}

namespace mozilla {
namespace psm {

nsresult
InitializeCipherSuite()
{
  NS_ASSERTION(NS_IsMainThread(), "InitializeCipherSuite() can only be accessed in main thread");

  if (NSS_SetDomesticPolicy() != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Disable any ciphers that NSS might have enabled by default
  for (uint16_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    uint16_t cipher_id = SSL_ImplementedCiphers[i];
    SSL_CipherPrefSetDefault(cipher_id, false);
  }

  // Now only set SSL/TLS ciphers we knew about at compile time
  uint32_t enabledWeakCiphers = 0;
  const CipherPref* const cp = sCipherPrefs;
  for (size_t i = 0; cp[i].pref; ++i) {
    bool cipherEnabled = Preferences::GetBool(cp[i].pref,
                                              cp[i].enabledByDefault);
    if (cp[i].weak) {
      // Weak ciphers are not used by default. See the comment
      // in CipherSuiteChangeObserver::Observe for details.
      if (cipherEnabled) {
        enabledWeakCiphers |= ((uint32_t)1 << i);
      }
    } else {
      SSL_CipherPrefSetDefault(cp[i].id, cipherEnabled);
    }
  }
  sEnabledWeakCiphers = enabledWeakCiphers;

  // Enable ciphers for PKCS#12
  SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
  SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
  SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
  SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
  PORT_SetUCS2_ASCIIConversionFunction(pip_ucs2_ascii_conversion_fn);

  // Observe preference change around cipher suite setting.
  return CipherSuiteChangeObserver::StartObserve();
}

} // namespace psm
} // namespace mozilla
