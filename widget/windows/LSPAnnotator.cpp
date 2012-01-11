/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Kyle Huey <me@kylehuey.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include <windows.h>
#include <Ws2spi.h>

namespace mozilla {
namespace crashreporter {

class LSPAnnotationGatherer : public nsRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  void Annotate();
  nsCString mString;
  nsCOMPtr<nsIThread> mThread;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(LSPAnnotationGatherer, nsIRunnable)

void
LSPAnnotationGatherer::Annotate()
{
  nsCOMPtr<nsICrashReporter> cr =
    do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  bool enabled;
  if (cr && NS_SUCCEEDED(cr->GetEnabled(&enabled)) && enabled) {
    cr->AnnotateCrashReport(NS_LITERAL_CSTRING("Winsock_LSP"), mString);
  }
  mThread->Shutdown();
}

NS_IMETHODIMP
LSPAnnotationGatherer::Run()
{
  mThread = NS_GetCurrentThread();

  DWORD size = 0;
  int err;
  // Get the size of the buffer we need
  if (SOCKET_ERROR != WSCEnumProtocols(NULL, NULL, &size, &err) ||
      err != WSAENOBUFS) {
    // Er, what?
    NS_NOTREACHED("WSCEnumProtocols suceeded when it should have failed ...");
    return NS_ERROR_FAILURE;
  }

  nsAutoArrayPtr<char> byteArray(new char[size]);
  WSAPROTOCOL_INFOW* providers =
    reinterpret_cast<WSAPROTOCOL_INFOW*>(byteArray.get());

  int n = WSCEnumProtocols(NULL, providers, &size, &err);
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
    str.AppendInt(providers[i].iSocketType);
    str.AppendLiteral(" : ");

    wchar_t path[MAX_PATH];
    int dummy;
    if (!WSCGetProviderPath(&providers[i].ProviderId, path,
                            &dummy, &err)) {
      AppendUTF16toUTF8(nsDependentString(path), str);
    }

    if (i + 1 != n) {
      str.AppendLiteral(" \n ");
    }
  }

  mString = str;
  NS_DispatchToMainThread(NS_NewRunnableMethod(this, &LSPAnnotationGatherer::Annotate));
  return NS_OK;
}

void LSPAnnotate()
{
  nsCOMPtr<nsIThread> thread;
  nsCOMPtr<nsIRunnable> runnable =
    do_QueryObject(new LSPAnnotationGatherer());
  NS_NewThread(getter_AddRefs(thread), runnable);
}

} // namespace crashreporter
} // namespace mozilla
