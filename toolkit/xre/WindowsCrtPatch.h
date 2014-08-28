/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file works around an incompatibility between Visual Studio 2013's
 * C Runtime DLL and Windows XP Service Pack 2.
 *
 * On XP SP2, msvcr120.dll fails to load, because it has a load-time dependency
 * on a kernel32 export named GetLogicalProcessorInformation, which is only
 * available in XP SP3 and newer. Microsoft has declared this to be by design.
 * See: https://connect.microsoft.com/VisualStudio/feedback/details/811379/
 *
 * The CRT calls GetLogicalProcessorInformation only from the concurrency
 * runtime, which our code does not use. A potential workaround is to 
 * static-link the CRT into all of our binaries and let the linker drop the
 * unused API calls. We don't want to take that approach, due to concerns
 * about binary bloat and jemalloc integration.
 *
 * Instead we hook the Windows loader and patch out the missing import.
 * We intercept ntdll!RtlImageNtHeader, which is a helper API called during
 * the DLL loading process. We walk the PE image and redirect the
 * GetLogicalProcessorInformation import to something benign like DebugBreak,
 * before the loader populates msvcr120.dll's import table.
 *
 * This is a fragile hack that only works if we can set up the hook before
 * Windows tries to load msvcr120.dll. This means that all .exe files:
 *  1) must static-link the CRT
 *  2) must delay-load anything with ties to msvcr120.dll (e.g. mozglue.dll)
 *  3) must not call malloc, because the linker would substitute our mozglue
 *     replacements, which leads to the CRT loading mozglue before main.
 * The remainder of our binaries can continue to dynamic-link the CRT.
 * Assertions enforce that our hooks are installed before msvcr120.dll.
 */

#ifndef WindowsCrtPatch_h
#define WindowsCrtPatch_h

#include "nsWindowsDllInterceptor.h"
#include "mozilla/WindowsVersion.h"

namespace WindowsCrtPatch {

mozilla::WindowsDllInterceptor NtdllIntercept;

typedef PIMAGE_NT_HEADERS (NTAPI *RtlImageNtHeader_func)(HMODULE module);
static RtlImageNtHeader_func stub_RtlImageNtHeader = 0;

// A helper to simplify the use of Relative Virtual Addresses.
template <typename T>
class RVAPtr
{
public:
  RVAPtr(HMODULE module, size_t rva)
    : _ptr(reinterpret_cast<T*>(reinterpret_cast<char*>(module) + rva)) {}
  operator T*() { return _ptr; }
  T* operator ->() { return _ptr; }
  T* operator ++() { return ++_ptr; }

private:
  T* _ptr;
};

void
PatchModuleImports(HMODULE module, PIMAGE_NT_HEADERS headers)
{
  static const WORD MAGIC_DOS = 0x5a4d; // "MZ"
  static const DWORD MAGIC_PE = 0x4550; // "PE\0\0"
  RVAPtr<IMAGE_DOS_HEADER> dosStub(module, 0);

  if (!module ||
      !headers ||
      dosStub->e_magic != MAGIC_DOS ||
      headers != RVAPtr<IMAGE_NT_HEADERS>(module, dosStub->e_lfanew) ||
      headers->Signature != MAGIC_PE ||
      headers->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER)) {
    return;
  }

  // The format of the import directory is described in:
  // "An In-Depth Look into the Win32 Portable Executable File Format, Part 2"
  // http://msdn.microsoft.com/en-us/magazine/cc301808.aspx

  IMAGE_DATA_DIRECTORY* importDirectory =
    &headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  RVAPtr<IMAGE_IMPORT_DESCRIPTOR> descriptor(module, importDirectory->VirtualAddress);

  for (; descriptor->OriginalFirstThunk; ++descriptor) {
    RVAPtr<char> importedModule(module, descriptor->Name);
    if (!stricmp(importedModule, "kernel32.dll")) {
      RVAPtr<IMAGE_THUNK_DATA> thunk(module, descriptor->OriginalFirstThunk);
      for (; thunk->u1.AddressOfData; ++thunk) {
        RVAPtr<IMAGE_IMPORT_BY_NAME> import(module, thunk->u1.AddressOfData);
        if (!strcmp(import->Name, "GetLogicalProcessorInformation")) {
          memcpy(import->Name, "DebugBreak", sizeof("DebugBreak"));
        }
      }
    }
  }
}

PIMAGE_NT_HEADERS NTAPI
patched_RtlImageNtHeader(HMODULE module)
{
  PIMAGE_NT_HEADERS headers = stub_RtlImageNtHeader(module);

  if (module == GetModuleHandleA("msvcr120.dll")) {
    PatchModuleImports(module, headers);
  }

  return headers;
}

// Non-inline to make the asserts stand out
MOZ_NEVER_INLINE void
Init()
{
  // If the C Runtime DLL is already loaded, our hooks will be ineffective,
  // and we will fail to load on XP SP2 when built with Visual Studio 2013.
  // We assert the absence of these modules on all Windows builds in order to
  // catch breakage faster.
  //
  // If these assertions fail, see the comment at the top of this file for
  // possible causes. Any changes to the lines below MUST be tested on XP SP2!
  MOZ_ASSERT(!GetModuleHandleA("mozglue.dll"));
  MOZ_ASSERT(!GetModuleHandleA("msvcr120.dll"));
  MOZ_ASSERT(!GetModuleHandleA("msvcr120d.dll"));

  // Temporary until we fully switch over to VS 2013:
  MOZ_ASSERT(!GetModuleHandleA("msvcr100.dll"));
  MOZ_ASSERT(!GetModuleHandleA("msvcr100d.dll"));

#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER >= 1800
  if (!mozilla::IsXPSP3OrLater()) {
    NtdllIntercept.Init("ntdll.dll");
    NtdllIntercept.AddHook("RtlImageNtHeader",
                           reinterpret_cast<intptr_t>(patched_RtlImageNtHeader),
                           reinterpret_cast<void**>(&stub_RtlImageNtHeader));
  }
#endif
}

} // namespace WindowsCrtPatch

#endif // WindowsCrtPatch_h
