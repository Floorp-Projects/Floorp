/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSKTabTipManager.h"

#include "mozilla/Preferences.h"
#include "nsDebug.h"
#include "mozilla/widget/WinRegistry.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

namespace mozilla {
namespace widget {

/**
 * Get the HWND for the on-screen keyboard, if it's up. Only
 * allowed for Windows 8 and higher.
 */
static HWND GetOnScreenKeyboardWindow() {
  const wchar_t kOSKClassName[] = L"IPTip_Main_Window";
  HWND osk = ::FindWindowW(kOSKClassName, nullptr);
  if (::IsWindow(osk) && ::IsWindowEnabled(osk) && ::IsWindowVisible(osk)) {
    return osk;
  }
  return nullptr;
}

// static
void OSKTabTipManager::ShowOnScreenKeyboard() {
  const char* kOskPathPrefName = "ui.osk.on_screen_keyboard_path";

  if (GetOnScreenKeyboardWindow()) {
    return;
  }

  nsAutoString cachedPath;
  nsresult result = Preferences::GetString(kOskPathPrefName, cachedPath);
  if (NS_FAILED(result) || cachedPath.IsEmpty()) {
    wchar_t path[MAX_PATH];
    // The path to TabTip.exe is defined at the following registry key.
    // This is pulled out of the 64-bit registry hive directly.
    constexpr auto kRegKeyName =
        u"Software\\Classes\\CLSID\\{054AAE20-4BEA-4347-8A35-64A533254A9D}\\LocalServer32"_ns;
    if (!WinRegistry::GetString(HKEY_LOCAL_MACHINE, kRegKeyName, u""_ns, path,
                                WinRegistry::kLegacyWinUtilsStringFlags)) {
      return;
    }

    std::wstring wstrpath(path);
    // The path provided by the registry will often contain
    // %CommonProgramFiles%, which will need to be replaced if it is present.
    size_t commonProgramFilesOffset = wstrpath.find(L"%CommonProgramFiles%");
    if (commonProgramFilesOffset != std::wstring::npos) {
      // The path read from the registry contains the %CommonProgramFiles%
      // environment variable prefix. On 64 bit Windows the
      // SHGetKnownFolderPath function returns the common program files path
      // with the X86 suffix for the FOLDERID_ProgramFilesCommon value.
      // To get the correct path to TabTip.exe we first read the environment
      // variable CommonProgramW6432 which points to the desired common
      // files path. Failing that we fallback to the SHGetKnownFolderPath API.
      // We then replace the %CommonProgramFiles% value with the actual common
      // files path found in the process.
      std::wstring commonProgramFilesPath;
      std::vector<wchar_t> commonProgramFilesPathW6432;
      DWORD bufferSize =
          ::GetEnvironmentVariableW(L"CommonProgramW6432", nullptr, 0);
      if (bufferSize) {
        commonProgramFilesPathW6432.resize(bufferSize);
        ::GetEnvironmentVariableW(L"CommonProgramW6432",
                                  commonProgramFilesPathW6432.data(),
                                  bufferSize);
        commonProgramFilesPath =
            std::wstring(commonProgramFilesPathW6432.data());
      } else {
        PWSTR path = nullptr;
        HRESULT hres = SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0,
                                            nullptr, &path);
        if (FAILED(hres) || !path) {
          return;
        }
        commonProgramFilesPath =
            static_cast<const wchar_t*>(nsDependentString(path).get());
        ::CoTaskMemFree(path);
      }
      wstrpath.replace(commonProgramFilesOffset,
                       wcslen(L"%CommonProgramFiles%"), commonProgramFilesPath);
    }

    cachedPath.Assign(wstrpath.data());
    Preferences::SetString(kOskPathPrefName, cachedPath);
  }

  const char16_t* cachedPathPtr;
  cachedPath.GetData(&cachedPathPtr);
  ShellExecuteW(nullptr, L"", char16ptr_t(cachedPathPtr), nullptr, nullptr,
                SW_SHOW);
}

// static
void OSKTabTipManager::DismissOnScreenKeyboard() {
  HWND osk = GetOnScreenKeyboardWindow();
  if (osk) {
    ::PostMessage(osk, WM_SYSCOMMAND, SC_CLOSE, 0);
  }
}

}  // namespace widget
}  // namespace mozilla
