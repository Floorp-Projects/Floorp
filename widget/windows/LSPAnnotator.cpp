/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LSPs are evil little bits of code that are allowed to inject into our
 * networking stack by Windows.  Once they have wormed into our process
 * they gnaw at our innards until we crash.  Here we force the buggers
 * into the light by recording them in our crash information.
 * We do the enumeration on a thread because I'm concerned about startup perf
 * on machines with several LSPs.
 */

#include "nsICrashReporter.h"
#include "nsISupportsImpl.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsQueryObject.h"
#include "nsWindowsHelpers.h"
#include <windows.h>
#include <rpc.h>
#include <ws2spi.h>

namespace mozilla {
namespace crashreporter {

class LSPAnnotationGatherer : public Runnable {
  ~LSPAnnotationGatherer() {}

 public:
  LSPAnnotationGatherer() : Runnable("crashreporter::LSPAnnotationGatherer") {}
  NS_DECL_NSIRUNNABLE

  void Annotate();
  nsCString mString;
};

void LSPAnnotationGatherer::Annotate() {
  nsCOMPtr<nsICrashReporter> cr =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  bool enabled;
  if (cr && NS_SUCCEEDED(cr->GetCrashReporterEnabled(&enabled)) && enabled) {
    cr->AnnotateCrashReport("Winsock_LSP"_ns, mString);
  }
}

NS_IMETHODIMP
LSPAnnotationGatherer::Run() {
  DWORD size = 0;
  int err;
  // Get the size of the buffer we need
  if (SOCKET_ERROR != WSCEnumProtocols(nullptr, nullptr, &size, &err) ||
      err != WSAENOBUFS) {
    // Er, what?
    MOZ_ASSERT_UNREACHABLE(
        "WSCEnumProtocols succeeded when it should have "
        "failed");
    return NS_ERROR_FAILURE;
  }

  auto byteArray = MakeUnique<char[]>(size);
  WSAPROTOCOL_INFOW* providers =
      reinterpret_cast<WSAPROTOCOL_INFOW*>(byteArray.get());

  int n = WSCEnumProtocols(nullptr, providers, &size, &err);
  if (n == SOCKET_ERROR) {
    // Lame. We provided the right size buffer; we'll just give up now.
    NS_WARNING("Could not get LSP list");
    return NS_ERROR_FAILURE;
  }

  nsCString str;
  for (int i = 0; i < n; i++) {
    AppendUTF16toUTF8(nsDependentString(providers[i].szProtocol), str);
    str.AppendLiteral(" : ");
    str.AppendInt(providers[i].iVersion);
    str.AppendLiteral(" : ");
    str.AppendInt(providers[i].iAddressFamily);
    str.AppendLiteral(" : ");
    str.AppendInt(providers[i].iSocketType);
    str.AppendLiteral(" : ");
    str.AppendInt(providers[i].iProtocol);
    str.AppendLiteral(" : ");
    str.AppendPrintf("0x%lx", providers[i].dwServiceFlags1);
    str.AppendLiteral(" : ");
    str.AppendPrintf("0x%lx", providers[i].dwProviderFlags);
    str.AppendLiteral(" : ");

    wchar_t path[MAX_PATH];
    int pathLen = MAX_PATH;
    if (!WSCGetProviderPath(&providers[i].ProviderId, path, &pathLen, &err)) {
      AppendUTF16toUTF8(nsDependentString(path), str);
    }

    str.AppendLiteral(" : ");
    // Call WSCGetProviderInfo to obtain the category flags for this provider.
    // When present, these flags inform Windows as to which order to chain the
    // providers.
    DWORD categoryInfo;
    size_t categoryInfoSize = sizeof(categoryInfo);
    if (!WSCGetProviderInfo(&providers[i].ProviderId, ProviderInfoLspCategories,
                            (PBYTE)&categoryInfo, &categoryInfoSize, 0, &err)) {
      str.AppendPrintf("0x%lx", categoryInfo);
    }

    str.AppendLiteral(" : ");
    if (providers[i].ProtocolChain.ChainLen <= BASE_PROTOCOL) {
      // If we're dealing with a catalog entry that identifies an individual
      // base or layer provider, log its provider GUID.
      RPC_CSTR provIdStr = nullptr;
      if (UuidToStringA(&providers[i].ProviderId, &provIdStr) == RPC_S_OK) {
        str.Append(reinterpret_cast<char*>(provIdStr));
        RpcStringFreeA(&provIdStr);
      }
    }

    if (i + 1 != n) {
      str.AppendLiteral(" \n ");
    }
  }

  mString = str;
  NS_DispatchToMainThread(
      NewRunnableMethod("crashreporter::LSPAnnotationGatherer::Annotate", this,
                        &LSPAnnotationGatherer::Annotate));
  return NS_OK;
}

void LSPAnnotate() {
  nsCOMPtr<nsIRunnable> runnable(new LSPAnnotationGatherer());
  NS_DispatchBackgroundTask(runnable.forget());
}

}  // namespace crashreporter
}  // namespace mozilla
