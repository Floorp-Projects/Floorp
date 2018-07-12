/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, you can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is an NSIS plugin which exports a function that starts a process
// from a provided path by using the shell automation API to have explorer.exe
// invoke ShellExecute. This roundabout method of starting a process is useful
// because it means the new process will use the integrity level and security
// token of the shell, so it allows starting an unelevated process from inside
// an elevated one. The method is based on
// https://blogs.msdn.microsoft.com/oldnewthing/20131118-00/?p=2643
// but the code has been rewritten to remove the need for ATL or the C runtime.

// Normally an NSIS installer would use the UAC plugin, which itself uses both
// an unelevated and an elevated process, and the elevated process can invoke
// functions in the unelevated one, so this plugin wouldn't be needed.
// But uninstallers are often directly run elevated because that's just how
// the Windows UI launches them, so there is no unelevated process. This
// plugin allows starting a needed unelevated process in that situation.

#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "shlwapi.lib")

static IShellView*
GetDesktopWindowShellView()
{
  IShellView* view = nullptr;
  IShellWindows* shell = nullptr;
  CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER,
                   IID_PPV_ARGS(&shell));
  if (shell) {
    VARIANT empty;
    VariantInit(&empty);

    VARIANT loc;
    loc.vt = VT_VARIANT | VT_BYREF;
    PIDLIST_ABSOLUTE locList;
    SHGetFolderLocation(nullptr, CSIDL_DESKTOP, nullptr, 0, &locList);
    loc.byref = locList;

    HWND windowHandle = 0;
    IDispatch* dispatch = nullptr;

    shell->FindWindowSW(&loc, &empty, SWC_DESKTOP, (long*)&windowHandle,
                        SWFO_NEEDDISPATCH, &dispatch);
    if (dispatch) {
      IServiceProvider* provider = nullptr;
      dispatch->QueryInterface(IID_PPV_ARGS(&provider));
      if (provider) {
        IShellBrowser* browser = nullptr;
        provider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
        if (browser) {
          browser->QueryActiveShellView(&view);
          browser->Release();
        }
        provider->Release();
      }
      dispatch->Release();
    }
    shell->Release();
  }

  return view;
}

static IShellDispatch2*
GetApplicationFromShellView(IShellView* view)
{
  IShellDispatch2* shellDispatch = nullptr;
  IDispatch* viewDisp = nullptr;
  HRESULT hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&viewDisp));
  if (SUCCEEDED(hr)) {
    IShellFolderViewDual* shellViewFolder = nullptr;
    viewDisp->QueryInterface(IID_PPV_ARGS(&shellViewFolder));
    if (shellViewFolder) {
      IDispatch* dispatch = nullptr;
      shellViewFolder->get_Application(&dispatch);
      if (dispatch) {
        dispatch->QueryInterface(IID_PPV_ARGS(&shellDispatch));
        dispatch->Release();
      }
      shellViewFolder->Release();
    }
    viewDisp->Release();
  }
  return shellDispatch;
}

static bool
ShellExecInExplorerProcess(wchar_t* path)
{
  bool rv = false;
  if (SUCCEEDED(CoInitialize(nullptr))) {
    IShellView *desktopView = GetDesktopWindowShellView();
    if (desktopView) {
      IShellDispatch2 *shellDispatch = GetApplicationFromShellView(desktopView);
      if (shellDispatch) {
        BSTR bstrPath = SysAllocString(path);
        rv = SUCCEEDED(shellDispatch->ShellExecuteW(bstrPath,
                                                    VARIANT{}, VARIANT{},
                                                    VARIANT{}, VARIANT{}));
        SysFreeString(bstrPath);
        shellDispatch->Release();
      }
      desktopView->Release();
    }
    CoUninitialize();
  }
  return rv;
}

struct stack_t {
  stack_t* next;
  TCHAR text[MAX_PATH];
};

/**
* Removes an element from the top of the NSIS stack
*
* @param  stacktop A pointer to the top of the stack
* @param  str      The string to pop to
* @param  len      The max length
* @return 0 on success
*/
int
popstring(stack_t **stacktop, TCHAR *str, int len)
{
  // Removes the element from the top of the stack and puts it in the buffer
  stack_t *th;
  if (!stacktop || !*stacktop) {
    return 1;
  }

  th = (*stacktop);
  lstrcpyn(str, th->text, len);
  *stacktop = th->next;
  HeapFree(GetProcessHeap(), 0, th);
  return 0;
}

/**
* Adds an element to the top of the NSIS stack
*
* @param  stacktop A pointer to the top of the stack
* @param  str      The string to push on the stack
* @param  len      The length of the string to push on the stack
* @return 0 on success
*/
void
pushstring(stack_t **stacktop, const TCHAR *str, int len)
{
  stack_t *th;
  if (!stacktop) {
    return;
  }
  th = (stack_t*)HeapAlloc(GetProcessHeap(), 0, sizeof(stack_t) + len);
  lstrcpyn(th->text, str, len);
  th->next = *stacktop;
  *stacktop = th;
}

/**
* Starts an executable or URL from the shell process.
*
* @param  stacktop  Pointer to the top of the stack, AKA the first parameter to
                    the plugin call. Should contain the file or URL to execute.
* @return 1 if the file/URL was executed successfully, 0 if it was not
*/
extern "C" void __declspec(dllexport)
Exec(HWND, int, TCHAR *, stack_t **stacktop, void *)
{
  wchar_t path[MAX_PATH + 1];
  // We're skipping building the C runtime to keep the file size low, so we
  // can't use a normal string initialization because that would call memset.
  path[0] = L'\0';
  popstring(stacktop, path, MAX_PATH);
  bool rv = ShellExecInExplorerProcess(path);
  pushstring(stacktop, rv ? L"1" : L"0", 2);
}

BOOL APIENTRY
DllMain(HMODULE, DWORD, LPVOID)
{
  return TRUE;
}
