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

class LSPAnnotationGatherer : public Runnable
{
  ~LSPAnnotationGatherer() {}

public:
  NS_DECL_NSIRUNNABLE

  void Annotate();
  nsCString mString;
  nsCOMPtr<nsIThread> mThread;
};

void
LSPAnnotationGatherer::Annotate()
{
  nsCOMPtr<nsICrashReporter> cr =
    do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  bool enabled;
  if (cr && NS_SUCCEEDED(cr->GetEnabled(&enabled)) && enabled) {
    cr->AnnotateCrashReport(NS_LITERAL_CSTRING("Winsock_LSP"), mString);
  }
  mThread->AsyncShutdown();
}

NS_IMETHODIMP
LSPAnnotationGatherer::Run()
{
  NS_SetCurrentThreadName("LSP Annotator");

  mThread = NS_GetCurrentThread();

  DWORD size = 0;
  int err;
  // Get the size of the buffer we need
  if (SOCKET_ERROR != WSCEnumProtocols(nullptr, nullptr, &size, &err) ||
      err != WSAENOBUFS) {
    // Er, what?
    NS_NOTREACHED("WSCEnumProtocols suceeded when it should have failed ...");
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
    str.AppendPrintf("0x%x", providers[i].dwServiceFlags1);
    str.AppendLiteral(" : ");
    str.AppendPrintf("0x%x", providers[i].dwProviderFlags);
    str.AppendLiteral(" : ");

    wchar_t path[MAX_PATH];
    int pathLen = MAX_PATH;
    if (!WSCGetProviderPath(&providers[i].ProviderId, path, &pathLen, &err)) {
      AppendUTF16toUTF8(nsDependentString(path), str);
    }

    str.AppendLiteral(" : ");
    // If WSCGetProviderInfo is available, we should call it to obtain the
    // category flags for this provider. When present, these flags inform
    // Windows as to which order to chain the providers.
    nsModuleHandle ws2_32(LoadLibraryW(L"ws2_32.dll"));
    if (ws2_32) {
      decltype(WSCGetProviderInfo)* pWSCGetProviderInfo =
        reinterpret_cast<decltype(WSCGetProviderInfo)*>(
            GetProcAddress(ws2_32, "WSCGetProviderInfo"));
      if (pWSCGetProviderInfo) {
        DWORD categoryInfo;
        size_t categoryInfoSize = sizeof(categoryInfo);
        if (!pWSCGetProviderInfo(&providers[i].ProviderId,
                                 ProviderInfoLspCategories,
                                 (PBYTE)&categoryInfo, &categoryInfoSize, 0,
                                 &err)) {
          str.AppendPrintf("0x%x", categoryInfo);
        }
      }
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
  NS_DispatchToMainThread(NewRunnableMethod(this, &LSPAnnotationGatherer::Annotate));
  return NS_OK;
}

void LSPAnnotate()
{
  nsCOMPtr<nsIThread> thread;
  nsCOMPtr<nsIRunnable> runnable =
    do_QueryObject(new LSPAnnotationGatherer());
  NS_NewNamedThread("LSP Annotate", getter_AddRefs(thread), runnable);
}

} // namespace crashreporter
} // namespace mozilla
