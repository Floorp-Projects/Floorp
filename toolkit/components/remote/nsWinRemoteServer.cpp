/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CmdLineAndEnvUtils.h"
#include "nsWinRemoteServer.h"
#include "RemoteUtils.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsPIDOMWindow.h"
#include "nsIWindowMediator.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsICommandLineRunner.h"
#include "nsICommandLine.h"
#include "nsCommandLine.h"
#include "nsIDocShell.h"
#include "WinRemoteMessage.h"

HWND hwndForDOMWindow(mozIDOMWindowProxy* window) {
  if (!window) {
    return 0;
  }
  nsCOMPtr<nsPIDOMWindowOuter> pidomwindow = nsPIDOMWindowOuter::From(window);

  nsCOMPtr<nsIBaseWindow> ppBaseWindow =
      do_QueryInterface(pidomwindow->GetDocShell());
  if (!ppBaseWindow) {
    return 0;
  }

  nsCOMPtr<nsIWidget> ppWidget;
  ppBaseWindow->GetMainWidget(getter_AddRefs(ppWidget));

  return (HWND)(ppWidget->GetNativeData(NS_NATIVE_WIDGET));
}

static nsresult GetMostRecentWindow(mozIDOMWindowProxy** aWindow) {
  nsresult rv;
  nsCOMPtr<nsIWindowMediator> med(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  if (med) return med->GetMostRecentWindow(nullptr, aWindow);

  return NS_ERROR_FAILURE;
}

LRESULT CALLBACK WindowProc(HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_COPYDATA) {
    WinRemoteMessageReceiver receiver;
    if (NS_SUCCEEDED(receiver.Parse(reinterpret_cast<COPYDATASTRUCT*>(lp)))) {
      receiver.CommandLineRunner()->Run();
    } else {
      NS_ERROR("Error initializing command line.");
    }

    // Get current window and return its window handle.
    nsCOMPtr<mozIDOMWindowProxy> win;
    GetMostRecentWindow(getter_AddRefs(win));
    return win ? (LRESULT)hwndForDOMWindow(win) : 0;
  }
  return DefWindowProc(msgWindow, msg, wp, lp);
}

nsresult nsWinRemoteServer::Startup(const char* aAppName,
                                    const char* aProfileName) {
  nsString className;
  BuildClassName(aAppName, aProfileName, className);

  WNDCLASSW classStruct = {0,                 // style
                           &WindowProc,       // lpfnWndProc
                           0,                 // cbClsExtra
                           0,                 // cbWndExtra
                           0,                 // hInstance
                           0,                 // hIcon
                           0,                 // hCursor
                           0,                 // hbrBackground
                           0,                 // lpszMenuName
                           className.get()};  // lpszClassName

  // Register the window class.
  NS_ENSURE_TRUE(::RegisterClassW(&classStruct), NS_ERROR_FAILURE);

  // Create the window.
  mHandle = ::CreateWindowW(className.get(),
                            0,           // title
                            WS_CAPTION,  // style
                            0, 0, 0, 0,  // x, y, cx, cy
                            0,           // parent
                            0,           // menu
                            0,           // instance
                            0);          // create struct

  return mHandle ? NS_OK : NS_ERROR_FAILURE;
}

void nsWinRemoteServer::Shutdown() {
  DestroyWindow(mHandle);
  mHandle = nullptr;
}
