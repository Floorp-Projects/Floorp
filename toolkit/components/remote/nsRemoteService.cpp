/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#  include "nsGTKRemoteServer.h"
#  ifdef MOZ_ENABLE_DBUS
#    include "nsDBusRemoteServer.h"
#  endif
#endif
#include "nsRemoteService.h"

#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsRemoteService, nsIObserver)

void nsRemoteService::Startup(const char* aAppName, const char* aProfileName) {
  if (mRemoteServer) {
    return;
  }

#ifdef MOZ_WIDGET_GTK
  bool useX11Remote = GDK_IS_X11_DISPLAY(gdk_display_get_default());

#  if defined(MOZ_ENABLE_DBUS)
  if (!useX11Remote) {
    mRemoteServer = MakeUnique<nsDBusRemoteServer>();
  }
#  endif
  if (useX11Remote) {
    mRemoteServer = MakeUnique<nsGTKRemoteServer>();
  }
#endif

  nsresult rv = mRemoteServer->Startup(aAppName, aProfileName);

  if (NS_FAILED(rv)) {
    mRemoteServer = nullptr;
    return;
  }

  nsCOMPtr<nsIObserverService> obs(
      do_GetService("@mozilla.org/observer-service;1"));
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "quit-application", false);
  }
}

void nsRemoteService::Shutdown() { mRemoteServer = nullptr; }

nsRemoteService::~nsRemoteService() { Shutdown(); }

NS_IMETHODIMP
nsRemoteService::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  // This can be xpcom-shutdown or quit-application, but it's the same either
  // way.
  Shutdown();
  return NS_OK;
}
