/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICaptivePortalService.h"
#include "nsIParentalControlsService.h"
#include "nsINetworkLinkService.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsStandardURL.h"
#include "TRR.h"
#include "TRRService.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Tokenizer.h"

static const char kOpenCaptivePortalLoginEvent[] = "captive-portal-login";
static const char kClearPrivateData[] = "clear-private-data";
static const char kPurge[] = "browser:purge-session-history";
static const char kDisableIpv6Pref[] = "network.dns.disableIPv6";
static const char kPrefSkipTRRParentalControl[] =
    "network.dns.skipTRR-when-parental-control-enabled";
static const char kRolloutURIPref[] = "doh-rollout.uri";

#define TRR_PREF_PREFIX "network.trr."
#define TRR_PREF(x) TRR_PREF_PREFIX x

namespace mozilla {
namespace net {

#undef LOG
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)

TRRService* gTRRService = nullptr;
StaticRefPtr<nsIThread> sTRRBackgroundThread;

NS_IMPL_ISUPPORTS(TRRService, nsIObserver, nsISupportsWeakReference)

TRRService::TRRService()
    : mInitialized(false),
      mMode(0),
      mTRRBlacklistExpireTime(72 * 3600),
      mLock("trrservice"),
      mConfirmationNS(NS_LITERAL_CSTRING("example.com")),
      mWaitForCaptive(true),
      mRfc1918(false),
      mCaptiveIsPassed(false),
      mUseGET(false),
      mDisableECS(true),
      mSkipTRRWhenParentalControlEnabled(true),
      mDisableAfterFails(5),
      mPlatformDisabledTRR(false),
      mClearTRRBLStorage(false),
      mConfirmationState(CONFIRM_INIT),
      mRetryConfirmInterval(1000),
      mTRRFailures(0),
      mParentalControlEnabled(false) {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
}

nsresult TRRService::Init() {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (mInitialized) {
    return NS_OK;
  }
  mInitialized = true;

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, NS_CAPTIVE_PORTAL_CONNECTIVITY, true);
    observerService->AddObserver(this, kOpenCaptivePortalLoginEvent, true);
    observerService->AddObserver(this, kClearPrivateData, true);
    observerService->AddObserver(this, kPurge, true);
    observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, true);
    observerService->AddObserver(this, NS_DNS_SUFFIX_LIST_UPDATED_TOPIC, true);
    observerService->AddObserver(this, "xpcom-shutdown-threads", true);
  }
  nsCOMPtr<nsIPrefBranch> prefBranch;
  GetPrefBranch(getter_AddRefs(prefBranch));
  if (prefBranch) {
    prefBranch->AddObserver(TRR_PREF_PREFIX, this, true);
    prefBranch->AddObserver(kDisableIpv6Pref, this, true);
    prefBranch->AddObserver(kPrefSkipTRRParentalControl, this, true);
    prefBranch->AddObserver(kRolloutURIPref, this, true);
  }
  nsCOMPtr<nsICaptivePortalService> captivePortalService =
      do_GetService(NS_CAPTIVEPORTAL_CID);
  if (captivePortalService) {
    int32_t captiveState;
    MOZ_ALWAYS_SUCCEEDS(captivePortalService->GetState(&captiveState));

    if ((captiveState == nsICaptivePortalService::UNLOCKED_PORTAL) ||
        (captiveState == nsICaptivePortalService::NOT_CAPTIVE)) {
      mCaptiveIsPassed = true;
    }
    LOG(("TRRService::Init mCaptiveState=%d mCaptiveIsPassed=%d\n",
         captiveState, (int)mCaptiveIsPassed));
  }

  GetParentalControlEnabledInternal();

  ReadPrefs(nullptr);

  gTRRService = this;

  nsCOMPtr<nsINetworkLinkService> nls =
      do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID);
  RebuildSuffixList(nls);

  nsCOMPtr<nsIThread> thread;
  if (NS_FAILED(NS_NewNamedThread("TRR Background", getter_AddRefs(thread)))) {
    NS_WARNING("NS_NewNamedThread failed!");
    return NS_ERROR_FAILURE;
  }

  sTRRBackgroundThread = thread;

  LOG(("Initialized TRRService\n"));
  return NS_OK;
}

void TRRService::GetParentalControlEnabledInternal() {
  nsCOMPtr<nsIParentalControlsService> pc =
      do_CreateInstance("@mozilla.org/parental-controls-service;1");
  if (pc) {
    pc->GetParentalControlsEnabled(&mParentalControlEnabled);
    LOG(("TRRService::GetParentalControlEnabledInternal=%d\n",
         mParentalControlEnabled));
  }
}

void TRRService::SetDetectedTrrURI(const nsACString& aURI) {
  // If the user has set a custom URI then we don't want to override that.
  if (mURIPrefHasUserValue) {
    return;
  }

  mURISetByDetection = MaybeSetPrivateURI(aURI);
}

bool TRRService::Enabled(nsIRequest::TRRMode aMode) {
  if (mMode == MODE_TRROFF) {
    return false;
  }
  if (mConfirmationState == CONFIRM_INIT &&
      (!mWaitForCaptive || mCaptiveIsPassed ||
       (mMode == MODE_TRRONLY || aMode == nsIRequest::TRR_ONLY_MODE))) {
    LOG(("TRRService::Enabled => CONFIRM_TRYING\n"));
    mConfirmationState = CONFIRM_TRYING;
  }

  if (mConfirmationState == CONFIRM_TRYING) {
    LOG(("TRRService::Enabled MaybeConfirm()\n"));
    MaybeConfirm();
  }

  if (mConfirmationState != CONFIRM_OK) {
    LOG(("TRRService::Enabled mConfirmationState=%d mCaptiveIsPassed=%d\n",
         (int)mConfirmationState, (int)mCaptiveIsPassed));
  }

  return (mConfirmationState == CONFIRM_OK);
}

void TRRService::GetPrefBranch(nsIPrefBranch** result) {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  *result = nullptr;
  CallGetService(NS_PREFSERVICE_CONTRACTID, result);
}

void TRRService::ProcessURITemplate(nsACString& aURI) {
  // URI Template, RFC 6570.
  if (aURI.IsEmpty()) {
    return;
  }
  nsAutoCString scheme;
  nsCOMPtr<nsIIOService> ios(do_GetIOService());
  if (ios) {
    ios->ExtractScheme(aURI, scheme);
  }
  if (!scheme.Equals("https")) {
    LOG(("TRRService TRR URI %s is not https. Not used.\n",
         PromiseFlatCString(aURI).get()));
    aURI.Truncate();
    return;
  }

  // cut off everything from "{" to "}" sequences (potentially multiple),
  // as a crude conversion from template into URI.
  nsAutoCString uri(aURI);

  do {
    nsCCharSeparatedTokenizer openBrace(uri, '{');
    if (openBrace.hasMoreTokens()) {
      // the 'nextToken' is the left side of the open brace (or full uri)
      nsAutoCString prefix(openBrace.nextToken());

      // if there is an open brace, there's another token
      const nsACString& endBrace = openBrace.nextToken();
      nsCCharSeparatedTokenizer closeBrace(endBrace, '}');
      if (closeBrace.hasMoreTokens()) {
        // there is a close brace as well, make a URI out of the prefix
        // and the suffix
        closeBrace.nextToken();
        nsAutoCString suffix(closeBrace.nextToken());
        uri = prefix + suffix;
      } else {
        // no (more) close brace
        break;
      }
    } else {
      // no (more) open brace
      break;
    }
  } while (true);

  aURI = uri;
}

bool TRRService::MaybeSetPrivateURI(const nsACString& aURI) {
  bool clearCache = false;
  nsAutoCString newURI(aURI);
  ProcessURITemplate(newURI);

  {
    MutexAutoLock lock(mLock);
    if (mPrivateURI.Equals(newURI)) {
      return false;
    }

    if (!mPrivateURI.IsEmpty()) {
      mClearTRRBLStorage = true;
      LOG(("TRRService clearing blacklist because of change in uri service\n"));
      clearCache = true;
    }
    mPrivateURI = newURI;
  }

  // Clear the cache because we changed the URI
  if (clearCache) {
    ClearEntireCache();
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, NS_NETWORK_TRR_URI_CHANGED_TOPIC, nullptr);
  }
  return true;
}

void TRRService::CheckURIPrefs() {
  mURISetByDetection = false;

  // The user has set a custom URI so it takes precedence.
  if (mURIPrefHasUserValue) {
    MaybeSetPrivateURI(mURIPref);
    return;
  }

  // Check if the rollout addon has set a pref.
  if (!mRolloutURIPref.IsEmpty()) {
    MaybeSetPrivateURI(mRolloutURIPref);
    return;
  }

  // Otherwise just use the default value.
  MaybeSetPrivateURI(mURIPref);
}

nsresult TRRService::ReadPrefs(const char* name) {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");

  // Whenever a pref change occurs that would cause us to clear the cache
  // we set this to true then do it at the end of the method.
  bool clearEntireCache = false;

  if (!name || !strcmp(name, TRR_PREF("mode"))) {
    // 0 - off, 1 - reserved, 2 - TRR first, 3 - TRR only, 4 - reserved,
    // 5 - explicit off
    uint32_t tmp;
    if (NS_SUCCEEDED(Preferences::GetUint(TRR_PREF("mode"), &tmp))) {
      if (tmp > MODE_TRROFF) {
        tmp = MODE_TRROFF;
      }
      if (tmp == MODE_RESERVED1) {
        tmp = MODE_TRROFF;
      }
      if (tmp == MODE_RESERVED4) {
        tmp = MODE_TRROFF;
      }
      mMode = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("uri")) ||
      !strcmp(name, kRolloutURIPref)) {

    mURIPrefHasUserValue = Preferences::HasUserValue(TRR_PREF("uri"));
    Preferences::GetCString(TRR_PREF("uri"), mURIPref);
    Preferences::GetCString(kRolloutURIPref, mRolloutURIPref);

    CheckURIPrefs();
  }
  if (!name || !strcmp(name, TRR_PREF("credentials"))) {
    MutexAutoLock lock(mLock);
    Preferences::GetCString(TRR_PREF("credentials"), mPrivateCred);
  }
  if (!name || !strcmp(name, TRR_PREF("confirmationNS"))) {
    MutexAutoLock lock(mLock);
    nsAutoCString old(mConfirmationNS);
    Preferences::GetCString(TRR_PREF("confirmationNS"), mConfirmationNS);
    if (name && !old.IsEmpty() && !mConfirmationNS.Equals(old) &&
        (mConfirmationState > CONFIRM_TRYING)) {
      LOG(("TRR::ReadPrefs: restart confirmationNS state\n"));
      mConfirmationState = CONFIRM_TRYING;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("bootstrapAddress"))) {
    MutexAutoLock lock(mLock);
    Preferences::GetCString(TRR_PREF("bootstrapAddress"), mBootstrapAddr);
    clearEntireCache = true;
  }
  if (!name || !strcmp(name, TRR_PREF("wait-for-portal"))) {
    // Wait for captive portal?
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(TRR_PREF("wait-for-portal"), &tmp))) {
      mWaitForCaptive = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("allow-rfc1918"))) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(TRR_PREF("allow-rfc1918"), &tmp))) {
      mRfc1918 = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("useGET"))) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(TRR_PREF("useGET"), &tmp))) {
      mUseGET = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("blacklist-duration"))) {
    // prefs is given in number of seconds
    uint32_t secs;
    if (NS_SUCCEEDED(
            Preferences::GetUint(TRR_PREF("blacklist-duration"), &secs))) {
      mTRRBlacklistExpireTime = secs;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("early-AAAA"))) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(TRR_PREF("early-AAAA"), &tmp))) {
      mEarlyAAAA = tmp;
    }
  }

  if (!name || !strcmp(name, TRR_PREF("skip-AAAA-when-not-supported"))) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(
            TRR_PREF("skip-AAAA-when-not-supported"), &tmp))) {
      mCheckIPv6Connectivity = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("wait-for-A-and-AAAA"))) {
    bool tmp;
    if (NS_SUCCEEDED(
            Preferences::GetBool(TRR_PREF("wait-for-A-and-AAAA"), &tmp))) {
      mWaitForAllResponses = tmp;
    }
  }
  if (!name || !strcmp(name, kDisableIpv6Pref)) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(kDisableIpv6Pref, &tmp))) {
      mDisableIPv6 = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("disable-ECS"))) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(TRR_PREF("disable-ECS"), &tmp))) {
      mDisableECS = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("max-fails"))) {
    uint32_t fails;
    if (NS_SUCCEEDED(Preferences::GetUint(TRR_PREF("max-fails"), &fails))) {
      mDisableAfterFails = fails;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("excluded-domains")) ||
      !strcmp(name, TRR_PREF("builtin-excluded-domains"))) {
    MutexAutoLock lock(mLock);
    mExcludedDomains.Clear();

    auto parseExcludedDomains = [this](const char* aPrefName) {
      nsAutoCString excludedDomains;
      Preferences::GetCString(aPrefName, excludedDomains);
      if (excludedDomains.IsEmpty()) {
        return;
      }

      nsCCharSeparatedTokenizer tokenizer(
          excludedDomains, ',', nsCCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
      while (tokenizer.hasMoreTokens()) {
        nsAutoCString token(tokenizer.nextToken());
        LOG(("TRRService::ReadPrefs %s host:[%s]\n", aPrefName, token.get()));
        mExcludedDomains.PutEntry(token);
      }
    };

    parseExcludedDomains(TRR_PREF("excluded-domains"));
    parseExcludedDomains(TRR_PREF("builtin-excluded-domains"));
    clearEntireCache = true;
  }

  if (!name || !strcmp(name, kPrefSkipTRRParentalControl)) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(kPrefSkipTRRParentalControl, &tmp))) {
      mSkipTRRWhenParentalControlEnabled = tmp;
    }
  }

  // if name is null, then we're just now initializing. In that case we don't
  // need to clear the cache.
  if (name && clearEntireCache) {
    ClearEntireCache();
  }

  return NS_OK;
}

void TRRService::ClearEntireCache() {
  bool tmp;
  nsresult rv =
      Preferences::GetBool(TRR_PREF("clear-cache-on-pref-change"), &tmp);
  if (NS_FAILED(rv)) {
    return;
  }
  if (!tmp) {
    return;
  }
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (!dns) {
    return;
  }
  dns->ClearCache(true);
}

nsresult TRRService::GetURI(nsACString& result) {
  MutexAutoLock lock(mLock);
  result = mPrivateURI;
  return NS_OK;
}

nsresult TRRService::GetCredentials(nsCString& result) {
  MutexAutoLock lock(mLock);
  result = mPrivateCred;
  return NS_OK;
}

uint32_t TRRService::GetRequestTimeout() {
  if (mMode == MODE_TRRONLY) {
    return StaticPrefs::network_trr_request_timeout_mode_trronly_ms();
  }

  return StaticPrefs::network_trr_request_timeout_ms();
}

nsresult TRRService::Start() {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return NS_OK;
}

TRRService::~TRRService() {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  LOG(("Exiting TRRService\n"));
  gTRRService = nullptr;
}

nsresult TRRService::DispatchTRRRequest(TRR* aTrrRequest) {
  return DispatchTRRRequestInternal(aTrrRequest, true);
}

nsresult TRRService::DispatchTRRRequestInternal(TRR* aTrrRequest,
                                                bool aWithLock) {
  NS_ENSURE_ARG_POINTER(aTrrRequest);
  if (!StaticPrefs::network_trr_fetch_off_main_thread()) {
    return NS_DispatchToMainThread(aTrrRequest);
  }

  RefPtr<TRR> trr = aTrrRequest;
  nsCOMPtr<nsIThread> thread = aWithLock ? TRRThread() : TRRThread_locked();
  if (!thread) {
    return NS_ERROR_FAILURE;
  }

  return thread->Dispatch(trr.forget());
}

already_AddRefed<nsIThread> TRRService::TRRThread() {
  MutexAutoLock lock(mLock);
  return TRRThread_locked();
}

already_AddRefed<nsIThread> TRRService::TRRThread_locked() {
  RefPtr<nsIThread> thread = sTRRBackgroundThread;
  return thread.forget();
}

bool TRRService::IsOnTRRThread() {
  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock lock(mLock);
    thread = sTRRBackgroundThread;
  }
  if (!thread) {
    return false;
  }

  return thread->IsOnCurrentThread();
}

NS_IMETHODIMP
TRRService::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  LOG(("TRR::Observe() topic=%s\n", aTopic));
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(aData).get());

    MutexAutoLock lock(mLock);
    if (((mConfirmationState == CONFIRM_INIT) && !mBootstrapAddr.IsEmpty() &&
         (mMode == MODE_TRRONLY)) ||
        (mConfirmationState == CONFIRM_FAILED)) {
      mConfirmationState = CONFIRM_TRYING;
      MaybeConfirm_locked();
    }

  } else if (!strcmp(aTopic, kOpenCaptivePortalLoginEvent)) {
    // We are in a captive portal
    LOG(("TRRservice in captive portal\n"));
    mCaptiveIsPassed = false;
  } else if (!strcmp(aTopic, NS_CAPTIVE_PORTAL_CONNECTIVITY)) {
    nsAutoCString data = NS_ConvertUTF16toUTF8(aData);
    LOG(("TRRservice captive portal was %s\n", data.get()));
    if (!mTRRBLStorage) {
      // We need a lock if we modify mTRRBLStorage variable because it is
      // access off the main thread as well.
      MutexAutoLock lock(mLock);
      mTRRBLStorage = DataStorage::Get(DataStorageClass::TRRBlacklist);
      if (mTRRBLStorage) {
        if (NS_FAILED(mTRRBLStorage->Init(nullptr))) {
          mTRRBLStorage = nullptr;
        }
        if (mClearTRRBLStorage) {
          if (mTRRBLStorage) {
            mTRRBLStorage->Clear();
          }
          mClearTRRBLStorage = false;
        }
      }
    }

    // We should avoid doing calling MaybeConfirm in response to a pref change
    // unless the service is in a TRR=enabled mode.
    if (mMode == MODE_TRRFIRST || mMode == MODE_TRRONLY) {
      if (!mCaptiveIsPassed) {
        if (mConfirmationState != CONFIRM_OK) {
          mConfirmationState = CONFIRM_TRYING;
          MaybeConfirm();
        }
      } else {
        LOG(("TRRservice CP clear when already up!\n"));
      }
      mCaptiveIsPassed = true;
    }

  } else if (!strcmp(aTopic, kClearPrivateData) || !strcmp(aTopic, kPurge)) {
    // flush the TRR blacklist, both in-memory and on-disk
    if (mTRRBLStorage) {
      mTRRBLStorage->Clear();
    }
  } else if (!strcmp(aTopic, NS_DNS_SUFFIX_LIST_UPDATED_TOPIC) ||
             !strcmp(aTopic, NS_NETWORK_LINK_TOPIC)) {
    nsCOMPtr<nsINetworkLinkService> link = do_QueryInterface(aSubject);
    RebuildSuffixList(link);
    CheckPlatformDNSStatus(link);

    if (!strcmp(aTopic, NS_NETWORK_LINK_TOPIC) && mURISetByDetection) {
      // If the URI was set via SetDetectedTrrURI we need to restore it to the
      // default pref when a network link change occurs.
      CheckURIPrefs();
    }
  } else if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    if (sTRRBackgroundThread) {
      nsCOMPtr<nsIThread> thread;
      {
        MutexAutoLock lock(mLock);
        thread = sTRRBackgroundThread.get();
        sTRRBackgroundThread = nullptr;
      }
      MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
    }
  }
  return NS_OK;
}

void TRRService::RebuildSuffixList(nsINetworkLinkService* aLinkService) {
  // The network link service notification normally passes itself as the
  // subject, but some unit tests will sometimes pass a null subject.
  if (!aLinkService) {
    return;
  }

  nsTArray<nsCString> suffixList;
  aLinkService->GetDnsSuffixList(suffixList);

  MutexAutoLock lock(mLock);
  mDNSSuffixDomains.Clear();
  for (const auto& item : suffixList) {
    LOG(("TRRService adding %s to suffix list", item.get()));
    mDNSSuffixDomains.PutEntry(item);
  }
}

void TRRService::CheckPlatformDNSStatus(nsINetworkLinkService* aLinkService) {
  if (!aLinkService) {
    return;
  }

  uint32_t platformIndications = nsINetworkLinkService::NONE_DETECTED;
  aLinkService->GetPlatformDNSIndications(&platformIndications);
  LOG(("TRRService platformIndications=%u", platformIndications));
  mPlatformDisabledTRR =
      (!StaticPrefs::network_trr_enable_when_vpn_detected() &&
       (platformIndications & nsINetworkLinkService::VPN_DETECTED)) ||
      (!StaticPrefs::network_trr_enable_when_proxy_detected() &&
       (platformIndications & nsINetworkLinkService::PROXY_DETECTED)) ||
      (!StaticPrefs::network_trr_enable_when_nrpt_detected() &&
       (platformIndications & nsINetworkLinkService::NRPT_DETECTED));
}

void TRRService::MaybeConfirm() {
  MutexAutoLock lock(mLock);
  MaybeConfirm_locked();
}

void TRRService::MaybeConfirm_locked() {
  mLock.AssertCurrentThreadOwns();
  if (mMode == MODE_TRROFF || mConfirmer ||
      mConfirmationState != CONFIRM_TRYING) {
    LOG(
        ("TRRService:MaybeConfirm mode=%d, mConfirmer=%p "
         "mConfirmationState=%d\n",
         (int)mMode, (void*)mConfirmer, (int)mConfirmationState));
    return;
  }

  if (mConfirmationNS.Equals("skip") || mMode == MODE_TRRONLY) {
    LOG(("TRRService starting confirmation test %s SKIPPED\n",
         mPrivateURI.get()));
    mConfirmationState = CONFIRM_OK;
  } else {
    LOG(("TRRService starting confirmation test %s %s\n", mPrivateURI.get(),
         mConfirmationNS.get()));
    mConfirmer =
        new TRR(this, mConfirmationNS, TRRTYPE_NS, EmptyCString(), false);
    DispatchTRRRequestInternal(mConfirmer, false);
  }
}

bool TRRService::MaybeBootstrap(const nsACString& aPossible,
                                nsACString& aResult) {
  MutexAutoLock lock(mLock);
  if (mMode == MODE_TRROFF || mBootstrapAddr.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv =
      NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
          .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                  nsIStandardURL::URLTYPE_STANDARD, 443,
                                  mPrivateURI, nullptr, nullptr, nullptr))
          .Finalize(url);
  if (NS_FAILED(rv)) {
    LOG(("TRRService::MaybeBootstrap failed to create URI!\n"));
    return false;
  }

  nsAutoCString host;
  url->GetHost(host);
  if (!aPossible.Equals(host)) {
    return false;
  }
  LOG(("TRRService::MaybeBootstrap: use %s instead of %s\n",
       mBootstrapAddr.get(), host.get()));
  aResult = mBootstrapAddr;
  return true;
}

bool TRRService::IsDomainBlacklisted(const nsACString& aHost,
                                     const nsACString& aOriginSuffix,
                                     bool aPrivateBrowsing) {
  if (!Enabled(nsIRequest::TRR_DEFAULT_MODE)) {
    return true;
  }

  // It's OK to call this method here because it only happens on the main
  // thread, and we only change the excluded domains/dns suffix list
  // on the main thread in response to observer notifications.
  // Calling the locking version of this method would cause us to grab
  // the mutex for every label of the hostname, which would be very
  // inefficient.
  if (NS_IsMainThread()) {
    if (IsExcludedFromTRR_unlocked(aHost)) {
      return true;
    }
  } else {
    MOZ_ASSERT(IsOnTRRThread());
    if (IsExcludedFromTRR(aHost)) {
      return true;
    }
  }

  if (!mTRRBLStorage) {
    return false;
  }

  if (mClearTRRBLStorage) {
    mTRRBLStorage->Clear();
    mClearTRRBLStorage = false;
    return false;  // just cleared!
  }

  // use a unified casing for the hashkey
  nsAutoCString hashkey(aHost + aOriginSuffix);
  nsCString val(mTRRBLStorage->Get(hashkey, aPrivateBrowsing
                                                ? DataStorage_Private
                                                : DataStorage_Persistent));

  if (!val.IsEmpty()) {
    nsresult code;
    int32_t until = val.ToInteger(&code) + mTRRBlacklistExpireTime;
    int32_t expire = NowInSeconds();
    if (NS_SUCCEEDED(code) && (until > expire)) {
      LOG(("Host [%s] is TRR blacklisted\n", nsCString(aHost).get()));
      return true;
    }

    // the blacklisted entry has expired
    mTRRBLStorage->Remove(hashkey, aPrivateBrowsing ? DataStorage_Private
                                                    : DataStorage_Persistent);
  }
  return false;
}

// When running in TRR-only mode, the blacklist is not used and it will also
// try resolving the localhost / .local names.
bool TRRService::IsTRRBlacklisted(const nsACString& aHost,
                                  const nsACString& aOriginSuffix,
                                  bool aPrivateBrowsing,
                                  bool aParentsToo)  // false if domain
{
  if (mMode == MODE_TRRONLY) {
    return false;  // might as well try
  }

  LOG(("Checking if host [%s] is blacklisted", aHost.BeginReading()));

  int32_t dot = aHost.FindChar('.');
  if ((dot == kNotFound) && aParentsToo) {
    // Only if a full host name. Domains can be dotless to be able to
    // blacklist entire TLDs
    return true;
  }

  if (IsDomainBlacklisted(aHost, aOriginSuffix, aPrivateBrowsing)) {
    return true;
  }

  nsDependentCSubstring domain = Substring(aHost, 0);
  while (dot != kNotFound) {
    dot++;
    domain.Rebind(domain, dot, domain.Length() - dot);

    if (IsDomainBlacklisted(domain, aOriginSuffix, aPrivateBrowsing)) {
      return true;
    }

    dot = domain.FindChar('.');
  }

  return false;
}

bool TRRService::IsExcludedFromTRR(const nsACString& aHost) {
  // This method may be called off the main thread. We need to lock so
  // mExcludedDomains and mDNSSuffixDomains don't change while this code
  // is running.
  MutexAutoLock lock(mLock);

  return IsExcludedFromTRR_unlocked(aHost);
}

bool TRRService::IsExcludedFromTRR_unlocked(const nsACString& aHost) {
  if (!NS_IsMainThread()) {
    mLock.AssertCurrentThreadOwns();
  }

  if (mPlatformDisabledTRR) {
    LOG(("%s is excluded from TRR because of platform indications",
         aHost.BeginReading()));
    return true;
  }

  int32_t dot = 0;
  // iteratively check the sub-domain of |aHost|
  while (dot < static_cast<int32_t>(aHost.Length())) {
    nsDependentCSubstring subdomain =
        Substring(aHost, dot, aHost.Length() - dot);

    if (mExcludedDomains.GetEntry(subdomain)) {
      LOG(("Subdomain [%s] of host [%s] Is Excluded From TRR via pref\n",
           subdomain.BeginReading(), aHost.BeginReading()));
      return true;
    }
    if (mDNSSuffixDomains.GetEntry(subdomain)) {
      LOG(("Subdomain [%s] of host [%s] Is Excluded From TRR via pref\n",
           subdomain.BeginReading(), aHost.BeginReading()));
      return true;
    }

    dot = aHost.FindChar('.', dot + 1);
    if (dot == kNotFound) {
      break;
    }
    dot++;
  }

  return false;
}

class ProxyBlacklist : public Runnable {
 public:
  ProxyBlacklist(TRRService* service, const nsACString& aHost,
                 const nsACString& aOriginSuffix, bool pb, bool aParentsToo)
      : mozilla::Runnable("proxyBlackList"),
        mService(service),
        mHost(aHost),
        mOriginSuffix(aOriginSuffix),
        mPB(pb),
        mParentsToo(aParentsToo) {}

  NS_IMETHOD Run() override {
    mService->TRRBlacklist(mHost, mOriginSuffix, mPB, mParentsToo);
    mService = nullptr;
    return NS_OK;
  }

 private:
  RefPtr<TRRService> mService;
  nsCString mHost;
  nsCString mOriginSuffix;
  bool mPB;
  bool mParentsToo;
};

void TRRService::TRRBlacklist(const nsACString& aHost,
                              const nsACString& aOriginSuffix,
                              bool privateBrowsing, bool aParentsToo) {
  {
    MutexAutoLock lock(mLock);
    if (!mTRRBLStorage) {
      return;
    }
  }

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(new ProxyBlacklist(this, aHost, aOriginSuffix,
                                               privateBrowsing, aParentsToo));
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());

  LOG(("TRR blacklist %s\n", nsCString(aHost).get()));
  nsAutoCString hashkey(aHost + aOriginSuffix);
  nsAutoCString val;
  val.AppendInt(NowInSeconds());  // creation time

  // this overwrites any existing entry
  mTRRBLStorage->Put(
      hashkey, val,
      privateBrowsing ? DataStorage_Private : DataStorage_Persistent);

  if (aParentsToo) {
    // when given a full host name, verify its domain as well
    int32_t dot = aHost.FindChar('.');
    if (dot != kNotFound) {
      // this has a domain to be checked
      dot++;
      nsDependentCSubstring domain =
          Substring(aHost, dot, aHost.Length() - dot);
      nsAutoCString check(domain);
      if (IsTRRBlacklisted(check, aOriginSuffix, privateBrowsing, false)) {
        // the domain part is already blacklisted, no need to add this entry
        return;
      }
      // verify 'check' over TRR
      LOG(("TRR: verify if '%s' resolves as NS\n", check.get()));

      // check if there's an NS entry for this name
      RefPtr<TRR> trr =
          new TRR(this, check, TRRTYPE_NS, aOriginSuffix, privateBrowsing);
      DispatchTRRRequest(trr);
    }
  }
}

NS_IMETHODIMP
TRRService::Notify(nsITimer* aTimer) {
  if (aTimer == mRetryConfirmTimer) {
    mRetryConfirmTimer = nullptr;
    if (mConfirmationState == CONFIRM_FAILED) {
      LOG(("TRRService retry NS of %s\n", mConfirmationNS.get()));
      mConfirmationState = CONFIRM_TRYING;
      MaybeConfirm();
    }
  } else {
    MOZ_CRASH("Unknown timer");
  }

  return NS_OK;
}

void TRRService::TRRIsOkay(enum TrrOkay aReason) {
  MOZ_ASSERT_IF(StaticPrefs::network_trr_fetch_off_main_thread(),
                IsOnTRRThread());
  MOZ_ASSERT_IF(!StaticPrefs::network_trr_fetch_off_main_thread(),
                NS_IsMainThread());

  Telemetry::AccumulateCategorical(
      aReason == OKAY_NORMAL ? Telemetry::LABELS_DNS_TRR_SUCCESS::Fine
                             : (aReason == OKAY_TIMEOUT
                                    ? Telemetry::LABELS_DNS_TRR_SUCCESS::Timeout
                                    : Telemetry::LABELS_DNS_TRR_SUCCESS::Bad));
  if (aReason == OKAY_NORMAL) {
    mTRRFailures = 0;
  } else if ((mMode == MODE_TRRFIRST) && (mConfirmationState == CONFIRM_OK)) {
    // only count failures while in OK state
    uint32_t fails = ++mTRRFailures;
    if (fails >= mDisableAfterFails) {
      LOG(("TRRService goes FAILED after %u failures in a row\n", fails));
      mConfirmationState = CONFIRM_FAILED;
      // Fire off a timer and start re-trying the NS domain again
      NS_NewTimerWithCallback(getter_AddRefs(mRetryConfirmTimer), this,
                              mRetryConfirmInterval, nsITimer::TYPE_ONE_SHOT);
      mTRRFailures = 0;  // clear it again
    }
  }
}

AHostResolver::LookupStatus TRRService::CompleteLookup(
    nsHostRecord* rec, nsresult status, AddrInfo* aNewRRSet, bool pb,
    const nsACString& aOriginSuffix) {
  // this is an NS check for the TRR blacklist or confirmationNS check

  MOZ_ASSERT_IF(StaticPrefs::network_trr_fetch_off_main_thread(),
                IsOnTRRThread());
  MOZ_ASSERT_IF(!StaticPrefs::network_trr_fetch_off_main_thread(),
                NS_IsMainThread());
  MOZ_ASSERT(!rec);

  RefPtr<AddrInfo> newRRSet(aNewRRSet);
  MOZ_ASSERT(newRRSet && newRRSet->IsTRR() == TRRTYPE_NS);

#ifdef DEBUG
  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(!mConfirmer || (mConfirmationState == CONFIRM_TRYING));
  }
#endif
  if (mConfirmationState == CONFIRM_TRYING) {
    {
      MutexAutoLock lock(mLock);
      MOZ_ASSERT(mConfirmer);
      mConfirmationState = NS_SUCCEEDED(status) ? CONFIRM_OK : CONFIRM_FAILED;
      LOG(("TRRService finishing confirmation test %s %d %X\n",
           mPrivateURI.get(), (int)mConfirmationState, (unsigned int)status));
      mConfirmer = nullptr;
    }
    if (mConfirmationState == CONFIRM_FAILED) {
      // retry failed NS confirmation
      NS_NewTimerWithCallback(getter_AddRefs(mRetryConfirmTimer), this,
                              mRetryConfirmInterval, nsITimer::TYPE_ONE_SHOT);
      if (mRetryConfirmInterval < 64000) {
        // double the interval up to this point
        mRetryConfirmInterval *= 2;
      }
    } else {
      if (mMode != MODE_TRRONLY) {
        // don't accumulate trronly data here since trronly failures are
        // handled above by trying again, so counting the successes here would
        // skew the numbers
        Telemetry::Accumulate(Telemetry::DNS_TRR_NS_VERFIFIED,
                              (mConfirmationState == CONFIRM_OK));
      }
      mRetryConfirmInterval = 1000;
    }
    return LOOKUP_OK;
  }

  // when called without a host record, this is a domain name check response.
  if (NS_SUCCEEDED(status)) {
    LOG(("TRR verified %s to be fine!\n", newRRSet->mHostName.get()));
  } else {
    LOG(("TRR says %s doesn't resolve as NS!\n", newRRSet->mHostName.get()));
    TRRBlacklist(newRRSet->mHostName, aOriginSuffix, pb, false);
  }
  return LOOKUP_OK;
}

AHostResolver::LookupStatus TRRService::CompleteLookupByType(
    nsHostRecord*, nsresult, const nsTArray<nsCString>* aResult, uint32_t aTtl,
    bool aPb) {
  return LOOKUP_OK;
}

#undef LOG

}  // namespace net
}  // namespace mozilla
