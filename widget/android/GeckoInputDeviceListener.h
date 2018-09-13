/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoInputDeviceListener_h
#define GeckoInputDeviceListener_h

#include "GeneratedJNINatives.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Assertions.h"

namespace mozilla {

class GeckoInputDeviceListener final
    : public java::GeckoInputDeviceListener::Natives<GeckoInputDeviceListener>
{
    GeckoInputDeviceListener() = delete;

public:
    static void
    OnDeviceChanged()
    {
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
          if (nsIDocument* doc = window->GetExtantDoc()) {
            if (nsIPresShell* presShell = doc->GetShell()) {
              presShell->ThemeChanged();
            }
          }
        }
      }
    }
};

} // namespace mozilla

#endif // GeckoInputDeviceListener_h
