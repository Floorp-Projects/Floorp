// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/window.h"

#include <aclapi.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/sid.h"

namespace {

// Gets the security attributes of a window object referenced by |handle|. The
// lpSecurityDescriptor member of the SECURITY_ATTRIBUTES parameter returned
// must be freed using LocalFree by the caller.
bool GetSecurityAttributes(HANDLE handle, SECURITY_ATTRIBUTES* attributes) {
  attributes->bInheritHandle = FALSE;
  attributes->nLength = sizeof(SECURITY_ATTRIBUTES);

  PACL dacl = NULL;
  DWORD result = ::GetSecurityInfo(handle, SE_WINDOW_OBJECT,
                                   DACL_SECURITY_INFORMATION, NULL, NULL, &dacl,
                                   NULL, &attributes->lpSecurityDescriptor);
  if (ERROR_SUCCESS == result)
    return true;

  return false;
}

}

namespace sandbox {

ResultCode CreateAltWindowStation(HWINSTA* winsta) {
  // Get the security attributes from the current window station; we will
  // use this as the base security attributes for the new window station.
  SECURITY_ATTRIBUTES attributes = {0};
  if (!GetSecurityAttributes(::GetProcessWindowStation(), &attributes)) {
    return SBOX_ERROR_CANNOT_CREATE_WINSTATION;
  }

  // Create the window station using NULL for the name to ask the os to
  // generate it.
  *winsta = ::CreateWindowStationW(
      NULL, 0, GENERIC_READ | WINSTA_CREATEDESKTOP, &attributes);
  LocalFree(attributes.lpSecurityDescriptor);

  if (*winsta)
    return SBOX_ALL_OK;

  return SBOX_ERROR_CANNOT_CREATE_WINSTATION;
}

ResultCode CreateAltDesktop(HWINSTA winsta, HDESK* desktop) {
  base::string16 desktop_name = L"sbox_alternate_desktop_";

  // Append the current PID to the desktop name.
  wchar_t buffer[16];
  _snwprintf_s(buffer, sizeof(buffer) / sizeof(wchar_t), L"0x%X",
               ::GetCurrentProcessId());
  desktop_name += buffer;

  // Get the security attributes from the current desktop, we will use this as
  // the base security attributes for the new desktop.
  SECURITY_ATTRIBUTES attributes = {0};
  if (!GetSecurityAttributes(GetThreadDesktop(GetCurrentThreadId()),
                             &attributes)) {
    return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
  }

  // Back up the current window station, in case we need to switch it.
  HWINSTA current_winsta = ::GetProcessWindowStation();

  if (winsta) {
    // We need to switch to the alternate window station before creating the
    // desktop.
    if (!::SetProcessWindowStation(winsta)) {
      ::LocalFree(attributes.lpSecurityDescriptor);
      return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
    }
  }

  // Create the destkop.
  *desktop = ::CreateDesktop(desktop_name.c_str(),
                             NULL,
                             NULL,
                             0,
                             DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS |
                                 READ_CONTROL | WRITE_DAC | WRITE_OWNER,
                             &attributes);
  ::LocalFree(attributes.lpSecurityDescriptor);

  if (winsta) {
    // Revert to the right window station.
    if (!::SetProcessWindowStation(current_winsta)) {
      return SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION;
    }
  }

  if (*desktop) {
    // Replace the DACL on the new Desktop with a reduced privilege version.
    // We can soft fail on this for now, as it's just an extra mitigation.
    static const ACCESS_MASK kDesktopDenyMask = WRITE_DAC | WRITE_OWNER |
                                                DELETE |
                                                DESKTOP_CREATEMENU |
                                                DESKTOP_CREATEWINDOW |
                                                DESKTOP_HOOKCONTROL |
                                                DESKTOP_JOURNALPLAYBACK |
                                                DESKTOP_JOURNALRECORD |
                                                DESKTOP_SWITCHDESKTOP;
    AddKnownSidToObject(*desktop, SE_WINDOW_OBJECT, Sid(WinRestrictedCodeSid),
                        DENY_ACCESS, kDesktopDenyMask);
    return SBOX_ALL_OK;
  }

  return SBOX_ERROR_CANNOT_CREATE_DESKTOP;
}

base::string16 GetWindowObjectName(HANDLE handle) {
  // Get the size of the name.
  DWORD size = 0;
  ::GetUserObjectInformation(handle, UOI_NAME, NULL, 0, &size);

  if (!size) {
    NOTREACHED();
    return base::string16();
  }

  // Create the buffer that will hold the name.
  scoped_ptr<wchar_t[]> name_buffer(new wchar_t[size]);

  // Query the name of the object.
  if (!::GetUserObjectInformation(handle, UOI_NAME, name_buffer.get(), size,
                                  &size)) {
    NOTREACHED();
    return base::string16();
  }

  return base::string16(name_buffer.get());
}

base::string16 GetFullDesktopName(HWINSTA winsta, HDESK desktop) {
  if (!desktop) {
    NOTREACHED();
    return base::string16();
  }

  base::string16 name;
  if (winsta) {
    name = GetWindowObjectName(winsta);
    name += L'\\';
  }

  name += GetWindowObjectName(desktop);
  return name;
}

}  // namespace sandbox
