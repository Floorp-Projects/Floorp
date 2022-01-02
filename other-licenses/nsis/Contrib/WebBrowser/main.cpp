// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <windows.h>
#include "resource.h"
#include "WebBrowser.h"
#include "exdll.h"

// These variables are global because they're needed by more than one of
// our plugin methods. The expectation is that these are safe because the
// NSIS framework doesn't really support more than one dialog or thread
// at a time, and that means we don't have to either.

// Instance handle for this DLL
HINSTANCE gHInst = nullptr;
// Parent window proc which we'll need to override and then restore
WNDPROC gWndProcOld = nullptr;
// Handle to the dialog we'll create
HWND gHwnd = nullptr;
// Set to true when our dialog should be destroyed
bool gDone = false;
// Web browser OLE site
WebBrowser* gBrowser = nullptr;

// Set web browser control feature flags that are configured on a per-process
// basis, not for individual instances of the control. This mainly means
// disabling things that could turn into security holes.
static void ConfigurePerProcessBrowserFeatures() {
  // For most of the features we're configuring, setting them to TRUE means
  // we're disabling something, but for a few setting them to FALSE disables the
  // thing. We don't necessarily care about *every* feature that's in the
  // INTERNETFEATURELIST enum, but it seems safer to set them all anyway, to
  // make sure we don't miss anything we *do* care about.
  struct Feature {
    INTERNETFEATURELIST id;
    BOOL value;
  } features[] = {{FEATURE_OBJECT_CACHING, TRUE},
                  {FEATURE_ZONE_ELEVATION, TRUE},
                  {FEATURE_MIME_HANDLING, TRUE},
                  {FEATURE_MIME_SNIFFING, FALSE},
                  {FEATURE_WINDOW_RESTRICTIONS, TRUE},
                  {FEATURE_WEBOC_POPUPMANAGEMENT, TRUE},
                  {FEATURE_BEHAVIORS, TRUE},
                  {FEATURE_DISABLE_MK_PROTOCOL, TRUE},
                  // It isn't possible to set FEATURE_LOCALMACHINE_LOCKDOWN
                  // using the SET_FEATURE_ON_PROCESS mode; see the MSDN page
                  // on CoInternetSetFeatureEnabled for the explanation.
                  //{FEATURE_LOCALMACHINE_LOCKDOWN, TRUE},
                  {FEATURE_SECURITYBAND, FALSE},
                  {FEATURE_RESTRICT_ACTIVEXINSTALL, TRUE},
                  {FEATURE_VALIDATE_NAVIGATE_URL, TRUE},
                  {FEATURE_RESTRICT_FILEDOWNLOAD, TRUE},
                  {FEATURE_ADDON_MANAGEMENT, TRUE},
                  {FEATURE_PROTOCOL_LOCKDOWN, TRUE},
                  {FEATURE_HTTP_USERNAME_PASSWORD_DISABLE, TRUE},
                  {FEATURE_SAFE_BINDTOOBJECT, TRUE},
                  {FEATURE_UNC_SAVEDFILECHECK, TRUE},
                  {FEATURE_GET_URL_DOM_FILEPATH_UNENCODED, FALSE},
                  {FEATURE_TABBED_BROWSING, FALSE},
                  {FEATURE_SSLUX, FALSE},
                  {FEATURE_DISABLE_NAVIGATION_SOUNDS, TRUE},
                  {FEATURE_DISABLE_LEGACY_COMPRESSION, TRUE},
                  {FEATURE_FORCE_ADDR_AND_STATUS, FALSE},
                  {FEATURE_XMLHTTP, FALSE},
                  {FEATURE_DISABLE_TELNET_PROTOCOL, TRUE},
                  {FEATURE_FEEDS, FALSE},
                  {FEATURE_BLOCK_INPUT_PROMPTS, TRUE}};

  for (Feature feature : features) {
    CoInternetSetFeatureEnabled(feature.id, SET_FEATURE_ON_PROCESS,
                                feature.value);
  }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    gHInst = instance;
    (void)OleInitialize(nullptr);
    ConfigurePerProcessBrowserFeatures();
  }
  return TRUE;
}

UINT_PTR __cdecl NSISPluginCallback(NSPIM msg) {
  if (msg == NSPIM_UNLOAD) {
    OleUninitialize();
  }
  return 0;
}

BOOL CALLBACK ParentWndProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) {
  BOOL bRes =
      CallWindowProc((WNDPROC)gWndProcOld, hwnd, message, wParam, lParam);
  if (!bRes && message == WM_NOTIFY_OUTER_NEXT) {
    gDone = true;
    PostMessage(gHwnd, WM_CLOSE, 0, 0);
  }
  return bRes;
}

BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  return FALSE;
}

void Init(HWND hWndParent, int string_size, TCHAR* variables,
          stack_t** stacktop, extra_parameters* extra) {
  EXDLL_INIT();
  extra->RegisterPluginCallback(gHInst, NSISPluginCallback);

  HWND hwndChild = GetDlgItem(hWndParent, 1018);
  if (!hwndChild) {
    return;
  }

  HWND hwnd =
      CreateDialog(gHInst, MAKEINTRESOURCE(IDD_DIALOG1), hWndParent, DlgProc);
  if (!hwnd) {
    gDone = true;
  } else {
    gDone = false;
    gWndProcOld =
        (WNDPROC)SetWindowLong(hWndParent, DWL_DLGPROC, (LONG)ParentWndProc);

    // Tell NSIS to replace its inner dialog with ours.
    SendMessage(hWndParent, WM_NOTIFY_CUSTOM_READY, (WPARAM)hwnd, 0);

    // Initialize the browser control.
    if (gBrowser) {
      gBrowser->Shutdown();
      gBrowser->Release();
    }
    gBrowser = new WebBrowser(hwnd);

    if (!gBrowser || !gBrowser->IsInitialized()) {
      return;
    }

    gHwnd = hwnd;

    // Move our dialog to match the size of the parent.
    RECT r;
    GetClientRect(hwndChild, &r);
    MoveWindow(hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top, FALSE);
    gBrowser->SetRect(r);
  }
}

PLUGINFUNCTION(ShowPage) {
  if (!gBrowser) {
    Init(hWndParent, string_size, variables, stacktop, extra);
  }

  if (!gBrowser->IsInitialized()) {
    return;
  }

  TCHAR* sUrl =
      (TCHAR*)HeapAlloc(GetProcessHeap(), 0, g_stringsize * sizeof(TCHAR));
  popstring(sUrl);

  if (gBrowser) {
    gBrowser->Navigate(sUrl);

    ShowWindow(gHwnd, SW_SHOWNA);
    UpdateWindow(gHwnd);

    while (!gDone) {
      // This explicit wait call is needed rather than just a blocking
      // GetMessage because we need this thread to be alertable so that our
      // timers can wake it up and run their callbacks.
      MsgWaitForMultipleObjectsEx(0, nullptr, 100, QS_ALLINPUT,
                                  MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);
      MSG msg;
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        bool tab = msg.message == WM_KEYDOWN && msg.wParam == VK_TAB;

        if (gBrowser->ActiveObjectTranslateAccelerator(tab, &msg) != S_OK &&
            !IsDialogMessage(gHwnd, &msg) &&
            !IsDialogMessage(g_hwndParent, &msg)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
    }

    SetWindowLong(g_hwndParent, DWL_DLGPROC, (LONG)gWndProcOld);
    if (gHwnd) {
      DestroyWindow(gHwnd);
    }

    gBrowser->Shutdown();
    gBrowser->Release();
    gBrowser = nullptr;
  }

  HeapFree(GetProcessHeap(), 0, (char*)sUrl);
}
