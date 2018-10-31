/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGTKRemoteService.h"
#ifdef MOZ_ENABLE_DBUS
#include "nsDBusRemoteService.h"
#endif
#include "nsRemoteService.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "nsInterfaceHashtable.h"
#include "mozilla/ModuleUtils.h"
#include "nsGTKToolkit.h"
#include "nsICommandLineRunner.h"
#include "nsCommandLine.h"
#include "nsString.h"
#include "nsIFile.h"

NS_IMPL_ISUPPORTS(nsRemoteService,
                  nsIRemoteService,
                  nsIObserver)

NS_IMETHODIMP
nsRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
#if defined(MOZ_ENABLE_DBUS) && defined(MOZ_WAYLAND)
    nsresult rv;
    mDBusRemoteService = new nsDBusRemoteService();
    rv = mDBusRemoteService->Startup(aAppName, aProfileName);
    if (NS_FAILED(rv)) {
        mDBusRemoteService = nullptr;
    }
#elif !defined(MOZ_WAYLAND)
    mGtkRemoteService = new nsGTKRemoteService();
    mGtkRemoteService->Startup(aAppName, aProfileName);
#endif

    if (!mDBusRemoteService && !mGtkRemoteService)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1"));
    if (obs) {
        obs->AddObserver(this, "xpcom-shutdown", false);
        obs->AddObserver(this, "quit-application", false);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsRemoteService::RegisterWindow(mozIDOMWindow* aWindow)
{
    // Note: RegisterWindow() is not implemented/needed by DBus service.
    if (mGtkRemoteService) {
        mGtkRemoteService->RegisterWindow(aWindow);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsRemoteService::Shutdown()
{
#if defined(MOZ_ENABLE_DBUS) && defined(MOZ_WAYLAND)
    if (mDBusRemoteService) {
        mDBusRemoteService->Shutdown();
        mDBusRemoteService = nullptr;
    }
#endif
    if (mGtkRemoteService) {
        mGtkRemoteService->Shutdown();
        mGtkRemoteService = nullptr;
    }
    return NS_OK;
}

nsRemoteService::~nsRemoteService()
{
    Shutdown();
}

NS_IMETHODIMP
nsRemoteService::Observe(nsISupports* aSubject,
                          const char *aTopic,
                          const char16_t *aData)
{
    // This can be xpcom-shutdown or quit-application, but it's the same either
    // way.
    Shutdown();
    return NS_OK;
}

// Set desktop startup ID to the passed ID, if there is one, so that any created
// windows get created with the right window manager metadata, and any windows
// that get new tabs and are activated also get the right WM metadata.
// The timestamp will be used if there is no desktop startup ID, or if we're
// raising an existing window rather than showing a new window for the first time.
void
nsRemoteService::SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                                uint32_t aTimestamp) {
  nsGTKToolkit* toolkit = nsGTKToolkit::GetToolkit();
  if (!toolkit)
    return;

  if (!aDesktopStartupID.IsEmpty()) {
    toolkit->SetDesktopStartupID(aDesktopStartupID);
  }

  toolkit->SetFocusTimestamp(aTimestamp);
}

static bool
FindExtensionParameterInCommand(const char* aParameterName,
                                const nsACString& aCommand,
                                char aSeparator,
                                nsACString* aValue)
{
  nsAutoCString searchFor;
  searchFor.Append(aSeparator);
  searchFor.Append(aParameterName);
  searchFor.Append('=');

  nsACString::const_iterator start, end;
  aCommand.BeginReading(start);
  aCommand.EndReading(end);
  if (!FindInReadable(searchFor, start, end))
    return false;

  nsACString::const_iterator charStart, charEnd;
  charStart = end;
  aCommand.EndReading(charEnd);
  nsACString::const_iterator idStart = charStart, idEnd;
  if (FindCharInReadable(aSeparator, charStart, charEnd)) {
    idEnd = charStart;
  } else {
    idEnd = charEnd;
  }
  *aValue = nsDependentCSubstring(idStart, idEnd);
  return true;
}

const char*
nsRemoteService::HandleCommandLine(const char* aBuffer, nsIDOMWindow* aWindow,
                                   uint32_t aTimestamp)
{
  nsCOMPtr<nsICommandLineRunner> cmdline(new nsCommandLine());

  // the commandline property is constructed as an array of int32_t
  // followed by a series of null-terminated strings:
  //
  // [argc][offsetargv0][offsetargv1...]<workingdir>\0<argv[0]>\0argv[1]...\0
  // (offset is from the beginning of the buffer)

  int32_t argc = TO_LITTLE_ENDIAN32(*reinterpret_cast<const int32_t*>(aBuffer));
  const char *wd   = aBuffer + ((argc + 1) * sizeof(int32_t));

  nsCOMPtr<nsIFile> lf;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(wd), true,
                                      getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return "509 internal error";

  nsAutoCString desktopStartupID;

  const char **argv = (const char**) malloc(sizeof(char*) * argc);
  if (!argv) return "509 internal error";

  const int32_t *offset = reinterpret_cast<const int32_t*>(aBuffer) + 1;

  for (int i = 0; i < argc; ++i) {
    argv[i] = aBuffer + TO_LITTLE_ENDIAN32(offset[i]);

    if (i == 0) {
      nsDependentCString cmd(argv[0]);
      FindExtensionParameterInCommand("DESKTOP_STARTUP_ID",
                                      cmd, ' ',
                                      &desktopStartupID);
    }
  }

  rv = cmdline->Init(argc, argv, lf, nsICommandLine::STATE_REMOTE_AUTO);

  free (argv);
  if (NS_FAILED(rv)) {
    return "509 internal error";
  }

  if (aWindow)
    cmdline->SetWindowContext(aWindow);

  SetDesktopStartupIDOrTimestamp(desktopStartupID, aTimestamp);

  rv = cmdline->Run();

  if (NS_ERROR_ABORT == rv)
    return "500 command not parseable";

  if (NS_FAILED(rv))
    return "509 internal error";

  return "200 executed command";
}

// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRemoteService)
NS_DEFINE_NAMED_CID(NS_REMOTESERVICE_CID);

static const mozilla::Module::CIDEntry kRemoteCIDs[] = {
  { &kNS_REMOTESERVICE_CID, false, nullptr, nsRemoteServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kRemoteContracts[] = {
  { "@mozilla.org/toolkit/remote-service;1", &kNS_REMOTESERVICE_CID },
  { nullptr }
};

static const mozilla::Module kRemoteModule = {
  mozilla::Module::kVersion,
  kRemoteCIDs,
  kRemoteContracts
};

NSMODULE_DEFN(RemoteServiceModule) = &kRemoteModule;
