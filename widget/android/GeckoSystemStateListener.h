/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoSystemStateListener_h
#define GeckoSystemStateListener_h

#include "GeneratedJNINatives.h"
#include "mozilla/Assertions.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"

namespace mozilla {

class GeckoSystemStateListener final
    : public java::GeckoSystemStateListener::Natives<GeckoSystemStateListener> {
  GeckoSystemStateListener() = delete;

 public:
  static void OnDeviceChanged() {
    MOZ_ASSERT(NS_IsMainThread());

    // Iterate all toplevel windows
    nsCOMPtr<nsIWindowMediator> windowMediator =
        do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
    NS_ENSURE_TRUE_VOID(windowMediator);

    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
    windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
    NS_ENSURE_TRUE_VOID(windowEnumerator);

    bool more;
    while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsISupports> elements;
      if (NS_FAILED(windowEnumerator->GetNext(getter_AddRefs(elements)))) {
        break;
      }

      if (nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(elements)) {
        if (window->Closed()) {
          continue;
        }
        if (dom::Document* doc = window->GetExtantDoc()) {
          if (PresShell* presShell = doc->GetPresShell()) {
            presShell->ThemeChanged();
          }
        }
      }
    }
  }
};

}  // namespace mozilla

#endif  // GeckoSystemStateListener_h
