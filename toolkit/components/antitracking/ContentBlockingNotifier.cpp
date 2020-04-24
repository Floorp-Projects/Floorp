/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "ContentBlockingNotifier.h"
#include "AntiTrackingUtils.h"

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "nsIClassifiedChannel.h"
#include "nsIRunnable.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIOService.h"
#include "nsGlobalWindowInner.h"
#include "nsJSUtils.h"
#include "mozIThirdPartyUtil.h"

using namespace mozilla;
using mozilla::dom::BrowsingContext;
using mozilla::dom::ContentChild;
using mozilla::dom::Document;

static const uint32_t kMaxConsoleOutputDelayMs = 100;

namespace {

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

void ReportUnblockingToConsole(
    uint64_t aWindowID, nsIPrincipal* aPrincipal,
    const nsAString& aTrackingOrigin,
    ContentBlockingNotifier::StorageAccessGrantedReason aReason) {
  MOZ_ASSERT(aWindowID);
  MOZ_ASSERT(aPrincipal);

  nsAutoString sourceLine;
  uint32_t lineNumber = 0, columnNumber = 0;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    nsJSUtils::GetCallingLocation(cx, sourceLine, &lineNumber, &columnNumber);
  }

  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  nsAutoString trackingOrigin(aTrackingOrigin);

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "ReportUnblockingToConsoleDelayed",
      [aWindowID, sourceLine, lineNumber, columnNumber, principal,
       trackingOrigin, aReason]() {
        const char* messageWithSameOrigin = nullptr;

        switch (aReason) {
          case ContentBlockingNotifier::eStorageAccessAPI:
            messageWithSameOrigin = "CookieAllowedForTrackerByStorageAccessAPI";
            break;

          case ContentBlockingNotifier::eOpenerAfterUserInteraction:
            [[fallthrough]];
          case ContentBlockingNotifier::eOpener:
            messageWithSameOrigin = "CookieAllowedForTrackerByHeuristic";
            break;
        }

        nsAutoString origin;
        nsresult rv = nsContentUtils::GetUTFOrigin(principal, origin);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        // Not adding grantedOrigin yet because we may not want it later.
        AutoTArray<nsString, 2> params = {origin, trackingOrigin};

        nsAutoString errorText;
        rv = nsContentUtils::FormatLocalizedString(
            nsContentUtils::eNECKO_PROPERTIES, messageWithSameOrigin, params,
            errorText);
        NS_ENSURE_SUCCESS_VOID(rv);

        nsContentUtils::ReportToConsoleByWindowID(
            errorText, nsIScriptError::warningFlag,
            ANTITRACKING_CONSOLE_CATEGORY, aWindowID, nullptr, sourceLine,
            lineNumber, columnNumber);
      });

  RunConsoleReportingRunnable(runnable.forget());
}

void ReportBlockingToConsole(uint64_t aWindowID, nsIURI* aURI,
                             uint32_t aRejectedReason) {
  MOZ_ASSERT(aWindowID);
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);

  nsAutoString sourceLine;
  uint32_t lineNumber = 0, columnNumber = 0;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (cx) {
    nsJSUtils::GetCallingLocation(cx, sourceLine, &lineNumber, &columnNumber);
  }

  nsCOMPtr<nsIURI> uri(aURI);

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "ReportBlockingToConsoleDelayed", [aWindowID, sourceLine, lineNumber,
                                         columnNumber, uri, aRejectedReason]() {
        const char* message = nullptr;
        nsAutoCString category;
        // When changing this list, please make sure to update the corresponding
        // code in antitracking_head.js (inside _createTask).
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
        nsCOMPtr<nsIURI> exposableURI =
            net::nsIOService::CreateExposableURI(uri);
        AutoTArray<nsString, 1> params;
        CopyUTF8toUTF16(exposableURI->GetSpecOrDefault(),
                        *params.AppendElement());

        nsAutoString errorText;
        nsresult rv = nsContentUtils::FormatLocalizedString(
            nsContentUtils::eNECKO_PROPERTIES, message, params, errorText);
        NS_ENSURE_SUCCESS_VOID(rv);

        nsContentUtils::ReportToConsoleByWindowID(
            errorText, nsIScriptError::warningFlag, category, aWindowID,
            nullptr, sourceLine, lineNumber, columnNumber);
      });

  RunConsoleReportingRunnable(runnable.forget());
}

void ReportBlockingToConsole(nsIChannel* aChannel, nsIURI* aURI,
                             uint32_t aRejectedReason) {
  MOZ_ASSERT(aChannel && aURI);

  uint64_t windowID;

  if (XRE_IsParentProcess()) {
    // Get the top-level window ID from the top-level BrowsingContext
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    RefPtr<dom::BrowsingContext> bc;
    loadInfo->GetBrowsingContext(getter_AddRefs(bc));

    if (!bc || bc->IsDiscarded()) {
      return;
    }

    bc = bc->Top();
    RefPtr<dom::WindowGlobalParent> wgp =
        bc->Canonical()->GetCurrentWindowGlobal();
    if (!wgp) {
      return;
    }

    windowID = wgp->InnerWindowId();
  } else {
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel, &rv);

    if (!httpChannel) {
      return;
    }

    rv = httpChannel->GetTopLevelContentWindowId(&windowID);
    if (NS_FAILED(rv) || !windowID) {
      windowID = nsContentUtils::GetInnerWindowID(httpChannel);
    }
  }

  ReportBlockingToConsole(windowID, aURI, aRejectedReason);
}

void NotifyBlockingDecision(nsIChannel* aTrackingChannel,
                            ContentBlockingNotifier::BlockingDecision aDecision,
                            uint32_t aRejectedReason, nsIURI* aURI) {
  MOZ_ASSERT(aTrackingChannel);

  // This can be called in either the parent process or the child processes.
  // When this is called in the child processes, we must have a window.
  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(aTrackingChannel, loadContext);
    if (!loadContext) {
      return;
    }

    nsCOMPtr<mozIDOMWindowProxy> window;
    loadContext->GetAssociatedWindow(getter_AddRefs(window));
    if (!window) {
      return;
    }

    nsCOMPtr<nsPIDOMWindowOuter> outer = nsPIDOMWindowOuter::From(window);
    if (!outer) {
      return;
    }

    // When this is called in the child processes with system privileges,
    // the decision should always be ALLOW. We can stop here because both
    // UI and content blocking log don't care this event.
    if (nsGlobalWindowOuter::Cast(outer)->GetPrincipal() ==
        nsContentUtils::GetSystemPrincipal()) {
      MOZ_DIAGNOSTIC_ASSERT(aDecision ==
                            ContentBlockingNotifier::BlockingDecision::eAllow);
      return;
    }
  }

  nsAutoCString trackingOrigin;
  if (aURI) {
    Unused << nsContentUtils::GetASCIIOrigin(aURI, trackingOrigin);
  }

  if (aDecision == ContentBlockingNotifier::BlockingDecision::eBlock) {
    ContentBlockingNotifier::OnEvent(aTrackingChannel, true, aRejectedReason,
                                     trackingOrigin);

    ReportBlockingToConsole(aTrackingChannel, aURI, aRejectedReason);
  }

  // Now send the generic "cookies loaded" notifications, from the most generic
  // to the most specific.
  ContentBlockingNotifier::OnEvent(aTrackingChannel, false,
                                   nsIWebProgressListener::STATE_COOKIES_LOADED,
                                   trackingOrigin);

  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aTrackingChannel);
  if (!classifiedChannel) {
    return;
  }

  uint32_t classificationFlags =
      classifiedChannel->GetThirdPartyClassificationFlags();
  if (classificationFlags &
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_TRACKING) {
    ContentBlockingNotifier::OnEvent(
        aTrackingChannel, false,
        nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER, trackingOrigin);
  }

  if (classificationFlags &
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_SOCIALTRACKING) {
    ContentBlockingNotifier::OnEvent(
        aTrackingChannel, false,
        nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER,
        trackingOrigin);
  }
}

// Send a message to notify OnContentBlockingEvent in the parent, which will
// update the ContentBlockingLog in the parent.
void NotifyEventInChild(
    nsIChannel* aTrackingChannel, bool aBlocked, uint32_t aRejectedReason,
    const nsACString& aTrackingOrigin,
    const Maybe<ContentBlockingNotifier::StorageAccessGrantedReason>& aReason) {
  MOZ_ASSERT(XRE_IsContentProcess());

  // We don't need to find the top-level window here because the
  // parent will do that for us.
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aTrackingChannel, loadContext);
  if (!loadContext) {
    return;
  }

  nsCOMPtr<mozIDOMWindowProxy> window;
  loadContext->GetAssociatedWindow(getter_AddRefs(window));
  if (!window) {
    return;
  }

  RefPtr<dom::BrowserChild> browserChild = dom::BrowserChild::GetFrom(window);
  NS_ENSURE_TRUE_VOID(browserChild);

  nsTArray<nsCString> trackingFullHashes;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aTrackingChannel);

  if (classifiedChannel) {
    Unused << classifiedChannel->GetMatchedTrackingFullHashes(
        trackingFullHashes);
  }

  browserChild->NotifyContentBlockingEvent(aRejectedReason, aTrackingChannel,
                                           aBlocked, aTrackingOrigin,
                                           trackingFullHashes, aReason);
}

// Update the ContentBlockingLog of the top-level WindowGlobalParent of
// the tracking channel.
void NotifyEventInParent(
    nsIChannel* aTrackingChannel, bool aBlocked, uint32_t aRejectedReason,
    const nsACString& aTrackingOrigin,
    const Maybe<ContentBlockingNotifier::StorageAccessGrantedReason>& aReason) {
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsILoadInfo> loadInfo = aTrackingChannel->LoadInfo();
  RefPtr<dom::BrowsingContext> bc;
  loadInfo->GetBrowsingContext(getter_AddRefs(bc));

  if (!bc || bc->IsDiscarded()) {
    return;
  }

  bc = bc->Top();
  RefPtr<dom::WindowGlobalParent> wgp =
      bc->Canonical()->GetCurrentWindowGlobal();
  NS_ENSURE_TRUE_VOID(wgp);

  nsTArray<nsCString> trackingFullHashes;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aTrackingChannel);

  if (classifiedChannel) {
    Unused << classifiedChannel->GetMatchedTrackingFullHashes(
        trackingFullHashes);
  }

  wgp->NotifyContentBlockingEvent(aRejectedReason, aTrackingChannel, aBlocked,
                                  aTrackingOrigin, trackingFullHashes, aReason);
}

}  // namespace

/* static */
void ContentBlockingNotifier::ReportUnblockingToConsole(
    BrowsingContext* aBrowsingContext, const nsAString& aTrackingOrigin,
    ContentBlockingNotifier::StorageAccessGrantedReason aReason) {
  MOZ_ASSERT(aBrowsingContext);

  uint64_t windowID = aBrowsingContext->GetCurrentInnerWindowId();

  nsCOMPtr<nsIPrincipal> principal =
      AntiTrackingUtils::GetPrincipal(aBrowsingContext);
  if (NS_WARN_IF(!principal)) {
    return;
  }

  ::ReportUnblockingToConsole(windowID, principal, aTrackingOrigin, aReason);
}

/* static */
void ContentBlockingNotifier::OnDecision(nsIChannel* aChannel,
                                         BlockingDecision aDecision,
                                         uint32_t aRejectedReason) {
  MOZ_ASSERT(
      aRejectedReason == 0 ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  MOZ_ASSERT(aDecision == BlockingDecision::eBlock ||
             aDecision == BlockingDecision::eAllow);

  if (!aChannel) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  // Can be called in EITHER the parent or child process.
  NotifyBlockingDecision(aChannel, aDecision, aRejectedReason, uri);
}

/* static */
void ContentBlockingNotifier::OnDecision(nsPIDOMWindowInner* aWindow,
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
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL ||
      aRejectedReason == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN);
  MOZ_ASSERT(aDecision == BlockingDecision::eBlock ||
             aDecision == BlockingDecision::eAllow);

  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return;
  }

  nsIChannel* channel = document->GetChannel();
  if (!channel) {
    return;
  }

  nsIURI* uri = document->GetDocumentURI();

  NotifyBlockingDecision(channel, aDecision, aRejectedReason, uri);
}

/* static */
void ContentBlockingNotifier::OnDecision(BrowsingContext* aBrowsingContext,
                                         BlockingDecision aDecision,
                                         uint32_t aRejectedReason) {
  MOZ_ASSERT(aBrowsingContext);
  MOZ_ASSERT_IF(XRE_IsContentProcess(), aBrowsingContext->IsInProcess());

  if (aBrowsingContext->IsInProcess()) {
    nsCOMPtr<nsPIDOMWindowOuter> outer = aBrowsingContext->GetDOMWindow();
    if (NS_WARN_IF(!outer)) {
      return;
    }

    nsCOMPtr<nsPIDOMWindowInner> inner = outer->GetCurrentInnerWindow();
    if (NS_WARN_IF(!inner)) {
      return;
    }

    ContentBlockingNotifier::OnDecision(inner, aDecision, aRejectedReason);
  } else {
    // we send an IPC to the content process when we don't have an in-process
    // browsing context. This is not smart because this should be able to be
    // done directly in the parent. The reason we are doing this is because we
    // need the channel, which is not accessible in the parent when you only
    // have a browsing context.
    MOZ_ASSERT(XRE_IsParentProcess());

    ContentParent* cp = aBrowsingContext->Canonical()->GetContentParent();
    Unused << cp->SendOnContentBlockingDecision(aBrowsingContext, aDecision,
                                                aRejectedReason);
  }
}

/* static */
void ContentBlockingNotifier::OnEvent(nsIChannel* aTrackingChannel,
                                      uint32_t aRejectedReason) {
  MOZ_ASSERT(XRE_IsParentProcess() && aTrackingChannel);

  nsCOMPtr<nsIURI> uri;
  aTrackingChannel->GetURI(getter_AddRefs(uri));

  nsAutoCString trackingOrigin;
  if (uri) {
    Unused << nsContentUtils::GetASCIIOrigin(uri, trackingOrigin);
  }

  return ContentBlockingNotifier::OnEvent(aTrackingChannel, true,
                                          aRejectedReason, trackingOrigin);
}

/* static */
void ContentBlockingNotifier::OnEvent(
    nsIChannel* aTrackingChannel, bool aBlocked, uint32_t aRejectedReason,
    const nsACString& aTrackingOrigin,
    const Maybe<StorageAccessGrantedReason>& aReason) {
  if (XRE_IsParentProcess()) {
    NotifyEventInParent(aTrackingChannel, aBlocked, aRejectedReason,
                        aTrackingOrigin, aReason);
  } else {
    NotifyEventInChild(aTrackingChannel, aBlocked, aRejectedReason,
                       aTrackingOrigin, aReason);
  }
}
