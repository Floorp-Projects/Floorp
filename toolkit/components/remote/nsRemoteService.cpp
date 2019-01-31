/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#  include "nsGTKRemoteServer.h"
#  include "nsXRemoteClient.h"
#  ifdef MOZ_ENABLE_DBUS
#    include "nsDBusRemoteServer.h"
#    include "nsDBusRemoteClient.h"
#  endif
#endif
#include "nsRemoteService.h"

#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"
#include "SpecialSystemDirectory.h"

// Time to wait for the remoting service to start
#define START_TIMEOUT_SEC 5
#define START_SLEEP_MSEC 100

using namespace mozilla;

extern int gArgc;
extern char** gArgv;

NS_IMPL_ISUPPORTS(nsRemoteService, nsIObserver)

void nsRemoteService::LockStartup(nsCString& aProgram, const char* aProfile) {
  nsCOMPtr<nsIFile> mutexDir;
  nsresult rv = GetSpecialSystemDirectory(OS_TemporaryDirectory,
                                          getter_AddRefs(mutexDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString mutexPath(aProgram);
    if (aProfile) {
      mutexPath.Append(NS_LITERAL_CSTRING("_") + nsDependentCString(aProfile));
    }
    mutexDir->AppendNative(mutexPath);

    rv = mutexDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      mRemoteLockDir = mutexDir;
    }
  }

  if (mRemoteLockDir) {
    const mozilla::TimeStamp epoch = mozilla::TimeStamp::Now();
    do {
      rv = mRemoteLock.Lock(mRemoteLockDir, nullptr);
      if (NS_SUCCEEDED(rv)) break;
      PR_Sleep(START_SLEEP_MSEC);
    } while ((mozilla::TimeStamp::Now() - epoch) <
             mozilla::TimeDuration::FromSeconds(START_TIMEOUT_SEC));
    if (NS_FAILED(rv)) {
      NS_WARNING("Cannot lock remote start mutex");
    }
  }
}

void nsRemoteService::UnlockStartup() {
  mRemoteLock.Unlock();
  mRemoteLock.Cleanup();

  if (mRemoteLockDir) {
    mRemoteLockDir->Remove(false);
    mRemoteLockDir = nullptr;
  }
}

RemoteResult nsRemoteService::StartClient(const char* aDesktopStartupID,
                                          nsCString& program,
                                          const char* profile) {
  nsAutoPtr<nsRemoteClient> client;

#ifdef MOZ_WIDGET_GTK
  bool useX11Remote = GDK_IS_X11_DISPLAY(gdk_display_get_default());

#  if defined(MOZ_ENABLE_DBUS)
  if (!useX11Remote) {
    client = new nsDBusRemoteClient();
  }
#  endif
  if (useX11Remote) {
    client = new nsXRemoteClient();
  }

  nsresult rv = client ? client->Init() : NS_ERROR_FAILURE;
  if (NS_FAILED(rv)) return REMOTE_NOT_FOUND;

  nsCString response;
  bool success = false;
  rv = client->SendCommandLine(program.get(), profile, gArgc, gArgv,
                               aDesktopStartupID, getter_Copies(response),
                               &success);
  // did the command fail?
  if (!success) return REMOTE_NOT_FOUND;

  // The "command not parseable" error is returned when the
  // nsICommandLineHandler throws a NS_ERROR_ABORT.
  if (response.EqualsLiteral("500 command not parseable"))
    return REMOTE_ARG_BAD;

  if (NS_FAILED(rv)) return REMOTE_NOT_FOUND;

  return REMOTE_FOUND;
#else
  return REMOTE_NOT_FOUND;
#endif
}

void nsRemoteService::StartupServer(const char* aAppName,
                                    const char* aProfileName) {
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

void nsRemoteService::ShutdownServer() { mRemoteServer = nullptr; }

nsRemoteService::~nsRemoteService() { ShutdownServer(); }

NS_IMETHODIMP
nsRemoteService::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  // This can be xpcom-shutdown or quit-application, but it's the same either
  // way.
  ShutdownServer();
  return NS_OK;
}
