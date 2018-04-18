/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICaptivePortalService.h"
#include "nsIObserverService.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"
#include "nsStandardURL.h"
#include "TRR.h"
#include "TRRService.h"

#include "mozilla/Preferences.h"

static const char kOpenCaptivePortalLoginEvent[] = "captive-portal-login";
static const char kClearPrivateData[] = "clear-private-data";
static const char kPurge[] = "browser:purge-session-history";
static const char kDisableIpv6Pref[] = "network.dns.disableIPv6";

#define TRR_PREF_PREFIX           "network.trr."
#define TRR_PREF(x)               TRR_PREF_PREFIX x

namespace mozilla {
namespace net {

#undef LOG
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)

TRRService *gTRRService = nullptr;

NS_IMPL_ISUPPORTS(TRRService, nsIObserver, nsISupportsWeakReference)

TRRService::TRRService()
  : mInitialized(false)
  , mMode(0)
  , mTRRBlacklistExpireTime(72 * 3600)
  , mTRRTimeout(3000)
  , mLock("trrservice")
  , mConfirmationNS(NS_LITERAL_CSTRING("example.com"))
  , mWaitForCaptive(true)
  , mRfc1918(false)
  , mCaptiveIsPassed(false)
  , mUseGET(false)
  , mClearTRRBLStorage(false)
  , mConfirmationState(CONFIRM_INIT)
  , mRetryConfirmInterval(1000)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
}

nsresult
TRRService::Init()
{
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
  }
  nsCOMPtr<nsIPrefBranch> prefBranch;
  GetPrefBranch(getter_AddRefs(prefBranch));
  if (prefBranch) {
    prefBranch->AddObserver(TRR_PREF_PREFIX, this, true);
    prefBranch->AddObserver(kDisableIpv6Pref, this, true);
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

  ReadPrefs(NULL);

  gTRRService = this;

  LOG(("Initialized TRRService\n"));
  return NS_OK;
}

bool
TRRService::Enabled()
{
  if (mConfirmationState == CONFIRM_INIT &&
      (!mWaitForCaptive || mCaptiveIsPassed || (mMode == MODE_TRRONLY))) {
    LOG(("TRRService::Enabled => CONFIRM_TRYING\n"));
    mConfirmationState = CONFIRM_TRYING;
  }

  if (mConfirmationState == CONFIRM_TRYING) {
    LOG(("TRRService::Enabled MaybeConfirm()\n"));
    MaybeConfirm();
  }

  if (mConfirmationState != CONFIRM_OK) {
    LOG(("TRRService::Enabled mConfirmationState=%d mCaptiveIsPassed=%d\n",
         (int)mConfirmationState,
         (int)mCaptiveIsPassed));
  }

  return (mConfirmationState == CONFIRM_OK);
}

void
TRRService::GetPrefBranch(nsIPrefBranch **result)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  *result = nullptr;
  CallGetService(NS_PREFSERVICE_CONTRACTID, result);
}

nsresult
TRRService::ReadPrefs(const char *name)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (!name || !strcmp(name, TRR_PREF("mode"))) {
    // 0 - off, 1 - parallel, 2 - TRR first, 3 - TRR only, 4 - shadow
    uint32_t tmp;
    if (NS_SUCCEEDED(Preferences::GetUint(TRR_PREF("mode"), &tmp))) {
      mMode = tmp;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("uri"))) {
    // Base URI, appends "?ct&dns=..."
    MutexAutoLock lock(mLock);
    nsAutoCString old(mPrivateURI);
    Preferences::GetCString(TRR_PREF("uri"), mPrivateURI);
    nsAutoCString scheme;
    if (!mPrivateURI.IsEmpty()) {
      nsCOMPtr<nsIIOService> ios(do_GetIOService());
      if (ios) {
        ios->ExtractScheme(mPrivateURI, scheme);
      }
    }
    if (!mPrivateURI.IsEmpty() && !scheme.Equals("https")) {
      LOG(("TRRService TRR URI %s is not https. Not used.\n",
           mPrivateURI.get()));
      mPrivateURI.Truncate();
    }
    if (!mPrivateURI.IsEmpty()) {
      LOG(("TRRService TRR URI %s\n", mPrivateURI.get()));
    }
    if (!old.IsEmpty() && !mPrivateURI.Equals(old)) {
      mClearTRRBLStorage = true;
      LOG(("TRRService clearing blacklist because of change is uri service\n"));
    }
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
    if (NS_SUCCEEDED(Preferences::GetUint(TRR_PREF("blacklist-duration"), &secs))) {
      mTRRBlacklistExpireTime = secs;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("request-timeout"))) {
    // number of milliseconds
    uint32_t ms;
    if (NS_SUCCEEDED(Preferences::GetUint(TRR_PREF("request-timeout"), &ms))) {
      mTRRTimeout = ms;
    }
  }
  if (!name || !strcmp(name, TRR_PREF("early-AAAA"))) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(TRR_PREF("early-AAAA"), &tmp))) {
      mEarlyAAAA = tmp;
    }
  }
  if (!name || !strcmp(name, kDisableIpv6Pref)) {
    bool tmp;
    if (NS_SUCCEEDED(Preferences::GetBool(kDisableIpv6Pref, &tmp))) {
      mDisableIPv6 = tmp;
    }
  }

  return NS_OK;
}

nsresult
TRRService::GetURI(nsCString &result)
{
  MutexAutoLock lock(mLock);
  result = mPrivateURI;
  return NS_OK;
}

nsresult
TRRService::GetCredentials(nsCString &result)
{
  MutexAutoLock lock(mLock);
  result = mPrivateCred;
  return NS_OK;
}

nsresult
TRRService::Start()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return NS_OK;
}

TRRService::~TRRService()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  LOG(("Exiting TRRService\n"));
  gTRRService = nullptr;
}

NS_IMETHODIMP
TRRService::Observe(nsISupports *aSubject,
                    const char * aTopic,
                    const char16_t * aData)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  LOG(("TRR::Observe() topic=%s\n", aTopic));
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(aData).get());

    if ((mConfirmationState == CONFIRM_INIT) &&
        !mBootstrapAddr.IsEmpty() &&
        (mMode == MODE_TRRONLY)) {
      mConfirmationState = CONFIRM_TRYING;
      MaybeConfirm();
    }

  } else if (!strcmp(aTopic, kOpenCaptivePortalLoginEvent)) {
    // We are in a captive portal
    LOG(("TRRservice in captive portal\n"));
    mCaptiveIsPassed = false;
  } else if (!strcmp(aTopic, NS_CAPTIVE_PORTAL_CONNECTIVITY)) {
    nsAutoCString data = NS_ConvertUTF16toUTF8(aData);
    LOG(("TRRservice captive portal was %s\n", data.get()));
    if (!mTRRBLStorage) {
      mTRRBLStorage = DataStorage::Get(DataStorageClass::TRRBlacklist);
      if (mTRRBLStorage) {
        bool storageWillPersist = true;
        if (NS_FAILED(mTRRBLStorage->Init(storageWillPersist))) {
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

    if (mConfirmationState != CONFIRM_OK) {
      mConfirmationState = CONFIRM_TRYING;
      MaybeConfirm();
    } else {
      LOG(("TRRservice CP clear when already up!\n"));
    }

    mCaptiveIsPassed = true;

  } else if (!strcmp(aTopic, kClearPrivateData) ||
             !strcmp(aTopic, kPurge)) {
    // flush the TRR blacklist, both in-memory and on-disk
    if (mTRRBLStorage) {
      mTRRBLStorage->Clear();
    }
  }
  return NS_OK;
}

void
TRRService::MaybeConfirm()
{
  if (TRR_DISABLED(mMode) || mConfirmer ||
      mConfirmationState != CONFIRM_TRYING) {
    LOG(("TRRService:MaybeConfirm mode=%d, mConfirmer=%p mConfirmationState=%d\n",
         (int)mMode, (void *)mConfirmer, (int)mConfirmationState));
    return;
  }
  nsAutoCString host;
  {
    MutexAutoLock lock(mLock);
    host = mConfirmationNS;
  }
  if (host.Equals("skip")) {
    LOG(("TRRService starting confirmation test %s SKIPPED\n",
         mPrivateURI.get()));
    mConfirmationState = CONFIRM_OK;
  } else {
    LOG(("TRRService starting confirmation test %s %s\n",
         mPrivateURI.get(), host.get()));
    mConfirmer = new TRR(this, host, TRRTYPE_NS, false);
    NS_DispatchToMainThread(mConfirmer);
  }
}

bool
TRRService::MaybeBootstrap(const nsACString &aPossible, nsACString &aResult)
{
  MutexAutoLock lock(mLock);
  if (TRR_DISABLED(mMode) || mBootstrapAddr.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
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

// When running in TRR-only mode, the blacklist is not used and it will also
// try resolving the localhost / .local names.
bool
TRRService::IsTRRBlacklisted(const nsACString &aHost, bool privateBrowsing,
                             bool aParentsToo) // false if domain
{
  if (mClearTRRBLStorage) {
    if (mTRRBLStorage) {
      mTRRBLStorage->Clear();
    }
    mClearTRRBLStorage = false;
  }

  if (mMode == MODE_TRRONLY) {
    return false; // might as well try
  }

  // hardcode these so as to not worry about expiration
  if (StringEndsWith(aHost, NS_LITERAL_CSTRING(".local")) ||
      aHost.Equals(NS_LITERAL_CSTRING("localhost"))) {
    return true;
  }

  if (!Enabled()) {
    return true;
  }
  if (!mTRRBLStorage) {
    return false;
  }

  int32_t dot = aHost.FindChar('.');
  if ((dot == kNotFound) && aParentsToo) {
    // Only if a full host name. Domains can be dotless to be able to
    // blacklist entire TLDs
    return true;
  } else if(dot != kNotFound) {
    // there was a dot, check the parent first
    dot++;
    nsDependentCSubstring domain = Substring(aHost, dot, aHost.Length() - dot);
    nsAutoCString check(domain);

    // recursively check the domain part of this name
    if (IsTRRBlacklisted(check, privateBrowsing, false)) {
      // the domain name of this name is already TRR blacklisted
      return true;
    }
  }

  MutexAutoLock lock(mLock);
  // use a unified casing for the hashkey
  nsAutoCString hashkey(aHost);
  nsCString val(mTRRBLStorage->Get(hashkey, privateBrowsing ?
                                   DataStorage_Private : DataStorage_Persistent));

  if (!val.IsEmpty()) {
    nsresult code;
    int32_t until = val.ToInteger(&code) + mTRRBlacklistExpireTime;
    int32_t expire = NowInSeconds();
    if (NS_SUCCEEDED(code) && (until > expire)) {
      LOG(("Host [%s] is TRR blacklisted\n", nsCString(aHost).get()));
      return true;
    } else {
      // the blacklisted entry has expired
      RefPtr<DataStorage> storage = mTRRBLStorage;
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction("proxyStorageRemove",
                               [storage, hashkey, privateBrowsing]() {
                                 storage->Remove(hashkey, privateBrowsing ?
                                                 DataStorage_Private :
                                                 DataStorage_Persistent);
                               });
      if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(runnable);
      } else {
        runnable->Run();
      }
    }
  }
  return false;
}

class ProxyBlacklist : public Runnable
{
public:
  ProxyBlacklist(TRRService *service, const nsACString &aHost, bool pb, bool aParentsToo)
    : mozilla::Runnable("proxyBlackList")
    , mService(service), mHost(aHost), mPB(pb), mParentsToo(aParentsToo)
  { }

  NS_IMETHOD Run() override
  {
    mService->TRRBlacklist(mHost, mPB, mParentsToo);
    mService = nullptr;
    return NS_OK;
  }

private:
  RefPtr<TRRService> mService;
  nsCString mHost;
  bool      mPB;
  bool      mParentsToo;
};

void
TRRService::TRRBlacklist(const nsACString &aHost, bool privateBrowsing, bool aParentsToo)
{
  if (!mTRRBLStorage) {
    return;
  }

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(new ProxyBlacklist(this, aHost,
                                               privateBrowsing, aParentsToo));
    return;
  }

  LOG(("TRR blacklist %s\n", nsCString(aHost).get()));
  nsAutoCString hashkey(aHost);
  nsAutoCString val;
  val.AppendInt( NowInSeconds() ); // creation time

  // this overwrites any existing entry
  {
    MutexAutoLock lock(mLock);
    mTRRBLStorage->Put(hashkey, val, privateBrowsing ?
                       DataStorage_Private : DataStorage_Persistent);
  }

  if (aParentsToo) {
    // when given a full host name, verify its domain as well
    int32_t dot = aHost.FindChar('.');
    if (dot != kNotFound) {
      // this has a domain to be checked
      dot++;
      nsDependentCSubstring domain = Substring(aHost, dot, aHost.Length() - dot);
      nsAutoCString check(domain);
      if (IsTRRBlacklisted(check, privateBrowsing, false)) {
        // the domain part is already blacklisted, no need to add this entry
        return;
      }
      // verify 'check' over TRR
      LOG(("TRR: verify if '%s' resolves as NS\n", check.get()));

      // check if there's an NS entry for this name
      RefPtr<TRR> trr = new TRR(this, check, TRRTYPE_NS, privateBrowsing);
      NS_DispatchToMainThread(trr);
    }
  }
}

NS_IMETHODIMP
TRRService::Notify(nsITimer *aTimer)
{
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


AHostResolver::LookupStatus
TRRService::CompleteLookup(nsHostRecord *rec, nsresult status, AddrInfo *aNewRRSet, bool pb)
{
  // this is an NS check for the TRR blacklist or confirmationNS check

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!rec);

  nsAutoPtr<AddrInfo> newRRSet(aNewRRSet);
  MOZ_ASSERT(newRRSet && newRRSet->IsTRR() == TRRTYPE_NS);

  MOZ_ASSERT(!mConfirmer || (mConfirmationState == CONFIRM_TRYING));
  if (mConfirmationState == CONFIRM_TRYING) {
    MOZ_ASSERT(mConfirmer);
    mConfirmationState = NS_SUCCEEDED(status) ? CONFIRM_OK : CONFIRM_FAILED;
    LOG(("TRRService finishing confirmation test %s %d %X\n",
         mPrivateURI.get(), (int)mConfirmationState, (unsigned int)status));
    mConfirmer = nullptr;
    if ((mConfirmationState == CONFIRM_FAILED) && (mMode == MODE_TRRONLY)) {
      // in TRR-only mode; retry failed confirmations
      NS_NewTimerWithCallback(getter_AddRefs(mRetryConfirmTimer),
                              this, mRetryConfirmInterval,
                              nsITimer::TYPE_ONE_SHOT);
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
    LOG(("TRR verified %s to be fine!\n", newRRSet->mHostName));
  } else {
    LOG(("TRR says %s doesn't resolve as NS!\n", newRRSet->mHostName));
    TRRBlacklist(nsCString(newRRSet->mHostName), pb, false);
  }
  return LOOKUP_OK;
}

#undef LOG

} // namespace net
} // namespace mozilla
