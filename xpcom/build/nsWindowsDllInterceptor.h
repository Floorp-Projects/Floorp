/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_WINDOWS_DLL_INTERCEPTOR_H_
#define NS_WINDOWS_DLL_INTERCEPTOR_H_

#include "mozilla/Assertions.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"
#include "nsWindowsHelpers.h"

#include <wchar.h>
#include <windows.h>
#include <winternl.h>

/*
 * Simple function interception.
 *
 * We have two separate mechanisms for intercepting a function: We can use the
 * built-in nop space, if it exists, or we can create a detour.
 *
 * Using the built-in nop space works as follows: On x86-32, DLL functions
 * begin with a two-byte nop (mov edi, edi) and are preceeded by five bytes of
 * NOP instructions.
 *
 * When we detect a function with this prelude, we do the following:
 *
 * 1. Write a long jump to our interceptor function into the five bytes of NOPs
 *    before the function.
 *
 * 2. Write a short jump -5 into the two-byte nop at the beginning of the function.
 *
 * This mechanism is nice because it's thread-safe.  It's even safe to do if
 * another thread is currently running the function we're modifying!
 *
 * When the WindowsDllNopSpacePatcher is destroyed, we overwrite the short jump
 * but not the long jump, so re-intercepting the same function won't work,
 * because its prelude won't match.
 *
 *
 * Unfortunately nop space patching doesn't work on functions which don't have
 * this magic prelude (and in particular, x86-64 never has the prelude).  So
 * when we can't use the built-in nop space, we fall back to using a detour,
 * which works as follows:
 *
 * 1. Save first N bytes of OrigFunction to trampoline, where N is a
 *    number of bytes >= 5 that are instruction aligned.
 *
 * 2. Replace first 5 bytes of OrigFunction with a jump to the Hook
 *    function.
 *
 * 3. After N bytes of the trampoline, add a jump to OrigFunction+N to
 *    continue original program flow.
 *
 * 4. Hook function needs to call the trampoline during its execution,
 *    to invoke the original function (so address of trampoline is
 *    returned).
 *
 * When the WindowsDllDetourPatcher object is destructed, OrigFunction is
 * patched again to jump directly to the trampoline instead of going through
 * the hook function. As such, re-intercepting the same function won't work, as
 * jump instructions are not supported.
 *
 * Note that this is not thread-safe.  Sad day.
 *
 */

#include <stdint.h>

namespace mozilla {
namespace internal {

class AutoVirtualProtect
{
public:
  AutoVirtualProtect(void* aFunc, size_t aSize, DWORD aProtect)
    : mFunc(aFunc), mSize(aSize), mNewProtect(aProtect), mOldProtect(0),
      mSuccess(false)
  {}

  ~AutoVirtualProtect()
  {
    if (mSuccess) {
      VirtualProtectEx(GetCurrentProcess(), mFunc, mSize, mOldProtect,
                       &mOldProtect);
    }
  }

  bool Protect()
  {
    mSuccess = !!VirtualProtectEx(GetCurrentProcess(), mFunc, mSize,
                                  mNewProtect, &mOldProtect);
    return mSuccess;
  }

private:
  void* const mFunc;
  size_t const mSize;
  DWORD const mNewProtect;
  DWORD mOldProtect;
  bool mSuccess;
};

class WindowsDllNopSpacePatcher
{
  typedef uint8_t* byteptr_t;
  HMODULE mModule;

  // Dumb array for remembering the addresses of functions we've patched.
  // (This should be nsTArray, but non-XPCOM code uses this class.)
  static const size_t maxPatchedFns = 128;
  byteptr_t mPatchedFns[maxPatchedFns];
  int mPatchedFnsLen;

public:
  WindowsDllNopSpacePatcher()
    : mModule(0)
    , mPatchedFnsLen(0)
  {}

#if defined(_M_IX86)
  ~WindowsDllNopSpacePatcher()
  {
    // Restore the mov edi, edi to the beginning of each function we patched.

    for (int i = 0; i < mPatchedFnsLen; i++) {
      byteptr_t fn = mPatchedFns[i];

      // Ensure we can write to the code.
      AutoVirtualProtect protect(fn, 2, PAGE_EXECUTE_READWRITE);
      if (!protect.Protect()) {
        // printf("VirtualProtectEx failed! %d\n", GetLastError());
        continue;
      }

      // mov edi, edi
      *((uint16_t*)fn) = 0xff8b;

      // I don't think this is actually necessary, but it can't hurt.
      FlushInstructionCache(GetCurrentProcess(),
                            /* ignored */ nullptr,
                            /* ignored */ 0);
    }
  }

  void Init(const char* aModuleName)
  {
    if (!IsCompatible()) {
#if defined(MOZILLA_INTERNAL_API)
      NS_WARNING("NOP space patching is unavailable for compatibility reasons");
#endif
      return;
    }

    mModule = LoadLibraryExA(aModuleName, nullptr, 0);
    if (!mModule) {
      //printf("LoadLibraryEx for '%s' failed\n", aModuleName);
      return;
    }
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
  static bool IsCompatible()
  {
    // These DLLs are known to have bad interactions with this style of patching
    const wchar_t* kIncompatibleDLLs[] = {
      L"detoured.dll",
      L"_etoured.dll",
      L"nvd3d9wrap.dll",
      L"nvdxgiwrap.dll"
    };
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
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE,
          L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
          0, KEY_QUERY_VALUE, &hkey)) {
      nsAutoRegKey key(hkey);
      DWORD numBytes = 0;
      const wchar_t kAppInitDLLs[] = L"AppInit_DLLs";
      // Query for required buffer size
      LONG status = RegQueryValueExW(hkey, kAppInitDLLs, nullptr,
                                     nullptr, nullptr, &numBytes);
      mozilla::UniquePtr<wchar_t[]> data;
      if (!status) {
        // Allocate the buffer and query for the actual data
        data = mozilla::MakeUnique<wchar_t[]>(numBytes / sizeof(wchar_t));
        status = RegQueryValueExW(hkey, kAppInitDLLs, nullptr,
                                  nullptr, (LPBYTE)data.get(), &numBytes);
      }
      if (!status) {
        // For each token, split up the filename components and then check the
        // name of the file.
        const wchar_t kDelimiters[] = L", ";
        wchar_t* tokenContext = nullptr;
        wchar_t* token = wcstok_s(data.get(), kDelimiters, &tokenContext);
        while (token) {
          wchar_t fname[_MAX_FNAME] = {0};
          if (!_wsplitpath_s(token, nullptr, 0, nullptr, 0,
                             fname, mozilla::ArrayLength(fname),
                             nullptr, 0)) {
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

  bool AddHook(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    if (!mModule) {
      return false;
    }

    if (!IsCompatible()) {
#if defined(MOZILLA_INTERNAL_API)
      NS_WARNING("NOP space patching is unavailable for compatibility reasons");
#endif
      return false;
    }

    if (mPatchedFnsLen == maxPatchedFns) {
      // printf ("No space for hook in mPatchedFns.\n");
      return false;
    }

    byteptr_t fn = reinterpret_cast<byteptr_t>(GetProcAddress(mModule, aName));
    if (!fn) {
      //printf ("GetProcAddress failed\n");
      return false;
    }

    fn = ResolveRedirectedAddress(fn);

    // Ensure we can read and write starting at fn - 5 (for the long jmp we're
    // going to write) and ending at fn + 2 (for the short jmp up to the long
    // jmp). These bytes may span two pages with different protection.
    AutoVirtualProtect protectBefore(fn - 5, 5, PAGE_EXECUTE_READWRITE);
    AutoVirtualProtect protectAfter(fn, 2, PAGE_EXECUTE_READWRITE);
    if (!protectBefore.Protect() || !protectAfter.Protect()) {
      //printf ("VirtualProtectEx failed! %d\n", GetLastError());
      return false;
    }

    bool rv = WriteHook(fn, aHookDest, aOrigFunc);

    if (rv) {
      mPatchedFns[mPatchedFnsLen] = fn;
      mPatchedFnsLen++;
    }

    return rv;
  }

  bool WriteHook(byteptr_t aFn, intptr_t aHookDest, void** aOrigFunc)
  {
    // Check that the 5 bytes before aFn are NOP's or INT 3's,
    // and that the 2 bytes after aFn are mov(edi, edi).
    //
    // It's safe to read aFn[-5] because we set it to PAGE_EXECUTE_READWRITE
    // before calling WriteHook.

    for (int i = -5; i <= -1; i++) {
      if (aFn[i] != 0x90 && aFn[i] != 0xcc) { // nop or int 3
        return false;
      }
    }

    // mov edi, edi.  Yes, there are two ways to encode the same thing:
    //
    //   0x89ff == mov r/m, r
    //   0x8bff == mov r, r/m
    //
    // where "r" is register and "r/m" is register or memory.  Windows seems to
    // use 8bff; I include 89ff out of paranoia.
    if ((aFn[0] != 0x8b && aFn[0] != 0x89) || aFn[1] != 0xff) {
      return false;
    }

    // Write a long jump into the space above the function.
    aFn[-5] = 0xe9; // jmp
    *((intptr_t*)(aFn - 4)) = aHookDest - (uintptr_t)(aFn); // target displacement

    // Set aOrigFunc here, because after this point, aHookDest might be called,
    // and aHookDest might use the aOrigFunc pointer.
    *aOrigFunc = aFn + 2;

    // Short jump up into our long jump.
    *((uint16_t*)(aFn)) = 0xf9eb; // jmp $-5

    // I think this routine is safe without this, but it can't hurt.
    FlushInstructionCache(GetCurrentProcess(),
                          /* ignored */ nullptr,
                          /* ignored */ 0);

    return true;
  }

private:
  static byteptr_t ResolveRedirectedAddress(const byteptr_t aOriginalFunction)
  {
    // If function entry is jmp [disp32] such as used by kernel32,
    // we resolve redirected address from import table.
    if (aOriginalFunction[0] == 0xff && aOriginalFunction[1] == 0x25) {
      return (byteptr_t)(**((uint32_t**) (aOriginalFunction + 2)));
    }

    return aOriginalFunction;
  }
#else
  void Init(const char* aModuleName)
  {
    // Not implemented except on x86-32.
  }

  bool AddHook(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    // Not implemented except on x86-32.
    return false;
  }
#endif
};

class WindowsDllDetourPatcher
{
  typedef unsigned char* byteptr_t;
public:
  WindowsDllDetourPatcher()
    : mModule(0), mHookPage(0), mMaxHooks(0), mCurHooks(0)
  {
  }

  ~WindowsDllDetourPatcher()
  {
    int i;
    byteptr_t p;
    for (i = 0, p = mHookPage; i < mCurHooks; i++, p += kHookSize) {
#if defined(_M_IX86)
      size_t nBytes = 1 + sizeof(intptr_t);
#elif defined(_M_X64)
      size_t nBytes = 2 + sizeof(intptr_t);
#else
#error "Unknown processor type"
#endif
      byteptr_t origBytes = *((byteptr_t*)p);

      // ensure we can modify the original code
      AutoVirtualProtect protect(origBytes, nBytes, PAGE_EXECUTE_READWRITE);
      if (!protect.Protect()) {
        //printf ("VirtualProtectEx failed! %d\n", GetLastError());
        continue;
      }

      // Remove the hook by making the original function jump directly
      // in the trampoline.
      intptr_t dest = (intptr_t)(p + sizeof(void*));
#if defined(_M_IX86)
      *((intptr_t*)(origBytes + 1)) =
        dest - (intptr_t)(origBytes + 5); // target displacement
#elif defined(_M_X64)
      *((intptr_t*)(origBytes + 2)) = dest;
#else
#error "Unknown processor type"
#endif
    }
  }

  void Init(const char* aModuleName, int aNumHooks = 0)
  {
    if (mModule) {
      return;
    }

    mModule = LoadLibraryExA(aModuleName, nullptr, 0);
    if (!mModule) {
      //printf("LoadLibraryEx for '%s' failed\n", aModuleName);
      return;
    }

    int hooksPerPage = 4096 / kHookSize;
    if (aNumHooks == 0) {
      aNumHooks = hooksPerPage;
    }

    mMaxHooks = aNumHooks + (hooksPerPage % aNumHooks);

    mHookPage = (byteptr_t)VirtualAllocEx(GetCurrentProcess(), nullptr,
                                          mMaxHooks * kHookSize,
                                          MEM_COMMIT | MEM_RESERVE,
                                          PAGE_EXECUTE_READWRITE);
    if (!mHookPage) {
      mModule = 0;
      return;
    }
  }

  bool Initialized() { return !!mModule; }

  void LockHooks()
  {
    if (!mModule) {
      return;
    }

    DWORD op;
    VirtualProtectEx(GetCurrentProcess(), mHookPage, mMaxHooks * kHookSize,
                     PAGE_EXECUTE_READ, &op);

    mModule = 0;
  }

  bool AddHook(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    if (!mModule) {
      return false;
    }

    void* pAddr = (void*)GetProcAddress(mModule, aName);
    if (!pAddr) {
      //printf ("GetProcAddress failed\n");
      return false;
    }

    pAddr = ResolveRedirectedAddress((byteptr_t)pAddr);

    CreateTrampoline(pAddr, aHookDest, aOrigFunc);
    if (!*aOrigFunc) {
      //printf ("CreateTrampoline failed\n");
      return false;
    }

    return true;
  }

protected:
  const static int kPageSize = 4096;
  const static int kHookSize = 128;

  HMODULE mModule;
  byteptr_t mHookPage;
  int mMaxHooks;
  int mCurHooks;

  // rex bits
  static const BYTE kMaskHighNibble = 0xF0;
  static const BYTE kRexOpcode = 0x40;
  static const BYTE kMaskRexW = 0x08;
  static const BYTE kMaskRexR = 0x04;
  static const BYTE kMaskRexX = 0x02;
  static const BYTE kMaskRexB = 0x01;

  // mod r/m bits
  static const BYTE kRegFieldShift = 3;
  static const BYTE kMaskMod = 0xC0;
  static const BYTE kMaskReg = 0x38;
  static const BYTE kMaskRm = 0x07;
  static const BYTE kRmNeedSib = 0x04;
  static const BYTE kModReg = 0xC0;
  static const BYTE kModDisp32 = 0x80;
  static const BYTE kModDisp8 = 0x40;
  static const BYTE kModNoRegDisp = 0x00;
  static const BYTE kRmNoRegDispDisp32 = 0x05;

  // sib bits
  static const BYTE kMaskSibScale = 0xC0;
  static const BYTE kMaskSibIndex = 0x38;
  static const BYTE kMaskSibBase = 0x07;
  static const BYTE kSibBaseEbp = 0x05;

  int CountModRmSib(const BYTE *aModRm, BYTE* aSubOpcode = nullptr)
  {
    if (!aModRm) {
      return -1;
    }
    int numBytes = 1; // Start with 1 for mod r/m byte itself
    switch (*aModRm & kMaskMod) {
      case kModReg:
        return numBytes;
      case kModDisp8:
        numBytes += 1;
        break;
      case kModDisp32:
        numBytes += 4;
        break;
      case kModNoRegDisp:
        if ((*aModRm & kMaskRm) == kRmNoRegDispDisp32) {
#if defined(_M_X64)
          // RIP-relative on AMD64, currently unsupported
          return -1;
#else
          // On IA-32, all ModR/M instruction modes address memory relative to 0
          numBytes += 4;
#endif
        } else if (((*aModRm & kMaskRm) == kRmNeedSib &&
             (*(aModRm + 1) & kMaskSibBase) == kSibBaseEbp)) {
          numBytes += 4;
        }
        break;
      default:
        // This should not be reachable
        MOZ_ASSERT_UNREACHABLE("Impossible value for modr/m byte mod bits");
        return -1;
    }
    if ((*aModRm & kMaskRm) == kRmNeedSib) {
      // SIB byte
      numBytes += 1;
    }
    if (aSubOpcode) {
      *aSubOpcode = (*aModRm & kMaskReg) >> kRegFieldShift;
    }
    return numBytes;
  }

#if defined(_M_X64)
  // To patch for JMP and JE

  enum JumpType {
   Je,
   Jmp
  };

  struct JumpPatch {
    JumpPatch()
      : mHookOffset(0), mJumpAddress(0), mType(JumpType::Jmp)
    {
    }

    JumpPatch(size_t aOffset, intptr_t aAddress, JumpType aType = JumpType::Jmp)
      : mHookOffset(aOffset), mJumpAddress(aAddress), mType(aType)
    {
    }

    void AddJumpPatch(size_t aHookOffset, intptr_t aAbsJumpAddress,
                     JumpType aType = JumpType::Jmp)
    {
      mHookOffset = aHookOffset;
      mJumpAddress = aAbsJumpAddress;
      mType = aType;
    }

    size_t GenerateJump(uint8_t* aCode)
    {
      size_t offset = mHookOffset;
      if (mType == JumpType::Je) {
        // JNE RIP+14
        aCode[offset]     = 0x75;
        aCode[offset + 1] = 14;
        offset += 2;
      }

      // JMP [RIP+0]
      aCode[offset] = 0xff;
      aCode[offset + 1] = 0x25;
      *reinterpret_cast<int32_t*>(aCode + offset + 2) = 0;

      // Jump table
      *reinterpret_cast<int64_t*>(aCode + offset + 2 + 4) = mJumpAddress;

      return offset + 2 + 4 + 8;
    }

    bool HasJumpPatch() const
    {
      return !!mJumpAddress;
    }

    size_t mHookOffset;
    intptr_t mJumpAddress;
    JumpType mType;
  };

#endif

  enum ePrefixGroupBits
  {
    eNoPrefixes = 0,
    ePrefixGroup1 = (1 << 0),
    ePrefixGroup2 = (1 << 1),
    ePrefixGroup3 = (1 << 2),
    ePrefixGroup4 = (1 << 3)
  };

  int CountPrefixBytes(byteptr_t aBytes, const int aBytesIndex,
                       unsigned char* aOutGroupBits)
  {
    unsigned char& groupBits = *aOutGroupBits;
    groupBits = eNoPrefixes;
    int index = aBytesIndex;
    while (true) {
      switch (aBytes[index]) {
        // Group 1
        case 0xF0: // LOCK
        case 0xF2: // REPNZ
        case 0xF3: // REP / REPZ
          if (groupBits & ePrefixGroup1) {
            return -1;
          }
          groupBits |= ePrefixGroup1;
          ++index;
          break;

        // Group 2
        case 0x2E: // CS override / branch not taken
        case 0x36: // SS override
        case 0x3E: // DS override / branch taken
        case 0x64: // FS override
        case 0x65: // GS override
          if (groupBits & ePrefixGroup2) {
            return -1;
          }
          groupBits |= ePrefixGroup2;
          ++index;
          break;

        // Group 3
        case 0x66: // operand size override
          if (groupBits & ePrefixGroup3) {
            return -1;
          }
          groupBits |= ePrefixGroup3;
          ++index;
          break;

        // Group 4
        case 0x67: // Address size override
          if (groupBits & ePrefixGroup4) {
            return -1;
          }
          groupBits |= ePrefixGroup4;
          ++index;
          break;

        default:
          return index - aBytesIndex;
      }
    }
  }

  void CreateTrampoline(void* aOrigFunction, intptr_t aDest, void** aOutTramp)
  {
    *aOutTramp = nullptr;

    byteptr_t tramp = FindTrampolineSpace();
    if (!tramp) {
      return;
    }

    byteptr_t origBytes = (byteptr_t)aOrigFunction;

    int nBytes = 0;

#if defined(_M_IX86)
    int pJmp32 = -1;
    while (nBytes < 5) {
      // Understand some simple instructions that might be found in a
      // prologue; we might need to extend this as necessary.
      //
      // Note!  If we ever need to understand jump instructions, we'll
      // need to rewrite the displacement argument.
      unsigned char prefixGroups;
      int numPrefixBytes = CountPrefixBytes(origBytes, nBytes, &prefixGroups);
      if (numPrefixBytes < 0 || (prefixGroups & (ePrefixGroup3 | ePrefixGroup4))) {
        // Either the prefix sequence was bad, or there are prefixes that
        // we don't currently support (groups 3 and 4)
        return;
      }
      nBytes += numPrefixBytes;
      if (origBytes[nBytes] >= 0x88 && origBytes[nBytes] <= 0x8B) {
        // various MOVs
        ++nBytes;
        int len = CountModRmSib(origBytes + nBytes);
        if (len < 0) {
          return;
        }
        nBytes += len;
      } else if (origBytes[nBytes] == 0xA1) {
        // MOV eax, [seg:offset]
        nBytes += 5;
      } else if (origBytes[nBytes] == 0xB8) {
        // MOV 0xB8: http://ref.x86asm.net/coder32.html#xB8
        nBytes += 5;
      } else if (origBytes[nBytes] == 0x83) {
        // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r/m, imm8
        unsigned char b = origBytes[nBytes + 1];
        if ((b & 0xc0) == 0xc0) {
          // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r, imm8
          nBytes += 3;
        } else {
          // bail
          return;
        }
      } else if (origBytes[nBytes] == 0x68) {
        // PUSH with 4-byte operand
        nBytes += 5;
      } else if ((origBytes[nBytes] & 0xf0) == 0x50) {
        // 1-byte PUSH/POP
        nBytes++;
      } else if (origBytes[nBytes] == 0x6A) {
        // PUSH imm8
        nBytes += 2;
      } else if (origBytes[nBytes] == 0xe9) {
        pJmp32 = nBytes;
        // jmp 32bit offset
        nBytes += 5;
      } else if (origBytes[nBytes] == 0xff && origBytes[nBytes + 1] == 0x25) {
        // jmp [disp32]
        nBytes += 6;
      } else {
        //printf ("Unknown x86 instruction byte 0x%02x, aborting trampoline\n", origBytes[nBytes]);
        return;
      }
    }
#elif defined(_M_X64)
    JumpPatch jump;

    while (nBytes < 13) {

      // if found JMP 32bit offset, next bytes must be NOP or INT3
      if (jump.HasJumpPatch()) {
        if (origBytes[nBytes] == 0x90 || origBytes[nBytes] == 0xcc) {
          nBytes++;
          continue;
        }
        return;
      }
      if (origBytes[nBytes] == 0x0f) {
        nBytes++;
        if (origBytes[nBytes] == 0x1f) {
          // nop (multibyte)
          nBytes++;
          if ((origBytes[nBytes] & 0xc0) == 0x40 &&
              (origBytes[nBytes] & 0x7) == 0x04) {
            nBytes += 3;
          } else {
            return;
          }
        } else if (origBytes[nBytes] == 0x05) {
          // syscall
          nBytes++;
        } else if (origBytes[nBytes] == 0x84) {
          // je rel32
          jump.AddJumpPatch(nBytes - 1,
                            (intptr_t)
                              origBytes + nBytes + 5 +
                            *(reinterpret_cast<int32_t*>(origBytes +
                                                         nBytes + 1)),
                            JumpType::Je);
          nBytes += 5;
        } else {
          return;
        }
      } else if (origBytes[nBytes] == 0x40 ||
                 origBytes[nBytes] == 0x41) {
        // Plain REX or REX.B
        nBytes++;

        if ((origBytes[nBytes] & 0xf0) == 0x50) {
          // push/pop with Rx register
          nBytes++;
        } else if (origBytes[nBytes] >= 0xb8 && origBytes[nBytes] <= 0xbf) {
          // mov r32, imm32
          nBytes += 5;
        } else {
          return;
        }
      } else if (origBytes[nBytes] == 0x45) {
        // REX.R & REX.B
        nBytes++;

        if (origBytes[nBytes] == 0x33) {
          // xor r32, r32
          nBytes += 2;
        } else {
          return;
        }
      } else if ((origBytes[nBytes] & 0xfb) == 0x48) {
        // REX.W | REX.WR
        nBytes++;

        if (origBytes[nBytes] == 0x81 &&
            (origBytes[nBytes + 1] & 0xf8) == 0xe8) {
          // sub r, dword
          nBytes += 6;
        } else if (origBytes[nBytes] == 0x83 &&
                   (origBytes[nBytes + 1] & 0xf8) == 0xe8) {
          // sub r, byte
          nBytes += 3;
        } else if (origBytes[nBytes] == 0x83 &&
                   (origBytes[nBytes + 1] & 0xf8) == 0x60) {
          // and [r+d], imm8
          nBytes += 5;
        } else if (origBytes[nBytes] == 0x85) {
          // 85 /r => TEST r/m32, r32
          if ((origBytes[nBytes + 1] & 0xc0) == 0xc0) {
            nBytes += 2;
          } else {
            return;
          }
        } else if ((origBytes[nBytes] & 0xfd) == 0x89) {
          ++nBytes;
          // MOV r/m64, r64 | MOV r64, r/m64
          int len = CountModRmSib(origBytes + nBytes);
          if (len < 0) {
            return;
          }
          nBytes += len;
        } else if (origBytes[nBytes] == 0xc7) {
          // MOV r/m64, imm32
          if (origBytes[nBytes + 1] == 0x44) {
            // MOV [r64+disp8], imm32
            // ModR/W + SIB + disp8 + imm32
            nBytes += 8;
          } else {
            return;
          }
        } else if (origBytes[nBytes] == 0xff) {
          // JMP /4
          if ((origBytes[nBytes + 1] & 0xc0) == 0x0 &&
              (origBytes[nBytes + 1] & 0x07) == 0x5) {
            // [rip+disp32]
            // convert JMP 32bit offset to JMP 64bit direct
            jump.AddJumpPatch(nBytes - 1,
                              *reinterpret_cast<intptr_t*>(
                                origBytes + nBytes + 6 +
                              *reinterpret_cast<int32_t*>(origBytes + nBytes +
                                                          2)));
            nBytes += 6;
          } else {
            // not support yet!
            return;
          }
        } else {
          // not support yet!
          return;
        }
      } else if (origBytes[nBytes] == 0x66) {
        // operand override prefix
        nBytes += 1;
        // This is the same as the x86 version
        if (origBytes[nBytes] >= 0x88 && origBytes[nBytes] <= 0x8B) {
          // various MOVs
          unsigned char b = origBytes[nBytes + 1];
          if (((b & 0xc0) == 0xc0) ||
              (((b & 0xc0) == 0x00) &&
               ((b & 0x07) != 0x04) && ((b & 0x07) != 0x05))) {
            // REG=r, R/M=r or REG=r, R/M=[r]
            nBytes += 2;
          } else if ((b & 0xc0) == 0x40) {
            if ((b & 0x07) == 0x04) {
              // REG=r, R/M=[SIB + disp8]
              nBytes += 4;
            } else {
              // REG=r, R/M=[r + disp8]
              nBytes += 3;
            }
          } else {
            // complex MOV, bail
            return;
          }
        }
      } else if ((origBytes[nBytes] & 0xf0) == 0x50) {
        // 1-byte push/pop
        nBytes++;
      } else if (origBytes[nBytes] == 0x65) {
        // GS prefix
        //
        // The entry of GetKeyState on Windows 10 has the following code.
        // 65 48 8b 04 25 30 00 00 00    mov   rax,qword ptr gs:[30h]
        // (GS prefix + REX + MOV (0x8b) ...)
        if (origBytes[nBytes + 1] == 0x48 &&
            (origBytes[nBytes + 2] >= 0x88 && origBytes[nBytes + 2] <= 0x8b)) {
          nBytes += 3;
          int len = CountModRmSib(origBytes + nBytes);
          if (len < 0) {
            // no way to support this yet.
            return;
          }
          nBytes += len;
        } else {
          return;
        }
      } else if (origBytes[nBytes] == 0x90) {
        // nop
        nBytes++;
      } else if (origBytes[nBytes] == 0xb8) {
        // MOV 0xB8: http://ref.x86asm.net/coder32.html#xB8
        nBytes += 5;
      } else if (origBytes[nBytes] == 0x33) {
        // xor r32, r/m32
        nBytes += 2;
      } else if (origBytes[nBytes] == 0xf6) {
        // test r/m8, imm8 (used by ntdll on Windows 10 x64)
        // (no flags are affected by near jmp since there is no task switch,
        // so it is ok for a jmp to be written immediately after a test)
        BYTE subOpcode = 0;
        int nModRmSibBytes = CountModRmSib(&origBytes[nBytes + 1], &subOpcode);
        if (nModRmSibBytes < 0 || subOpcode != 0) {
          // Unsupported
          return;
        }
        nBytes += 2 + nModRmSibBytes;
      } else if (origBytes[nBytes] == 0xc3) {
        // ret
        nBytes++;
      } else if (origBytes[nBytes] == 0xcc) {
        // int 3
        nBytes++;
      } else if (origBytes[nBytes] == 0xe9) {
        // jmp 32bit offset
        jump.AddJumpPatch(nBytes,
                          // convert JMP 32bit offset to JMP 64bit direct
                          (intptr_t)
                            origBytes + nBytes + 5 +
                          *(reinterpret_cast<int32_t*>(origBytes + nBytes + 1)));
        nBytes += 5;
      } else if (origBytes[nBytes] == 0xff) {
        nBytes++;
        if ((origBytes[nBytes] & 0xf8) == 0xf0) {
          // push r64
          nBytes++;
        } else {
          return;
        }
      } else {
        return;
      }
    }
#else
#error "Unknown processor type"
#endif

    if (nBytes > 100) {
      //printf ("Too big!");
      return;
    }

    // We keep the address of the original function in the first bytes of
    // the trampoline buffer
    *((void**)tramp) = aOrigFunction;
    tramp += sizeof(void*);

    memcpy(tramp, aOrigFunction, nBytes);

    // OrigFunction+N, the target of the trampoline
    byteptr_t trampDest = origBytes + nBytes;

#if defined(_M_IX86)
    if (pJmp32 >= 0) {
      // Jump directly to the original target of the jump instead of jumping to the
      // original function.
      // Adjust jump target displacement to jump location in the trampoline.
      *((intptr_t*)(tramp + pJmp32 + 1)) += origBytes - tramp;
    } else {
      tramp[nBytes] = 0xE9; // jmp
      *((intptr_t*)(tramp + nBytes + 1)) =
        (intptr_t)trampDest - (intptr_t)(tramp + nBytes + 5); // target displacement
    }
#elif defined(_M_X64)
    // If JMP/JE opcode found, we don't insert to trampoline jump
    if (jump.HasJumpPatch()) {
      size_t offset = jump.GenerateJump(tramp);
      if (jump.mType != JumpType::Jmp) {
        JumpPatch patch(offset, reinterpret_cast<intptr_t>(trampDest));
        patch.GenerateJump(tramp);
      }
    } else {
      JumpPatch patch(nBytes, reinterpret_cast<intptr_t>(trampDest));
      patch.GenerateJump(tramp);
    }
#endif

    // The trampoline is now valid.
    *aOutTramp = tramp;

    // ensure we can modify the original code
    AutoVirtualProtect protect(aOrigFunction, nBytes, PAGE_EXECUTE_READWRITE);
    if (!protect.Protect()) {
      //printf ("VirtualProtectEx failed! %d\n", GetLastError());
      return;
    }

#if defined(_M_IX86)
    // now modify the original bytes
    origBytes[0] = 0xE9; // jmp
    *((intptr_t*)(origBytes + 1)) =
      aDest - (intptr_t)(origBytes + 5); // target displacement
#elif defined(_M_X64)
    // mov r11, address
    origBytes[0] = 0x49;
    origBytes[1] = 0xbb;

    *((intptr_t*)(origBytes + 2)) = aDest;

    // jmp r11
    origBytes[10] = 0x41;
    origBytes[11] = 0xff;
    origBytes[12] = 0xe3;
#endif
  }

  byteptr_t FindTrampolineSpace()
  {
    if (mCurHooks >= mMaxHooks) {
      return 0;
    }

    byteptr_t p = mHookPage + mCurHooks * kHookSize;

    mCurHooks++;

    return p;
  }

  static void* ResolveRedirectedAddress(const byteptr_t aOriginalFunction)
  {
#if defined(_M_IX86)
    // If function entry is jmp [disp32] such as used by kernel32,
    // we resolve redirected address from import table.
    if (aOriginalFunction[0] == 0xff && aOriginalFunction[1] == 0x25) {
      return (void*)(**((uint32_t**) (aOriginalFunction + 2)));
    }
#elif defined(_M_X64)
    if (aOriginalFunction[0] == 0xe9) {
      // require for TestDllInterceptor with --disable-optimize
      int32_t offset = *((int32_t*)(aOriginalFunction + 1));
      return aOriginalFunction + 5 + offset;
    }
#endif

    return aOriginalFunction;
  }
};

} // namespace internal

class WindowsDllInterceptor
{
  internal::WindowsDllNopSpacePatcher mNopSpacePatcher;
  internal::WindowsDllDetourPatcher mDetourPatcher;

  const char* mModuleName;
  int mNHooks;

public:
  WindowsDllInterceptor()
    : mModuleName(nullptr)
    , mNHooks(0)
  {}

  void Init(const char* aModuleName, int aNumHooks = 0)
  {
    if (mModuleName) {
      return;
    }

    mModuleName = aModuleName;
    mNHooks = aNumHooks;
    mNopSpacePatcher.Init(aModuleName);

    // Lazily initialize mDetourPatcher, since it allocates memory and we might
    // not need it.
  }

  void LockHooks()
  {
    if (mDetourPatcher.Initialized()) {
      mDetourPatcher.LockHooks();
    }
  }

  bool AddHook(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    // Use a nop space patch if possible, otherwise fall back to a detour.
    // This should be the preferred method for adding hooks.

    if (!mModuleName) {
      return false;
    }

    if (mNopSpacePatcher.AddHook(aName, aHookDest, aOrigFunc)) {
      return true;
    }

    return AddDetour(aName, aHookDest, aOrigFunc);
  }

  bool AddDetour(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    // Generally, code should not call this method directly. Use AddHook unless
    // there is a specific need to avoid nop space patches.

    if (!mModuleName) {
      return false;
    }

    if (!mDetourPatcher.Initialized()) {
      mDetourPatcher.Init(mModuleName, mNHooks);
    }

    return mDetourPatcher.AddHook(aName, aHookDest, aOrigFunc);
  }
};

} // namespace mozilla

#endif /* NS_WINDOWS_DLL_INTERCEPTOR_H_ */
