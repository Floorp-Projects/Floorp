/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "RestoreTabContentObserver.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(RestoreTabContentObserver, nsIObserver)

const char* const kAboutReaderTopic = "AboutReader:Ready";
const char* const kContentDocumentLoaded = "content-document-loaded";
const char* const kChromeDocumentLoaded = "chrome-document-loaded";

/* static */
void RestoreTabContentObserver::Initialize() {
  MOZ_ASSERT(!gRestoreTabContentObserver);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<RestoreTabContentObserver> observer = new RestoreTabContentObserver();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->AddObserver(observer, kAboutReaderTopic, false);
  obs->AddObserver(observer, kContentDocumentLoaded, false);
  obs->AddObserver(observer, kChromeDocumentLoaded, false);

  gRestoreTabContentObserver = observer;
}

/* static */
void RestoreTabContentObserver::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gRestoreTabContentObserver) {
    return;
  }

  RefPtr<RestoreTabContentObserver> observer = gRestoreTabContentObserver;
  gRestoreTabContentObserver = nullptr;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->RemoveObserver(observer, kAboutReaderTopic);
  obs->RemoveObserver(observer, kContentDocumentLoaded);
  obs->RemoveObserver(observer, kChromeDocumentLoaded);
}

NS_IMETHODIMP
RestoreTabContentObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                   const char16_t* aData) {
  nsCOMPtr<nsPIDOMWindowInner> inner;
  if (!strcmp(aTopic, kAboutReaderTopic)) {
    inner = do_QueryInterface(aSubject);
  } else if (!strcmp(aTopic, kContentDocumentLoaded) ||
             !strcmp(aTopic, kChromeDocumentLoaded)) {
    nsCOMPtr<Document> doc = do_QueryInterface(aSubject);
    inner = doc ? doc->GetInnerWindow() : nullptr;
  }
  if (!inner) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri = inner->GetDocumentURI();
  if (!uri) {
    return NS_OK;
  }

  // We'll handle loading about:reader with "AboutReader:Ready"
  // rather than "content-document-loaded".
  if (uri->SchemeIs("about") &&
      StringBeginsWith(uri->GetSpecOrDefault(), "about:reader"_ns) &&
      strcmp(aTopic, kAboutReaderTopic) != 0) {
    return NS_OK;
  }

  RefPtr<BrowsingContext> bc = inner->GetBrowsingContext();
  if (!bc || !bc->Top()->GetHasRestoreData()) {
    return NS_OK;
  }
  if (XRE_IsParentProcess()) {
    if (WindowGlobalParent* wgp = bc->Canonical()->GetCurrentWindowGlobal()) {
      bc->Canonical()->Top()->RequestRestoreTabContent(wgp);
    }
  } else if (WindowContext* windowContext = bc->GetCurrentWindowContext()) {
    if (WindowGlobalChild* wgc = windowContext->GetWindowGlobalChild()) {
      wgc->SendRequestRestoreTabContent();
    }
  }
  return NS_OK;
}

mozilla::StaticRefPtr<RestoreTabContentObserver>
    RestoreTabContentObserver::gRestoreTabContentObserver;
