// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_WIN_UTILS_H_
#define SANDBOX_SRC_WIN_UTILS_H_

#include <windows.h>
#include <stddef.h>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

// Prefix for path used by NT calls.
const wchar_t kNTPrefix[] = L"\\??\\";
const size_t kNTPrefixLen = arraysize(kNTPrefix) - 1;

const wchar_t kNTDevicePrefix[] = L"\\Device\\";
const size_t kNTDevicePrefixLen = arraysize(kNTDevicePrefix) - 1;

// Automatically acquires and releases a lock when the object is
// is destroyed.
class AutoLock {
 public:
  // Acquires the lock.
  explicit AutoLock(CRITICAL_SECTION *lock) : lock_(lock) {
    ::EnterCriticalSection(lock);
  };

  // Releases the lock;
  ~AutoLock() {
    ::LeaveCriticalSection(lock_);
  };

 private:
  CRITICAL_SECTION *lock_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutoLock);
};

// Basic implementation of a singleton which calls the destructor
// when the exe is shutting down or the DLL is being unloaded.
template <typename Derived>
class SingletonBase {
 public:
  static Derived* GetInstance() {
    static Derived* instance = NULL;
    if (NULL == instance) {
      instance = new Derived();
      // Microsoft CRT extension. In an exe this this called after
      // winmain returns, in a dll is called in DLL_PROCESS_DETACH
      _onexit(OnExit);
    }
    return instance;
  }

 private:
  // this is the function that gets called by the CRT when the
  // process is shutting down.
  static int __cdecl OnExit() {
    delete GetInstance();
    return 0;
  }
};

// Convert a short path (C:\path~1 or \\??\\c:\path~1) to the long version of
// the path. If the path is not a valid filesystem path, the function returns
// false and argument is not modified.
bool ConvertToLongPath(base::string16* path);

// Returns ERROR_SUCCESS if the path contains a reparse point,
// ERROR_NOT_A_REPARSE_POINT if there's no reparse point in this path, or an
// error code when the function fails.
// This function is not smart. It looks for each element in the path and
// returns true if any of them is a reparse point.
DWORD IsReparsePoint(const base::string16& full_path);

// Returns true if the handle corresponds to the object pointed by this path.
bool SameObject(HANDLE handle, const wchar_t* full_path);

// Resolves a handle to an nt path. Returns true if the handle can be resolved.
bool GetPathFromHandle(HANDLE handle, base::string16* path);

// Resolves a win32 path to an nt path using GetPathFromHandle. The path must
// exist. Returs true if the translation was succesful.
bool GetNtPathFromWin32Path(const base::string16& path,
                            base::string16* nt_path);

// Translates a reserved key name to its handle.
// For example "HKEY_LOCAL_MACHINE" returns HKEY_LOCAL_MACHINE.
// Returns NULL if the name does not represent any reserved key name.
HKEY GetReservedKeyFromName(const base::string16& name);

// Resolves a user-readable registry path to a system-readable registry path.
// For example, HKEY_LOCAL_MACHINE\\Software\\microsoft is translated to
// \\registry\\machine\\software\\microsoft. Returns false if the path
// cannot be resolved.
bool ResolveRegistryName(base::string16 name, base::string16* resolved_name);

// Writes |length| bytes from the provided |buffer| into the address space of
// |child_process|, at the specified |address|, preserving the original write
// protection attributes. Returns true on success.
bool WriteProtectedChildMemory(HANDLE child_process, void* address,
                               const void* buffer, size_t length);

// Returns true if the provided path points to a pipe.
bool IsPipe(const base::string16& path);

// Converts a NTSTATUS code to a Win32 error code.
DWORD GetLastErrorFromNtStatus(NTSTATUS status);

// Returns the address of the main exe module in memory taking in account
// address space layout randomization. While it will work on running processes
// it's recommended to only call this for a suspended process. Ideally also
// a process which has not been started. There's a slim chance that a process
// could map its own executables file multiple times, but this is pretty
// unlikely to occur in practice.
void* GetProcessBaseAddress(HANDLE process);

}  // namespace sandbox

// Resolves a function name in NTDLL to a function pointer. The second parameter
// is a pointer to the function pointer.
void ResolveNTFunctionPtr(const char* name, void* ptr);

#endif  // SANDBOX_SRC_WIN_UTILS_H_
