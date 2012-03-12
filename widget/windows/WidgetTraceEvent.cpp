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
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
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

/*
 * Windows widget support for event loop instrumentation.
 * See toolkit/xre/EventTracer.cpp for more details.
 */

#include <stdio.h>
#include <windows.h>

#include "mozilla/WidgetTraceEvent.h"
#include "nsAppShellCID.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIAppShellService.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIWidget.h"
#include "nsIXULWindow.h"
#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsWindowDefs.h"

namespace {

// Used for signaling the background thread from the main thread.
HANDLE sEventHandle = NULL;

// We need a runnable in order to find the hidden window on the main
// thread.
class HWNDGetter : public nsRunnable {
public:
  HWNDGetter() : hidden_window_hwnd(NULL) {}

  HWND hidden_window_hwnd;

  NS_IMETHOD Run() {
    // Jump through some hoops to locate the hidden window.
    nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    nsCOMPtr<nsIXULWindow> hiddenWindow;

    nsresult rv = appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIDocShell> docShell;
    rv = hiddenWindow->GetDocShell(getter_AddRefs(docShell));
    if (NS_FAILED(rv) || !docShell) {
      return rv;
    }

    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));
    
    if (!baseWindow)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIWidget> widget;
    baseWindow->GetMainWidget(getter_AddRefs(widget));

    if (!widget)
      return NS_ERROR_FAILURE;

    hidden_window_hwnd = (HWND)widget->GetNativeData(NS_NATIVE_WINDOW);

    return NS_OK;
  }
};

HWND GetHiddenWindowHWND()
{
  // Need to dispatch this to the main thread because plenty of
  // the things it wants to access are main-thread-only.
  nsRefPtr<HWNDGetter> getter = new HWNDGetter();
  NS_DispatchToMainThread(getter, NS_DISPATCH_SYNC);
  return getter->hidden_window_hwnd;
}

} // namespace

namespace mozilla {

bool InitWidgetTracing()
{
  sEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
  return sEventHandle != NULL;
}

void CleanUpWidgetTracing()
{
  CloseHandle(sEventHandle);
  sEventHandle = NULL;
}

// This function is called from the main (UI) thread.
void SignalTracerThread()
{
  if (sEventHandle != NULL)
    SetEvent(sEventHandle);
}

// This function is called from the background tracer thread.
bool FireAndWaitForTracerEvent()
{
  NS_ABORT_IF_FALSE(sEventHandle, "Tracing not initialized!");

  // First, try to find the hidden window.
  static HWND hidden_window = NULL;
  if (hidden_window == NULL) {
    hidden_window = GetHiddenWindowHWND();
  }

  if (hidden_window == NULL)
    return false;

  // Post the tracer message into the hidden window's message queue,
  // and then block until it's processed.
  PostMessage(hidden_window, MOZ_WM_TRACE, 0, 0);
  WaitForSingleObject(sEventHandle, INFINITE);
  return true;
}

}  // namespace mozilla
