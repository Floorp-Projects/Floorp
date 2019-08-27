/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingCommon.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/MruCache.h"
#include "mozilla/Pair.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsIDocShell.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIParentChannel.h"
#include "nsIPermission.h"
#include "nsPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsIURL.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsScriptSecurityManager.h"
#include "nsSandboxFlags.h"
#include "prtime.h"

#define ANTITRACKING_PERM_KEY "3rdPartyStorage"

using namespace mozilla;
using mozilla::dom::BrowsingContext;
using mozilla::dom::ContentChild;
using mozilla::dom::Document;

static LazyLogModule gAntiTrackingLog("AntiTracking");
static const nsCString::size_type sMaxSpecLength = 128;
static const uint32_t kMaxConsoleOutputDelayMs = 100;

#define LOG(format) MOZ_LOG(gAntiTrackingLog, mozilla::LogLevel::Debug, format)

#define LOG_SPEC(format, uri)                                       \
  PR_BEGIN_MACRO                                                    \
  if (MOZ_LOG_TEST(gAntiTrackingLog, mozilla::LogLevel::Debug)) {   \
    nsAutoCString _specStr(NS_LITERAL_CSTRING("(null)"));           \
    _specStr.Truncate(std::min(_specStr.Length(), sMaxSpecLength)); \
    if (uri) {                                                      \
      _specStr = uri->GetSpecOrDefault();                           \
    }                                                               \
    const char* _spec = _specStr.get();                             \
    LOG(format);                                                    \
  }                                                                 \
  PR_END_MACRO

namespace {

UniquePtr<nsTArray<AntiTrackingCommon::AntiTrackingSettingsChangedCallback>>
    gSettingsChangedCallbacks;

bool GetParentPrincipalAndTrackingOrigin(
    nsGlobalWindowInner* a3rdPartyTrackingWindow, uint32_t aBehavior,
    nsIPrincipal** aTopLevelStoragePrincipal, nsACString& aTrackingOrigin,
    nsIURI** aTrackingURI, nsIPrincipal** aTrackingPrincipal) {
  Document* doc = a3rdPartyTrackingWindow->GetDocument();
  // Make sure storage access isn't disabled
  if (doc && (doc->StorageAccessSandboxed())) {
    return false;
  }

  // Now we need the principal and the origin of the parent window.
  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal =
      // Use the "top-level storage area principal" behaviour in reject tracker
      // mode only.
      (aBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER)
          ? a3rdPartyTrackingWindow->GetTopLevelStorageAreaPrincipal()
          : a3rdPartyTrackingWindow->GetTopLevelPrincipal();
  if (!topLevelStoragePrincipal) {
    LOG(("No top-level storage area principal at hand"));
    return false;
  }

  // Let's take the principal and the origin of the tracker.
  nsCOMPtr<nsIPrincipal> trackingPrincipal =
      a3rdPartyTrackingWindow->GetPrincipal();
  if (NS_WARN_IF(!trackingPrincipal)) {
    return false;
  }

  nsCOMPtr<nsIURI> trackingURI;
  nsresult rv = trackingPrincipal->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = trackingPrincipal->GetOriginNoSuffix(aTrackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  topLevelStoragePrincipal.forget(aTopLevelStoragePrincipal);
  if (aTrackingURI) {
    trackingURI.forget(aTrackingURI);
  }
  if (aTrackingPrincipal) {
    trackingPrincipal.forget(aTrackingPrincipal);
  }
  return true;
};

void CreatePermissionKey(const nsCString& aTrackingOrigin,
                         nsACString& aPermissionKey) {
  MOZ_ASSERT(aPermissionKey.IsEmpty());

  static const nsLiteralCString prefix =
      NS_LITERAL_CSTRING(ANTITRACKING_PERM_KEY "^");

  aPermissionKey.SetCapacity(prefix.Length() + aTrackingOrigin.Length());
  aPermissionKey.Append(prefix);
  aPermissionKey.Append(aTrackingOrigin);
}

void CreatePermissionKey(const nsCString& aTrackingOrigin,
                         const nsCString& aGrantedOrigin,
                         nsACString& aPermissionKey) {
  MOZ_ASSERT(aPermissionKey.IsEmpty());

  if (aTrackingOrigin == aGrantedOrigin) {
    CreatePermissionKey(aTrackingOrigin, aPermissionKey);
    return;
  }

  static const nsLiteralCString prefix =
      NS_LITERAL_CSTRING(ANTITRACKING_PERM_KEY "^");

  aPermissionKey.SetCapacity(prefix.Length() + 1 + aTrackingOrigin.Length() +
                             aGrantedOrigin.Length());
  aPermissionKey.Append(prefix);
  aPermissionKey.Append(aTrackingOrigin);
  aPermissionKey.AppendLiteral("^");
  aPermissionKey.Append(aGrantedOrigin);
}

// This internal method returns ACCESS_DENY if the access is denied,
// ACCESS_DEFAULT if unknown, some other access code if granted.
uint32_t CheckCookiePermissionForPrincipal(nsICookieSettings* aCookieSettings,
                                           nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aCookieSettings);
  MOZ_ASSERT(aPrincipal);

  uint32_t cookiePermission = nsICookiePermission::ACCESS_DEFAULT;
  if (!aPrincipal->GetIsContentPrincipal()) {
    return cookiePermission;
  }

  nsresult rv =
      aCookieSettings->CookiePermission(aPrincipal, &cookiePermission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsICookiePermission::ACCESS_DEFAULT;
  }

  // If we have a custom cookie permission, let's use it.
  return cookiePermission;
}

int32_t CookiesBehavior(Document* aTopLevelDocument,
                        Document* a3rdPartyDocument) {
  MOZ_ASSERT(aTopLevelDocument);
  MOZ_ASSERT(a3rdPartyDocument);

  // Override the cookiebehavior to accept if the top level document has
  // an extension principal (if the static pref has been set to true
  // to force the old behavior here, See Bug 1525917).
  // This block (and the static pref) should be removed as part of
  // Bug 1537753.
  if (StaticPrefs::extensions_cookiesBehavior_overrideOnTopLevel() &&
      BasePrincipal::Cast(aTopLevelDocument->NodePrincipal())->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // (See Bug 1406675 and Bug 1525917 for rationale).
  if (BasePrincipal::Cast(a3rdPartyDocument->NodePrincipal())->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  return a3rdPartyDocument->CookieSettings()->GetCookieBehavior();
}

int32_t CookiesBehavior(nsILoadInfo* aLoadInfo,
                        nsIPrincipal* aTopLevelPrincipal,
                        nsIURI* a3rdPartyURI) {
  MOZ_ASSERT(aLoadInfo);
  MOZ_ASSERT(aTopLevelPrincipal);
  MOZ_ASSERT(a3rdPartyURI);

  // Override the cookiebehavior to accept if the top level principal is
  // an extension principal (if the static pref has been turned to true
  // to force the old behavior here, See Bug 1525917).
  // This block (and the static pref) should be removed as part of
  // Bug 1537753.
  if (StaticPrefs::extensions_cookiesBehavior_overrideOnTopLevel() &&
      BasePrincipal::Cast(aTopLevelPrincipal)->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  // WebExtensions 3rd party URI always get BEHAVIOR_ACCEPT as cookieBehavior,
  // this is semantically equivalent to the principal having a AddonPolicy().
  if (a3rdPartyURI->SchemeIs("moz-extension")) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  nsCOMPtr<nsICookieSettings> cookieSettings;
  nsresult rv = aLoadInfo->GetCookieSettings(getter_AddRefs(cookieSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsICookieService::BEHAVIOR_REJECT;
  }

  return cookieSettings->GetCookieBehavior();
}

int32_t CookiesBehavior(nsIPrincipal* aPrincipal,
                        nsICookieSettings* aCookieSettings) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCookieSettings);

  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // (See Bug 1406675 for rationale).
  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    return nsICookieService::BEHAVIOR_ACCEPT;
  }

  return aCookieSettings->GetCookieBehavior();
}

struct ContentBlockingAllowListKey {
  ContentBlockingAllowListKey() : mHash(mozilla::HashGeneric(uintptr_t(0))) {}

  // Ensure that we compute a different hash for window and channel pointers of
  // the same numeric value, in the off chance that we get unlucky and encounter
  // a case where the allocator reallocates a window object where a channel used
  // to live and vice versa.
  explicit ContentBlockingAllowListKey(nsPIDOMWindowInner* aWindow)
      : mHash(mozilla::AddToHash(aWindow->WindowID(),
                                 mozilla::HashString("window"))) {}
  explicit ContentBlockingAllowListKey(nsIHttpChannel* aChannel)
      : mHash(mozilla::AddToHash(aChannel->ChannelId(),
                                 mozilla::HashString("channel"))) {}

  ContentBlockingAllowListKey(const ContentBlockingAllowListKey& aRHS)
      : mHash(aRHS.mHash) {}

  bool operator==(const ContentBlockingAllowListKey& aRHS) const {
    return mHash == aRHS.mHash;
  }

  HashNumber GetHash() const { return mHash; }

 private:
  HashNumber mHash;
};

struct ContentBlockingAllowListEntry {
  ContentBlockingAllowListEntry() : mResult(false) {}
  ContentBlockingAllowListEntry(nsPIDOMWindowInner* aWindow, bool aResult)
      : mKey(aWindow), mResult(aResult) {}
  ContentBlockingAllowListEntry(nsIHttpChannel* aChannel, bool aResult)
      : mKey(aChannel), mResult(aResult) {}

  ContentBlockingAllowListKey mKey;
  bool mResult;
};

struct ContentBlockingAllowListCache
    : MruCache<ContentBlockingAllowListKey, ContentBlockingAllowListEntry,
               ContentBlockingAllowListCache> {
  static HashNumber Hash(const ContentBlockingAllowListKey& aKey) {
    return aKey.GetHash();
  }
  static bool Match(const ContentBlockingAllowListKey& aKey,
                    const ContentBlockingAllowListEntry& aValue) {
    return aValue.mKey == aKey;
  }
};

ContentBlockingAllowListCache& GetContentBlockingAllowListCache() {
  static bool initialized = false;
  static ContentBlockingAllowListCache cache;
  if (!initialized) {
    AntiTrackingCommon::OnAntiTrackingSettingsChanged([&] {
      // Drop everything in the cache, since the result of content blocking
      // allow list checks may change past this point.
      cache.Clear();
    });
    initialized = true;
  }
  return cache;
}

bool CheckContentBlockingAllowList(nsIPrincipal* aTopWinPrincipal,
                                   bool aIsPrivateBrowsing) {
  bool isAllowed = false;
  nsresult rv = AntiTrackingCommon::IsOnContentBlockingAllowList(
      aTopWinPrincipal, aIsPrivateBrowsing, isAllowed);
  if (NS_SUCCEEDED(rv) && isAllowed) {
    LOG(
        ("The top-level window is on the content blocking allow list, "
         "bail out early"));
    return true;
  }
  if (NS_FAILED(rv)) {
    LOG(("Checking the content blocking allow list for failed with %" PRIx32,
         static_cast<uint32_t>(rv)));
  }
  return false;
}

bool CheckContentBlockingAllowList(nsPIDOMWindowInner* aWindow) {
  ContentBlockingAllowListKey cacheKey(aWindow);
  auto entry = GetContentBlockingAllowListCache().Lookup(cacheKey);
  if (entry) {
    // We've recently performed a content blocking allow list check for this
    // window, so let's quickly return the answer instead of continuing with the
    // rest of this potentially expensive computation.
    return entry.Data().mResult;
  }

  nsPIDOMWindowOuter* top =
      aWindow->GetBrowsingContext()->Top()->GetDOMWindow();
  Document* doc = top ? top->GetExtantDoc() : nullptr;
  if (doc) {
    bool isPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(doc);

    const bool result = CheckContentBlockingAllowList(
        doc->GetContentBlockingAllowListPrincipal(), isPrivateBrowsing);

    entry.Set(ContentBlockingAllowListEntry(aWindow, result));

    return result;
  }

  LOG(
      ("Could not check the content blocking allow list because the top "
       "window wasn't accessible"));
  entry.Set(ContentBlockingAllowListEntry(aWindow, false));
  return false;
}

bool CheckContentBlockingAllowList(nsIHttpChannel* aChannel) {
  ContentBlockingAllowListKey cacheKey(aChannel);
  auto entry = GetContentBlockingAllowListCache().Lookup(cacheKey);
  if (entry) {
    // We've recently performed a content blocking allow list check for this
    // channel, so let's quickly return the answer instead of continuing with
    // the rest of this potentially expensive computation.
    return entry.Data().mResult;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIHttpChannelInternal> httpChan = do_QueryInterface(aChannel);
  if (httpChan) {
    nsresult rv = httpChan->GetContentBlockingAllowListPrincipal(
        getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal) {
      LOG(
          ("Could not check the content blocking allow list because the top "
           "window wasn't accessible"));
      entry.Set(ContentBlockingAllowListEntry(aChannel, false));
      return false;
    }
  }

  const bool result =
      CheckContentBlockingAllowList(principal, NS_UsePrivateBrowsing(aChannel));
  entry.Set(ContentBlockingAllowListEntry(aChannel, result));
  return result;
}

void RunConsoleReportingRunnable(already_AddRefed<nsIRunnable>&& aRunnable) {
  if (StaticPrefs::privacy_restrict3rdpartystorage_console_lazy()) {
    nsresult rv = NS_DispatchToCurrentThreadQueue(std::move(aRunnable),
                                                  kMaxConsoleOutputDelayMs,
                                                  EventQueuePriority::Idle);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  } else {
    nsCOMPtr<nsIRunnable> runnable(std::move(aRunnable));
    nsresult rv = runnable->Run();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }
}

void ReportBlockingToConsole(nsPIDOMWindowOuter* aWindow, nsIURI* aURI,
                             uint32_t aRejectedReason) {
  MOZ_ASSERT(aWindow && aURI);
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    return;
  }

  RefPtr<Document> doc = docShell->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsAutoString sourceLine;
  uint32_t lineNumber = 0, columnNumber = 0;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    nsJSUtils::GetCallingLocation(cx, sourceLine, &lineNumber, &columnNumber);
  }

  nsCOMPtr<nsIURI> uri(aURI);

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "ReportBlockingToConsoleDelayed",
      [doc, sourceLine, lineNumber, columnNumber, uri, aRejectedReason]() {
        const char* message = nullptr;
        nsAutoCString category;
        switch (aRejectedReason) {
          case nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION:
            message = "CookieBlockedByPermission";
            category = NS_LITERAL_CSTRING("cookieBlockedPermission");
            break;

          case nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER:
            message = "CookieBlockedTracker";
            category = NS_LITERAL_CSTRING("cookieBlockedTracker");
            break;

          case nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL:
            message = "CookieBlockedAll";
            category = NS_LITERAL_CSTRING("cookieBlockedAll");
            break;

          case nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN:
            message = "CookieBlockedForeign";
            category = NS_LITERAL_CSTRING("cookieBlockedForeign");
            break;

          default:
            return;
        }

        MOZ_ASSERT(message);

        // Strip the URL of any possible username/password and make it ready
        // to be presented in the UI.
        nsCOMPtr<nsIURIFixup> urifixup = services::GetURIFixup();
        NS_ENSURE_TRUE_VOID(urifixup);
        nsCOMPtr<nsIURI> exposableURI;
        nsresult rv =
            urifixup->CreateExposableURI(uri, getter_AddRefs(exposableURI));
        NS_ENSURE_SUCCESS_VOID(rv);

        AutoTArray<nsString, 1> params;
        CopyUTF8toUTF16(exposableURI->GetSpecOrDefault(),
                        *params.AppendElement());

        nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, category,
                                        doc, nsContentUtils::eNECKO_PROPERTIES,
                                        message, params, nullptr, sourceLine,
                                        lineNumber, columnNumber);
      });

  RunConsoleReportingRunnable(runnable.forget());
}

void ReportUnblockingToConsole(
    nsPIDOMWindowInner* aWindow, const nsAString& aTrackingOrigin,
    const nsAString& aGrantedOrigin,
    AntiTrackingCommon::StorageAccessGrantedReason aReason) {
  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    return;
  }

  RefPtr<Document> doc = docShell->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsAutoString trackingOrigin(aTrackingOrigin);
  nsAutoString grantedOrigin(aGrantedOrigin);

  nsAutoString sourceLine;
  uint32_t lineNumber = 0, columnNumber = 0;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    nsJSUtils::GetCallingLocation(cx, sourceLine, &lineNumber, &columnNumber);
  }

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "ReportUnblockingToConsoleDelayed",
      [doc, principal, trackingOrigin, grantedOrigin, sourceLine, lineNumber,
       columnNumber, aReason]() {
        nsAutoString origin;
        nsresult rv = nsContentUtils::GetUTFOrigin(principal, origin);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        // Not adding grantedOrigin yet because we may not want it later.
        AutoTArray<nsString, 3> params = {origin, trackingOrigin};
        const char* messageWithDifferentOrigin = nullptr;
        const char* messageWithSameOrigin = nullptr;

        switch (aReason) {
          case AntiTrackingCommon::eStorageAccessAPI:
            messageWithDifferentOrigin =
                "CookieAllowedForOriginOnTrackerByStorageAccessAPI";
            messageWithSameOrigin = "CookieAllowedForTrackerByStorageAccessAPI";
            break;

          case AntiTrackingCommon::eOpenerAfterUserInteraction:
            MOZ_FALLTHROUGH;
          case AntiTrackingCommon::eOpener:
            messageWithDifferentOrigin =
                "CookieAllowedForOriginOnTrackerByHeuristic";
            messageWithSameOrigin = "CookieAllowedForTrackerByHeuristic";
            break;
        }

        if (trackingOrigin == grantedOrigin) {
          nsContentUtils::ReportToConsole(
              nsIScriptError::warningFlag,
              NS_LITERAL_CSTRING("Content Blocking"), doc,
              nsContentUtils::eNECKO_PROPERTIES, messageWithSameOrigin, params,
              nullptr, sourceLine, lineNumber, columnNumber);
        } else {
          params.AppendElement(grantedOrigin);
          nsContentUtils::ReportToConsole(
              nsIScriptError::warningFlag,
              NS_LITERAL_CSTRING("Content Blocking"), doc,
              nsContentUtils::eNECKO_PROPERTIES, messageWithDifferentOrigin,
              params, nullptr, sourceLine, lineNumber, columnNumber);
        }
      });

  RunConsoleReportingRunnable(runnable.forget());
}

already_AddRefed<nsPIDOMWindowOuter> GetTopWindow(nsPIDOMWindowInner* aWindow) {
  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return nullptr;
  }

  nsIChannel* channel = document->GetChannel();
  if (!channel) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin =
      aWindow->GetBrowsingContext()->Top()->GetDOMWindow();

  if (!pwin) {
    return nullptr;
  }

  return pwin.forget();
}

class TemporaryAccessGrantCacheKey : public PLDHashEntryHdr {
 public:
  typedef Pair<nsCOMPtr<nsIPrincipal>, nsCString> KeyType;
  typedef const KeyType* KeyTypePointer;

  explicit TemporaryAccessGrantCacheKey(KeyTypePointer aKey)
      : mPrincipal(aKey->first()), mType(aKey->second()) {}
  TemporaryAccessGrantCacheKey(TemporaryAccessGrantCacheKey&& aOther) = default;

  ~TemporaryAccessGrantCacheKey() = default;

  KeyType GetKey() const { return MakePair(mPrincipal, mType); }
  bool KeyEquals(KeyTypePointer aKey) const {
    return !!mPrincipal == !!aKey->first() && mType == aKey->second() &&
           (mPrincipal ? (mPrincipal->Equals(aKey->first())) : true);
  }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    if (!aKey) {
      return 0;
    }

    BasePrincipal* bp = BasePrincipal::Cast(aKey->first());
    return HashGeneric(bp->GetOriginNoSuffixHash(), bp->GetOriginSuffixHash(),
                       HashString(aKey->second()));
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
};

class TemporaryAccessGrantObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Create(nsPermissionManager* aPM, nsIPrincipal* aPrincipal,
                     const nsACString& aType) {
    MOZ_ASSERT(XRE_IsParentProcess());

    if (!sObservers) {
      sObservers = MakeUnique<ObserversTable>();
    }
    Unused << sObservers
                  ->LookupForAdd(MakePair(nsCOMPtr<nsIPrincipal>(aPrincipal),
                                          nsCString(aType)))
                  .OrInsert([&]() -> nsITimer* {
                    // Only create a new observer if we don't have a matching
                    // entry in our hashtable.
                    nsCOMPtr<nsITimer> timer;
                    RefPtr<TemporaryAccessGrantObserver> observer =
                        new TemporaryAccessGrantObserver(aPM, aPrincipal,
                                                         aType);
                    nsresult rv = NS_NewTimerWithObserver(
                        getter_AddRefs(timer), observer,
                        24 * 60 * 60 * 1000,  // 24 hours
                        nsITimer::TYPE_ONE_SHOT);

                    if (NS_SUCCEEDED(rv)) {
                      observer->SetTimer(timer);
                      return timer;
                    }
                    timer->Cancel();
                    return nullptr;
                  });
  }

  void SetTimer(nsITimer* aTimer) {
    mTimer = aTimer;
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    }
  }

 private:
  TemporaryAccessGrantObserver(nsPermissionManager* aPM,
                               nsIPrincipal* aPrincipal,
                               const nsACString& aType)
      : mPM(aPM), mPrincipal(aPrincipal), mType(aType) {
    MOZ_ASSERT(XRE_IsParentProcess(),
               "Enforcing temporary access grant lifetimes can only be done in "
               "the parent process");
  }

  ~TemporaryAccessGrantObserver() = default;

 private:
  typedef nsDataHashtable<TemporaryAccessGrantCacheKey, nsCOMPtr<nsITimer>>
      ObserversTable;
  static UniquePtr<ObserversTable> sObservers;
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<nsPermissionManager> mPM;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
};

UniquePtr<TemporaryAccessGrantObserver::ObserversTable>
    TemporaryAccessGrantObserver::sObservers;

NS_IMPL_ISUPPORTS(TemporaryAccessGrantObserver, nsIObserver)

NS_IMETHODIMP
TemporaryAccessGrantObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  if (strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0) {
    Unused << mPM->RemoveFromPrincipal(mPrincipal, mType);

    MOZ_ASSERT(sObservers);
    sObservers->Remove(MakePair(mPrincipal, mType));
  } else if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
    sObservers.reset();
  }

  return NS_OK;
}

class SettingsChangeObserver final : public nsIObserver {
  ~SettingsChangeObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void PrivacyPrefChanged(const char* aPref = nullptr, void* = nullptr);

 private:
  static void RunAntiTrackingSettingsChangedCallbacks();
};

NS_IMPL_ISUPPORTS(SettingsChangeObserver, nsIObserver)

NS_IMETHODIMP SettingsChangeObserver::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "perm-added");
      obs->RemoveObserver(this, "perm-changed");
      obs->RemoveObserver(this, "perm-cleared");
      obs->RemoveObserver(this, "perm-deleted");
      obs->RemoveObserver(this, "xpcom-shutdown");

      Preferences::UnregisterPrefixCallback(
          SettingsChangeObserver::PrivacyPrefChanged,
          "browser.contentblocking.");
      Preferences::UnregisterPrefixCallback(
          SettingsChangeObserver::PrivacyPrefChanged, "network.cookie.");
      Preferences::UnregisterPrefixCallback(
          SettingsChangeObserver::PrivacyPrefChanged, "privacy.");

      gSettingsChangedCallbacks = nullptr;
    }
  } else {
    nsCOMPtr<nsIPermission> perm = do_QueryInterface(aSubject);
    if (perm) {
      nsAutoCString type;
      nsresult rv = perm->GetType(type);
      if (NS_WARN_IF(NS_FAILED(rv)) || type.Equals(USER_INTERACTION_PERM)) {
        // Ignore failures or notifications that have been sent because of
        // user interactions.
        return NS_OK;
      }
    }

    RunAntiTrackingSettingsChangedCallbacks();
  }

  return NS_OK;
}

// static
void SettingsChangeObserver::PrivacyPrefChanged(const char* aPref,
                                                void* aClosure) {
  RunAntiTrackingSettingsChangedCallbacks();
}

// static
void SettingsChangeObserver::RunAntiTrackingSettingsChangedCallbacks() {
  if (gSettingsChangedCallbacks) {
    for (auto& callback : *gSettingsChangedCallbacks) {
      callback();
    }
  }
}

bool CheckAntiTrackingPermission(nsIPrincipal* aPrincipal,
                                 const nsAutoCString& aType,
                                 bool aIsInPrivateBrowsing,
                                 uint32_t* aRejectedReason,
                                 uint32_t aBlockedReason,
                                 nsIURI* aPrincipalURI) {
  nsPermissionManager* permManager = nsPermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    LOG(("Failed to obtain the permission manager"));
    return false;
  }

  uint32_t result = 0;
  if (aIsInPrivateBrowsing) {
    LOG_SPEC(("Querying the permissions for private modei looking for a "
              "permission of type %s for %s",
              aType.get(), _spec),
             aPrincipalURI);
    nsCOMPtr<nsISimpleEnumerator> se;
    nsresult rv =
        permManager->GetAllForPrincipal(aPrincipal, getter_AddRefs(se));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Failed to get the list of permissions"));
      return false;
    }

    bool more = false;
    bool found = false;
    while (NS_SUCCEEDED(se->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsISupports> supports;
      Unused << se->GetNext(getter_AddRefs(supports));
      nsCOMPtr<nsIPermission> permission = do_QueryInterface(supports);
      if (!permission) {
        LOG(("Couldn't get the permission for unknown reasons"));
        continue;
      }

      nsAutoCString permissionType;
      if (NS_SUCCEEDED(permission->GetType(permissionType)) &&
          permissionType != aType) {
        LOG(("Non-matching permission type: %s", aType.get()));
        continue;
      }

      uint32_t capability = 0;
      if (NS_SUCCEEDED(permission->GetCapability(&capability)) &&
          capability != nsIPermissionManager::ALLOW_ACTION) {
        LOG(("Non-matching permission capability: %d", capability));
        continue;
      }

      uint32_t expirationType = 0;
      if (NS_SUCCEEDED(permission->GetExpireType(&expirationType)) &&
          expirationType != nsIPermissionManager ::EXPIRE_SESSION) {
        LOG(("Non-matching permission expiration type: %d", expirationType));
        continue;
      }

      int64_t expirationTime = 0;
      if (NS_SUCCEEDED(permission->GetExpireTime(&expirationTime)) &&
          expirationTime != 0) {
        LOG(("Non-matching permission expiration time: %" PRId64,
             expirationTime));
        continue;
      }

      LOG(("Found a matching permission"));
      found = true;
    }

    if (!found) {
      if (aRejectedReason) {
        *aRejectedReason = aBlockedReason;
      }
      return false;
    }
  } else {
    nsresult rv = permManager->TestPermissionWithoutDefaultsFromPrincipal(
        aPrincipal, aType, &result);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Failed to test the permission"));
      return false;
    }

    LOG_SPEC(
        ("Testing permission type %s for %s resulted in %d (%s)", aType.get(),
         _spec, int(result),
         result == nsIPermissionManager::ALLOW_ACTION ? "success" : "failure"),
        aPrincipalURI);

    if (result != nsIPermissionManager::ALLOW_ACTION) {
      if (aRejectedReason) {
        *aRejectedReason = aBlockedReason;
      }
      return false;
    }
  }

  return true;
}

}  // namespace

/* static */ RefPtr<AntiTrackingCommon::StorageAccessGrantPromise>
AntiTrackingCommon::AddFirstPartyStorageAccessGrantedFor(
    nsIPrincipal* aPrincipal, nsPIDOMWindowInner* aParentWindow,
    StorageAccessGrantedReason aReason,
    const AntiTrackingCommon::PerformFinalChecks& aPerformFinalChecks) {
  MOZ_ASSERT(aParentWindow);

  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(!uri)) {
    LOG(("Can't get the URI from the principal"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsAutoCString origin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(uri, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the origin from the URI"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  LOG(("Adding a first-party storage exception for %s...",
       PromiseFlatCString(origin).get()));

  Document* parentDoc = aParentWindow->GetExtantDoc();
  if (!parentDoc) {
    LOG(("Parent window has no doc"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }
  int32_t behavior = parentDoc->CookieSettings()->GetCookieBehavior();

  if (!parentDoc->CookieSettings()->GetRejectThirdPartyTrackers()) {
    LOG(
        ("Disabled by network.cookie.cookieBehavior pref (%d), bailing out "
         "early",
         behavior));
    return StorageAccessGrantPromise::CreateAndResolve(true, __func__);
  }

  MOZ_ASSERT(
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  if (CheckContentBlockingAllowList(aParentWindow)) {
    return StorageAccessGrantPromise::CreateAndResolve(true, __func__);
  }

  nsCOMPtr<nsIPrincipal> topLevelStoragePrincipal;
  nsCOMPtr<nsIURI> trackingURI;
  nsAutoCString trackingOrigin;
  nsCOMPtr<nsIPrincipal> trackingPrincipal;

  RefPtr<nsGlobalWindowInner> parentWindow =
      nsGlobalWindowInner::Cast(aParentWindow);
  nsGlobalWindowOuter* outerParentWindow =
      nsGlobalWindowOuter::Cast(parentWindow->GetOuterWindow());
  if (NS_WARN_IF(!outerParentWindow)) {
    LOG(("No outer window found for our parent window, bailing out early"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  LOG(("The current resource is %s-party",
       outerParentWindow->IsTopLevelWindow() ? "first" : "third"));

  // We are a first party resource.
  if (outerParentWindow->IsTopLevelWindow()) {
    trackingOrigin = origin;
    trackingPrincipal = aPrincipal;
    rv = trackingPrincipal->GetURI(getter_AddRefs(trackingURI));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Couldn't get the tracking principal URI"));
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }
    topLevelStoragePrincipal = parentWindow->GetPrincipal();
    if (NS_WARN_IF(!topLevelStoragePrincipal)) {
      LOG(("Top-level storage area principal not found, bailing out early"));
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }

  } else {
    // We should be a 3rd party source.
    bool isThirdParty = false;
    if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
      isThirdParty =
          nsContentUtils::IsThirdPartyTrackingResourceWindow(parentWindow);
    } else if (behavior == nsICookieService::
                               BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
      isThirdParty = nsContentUtils::IsThirdPartyWindowOrChannel(
          parentWindow, nullptr, nullptr);
    }

    if (!isThirdParty) {
      if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
        LOG(("Our window isn't a third-party tracking window"));
      } else if (behavior ==
                 nsICookieService::
                     BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
        LOG(("Our window isn't a third-party window"));
      }
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }

    if (!GetParentPrincipalAndTrackingOrigin(
            parentWindow, behavior, getter_AddRefs(topLevelStoragePrincipal),
            trackingOrigin, getter_AddRefs(trackingURI),
            getter_AddRefs(trackingPrincipal))) {
      LOG(
          ("Error while computing the parent principal and tracking origin, "
           "bailing out early"));
      return StorageAccessGrantPromise::CreateAndReject(false, __func__);
    }
  }

  nsPIDOMWindowOuter* topOuterWindow =
      aParentWindow->GetBrowsingContext()->Top()->GetDOMWindow();
  nsGlobalWindowOuter* topWindow = nsGlobalWindowOuter::Cast(topOuterWindow);
  if (NS_WARN_IF(!topWindow)) {
    LOG(("No top outer window."));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsPIDOMWindowInner* topInnerWindow = topWindow->GetCurrentInnerWindow();
  if (NS_WARN_IF(!topInnerWindow)) {
    LOG(("No top inner window."));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  // We hardcode this block reason since the first-party storage access
  // permission is granted for the purpose of blocking trackers.
  // Note that if aReason is eOpenerAfterUserInteraction and the
  // trackingPrincipal is not in a blacklist, we don't check the
  // user-interaction state, because it could be that the current process has
  // just sent the request to store the user-interaction permission into the
  // parent, without having received the permission itself yet.
  //
  // We define this as an enum, since without that MSVC fails to capturing this
  // name inside the lambda without the explicit capture and clang warns if
  // there is an explicit capture with -Wunused-lambda-capture.
  enum : uint32_t {
    blockReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER
  };
  if (nsContentUtils::IsURIInPrefList(trackingURI,
                                      "privacy.restrict3rdpartystorage."
                                      "userInteractionRequiredForHosts") &&
      !HasUserInteraction(trackingPrincipal)) {
    LOG_SPEC(("Tracking principal (%s) hasn't been interacted with before, "
              "refusing to add a first-party storage permission to access it",
              _spec),
             trackingURI);
    NotifyBlockingDecision(aParentWindow, BlockingDecision::eBlock,
                           blockReason);
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin = GetTopWindow(parentWindow);
  if (!pwin) {
    LOG(("Couldn't get the top window"));
    return StorageAccessGrantPromise::CreateAndReject(false, __func__);
  }

  auto storePermission =
      [pwin, parentWindow, origin, trackingOrigin, trackingPrincipal,
       trackingURI, topInnerWindow, topLevelStoragePrincipal,
       aReason](int aAllowMode) -> RefPtr<StorageAccessGrantPromise> {
    nsAutoCString permissionKey;
    CreatePermissionKey(trackingOrigin, origin, permissionKey);

    // Let's store the permission in the current parent window.
    topInnerWindow->SaveStorageAccessGranted(permissionKey);

    // Let's inform the parent window.
    parentWindow->StorageAccessGranted();

    nsIChannel* channel =
        pwin->GetCurrentInnerWindow()->GetExtantDoc()->GetChannel();

    pwin->NotifyContentBlockingEvent(blockReason, channel, false, trackingURI,
                                     parentWindow->GetExtantDoc()->GetChannel(),
                                     Some(aReason));

    ReportUnblockingToConsole(parentWindow,
                              NS_ConvertUTF8toUTF16(trackingOrigin),
                              NS_ConvertUTF8toUTF16(origin), aReason);

    if (XRE_IsParentProcess()) {
      LOG(("Saving the permission: trackingOrigin=%s, grantedOrigin=%s",
           trackingOrigin.get(), origin.get()));

      return SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(
                 topLevelStoragePrincipal, trackingPrincipal, trackingOrigin,
                 origin, aAllowMode)
          ->Then(GetCurrentThreadSerialEventTarget(), __func__,
                 [](FirstPartyStorageAccessGrantPromise::ResolveOrRejectValue&&
                        aValue) {
                   if (aValue.IsResolve()) {
                     return StorageAccessGrantPromise::CreateAndResolve(
                         NS_SUCCEEDED(aValue.ResolveValue()) ? eAllowOnAnySite
                                                             : eAllow,
                         __func__);
                   }
                   return StorageAccessGrantPromise::CreateAndReject(false,
                                                                     __func__);
                 });
    }

    ContentChild* cc = ContentChild::GetSingleton();
    MOZ_ASSERT(cc);

    LOG(
        ("Asking the parent process to save the permission for us: "
         "trackingOrigin=%s, grantedOrigin=%s",
         trackingOrigin.get(), origin.get()));

    // This is not really secure, because here we have the content process
    // sending the request of storing a permission.
    return cc
        ->SendFirstPartyStorageAccessGrantedForOrigin(
            IPC::Principal(topLevelStoragePrincipal),
            IPC::Principal(trackingPrincipal), trackingOrigin, origin,
            aAllowMode)
        ->Then(GetCurrentThreadSerialEventTarget(), __func__,
               [](const ContentChild::
                      FirstPartyStorageAccessGrantedForOriginPromise::
                          ResolveOrRejectValue& aValue) {
                 if (aValue.IsResolve()) {
                   return StorageAccessGrantPromise::CreateAndResolve(
                       aValue.ResolveValue(), __func__);
                 }
                 return StorageAccessGrantPromise::CreateAndReject(false,
                                                                   __func__);
               });
  };

  if (aPerformFinalChecks) {
    return aPerformFinalChecks()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [storePermission](
            StorageAccessGrantPromise::ResolveOrRejectValue&& aValue) {
          if (aValue.IsResolve()) {
            return storePermission(aValue.ResolveValue());
          }
          return StorageAccessGrantPromise::CreateAndReject(false, __func__);
        });
  }
  return storePermission(false);
}

/* static */
RefPtr<mozilla::AntiTrackingCommon::FirstPartyStorageAccessGrantPromise>
AntiTrackingCommon::SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(
    nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrincipal,
    const nsCString& aTrackingOrigin, const nsCString& aGrantedOrigin,
    int aAllowMode) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aAllowMode == eAllow || aAllowMode == eAllowAutoGrant ||
             aAllowMode == eAllowOnAnySite);

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << aParentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));
  LOG_SPEC(("Saving a first-party storage permission on %s for "
            "trackingOrigin=%s grantedOrigin=%s",
            _spec, aTrackingOrigin.get(), aGrantedOrigin.get()),
           parentPrincipalURI);

  if (NS_WARN_IF(!aParentPrincipal)) {
    // The child process is sending something wrong. Let's ignore it.
    LOG(("aParentPrincipal is null, bailing out early"));
    return FirstPartyStorageAccessGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  nsPermissionManager* permManager = nsPermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    LOG(("Permission manager is null, bailing out early"));
    return FirstPartyStorageAccessGrantPromise::CreateAndReject(false,
                                                                __func__);
  }

  // Remember that this pref is stored in seconds!
  uint32_t expirationType = nsIPermissionManager::EXPIRE_TIME;
  uint32_t expirationTime =
      StaticPrefs::privacy_restrict3rdpartystorage_expiration() * 1000;
  int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

  nsresult rv;
  if (aAllowMode == eAllowOnAnySite) {
    uint32_t privateBrowsingId = 0;
    rv = aTrackingPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) {
      // If we are coming from a private window, make sure to store a
      // session-only permission which won't get persisted to disk.
      expirationType = nsIPermissionManager::EXPIRE_SESSION;
      when = 0;
    }

    LOG(
        ("Setting 'any site' permission expiry: %u, proceeding to save in the "
         "permission manager",
         expirationTime));

    rv = permManager->AddFromPrincipal(
        aTrackingPrincipal, NS_LITERAL_CSTRING("cookie"),
        nsICookiePermission::ACCESS_ALLOW, expirationType, when);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  // We must grant the storage permission also if we allow it for any site
  // because the setting 'cookie' permission is not applied to existing
  // documents (See CookieSettings documentation).

  uint32_t privateBrowsingId = 0;
  rv = aParentPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if ((!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) ||
      (aAllowMode == eAllowAutoGrant)) {
    // If we are coming from a private window or are automatically granting a
    // permission, make sure to store a session-only permission which won't
    // get persisted to disk.
    expirationType = nsIPermissionManager::EXPIRE_SESSION;
    when = 0;
  }

  nsAutoCString type;
  CreatePermissionKey(aTrackingOrigin, aGrantedOrigin, type);

  LOG(
      ("Computed permission key: %s, expiry: %u, proceeding to save in the "
       "permission manager",
       type.get(), expirationTime));

  rv = permManager->AddFromPrincipal(aParentPrincipal, type,
                                     nsIPermissionManager::ALLOW_ACTION,
                                     expirationType, when);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  if (NS_SUCCEEDED(rv) && (aAllowMode == eAllowAutoGrant)) {
    // Make sure temporary access grants do not survive more than 24 hours.
    TemporaryAccessGrantObserver::Create(permManager, aParentPrincipal, type);
  }

  LOG(("Result: %s", NS_SUCCEEDED(rv) ? "success" : "failure"));
  return FirstPartyStorageAccessGrantPromise::CreateAndResolve(rv, __func__);
}

// static
bool AntiTrackingCommon::CreateStoragePermissionKey(nsIPrincipal* aPrincipal,
                                                    nsACString& aKey) {
  MOZ_ASSERT(aPrincipal);

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOriginNoSuffix(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  CreatePermissionKey(origin, aKey);
  return true;
}

// static
bool AntiTrackingCommon::IsStorageAccessPermission(nsIPermission* aPermission,
                                                   nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPermission);
  MOZ_ASSERT(aPrincipal);

  // The permission key may belong either to a tracking origin on the same
  // origin as the granted origin, or on another origin as the granted origin
  // (for example when a tracker in a third-party context uses window.open to
  // open another origin where that second origin would be the granted origin.)
  // But even in the second case, the type of the permission would still be
  // formed by concatenating the granted origin to the end of the type name
  // (see CreatePermissionKey).  Therefore, we pass in the same argument to
  // both tracking origin and granted origin here in order to compute the
  // shorter permission key and will then do a prefix match on the type of the
  // input permission to see if it is a storage access permission or not.
  nsAutoCString permissionKey;
  bool result = CreateStoragePermissionKey(aPrincipal, permissionKey);
  if (NS_WARN_IF(!result)) {
    return false;
  }

  nsAutoCString type;
  nsresult rv = aPermission->GetType(type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return StringBeginsWith(type, permissionKey);
}

bool AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
    nsPIDOMWindowInner* aWindow, nsIURI* aURI, uint32_t* aRejectedReason) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aURI);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  LOG_SPEC(("Computing whether window %p has access to URI %s", aWindow, _spec),
           aURI);

  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(aWindow);
  Document* document = innerWindow->GetExtantDoc();
  if (!document) {
    LOG(("Our window has no document"));
    return false;
  }

  nsGlobalWindowOuter* topWindow = nsGlobalWindowOuter::Cast(
      aWindow->GetBrowsingContext()->Top()->GetDOMWindow());
  if (NS_WARN_IF(!topWindow)) {
    LOG(("No top outer window"));
    return false;
  }

  nsPIDOMWindowInner* topInnerWindow = topWindow->GetCurrentInnerWindow();
  if (NS_WARN_IF(!topInnerWindow)) {
    LOG(("No top inner window."));
    return false;
  }

  Document* toplevelDocument = topInnerWindow->GetExtantDoc();
  if (!toplevelDocument) {
    LOG(("No top level document."));
    return false;
  }

  MOZ_ASSERT(toplevelDocument);

  uint32_t cookiePermission = CheckCookiePermissionForPrincipal(
      document->CookieSettings(), document->NodePrincipal());
  if (cookiePermission != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for window's principal, returning %s",
         int(cookiePermission),
         cookiePermission != nsICookiePermission::ACCESS_DENY ? "success"
                                                              : "failure"));
    if (cookiePermission != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  int32_t behavior = CookiesBehavior(toplevelDocument, document);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (CheckContentBlockingAllowList(aWindow)) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    LOG(("The cookie behavior pref mandates rejecting all cookies!"));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return false;
  }

  // As a performance optimization, we only perform this check for
  // BEHAVIOR_REJECT_FOREIGN and BEHAVIOR_LIMIT_FOREIGN.  For
  // BEHAVIOR_REJECT_TRACKER and BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  // third-partiness is implicily checked later below.
  if (behavior != nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      behavior !=
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    // Let's check if this is a 3rd party context.
    if (!nsContentUtils::IsThirdPartyWindowOrChannel(aWindow, nullptr, aURI)) {
      LOG(("Our window isn't a third-party window"));
      return true;
    }
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    LOG(("Nothing more to do due to the behavior code %d", int(behavior)));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    return false;
  }

  MOZ_ASSERT(
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  uint32_t blockedReason =
      nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;

  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    if (!nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow)) {
      LOG(("Our window isn't a third-party tracking window"));
      return true;
    }
  } else {
    MOZ_ASSERT(behavior ==
               nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);
    if (nsContentUtils::IsThirdPartyTrackingResourceWindow(aWindow)) {
      // fall through
    } else if (nsContentUtils::IsThirdPartyWindowOrChannel(aWindow, nullptr,
                                                           aURI)) {
      LOG(("We're in the third-party context, storage should be partitioned"));
      // fall through, but remember that we're partitioning.
      blockedReason = nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
    } else {
      LOG(("Our window isn't a third-party window, storage is allowed"));
      return true;
    }
  }

#ifdef DEBUG
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (thirdPartyUtil) {
    bool thirdParty = false;
    nsresult rv = thirdPartyUtil->IsThirdPartyWindow(aWindow->GetOuterWindow(),
                                                     aURI, &thirdParty);
    // The result of this assertion depends on whether IsThirdPartyWindow
    // succeeds, because otherwise IsThirdPartyWindowOrChannel artificially
    // fails.
    MOZ_ASSERT_IF(NS_SUCCEEDED(rv), nsContentUtils::IsThirdPartyWindowOrChannel(
                                        aWindow, nullptr, aURI) == thirdParty);
  }
#endif

  nsCOMPtr<nsIPrincipal> parentPrincipal;
  nsCOMPtr<nsIURI> parentPrincipalURI;
  nsCOMPtr<nsIURI> trackingURI;
  nsAutoCString trackingOrigin;
  if (!GetParentPrincipalAndTrackingOrigin(
          nsGlobalWindowInner::Cast(aWindow), behavior,
          getter_AddRefs(parentPrincipal), trackingOrigin,
          getter_AddRefs(trackingURI), nullptr)) {
    LOG(("Failed to obtain the parent principal and the tracking origin"));
    *aRejectedReason = blockedReason;
    return false;
  }
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));

  nsAutoCString grantedOrigin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(aURI, grantedOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(trackingOrigin, grantedOrigin, type);

  if (topInnerWindow->HasStorageAccessGranted(type)) {
    LOG(("Permission stored in the window. All good."));
    return true;
  }

  return CheckAntiTrackingPermission(
      parentPrincipal, type, nsContentUtils::IsInPrivateBrowsing(document),
      aRejectedReason, blockedReason, parentPrincipalURI);
}

bool AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
    nsIChannel* aChannel, nsIURI* aURI, uint32_t* aRejectedReason) {
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aChannel);

  // Let's avoid a null check on aRejectedReason everywhere else.
  uint32_t rejectedReason = 0;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  nsIScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  MOZ_ASSERT(ssm);

  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  if (NS_FAILED(rv)) {
    LOG(("Failed to get the channel final URI, bail out early"));
    return true;
  }
  LOG_SPEC(
      ("Computing whether channel %p has access to URI %s", aChannel, _spec),
      channelURI);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  // We need to find the correct principal to check the cookie permission. For
  // third-party contexts, we want to check if the top-level window has a custom
  // cookie permission.
  nsCOMPtr<nsIPrincipal> toplevelPrincipal = loadInfo->GetTopLevelPrincipal();

  // If this is already the top-level window, we should use the loading
  // principal.
  if (!toplevelPrincipal) {
    LOG(
        ("Our loadInfo lacks a top-level principal, use the loadInfo's loading "
         "principal instead"));
    toplevelPrincipal = loadInfo->LoadingPrincipal();
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);

  // If we don't have a loading principal and this is a document channel, we are
  // a top-level window!
  if (!toplevelPrincipal) {
    LOG(
        ("We don't have a loading principal, let's see if this is a document "
         "channel"
         " that belongs to a top-level window"));
    bool isDocument = false;
    if (httpChannel) {
      rv = httpChannel->GetIsMainDocumentChannel(&isDocument);
    }
    if (httpChannel && NS_SUCCEEDED(rv) && isDocument) {
      rv = ssm->GetChannelResultPrincipal(aChannel,
                                          getter_AddRefs(toplevelPrincipal));
      if (NS_SUCCEEDED(rv)) {
        LOG(("Yes, we guessed right!"));
      } else {
        LOG(
            ("Yes, we guessed right, but minting the channel result principal "
             "failed"));
      }
    } else {
      LOG(("No, we guessed wrong!"));
    }
  }

  // Let's use the triggering principal then.
  if (!toplevelPrincipal) {
    LOG(
        ("Our loadInfo lacks a top-level principal, use the loadInfo's "
         "triggering principal instead"));
    toplevelPrincipal = loadInfo->TriggeringPrincipal();
  }

  if (NS_WARN_IF(!toplevelPrincipal)) {
    LOG(("No top-level principal! Bail out early"));
    return false;
  }

  nsCOMPtr<nsICookieSettings> cookieSettings;
  rv = loadInfo->GetCookieSettings(getter_AddRefs(cookieSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(
        ("Failed to get the cookie settings from the loadinfo, bail out "
         "early"));
    return true;
  }

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  rv = ssm->GetChannelResultPrincipal(aChannel,
                                      getter_AddRefs(channelPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("No channel principal, bail out early"));
    return false;
  }

  uint32_t cookiePermission =
      CheckCookiePermissionForPrincipal(cookieSettings, channelPrincipal);
  if (cookiePermission != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d) for channel's principal, returning %s",
         int(cookiePermission),
         cookiePermission != nsICookiePermission::ACCESS_DENY ? "success"
                                                              : "failure"));
    if (cookiePermission != nsICookiePermission::ACCESS_DENY) {
      return true;
    }

    *aRejectedReason =
        nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
    return false;
  }

  if (!channelURI) {
    LOG(("No channel uri, bail out early"));
    return false;
  }

  int32_t behavior = CookiesBehavior(loadInfo, toplevelPrincipal, channelURI);
  if (behavior == nsICookieService::BEHAVIOR_ACCEPT) {
    LOG(("The cookie behavior pref mandates accepting all cookies!"));
    return true;
  }

  if (httpChannel && CheckContentBlockingAllowList(httpChannel)) {
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT) {
    LOG(("The cookie behavior pref mandates rejecting all cookies!"));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return false;
  }

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (!thirdPartyUtil) {
    LOG(("No thirdPartyUtil, bail out early"));
    return true;
  }

  bool thirdParty = false;
  rv = thirdPartyUtil->IsThirdPartyChannel(aChannel, aURI, &thirdParty);
  // Grant if it's not a 3rd party.
  // Be careful to check the return value of IsThirdPartyChannel, since
  // IsThirdPartyChannel() will fail if the channel's loading principal is the
  // system principal...
  if (NS_SUCCEEDED(rv) && !thirdParty) {
    LOG(("Our channel isn't a third-party channel"));
    return true;
  }

  if (behavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN ||
      behavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
    // XXX For non-cookie forms of storage, we handle BEHAVIOR_LIMIT_FOREIGN by
    // simply rejecting the request to use the storage. In the future, if we
    // change the meaning of BEHAVIOR_LIMIT_FOREIGN to be one which makes sense
    // for non-cookie storage types, this may change.
    LOG(("Nothing more to do due to the behavior code %d", int(behavior)));
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    return false;
  }

  MOZ_ASSERT(
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);

  uint32_t blockedReason =
      nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;

  // Not a tracker.
  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER) {
    if (httpChannel && !httpChannel->IsThirdPartyTrackingResource()) {
      LOG(("Our channel isn't a third-party tracking channel"));
      return true;
    }
  } else {
    MOZ_ASSERT(behavior ==
               nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN);
    if (httpChannel && httpChannel->IsThirdPartyTrackingResource()) {
      // fall through
    } else if (nsContentUtils::IsThirdPartyWindowOrChannel(nullptr, aChannel,
                                                           aURI)) {
      LOG(("We're in the third-party context, storage should be partitioned"));
      // fall through but remember that we're partitioning.
      blockedReason = nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
    } else {
      LOG(("Our channel isn't a third-party channel, storage is allowed"));
      return true;
    }
  }

  // Only use the "top-level storage area principal" behaviour for reject
  // tracker mode only.
  nsIPrincipal* parentPrincipal =
      (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER)
          ? loadInfo->GetTopLevelStorageAreaPrincipal()
          : loadInfo->GetTopLevelPrincipal();
  if (!parentPrincipal) {
    LOG(("No top-level storage area principal at hand"));

    // parentPrincipal can be null if the parent window is not the top-level
    // window.
    if (loadInfo->GetTopLevelPrincipal()) {
      LOG(("Parent window is the top-level window, bail out early"));
      *aRejectedReason = blockedReason;
      return false;
    }

    parentPrincipal = toplevelPrincipal;
    if (NS_WARN_IF(!parentPrincipal)) {
      LOG(
          ("No triggering principal, this shouldn't be happening! Bail out "
           "early"));
      // Why we are here?!?
      return true;
    }
  }

  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));

  // Let's see if we have to grant the access for this particular channel.

  nsCOMPtr<nsIURI> trackingURI;
  rv = aChannel->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to get the channel URI"));
    return true;
  }

  nsAutoCString trackingOrigin;
  rv = nsContentUtils::GetASCIIOrigin(trackingURI, trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), trackingURI);
    return false;
  }

  nsAutoCString origin;
  rv = nsContentUtils::GetASCIIOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  nsAutoCString type;
  CreatePermissionKey(trackingOrigin, origin, type);

  uint32_t privateBrowsingId = 0;
  rv = channelPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Failed to get the channel principal's private browsing ID"));
    return false;
  }

  return CheckAntiTrackingPermission(parentPrincipal, type, !!privateBrowsingId,
                                     aRejectedReason, blockedReason,
                                     parentPrincipalURI);
}

bool AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
    nsIPrincipal* aPrincipal, nsICookieSettings* aCookieSettings) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCookieSettings);

  uint32_t access = nsICookiePermission::ACCESS_DEFAULT;
  if (aPrincipal->GetIsContentPrincipal()) {
    nsPermissionManager* permManager = nsPermissionManager::GetInstance();
    if (permManager) {
      Unused << NS_WARN_IF(NS_FAILED(permManager->TestPermissionFromPrincipal(
          aPrincipal, NS_LITERAL_CSTRING("cookie"), &access)));
    }
  }

  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    return access != nsICookiePermission::ACCESS_DENY;
  }

  int32_t behavior = CookiesBehavior(aPrincipal, aCookieSettings);
  return behavior != nsICookieService::BEHAVIOR_REJECT;
}

/* static */
bool AntiTrackingCommon::MaybeIsFirstPartyStorageAccessGrantedFor(
    nsPIDOMWindowInner* aFirstPartyWindow, nsIURI* aURI) {
  MOZ_ASSERT(aFirstPartyWindow);
  MOZ_ASSERT(aURI);

  LOG_SPEC(
      ("Computing a best guess as to whether window %p has access to URI %s",
       aFirstPartyWindow, _spec),
      aURI);

  Document* parentDocument =
      nsGlobalWindowInner::Cast(aFirstPartyWindow)->GetExtantDoc();
  if (NS_WARN_IF(!parentDocument)) {
    LOG(("Failed to get the first party window's document"));
    return false;
  }

  if (!parentDocument->CookieSettings()->GetRejectThirdPartyTrackers()) {
    LOG(("Disabled by the pref (%d), bail out early",
         parentDocument->CookieSettings()->GetCookieBehavior()));
    return true;
  }

  if (CheckContentBlockingAllowList(aFirstPartyWindow)) {
    return true;
  }

  if (!nsContentUtils::IsThirdPartyWindowOrChannel(aFirstPartyWindow, nullptr,
                                                   aURI)) {
    LOG(("Our window isn't a third-party window"));
    return true;
  }

  uint32_t cookiePermission = CheckCookiePermissionForPrincipal(
      parentDocument->CookieSettings(), parentDocument->NodePrincipal());
  if (cookiePermission != nsICookiePermission::ACCESS_DEFAULT) {
    LOG(
        ("CheckCookiePermissionForPrincipal() returned a non-default access "
         "code (%d), returning %s",
         int(cookiePermission),
         cookiePermission != nsICookiePermission::ACCESS_DENY ? "success"
                                                              : "failure"));
    return cookiePermission != nsICookiePermission::ACCESS_DENY;
  }

  nsAutoCString origin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_SPEC(("Failed to compute the origin from %s", _spec), aURI);
    return false;
  }

  nsIPrincipal* parentPrincipal = parentDocument->NodePrincipal();
  nsCOMPtr<nsIURI> parentPrincipalURI;
  Unused << parentPrincipal->GetURI(getter_AddRefs(parentPrincipalURI));

  nsAutoCString type;
  CreatePermissionKey(origin, type);

  return CheckAntiTrackingPermission(
      parentPrincipal, type,
      nsContentUtils::IsInPrivateBrowsing(parentDocument), nullptr, 0,
      parentPrincipalURI);
}

nsresult AntiTrackingCommon::IsOnContentBlockingAllowList(
    nsIPrincipal* aTopWinPrincipal, bool aIsPrivateBrowsing,
    bool& aIsAllowListed) {
  aIsAllowListed = false;

  if (!aTopWinPrincipal) {
    // Nothing to do!
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  Unused << aTopWinPrincipal->GetURI(getter_AddRefs(uri));

  LOG_SPEC(("Deciding whether the user has overridden content blocking for %s",
            _spec),
           uri);

  nsPermissionManager* permManager = nsPermissionManager::GetInstance();
  NS_ENSURE_TRUE(permManager, NS_ERROR_FAILURE);

  // Check both the normal mode and private browsing mode user override
  // permissions.
  Pair<const nsLiteralCString, bool> types[] = {
      {NS_LITERAL_CSTRING("trackingprotection"), false},
      {NS_LITERAL_CSTRING("trackingprotection-pb"), true}};

  for (size_t i = 0; i < ArrayLength(types); ++i) {
    if (aIsPrivateBrowsing != types[i].second()) {
      continue;
    }

    uint32_t permissions = nsIPermissionManager::UNKNOWN_ACTION;
    nsresult rv = permManager->TestPermissionFromPrincipal(
        aTopWinPrincipal, types[i].first(), &permissions);
    NS_ENSURE_SUCCESS(rv, rv);

    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
      aIsAllowListed = true;
      LOG(("Found user override type %s", types[i].first().get()));
      // Stop checking the next permisson type if we decided to override.
      break;
    }
  }

  if (!aIsAllowListed) {
    LOG(("No user override found"));
  }

  return NS_OK;
}

/* static */ void AntiTrackingCommon::ComputeContentBlockingAllowListPrincipal(
    nsIPrincipal* aDocumentPrincipal, nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  auto returnInputArgument =
      MakeScopeExit([&] { NS_IF_ADDREF(*aPrincipal = aDocumentPrincipal); });

  BasePrincipal* bp = BasePrincipal::Cast(aDocumentPrincipal);
  if (!bp || !bp->IsContentPrincipal()) {
    // If we have something other than a content principal, just return what we
    // have.  This includes the case where we were passed a nullptr.
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aDocumentPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Take the host/port portion so we can allowlist by site. Also ignore the
  // scheme, since users who put sites on the allowlist probably don't expect
  // allowlisting to depend on scheme.
  nsAutoCString escaped(NS_LITERAL_CSTRING("https://"));
  nsAutoCString temp;
  rv = uri->GetHostPort(temp);
  // view-source URIs will be handled by the next block.
  if (NS_FAILED(rv) && !uri->SchemeIs("view-source")) {
    // Normal for some loads, no need to print a warning
    return;
  }

  // GetHostPort returns an empty string (with a success error code) for file://
  // URIs.
  if (temp.IsEmpty()) {
    // In this case we want to make sure that our allow list principal would be
    // computed as null.
    returnInputArgument.release();
    *aPrincipal = nullptr;
    return;
  }
  escaped.Append(temp);

  rv = NS_NewURI(getter_AddRefs(uri), escaped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = BasePrincipal::CreateContentPrincipal(
      uri, aDocumentPrincipal->OriginAttributesRef());
  if (NS_WARN_IF(!principal)) {
    return;
  }

  returnInputArgument.release();
  principal.forget(aPrincipal);
}

/* static */ void
AntiTrackingCommon::RecomputeContentBlockingAllowListPrincipal(
    nsIURI* aURIBeingLoaded, const OriginAttributes& aAttrs,
    nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  auto returnInputArgument = MakeScopeExit([&] { *aPrincipal = nullptr; });

  // Take the host/port portion so we can allowlist by site. Also ignore the
  // scheme, since users who put sites on the allowlist probably don't expect
  // allowlisting to depend on scheme.
  nsAutoCString escaped(NS_LITERAL_CSTRING("https://"));
  nsAutoCString temp;
  nsresult rv = aURIBeingLoaded->GetHostPort(temp);
  // view-source URIs will be handled by the next block.
  if (NS_FAILED(rv) && !aURIBeingLoaded->SchemeIs("view-source")) {
    // Normal for some loads, no need to print a warning
    return;
  }

  // GetHostPort returns an empty string (with a success error code) for file://
  // URIs.
  if (temp.IsEmpty()) {
    return;
  }
  escaped.Append(temp);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), escaped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(uri, aAttrs);
  if (NS_WARN_IF(!principal)) {
    return;
  }

  returnInputArgument.release();
  principal.forget(aPrincipal);
}

/* static */
void AntiTrackingCommon::NotifyBlockingDecision(nsIChannel* aChannel,
                                                BlockingDecision aDecision,
                                                uint32_t aRejectedReason) {
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  MOZ_ASSERT(aDecision == BlockingDecision::eBlock ||
             aDecision == BlockingDecision::eAllow);

  if (!aChannel) {
    return;
  }

  // Can be called in EITHER the parent or child process.
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIParentChannel> parentChannel;
    NS_QueryNotificationCallbacks(aChannel, parentChannel);
    if (parentChannel) {
      // This channel is a parent-process proxy for a child process request.
      // Tell the child process channel to do this instead.
      if (aDecision == BlockingDecision::eBlock) {
        parentChannel->NotifyCookieBlocked(aRejectedReason);
      } else {
        parentChannel->NotifyCookieAllowed();
      }
    }
    return;
  }

  MOZ_ASSERT(XRE_IsContentProcess());

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (!thirdPartyUtil) {
    return;
  }

  nsCOMPtr<nsIURI> uriBeingLoaded = MaybeGetDocumentURIBeingLoaded(aChannel);
  nsCOMPtr<mozIDOMWindowProxy> win;
  nsresult rv = thirdPartyUtil->GetTopWindowForChannel(aChannel, uriBeingLoaded,
                                                       getter_AddRefs(win));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(win);
  if (!pwin) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  if (aDecision == BlockingDecision::eBlock) {
    pwin->NotifyContentBlockingEvent(aRejectedReason, aChannel, true, uri,
                                     aChannel);

    ReportBlockingToConsole(pwin, uri, aRejectedReason);
  }

  pwin->NotifyContentBlockingEvent(nsIWebProgressListener::STATE_COOKIES_LOADED,
                                   aChannel, false, uri, aChannel);
}

/* static */
void AntiTrackingCommon::NotifyBlockingDecision(nsPIDOMWindowInner* aWindow,
                                                BlockingDecision aDecision,
                                                uint32_t aRejectedReason) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  MOZ_ASSERT(aDecision == BlockingDecision::eBlock ||
             aDecision == BlockingDecision::eAllow);

  nsCOMPtr<nsPIDOMWindowOuter> pwin = GetTopWindow(aWindow);
  if (!pwin) {
    return;
  }

  nsPIDOMWindowInner* inner = pwin->GetCurrentInnerWindow();
  if (!inner) {
    return;
  }
  Document* pwinDoc = inner->GetExtantDoc();
  if (!pwinDoc) {
    return;
  }
  nsIChannel* channel = pwinDoc->GetChannel();
  if (!channel) {
    return;
  }

  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return;
  }
  nsIURI* uri = document->GetDocumentURI();
  nsIChannel* trackingChannel = document->GetChannel();

  if (aDecision == BlockingDecision::eBlock) {
    pwin->NotifyContentBlockingEvent(aRejectedReason, channel, true, uri,
                                     trackingChannel);

    ReportBlockingToConsole(pwin, uri, aRejectedReason);
  }

  pwin->NotifyContentBlockingEvent(nsIWebProgressListener::STATE_COOKIES_LOADED,
                                   channel, false, uri, trackingChannel);
}

/* static */
void AntiTrackingCommon::StoreUserInteractionFor(nsIPrincipal* aPrincipal) {
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIURI> uri;
    Unused << aPrincipal->GetURI(getter_AddRefs(uri));
    LOG_SPEC(("Saving the userInteraction for %s", _spec), uri);

    nsPermissionManager* permManager = nsPermissionManager::GetInstance();
    if (NS_WARN_IF(!permManager)) {
      LOG(("Permission manager is null, bailing out early"));
      return;
    }

    // Remember that this pref is stored in seconds!
    uint32_t expirationType = nsIPermissionManager::EXPIRE_TIME;
    uint32_t expirationTime =
        StaticPrefs::privacy_userInteraction_expiration() * 1000;
    int64_t when = (PR_Now() / PR_USEC_PER_MSEC) + expirationTime;

    uint32_t privateBrowsingId = 0;
    nsresult rv = aPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (!NS_WARN_IF(NS_FAILED(rv)) && privateBrowsingId > 0) {
      // If we are coming from a private window, make sure to store a
      // session-only permission which won't get persisted to disk.
      expirationType = nsIPermissionManager::EXPIRE_SESSION;
      when = 0;
    }

    rv = permManager->AddFromPrincipal(aPrincipal, USER_INTERACTION_PERM,
                                       nsIPermissionManager::ALLOW_ACTION,
                                       expirationType, when);
    Unused << NS_WARN_IF(NS_FAILED(rv));
    return;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  MOZ_ASSERT(cc);

  nsCOMPtr<nsIURI> uri;
  Unused << aPrincipal->GetURI(getter_AddRefs(uri));
  LOG_SPEC(("Asking the parent process to save the user-interaction for us: %s",
            _spec),
           uri);
  cc->SendStoreUserInteractionAsPermission(IPC::Principal(aPrincipal));
}

/* static */
bool AntiTrackingCommon::HasUserInteraction(nsIPrincipal* aPrincipal) {
  nsPermissionManager* permManager = nsPermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    return false;
  }

  uint32_t result = 0;
  nsresult rv = permManager->TestPermissionWithoutDefaultsFromPrincipal(
      aPrincipal, USER_INTERACTION_PERM, &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return result == nsIPermissionManager::ALLOW_ACTION;
}

// static
void AntiTrackingCommon::OnAntiTrackingSettingsChanged(
    const AntiTrackingCommon::AntiTrackingSettingsChangedCallback& aCallback) {
  static bool initialized = false;
  if (!initialized) {
    // It is possible that while we have some data in our cache, something
    // changes in our environment that causes the anti-tracking checks below to
    // change their response.  Therefore, we need to clear our cache when we
    // detect a related change.
    Preferences::RegisterPrefixCallback(
        SettingsChangeObserver::PrivacyPrefChanged, "browser.contentblocking.");
    Preferences::RegisterPrefixCallback(
        SettingsChangeObserver::PrivacyPrefChanged, "network.cookie.");
    Preferences::RegisterPrefixCallback(
        SettingsChangeObserver::PrivacyPrefChanged, "privacy.");

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      RefPtr<SettingsChangeObserver> observer = new SettingsChangeObserver();
      obs->AddObserver(observer, "perm-added", false);
      obs->AddObserver(observer, "perm-changed", false);
      obs->AddObserver(observer, "perm-cleared", false);
      obs->AddObserver(observer, "perm-deleted", false);
      obs->AddObserver(observer, "xpcom-shutdown", false);
    }

    gSettingsChangedCallbacks =
        MakeUnique<nsTArray<AntiTrackingSettingsChangedCallback>>();

    initialized = true;
  }

  gSettingsChangedCallbacks->AppendElement(aCallback);
}

/* static */
already_AddRefed<nsIURI> AntiTrackingCommon::MaybeGetDocumentURIBeingLoaded(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uriBeingLoaded;
  nsLoadFlags loadFlags = 0;
  nsresult rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
    // If the channel being loaded is a document channel, this call may be
    // coming from an OnStopRequest notification, which might mean that our
    // document may still be in the loading process, so we may need to pass in
    // the uriBeingLoaded argument explicitly.
    rv = aChannel->GetURI(getter_AddRefs(uriBeingLoaded));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  return uriBeingLoaded.forget();
}
