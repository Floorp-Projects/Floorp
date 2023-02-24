/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/ExtensionsChild.h"
#include "mozilla/extensions/ExtensionsParent.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/InProcessParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsXULAppAPI.h"

using namespace mozilla::dom;

namespace mozilla {
namespace extensions {

NS_IMPL_ISUPPORTS(ExtensionsChild, nsIObserver)

/* static */
ExtensionsChild& ExtensionsChild::Get() {
  static RefPtr<ExtensionsChild> sInstance;

  if (MOZ_UNLIKELY(!sInstance)) {
    sInstance = new ExtensionsChild();
    sInstance->Init();
    ClearOnShutdown(&sInstance);
  }
  return *sInstance;
}

/* static */
already_AddRefed<ExtensionsChild> ExtensionsChild::GetSingleton() {
  return do_AddRef(&Get());
}

void ExtensionsChild::Init() {
  if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->SendPExtensionsConstructor(this);
  } else {
    InProcessChild* ipChild = InProcessChild::Singleton();
    InProcessParent* ipParent = InProcessParent::Singleton();
    if (!ipChild || !ipParent) {
      return;
    }

    RefPtr parent = new ExtensionsParent();

    ManagedEndpoint<PExtensionsParent> endpoint =
        ipChild->OpenPExtensionsEndpoint(this);
    ipParent->BindPExtensionsEndpoint(std::move(endpoint), parent);
  }
}

void ExtensionsChild::ActorDestroy(ActorDestroyReason aWhy) {}

/* nsIObserver */

NS_IMETHODIMP ExtensionsChild::Observe(nsISupports*, const char* aTopic,
                                       const char16_t*) {
  // Since this class is created at startup by the Category Manager, it's
  // expected to implement nsIObserver; however, we have nothing interesting
  // to do here.
  MOZ_ASSERT(strcmp(aTopic, "app-startup") == 0);

  return NS_OK;
}

}  // namespace extensions
}  // namespace mozilla
