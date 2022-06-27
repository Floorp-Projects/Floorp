/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is an NSIS plugin which exports a function that pins a provided
// Shortcut to the Windows Taskbar on Windows 10 (1903+) and Windows 11.
// This is an adapted version of the Pin to Taskbar code that is used in
// Firefox: https://searchfox.org/mozilla-central/rev/4bce7d85ba4796dd03c5dcc7cfe8eee0e4c07b3b/browser/components/shell/nsWindowsShellService.cpp#1178

#include <objbase.h>
#include <shlobj.h>

#pragma comment(lib, "shlwapi.lib")

static bool
PinShortcutToTaskbar(const wchar_t *shortcutPath)
{
  // This enum is likely only used for Windows telemetry, INT_MAX is chosen to
  // avoid confusion with existing uses.
  enum PINNEDLISTMODIFYCALLER { PLMC_INT_MAX = INT_MAX };

  // The types below, and the idea of using IPinnedList3::Modify,
  // are thanks to Gee Law <https://geelaw.blog/entries/msedge-pins/>
  static constexpr GUID CLSID_TaskbandPin = {
      0x90aa3a4e,
      0x1cba,
      0x4233,
      {0xb8, 0xbb, 0x53, 0x57, 0x73, 0xd4, 0x84, 0x49}};

  static constexpr GUID IID_IPinnedList3 = {
      0x0dd79ae2,
      0xd156,
      0x45d4,
      {0x9e, 0xeb, 0x3b, 0x54, 0x97, 0x69, 0xe9, 0x40}};

  struct IPinnedList3Vtbl;
  struct IPinnedList3 {
    IPinnedList3Vtbl* vtbl;
  };

  typedef ULONG STDMETHODCALLTYPE ReleaseFunc(IPinnedList3 * that);
  typedef HRESULT STDMETHODCALLTYPE ModifyFunc(
      IPinnedList3 * that, PCIDLIST_ABSOLUTE unpin, PCIDLIST_ABSOLUTE pin,
      PINNEDLISTMODIFYCALLER caller);

  struct IPinnedList3Vtbl {
    void* QueryInterface;  // 0
    void* AddRef;          // 1
    ReleaseFunc* Release;  // 2
    void* Other[13];       // 3-15
    ModifyFunc* Modify;    // 16
  };

  PIDLIST_ABSOLUTE path = nullptr;
  HRESULT hr = SHParseDisplayName(shortcutPath, nullptr, &path, 0, nullptr);
  if (FAILED(hr) || !path) {
    return false;
  }

  IPinnedList3* pinnedList = nullptr;
  hr = CoCreateInstance(CLSID_TaskbandPin, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IPinnedList3, (void**)&pinnedList);
  if (FAILED(hr) || !pinnedList) {
    return false;
  }

  hr = pinnedList->vtbl->Modify(pinnedList, nullptr, path, PLMC_INT_MAX);

  pinnedList->vtbl->Release(pinnedList);
  CoTaskMemFree(path);
  return true;
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
* Pins a provided shortcut to the Taskbar on Windows 10 (1903) and up.
*
* @param  stacktop  Pointer to the top of the stack, AKA the first parameter to
                    the plugin call. Should contain the shortcut to pin.
* @return 1 if the shortcut was pinned successfully, otherwise 0
*/
extern "C" void __declspec(dllexport)
Pin(HWND, int, TCHAR *, stack_t **stacktop, void *)
{
  wchar_t shortcutPath[MAX_PATH + 1];
  bool rv = false;
  // We're skipping building the C runtime to keep the file size low, so we
  // can't use a normal string initialization because that would call memset.
  shortcutPath[0] = L'\0';
  popstring(stacktop, shortcutPath, MAX_PATH);

  rv = PinShortcutToTaskbar(shortcutPath);

  pushstring(stacktop, rv ? L"1" : L"0", 2);
}

BOOL APIENTRY
DllMain(HMODULE, DWORD, LPVOID)
{
  return TRUE;
}
