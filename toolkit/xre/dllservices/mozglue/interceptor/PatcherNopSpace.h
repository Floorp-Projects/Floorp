/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_PatcherNopSpace_h
#define mozilla_interceptor_PatcherNopSpace_h

#if defined(_M_IX86)

#  include "mozilla/interceptor/PatcherBase.h"

namespace mozilla {
namespace interceptor {

template <typename VMPolicy>
class WindowsDllNopSpacePatcher final : public WindowsDllPatcherBase<VMPolicy> {
  typedef typename VMPolicy::MMPolicyT MMPolicyT;

  // For remembering the addresses of functions we've patched.
  mozilla::Vector<void*> mPatchedFns;

 public:
  template <typename... Args>
  explicit WindowsDllNopSpacePatcher(Args&&... aArgs)
      : WindowsDllPatcherBase<VMPolicy>(std::forward<Args>(aArgs)...) {}

  ~WindowsDllNopSpacePatcher() { Clear(); }

  WindowsDllNopSpacePatcher(const WindowsDllNopSpacePatcher&) = delete;
  WindowsDllNopSpacePatcher(WindowsDllNopSpacePatcher&&) = delete;
  WindowsDllNopSpacePatcher& operator=(const WindowsDllNopSpacePatcher&) =
      delete;
  WindowsDllNopSpacePatcher& operator=(WindowsDllNopSpacePatcher&&) = delete;

  void Clear() {
    // Restore the mov edi, edi to the beginning of each function we patched.

    for (auto&& ptr : mPatchedFns) {
      WritableTargetFunction<MMPolicyT> fn(
          this->mVMPolicy, reinterpret_cast<uintptr_t>(ptr), sizeof(uint16_t));
      if (!fn) {
        continue;
      }

      // mov edi, edi
      fn.CommitAndWriteShort(0xff8b);
    }

    mPatchedFns.clear();
  }

  /**
   * NVIDIA Optimus drivers utilize Microsoft Detours 2.x to patch functions
   * in our address space. There is a bug in Detours 2.x that causes it to
   * patch at the wrong address when attempting to detour code that is already
   * NOP space patched. This function is an effort to detect the presence of
   * this NVIDIA code in our address space and disable NOP space patching if it
   * is. We also check AppInit_DLLs since this is the mechanism that the Optimus
   * drivers use to inject into our process.
   */
  static bool IsCompatible() {
    // These DLLs are known to have bad interactions with this style of patching
    const wchar_t* kIncompatibleDLLs[] = {L"detoured.dll", L"_etoured.dll",
                                          L"nvd3d9wrap.dll", L"nvdxgiwrap.dll"};
    // See if the infringing DLLs are already loaded
    for (unsigned int i = 0; i < mozilla::ArrayLength(kIncompatibleDLLs); ++i) {
      if (GetModuleHandleW(kIncompatibleDLLs[i])) {
        return false;
      }
    }
    if (GetModuleHandleW(L"user32.dll")) {
      // user32 is loaded but the infringing DLLs are not, assume we're safe to
      // proceed.
      return true;
    }
    // If user32 has not loaded yet, check AppInit_DLLs to ensure that Optimus
    // won't be loaded once user32 is initialized.
    HKEY hkey = NULL;
    if (!RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0,
            KEY_QUERY_VALUE, &hkey)) {
      nsAutoRegKey key(hkey);
      DWORD numBytes = 0;
      const wchar_t kAppInitDLLs[] = L"AppInit_DLLs";
      // Query for required buffer size
      LONG status = RegQueryValueExW(hkey, kAppInitDLLs, nullptr, nullptr,
                                     nullptr, &numBytes);
      mozilla::UniquePtr<wchar_t[]> data;
      if (!status) {
        // Allocate the buffer and query for the actual data
        data = mozilla::MakeUnique<wchar_t[]>((numBytes + 1) / sizeof(wchar_t));
        status = RegQueryValueExW(hkey, kAppInitDLLs, nullptr, nullptr,
                                  (LPBYTE)data.get(), &numBytes);
      }
      if (!status) {
        // For each token, split up the filename components and then check the
        // name of the file.
        const wchar_t kDelimiters[] = L", ";
        wchar_t* tokenContext = nullptr;
        wchar_t* token = wcstok_s(data.get(), kDelimiters, &tokenContext);
        while (token) {
          wchar_t fname[_MAX_FNAME] = {0};
          if (!_wsplitpath_s(token, nullptr, 0, nullptr, 0, fname,
                             mozilla::ArrayLength(fname), nullptr, 0)) {
            // nvinit.dll is responsible for bootstrapping the DLL injection, so
            // that is the library that we check for here
            const wchar_t kNvInitName[] = L"nvinit";
            if (!_wcsnicmp(fname, kNvInitName,
                           mozilla::ArrayLength(kNvInitName))) {
              return false;
            }
          }
          token = wcstok_s(nullptr, kDelimiters, &tokenContext);
        }
      }
    }
    return true;
  }

  bool AddHook(FARPROC aTargetFn, intptr_t aHookDest, void** aOrigFunc) {
    if (!IsCompatible()) {
#  if defined(MOZILLA_INTERNAL_API)
      NS_WARNING("NOP space patching is unavailable for compatibility reasons");
#  endif
      return false;
    }

    MOZ_ASSERT(aTargetFn);
    if (!aTargetFn) {
      return false;
    }

    ReadOnlyTargetFunction<MMPolicyT> readOnlyTargetFn(
        this->ResolveRedirectedAddress(aTargetFn));

    if (!WriteHook(readOnlyTargetFn, aHookDest, aOrigFunc)) {
      return false;
    }

    return mPatchedFns.append(
        reinterpret_cast<void*>(readOnlyTargetFn.GetBaseAddress()));
  }

  bool WriteHook(const ReadOnlyTargetFunction<MMPolicyT>& aFn,
                 intptr_t aHookDest, void** aOrigFunc) {
    // Ensure we can read and write starting at fn - 5 (for the long jmp we're
    // going to write) and ending at fn + 2 (for the short jmp up to the long
    // jmp). These bytes may span two pages with different protection.
    WritableTargetFunction<MMPolicyT> writableFn(aFn.Promote(7, -5));
    if (!writableFn) {
      return false;
    }

    // Check that the 5 bytes before the function are NOP's or INT 3's,
    const uint8_t nopOrBp[] = {0x90, 0xCC};
    if (!writableFn.template VerifyValuesAreOneOf<uint8_t, 5>(nopOrBp)) {
      return false;
    }

    // ... and that the first 2 bytes of the function are mov(edi, edi).
    // There are two ways to encode the same thing:
    //
    //   0x89 0xff == mov r/m, r
    //   0x8b 0xff == mov r, r/m
    //
    // where "r" is register and "r/m" is register or memory.
    // Windows seems to use 0x8B 0xFF. We include 0x89 0xFF out of paranoia.

    // (These look backwards because little-endian)
    const uint16_t possibleEncodings[] = {0xFF8B, 0xFF89};
    if (!writableFn.template VerifyValuesAreOneOf<uint16_t, 1>(
            possibleEncodings, 5)) {
      return false;
    }

    // Write a long jump into the space above the function.
    writableFn.WriteByte(0xe9);  // jmp
    if (!writableFn) {
      return false;
    }

    writableFn.WriteDisp32(aHookDest);  // target
    if (!writableFn) {
      return false;
    }

    // Set aOrigFunc here, because after this point, aHookDest might be called,
    // and aHookDest might use the aOrigFunc pointer.
    *aOrigFunc = reinterpret_cast<void*>(writableFn.GetCurrentAddress() +
                                         sizeof(uint16_t));

    // Short jump up into our long jump.
    return writableFn.CommitAndWriteShort(0xF9EB);  // jmp $-5
  }
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // defined(_M_IX86)

#endif  // mozilla_interceptor_PatcherNopSpace_h
