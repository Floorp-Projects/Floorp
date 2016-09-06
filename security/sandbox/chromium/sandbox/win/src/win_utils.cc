// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/win_utils.h"

#include <stddef.h>

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/win/pe_image.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

// Holds the information about a known registry key.
struct KnownReservedKey {
  const wchar_t* name;
  HKEY key;
};

// Contains all the known registry key by name and by handle.
const KnownReservedKey kKnownKey[] = {
    { L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
    { L"HKEY_CURRENT_USER", HKEY_CURRENT_USER },
    { L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
    { L"HKEY_USERS", HKEY_USERS},
    { L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
    { L"HKEY_PERFORMANCE_TEXT", HKEY_PERFORMANCE_TEXT},
    { L"HKEY_PERFORMANCE_NLSTEXT", HKEY_PERFORMANCE_NLSTEXT},
    { L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
    { L"HKEY_DYN_DATA", HKEY_DYN_DATA}
};

// These functions perform case independent path comparisons.
bool EqualPath(const base::string16& first, const base::string16& second) {
  return _wcsicmp(first.c_str(), second.c_str()) == 0;
}

bool EqualPath(const base::string16& first, size_t first_offset,
               const base::string16& second, size_t second_offset) {
  return _wcsicmp(first.c_str() + first_offset,
                  second.c_str() + second_offset) == 0;
}

bool EqualPath(const base::string16& first,
               const wchar_t* second, size_t second_len) {
  return _wcsnicmp(first.c_str(), second, second_len) == 0;
}

bool EqualPath(const base::string16& first, size_t first_offset,
               const wchar_t* second, size_t second_len) {
  return _wcsnicmp(first.c_str() + first_offset, second, second_len) == 0;
}

// Returns true if |path| starts with "\??\" and returns a path without that
// component.
bool IsNTPath(const base::string16& path, base::string16* trimmed_path ) {
  if ((path.size() < sandbox::kNTPrefixLen) ||
      (0 != path.compare(0, sandbox::kNTPrefixLen, sandbox::kNTPrefix))) {
    *trimmed_path = path;
    return false;
  }

  *trimmed_path = path.substr(sandbox::kNTPrefixLen);
  return true;
}

// Returns true if |path| starts with "\Device\" and returns a path without that
// component.
bool IsDevicePath(const base::string16& path, base::string16* trimmed_path ) {
  if ((path.size() < sandbox::kNTDevicePrefixLen) ||
      (!EqualPath(path, sandbox::kNTDevicePrefix,
                  sandbox::kNTDevicePrefixLen))) {
    *trimmed_path = path;
    return false;
  }

  *trimmed_path = path.substr(sandbox::kNTDevicePrefixLen);
  return true;
}

bool StartsWithDriveLetter(const base::string16& path) {
  if (path.size() < 3)
    return false;

  if (path[1] != L':' || path[2] != L'\\')
    return false;

  return (path[0] >= 'a' && path[0] <= 'z') ||
         (path[0] >= 'A' && path[0] <= 'Z');
}

const wchar_t kNTDotPrefix[] = L"\\\\.\\";
const size_t kNTDotPrefixLen = arraysize(kNTDotPrefix) - 1;

// Removes "\\\\.\\" from the path.
void RemoveImpliedDevice(base::string16* path) {
  if (0 == path->compare(0, kNTDotPrefixLen, kNTDotPrefix))
    *path = path->substr(kNTDotPrefixLen);
}

}  // namespace

namespace sandbox {

// Returns true if the provided path points to a pipe.
bool IsPipe(const base::string16& path) {
  size_t start = 0;
  if (0 == path.compare(0, sandbox::kNTPrefixLen, sandbox::kNTPrefix))
    start = sandbox::kNTPrefixLen;

  const wchar_t kPipe[] = L"pipe\\";
  if (path.size() < start + arraysize(kPipe) - 1)
    return false;

  return EqualPath(path, start, kPipe, arraysize(kPipe) - 1);
}

HKEY GetReservedKeyFromName(const base::string16& name) {
  for (size_t i = 0; i < arraysize(kKnownKey); ++i) {
    if (name == kKnownKey[i].name)
      return kKnownKey[i].key;
  }

  return NULL;
}

bool ResolveRegistryName(base::string16 name, base::string16* resolved_name) {
  for (size_t i = 0; i < arraysize(kKnownKey); ++i) {
    if (name.find(kKnownKey[i].name) == 0) {
      HKEY key;
      DWORD disposition;
      if (ERROR_SUCCESS != ::RegCreateKeyEx(kKnownKey[i].key, L"", 0, NULL, 0,
                                            MAXIMUM_ALLOWED, NULL, &key,
                                            &disposition))
        return false;

      bool result = GetPathFromHandle(key, resolved_name);
      ::RegCloseKey(key);

      if (!result)
        return false;

      *resolved_name += name.substr(wcslen(kKnownKey[i].name));
      return true;
    }
  }

  return false;
}

// |full_path| can have any of the following forms:
//    \??\c:\some\foo\bar
//    \Device\HarddiskVolume0\some\foo\bar
//    \??\HarddiskVolume0\some\foo\bar
//    \??\UNC\SERVER\Share\some\foo\bar
DWORD IsReparsePoint(const base::string16& full_path) {
  // Check if it's a pipe. We can't query the attributes of a pipe.
  if (IsPipe(full_path))
    return ERROR_NOT_A_REPARSE_POINT;

  base::string16 path;
  bool nt_path = IsNTPath(full_path, &path);
  bool has_drive = StartsWithDriveLetter(path);
  bool is_device_path = IsDevicePath(path, &path);

  if (!has_drive && !is_device_path && !nt_path)
    return ERROR_INVALID_NAME;

  if (!has_drive) {
    // Add Win32 device namespace prefix, required for some Windows APIs.
    path.insert(0, kNTDotPrefix);
  }

  // Ensure that volume path matches start of path.
  wchar_t vol_path[MAX_PATH];
  if (!::GetVolumePathNameW(path.c_str(), vol_path, MAX_PATH)) {
    return ERROR_INVALID_NAME;
  }
  size_t vol_path_len = wcslen(vol_path);
  if (!EqualPath(path, vol_path, vol_path_len)) {
    return ERROR_INVALID_NAME;
  }

  // vol_path will include a trailing slash, so reduce size for loop check.
  --vol_path_len;

  do {
    DWORD attributes = ::GetFileAttributes(path.c_str());
    if (INVALID_FILE_ATTRIBUTES == attributes) {
      DWORD error = ::GetLastError();
      if (error != ERROR_FILE_NOT_FOUND &&
          error != ERROR_PATH_NOT_FOUND &&
          error != ERROR_INVALID_NAME) {
        // Unexpected error.
        NOTREACHED_NT();
        return error;
      }
    } else if (FILE_ATTRIBUTE_REPARSE_POINT & attributes) {
      // This is a reparse point.
      return ERROR_SUCCESS;
    }

    path.resize(path.rfind(L'\\'));
  } while (path.size() > vol_path_len);  // Skip root dir.

  return ERROR_NOT_A_REPARSE_POINT;
}

// We get a |full_path| of the forms accepted by IsReparsePoint(), and the name
// we'll get from |handle| will be \device\harddiskvolume1\some\foo\bar.
bool SameObject(HANDLE handle, const wchar_t* full_path) {
  // Check if it's a pipe.
  if (IsPipe(full_path))
    return true;

  base::string16 actual_path;
  if (!GetPathFromHandle(handle, &actual_path))
    return false;

  base::string16 path(full_path);
  DCHECK_NT(!path.empty());

  // This may end with a backslash.
  if (path.back() == L'\\') {
    path.pop_back();
  }

  // Perfect match (case-insensitive check).
  if (EqualPath(actual_path, path))
    return true;

  bool nt_path = IsNTPath(path, &path);
  bool has_drive = StartsWithDriveLetter(path);

  if (!has_drive && nt_path) {
    base::string16 simple_actual_path;
    if (IsDevicePath(path, &path)) {
      if (IsDevicePath(actual_path, &simple_actual_path)) {
        // Perfect match (case-insensitive check).
        return (EqualPath(simple_actual_path, path));
      } else {
        return false;
      }
    } else {
      // Add Win32 device namespace for GetVolumePathName.
      path.insert(0, kNTDotPrefix);
    }
  }

  // Get the volume path in the same format as actual_path.
  wchar_t vol_path[MAX_PATH];
  if (!::GetVolumePathName(path.c_str(), vol_path, MAX_PATH)) {
    return false;
  }
  size_t vol_path_len = wcslen(vol_path);
  base::string16 nt_vol;
  if (!GetNtPathFromWin32Path(vol_path, &nt_vol)) {
    return false;
  }

  // The two paths should be the same length.
  if (nt_vol.size() + path.size() - vol_path_len != actual_path.size()) {
    return false;
  }

  // Check the volume matches.
  if (!EqualPath(actual_path, nt_vol.c_str(), nt_vol.size())) {
    return false;
  }

  // Check the path after the volume matches.
  if (!EqualPath(actual_path, nt_vol.size(), path, vol_path_len)) {
    return false;
  }

  return true;
}

// Paths like \Device\HarddiskVolume0\some\foo\bar are assumed to be already
// expanded.
bool ConvertToLongPath(base::string16* path) {
  if (IsPipe(*path))
    return true;

  base::string16 temp_path;
  if (IsDevicePath(*path, &temp_path))
    return false;

  bool is_nt_path = IsNTPath(temp_path, &temp_path);
  bool added_implied_device = false;
  if (!StartsWithDriveLetter(temp_path) && is_nt_path) {
    temp_path = base::string16(kNTDotPrefix) + temp_path;
    added_implied_device = true;
  }

  DWORD size = MAX_PATH;
  scoped_ptr<wchar_t[]> long_path_buf(new wchar_t[size]);

  DWORD return_value = ::GetLongPathName(temp_path.c_str(), long_path_buf.get(),
                                         size);
  while (return_value >= size) {
    size *= 2;
    long_path_buf.reset(new wchar_t[size]);
    return_value = ::GetLongPathName(temp_path.c_str(), long_path_buf.get(),
                                     size);
  }

  DWORD last_error = ::GetLastError();
  if (0 == return_value && (ERROR_FILE_NOT_FOUND == last_error ||
                            ERROR_PATH_NOT_FOUND == last_error ||
                            ERROR_INVALID_NAME == last_error)) {
    // The file does not exist, but maybe a sub path needs to be expanded.
    base::string16::size_type last_slash = temp_path.rfind(L'\\');
    if (base::string16::npos == last_slash)
      return false;

    base::string16 begin = temp_path.substr(0, last_slash);
    base::string16 end = temp_path.substr(last_slash);
    if (!ConvertToLongPath(&begin))
      return false;

    // Ok, it worked. Let's reset the return value.
    temp_path = begin + end;
    return_value = 1;
  } else if (0 != return_value) {
    temp_path = long_path_buf.get();
  }

  if (return_value != 0) {
    if (added_implied_device)
      RemoveImpliedDevice(&temp_path);

    if (is_nt_path) {
      *path = kNTPrefix;
      *path += temp_path;
    } else {
      *path = temp_path;
    }

    return true;
  }

  return false;
}

bool GetPathFromHandle(HANDLE handle, base::string16* path) {
  NtQueryObjectFunction NtQueryObject = NULL;
  ResolveNTFunctionPtr("NtQueryObject", &NtQueryObject);

  OBJECT_NAME_INFORMATION initial_buffer;
  OBJECT_NAME_INFORMATION* name = &initial_buffer;
  ULONG size = sizeof(initial_buffer);
  // Query the name information a first time to get the size of the name.
  // Windows XP requires that the size of the buffer passed in here be != 0.
  NTSTATUS status = NtQueryObject(handle, ObjectNameInformation, name, size,
                                  &size);

  scoped_ptr<BYTE[]> name_ptr;
  if (size) {
    name_ptr.reset(new BYTE[size]);
    name = reinterpret_cast<OBJECT_NAME_INFORMATION*>(name_ptr.get());

    // Query the name information a second time to get the name of the
    // object referenced by the handle.
    status = NtQueryObject(handle, ObjectNameInformation, name, size, &size);
  }

  if (STATUS_SUCCESS != status)
    return false;

  path->assign(name->ObjectName.Buffer, name->ObjectName.Length /
                                        sizeof(name->ObjectName.Buffer[0]));
  return true;
}

bool GetNtPathFromWin32Path(const base::string16& path,
                            base::string16* nt_path) {
  HANDLE file = ::CreateFileW(path.c_str(), 0,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  bool rv = GetPathFromHandle(file, nt_path);
  ::CloseHandle(file);
  return rv;
}

bool WriteProtectedChildMemory(HANDLE child_process, void* address,
                               const void* buffer, size_t length) {
  // First, remove the protections.
  DWORD old_protection;
  if (!::VirtualProtectEx(child_process, address, length,
                          PAGE_WRITECOPY, &old_protection))
    return false;

  SIZE_T written;
  bool ok = ::WriteProcessMemory(child_process, address, buffer, length,
                                 &written) && (length == written);

  // Always attempt to restore the original protection.
  if (!::VirtualProtectEx(child_process, address, length,
                          old_protection, &old_protection))
    return false;

  return ok;
}

};  // namespace sandbox

void ResolveNTFunctionPtr(const char* name, void* ptr) {
  static volatile HMODULE ntdll = NULL;

  if (!ntdll) {
    HMODULE ntdll_local = ::GetModuleHandle(sandbox::kNtdllName);
    // Use PEImage to sanity-check that we have a valid ntdll handle.
    base::win::PEImage ntdll_peimage(ntdll_local);
    CHECK_NT(ntdll_peimage.VerifyMagic());
    // Race-safe way to set static ntdll.
    ::InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(&ntdll), ntdll_local, NULL);

  }

  CHECK_NT(ntdll);
  FARPROC* function_ptr = reinterpret_cast<FARPROC*>(ptr);
  *function_ptr = ::GetProcAddress(ntdll, name);
  CHECK_NT(*function_ptr);
}
