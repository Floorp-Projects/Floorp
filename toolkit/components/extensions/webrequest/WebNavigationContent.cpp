/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/WebNavigationContent.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace extensions {

WebNavigationContent::WebNavigationContent() {}

/* static */
already_AddRefed<WebNavigationContent> WebNavigationContent::GetSingleton() {
  static RefPtr<WebNavigationContent> sSingleton;
  if (!sSingleton) {
    sSingleton = new WebNavigationContent();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }
  return do_AddRef(sSingleton);
}

NS_IMPL_ISUPPORTS(WebNavigationContent, nsIObserver)

void WebNavigationContent::Init() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  obs->AddObserver(this, "chrome-event-target-created", false);
  obs->AddObserver(this, "webNavigation-createdNavigationTarget-from-js",
                   false);
}

NS_IMETHODIMP WebNavigationContent::Observe(nsISupports* aSubject,
                                            char const* aTopic,
                                            char16_t const* aData) {
  if (!nsCRT::strcmp(aTopic, "chrome-event-target-created")) {
    // This notification is sent whenever a new window root is created, with the
    // subject being an EventTarget corresponding to either an nsWindowRoot in a
    // child process or an InProcessBrowserChildMessageManager in the parent.
    // This is the same entry point used to register listeners for the JS window
    // actor API.
    if (RefPtr<dom::EventTarget> eventTarget = do_QueryObject(aSubject)) {
      AttachListeners(eventTarget);
    }
  } else if (!nsCRT::strcmp(aTopic,
                            "webNavigation-createdNavigationTarget-from-js")) {
  }
  return NS_OK;
}

void WebNavigationContent::AttachListeners(
    mozilla::dom::EventTarget* aEventTarget) {}

NS_IMETHODIMP
WebNavigationContent::HandleEvent(dom::Event* aEvent) { return NS_OK; }

}  // namespace extensions
}  // namespace mozilla
