/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_WINDOWS_DLL_INTERCEPTOR_H_
#define NS_WINDOWS_DLL_INTERCEPTOR_H_
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

class WindowsDllNopSpacePatcher
{
  typedef unsigned char *byteptr_t;
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

  ~WindowsDllNopSpacePatcher()
  {
    // Restore the mov edi, edi to the beginning of each function we patched.

    for (int i = 0; i < mPatchedFnsLen; i++) {
      byteptr_t fn = mPatchedFns[i];

      // Ensure we can write to the code.
      DWORD op;
      if (!VirtualProtectEx(GetCurrentProcess(), fn, 2, PAGE_EXECUTE_READWRITE, &op)) {
        // printf("VirtualProtectEx failed! %d\n", GetLastError());
        continue;
      }

      // mov edi, edi
      *((uint16_t*)fn) = 0xff8b;

      // Restore the old protection.
      VirtualProtectEx(GetCurrentProcess(), fn, 2, op, &op);

      // I don't think this is actually necessary, but it can't hurt.
      FlushInstructionCache(GetCurrentProcess(),
                            /* ignored */ nullptr,
                            /* ignored */ 0);
    }
  }

  void Init(const char *modulename)
  {
    mModule = LoadLibraryExA(modulename, nullptr, 0);
    if (!mModule) {
      //printf("LoadLibraryEx for '%s' failed\n", modulename);
      return;
    }
  }

#if defined(_M_IX86)
  bool AddHook(const char *pname, intptr_t hookDest, void **origFunc)
  {
    if (!mModule)
      return false;

    if (mPatchedFnsLen == maxPatchedFns) {
      // printf ("No space for hook in mPatchedFns.\n");
      return false;
    }

    byteptr_t fn = reinterpret_cast<byteptr_t>(GetProcAddress(mModule, pname));
    if (!fn) {
      //printf ("GetProcAddress failed\n");
      return false;
    }
  
    // Ensure we can read and write starting at fn - 5 (for the long jmp we're
    // going to write) and ending at fn + 2 (for the short jmp up to the long
    // jmp).
    DWORD op;
    if (!VirtualProtectEx(GetCurrentProcess(), fn - 5, 7, PAGE_EXECUTE_READWRITE, &op)) {
      //printf ("VirtualProtectEx failed! %d\n", GetLastError());
      return false;
    }

    bool rv = WriteHook(fn, hookDest, origFunc);
    
    // Re-protect, and we're done.
    VirtualProtectEx(GetCurrentProcess(), fn - 5, 7, op, &op);

    if (rv) {
      mPatchedFns[mPatchedFnsLen] = fn;
      mPatchedFnsLen++;
    }

    return rv;
  }

  bool WriteHook(byteptr_t fn, intptr_t hookDest, void **origFunc)
  {
    // Check that the 5 bytes before fn are NOP's or INT 3's,
    // and that the 2 bytes after fn are mov(edi, edi).
    //
    // It's safe to read fn[-5] because we set it to PAGE_EXECUTE_READWRITE
    // before calling WriteHook.

    for (int i = -5; i <= -1; i++) {
      if (fn[i] != 0x90 && fn[i] != 0xcc) // nop or int 3
        return false;
    }

    // mov edi, edi.  Yes, there are two ways to encode the same thing:
    //
    //   0x89ff == mov r/m, r
    //   0x8bff == mov r, r/m
    //
    // where "r" is register and "r/m" is register or memory.  Windows seems to
    // use 8bff; I include 89ff out of paranoia.
    if ((fn[0] != 0x8b && fn[0] != 0x89) || fn[1] != 0xff) {
      return false;
    }

    // Write a long jump into the space above the function.
    fn[-5] = 0xe9; // jmp
    *((intptr_t*)(fn - 4)) = hookDest - (uintptr_t)(fn); // target displacement

    // Set origFunc here, because after this point, hookDest might be called,
    // and hookDest might use the origFunc pointer.
    *origFunc = fn + 2;

    // Short jump up into our long jump.
    *((uint16_t*)(fn)) = 0xf9eb; // jmp $-5

    // I think this routine is safe without this, but it can't hurt.
    FlushInstructionCache(GetCurrentProcess(),
                          /* ignored */ nullptr,
                          /* ignored */ 0);

    return true;
  }
#else
  bool AddHook(const char *pname, intptr_t hookDest, void **origFunc)
  {
    // Not implemented except on x86-32.
    return false;
  }
#endif
};

class WindowsDllDetourPatcher
{
  typedef unsigned char *byteptr_t;
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
      byteptr_t origBytes = *((byteptr_t *)p);
      // ensure we can modify the original code
      DWORD op;
      if (!VirtualProtectEx(GetCurrentProcess(), origBytes, nBytes, PAGE_EXECUTE_READWRITE, &op)) {
        //printf ("VirtualProtectEx failed! %d\n", GetLastError());
        continue;
      }
      // Remove the hook by making the original function jump directly
      // in the trampoline.
      intptr_t dest = (intptr_t)(p + sizeof(void *));
#if defined(_M_IX86)
      *((intptr_t*)(origBytes+1)) = dest - (intptr_t)(origBytes+5); // target displacement
#elif defined(_M_X64)
      *((intptr_t*)(origBytes+2)) = dest;
#else
#error "Unknown processor type"
#endif
      // restore protection; if this fails we can't really do anything about it
      VirtualProtectEx(GetCurrentProcess(), origBytes, nBytes, op, &op);
    }
  }

  void Init(const char *modulename, int nhooks = 0)
  {
    if (mModule)
      return;

    mModule = LoadLibraryExA(modulename, nullptr, 0);
    if (!mModule) {
      //printf("LoadLibraryEx for '%s' failed\n", modulename);
      return;
    }

    int hooksPerPage = 4096 / kHookSize;
    if (nhooks == 0)
      nhooks = hooksPerPage;

    mMaxHooks = nhooks + (hooksPerPage % nhooks);

    mHookPage = (byteptr_t) VirtualAllocEx(GetCurrentProcess(), nullptr,
             mMaxHooks * kHookSize,
             MEM_COMMIT | MEM_RESERVE,
             PAGE_EXECUTE_READWRITE);

    if (!mHookPage) {
      mModule = 0;
      return;
    }
  }

  bool Initialized()
  {
    return !!mModule;
  }

  void LockHooks()
  {
    if (!mModule)
      return;

    DWORD op;
    VirtualProtectEx(GetCurrentProcess(), mHookPage, mMaxHooks * kHookSize, PAGE_EXECUTE_READ, &op);

    mModule = 0;
  }

  bool AddHook(const char *pname, intptr_t hookDest, void **origFunc)
  {
    if (!mModule)
      return false;

    void *pAddr = (void *) GetProcAddress(mModule, pname);
    if (!pAddr) {
      //printf ("GetProcAddress failed\n");
      return false;
    }

    CreateTrampoline(pAddr, hookDest, origFunc);
    if (!*origFunc) {
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

  void CreateTrampoline(void *origFunction,
                        intptr_t dest,
                        void **outTramp)
  {
    *outTramp = nullptr;

    byteptr_t tramp = FindTrampolineSpace();
    if (!tramp)
      return;

    byteptr_t origBytes = (byteptr_t) origFunction;

    int nBytes = 0;
    int pJmp32 = -1;

#if defined(_M_IX86)
    while (nBytes < 5) {
      // Understand some simple instructions that might be found in a
      // prologue; we might need to extend this as necessary.
      //
      // Note!  If we ever need to understand jump instructions, we'll
      // need to rewrite the displacement argument.
      if (origBytes[nBytes] >= 0x88 && origBytes[nBytes] <= 0x8B) {
        // various MOVs
        unsigned char b = origBytes[nBytes+1];
        if (((b & 0xc0) == 0xc0) ||
            (((b & 0xc0) == 0x00) &&
             ((b & 0x07) != 0x04) && ((b & 0x07) != 0x05)))
        {
          // REG=r, R/M=r or REG=r, R/M=[r]
          nBytes += 2;
        } else if (((b & 0xc0) == 0x40) && ((b & 0x38) != 0x20)) {
          // REG=r, R/M=[r + disp8]
          nBytes += 3;
        } else {
          // complex MOV, bail
          return;
        }
      } else if (origBytes[nBytes] == 0xB8) {
        // MOV 0xB8: http://ref.x86asm.net/coder32.html#xB8
        nBytes += 5;
      } else if (origBytes[nBytes] == 0x83) {
        // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r/m, imm8
        unsigned char b = origBytes[nBytes+1];
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
      } else {
        //printf ("Unknown x86 instruction byte 0x%02x, aborting trampoline\n", origBytes[nBytes]);
        return;
      }
    }
#elif defined(_M_X64)
    byteptr_t directJmpAddr;

    while (nBytes < 13) {

      // if found JMP 32bit offset, next bytes must be NOP 
      if (pJmp32 >= 0) {
        if (origBytes[nBytes++] != 0x90)
          return;

        continue;
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
        } else {
          return;
        }
      } else if (origBytes[nBytes] == 0x41) {
        // REX.B
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

        if (origBytes[nBytes] == 0x81 && (origBytes[nBytes+1] & 0xf8) == 0xe8) {
          // sub r, dword
          nBytes += 6;
        } else if (origBytes[nBytes] == 0x83 &&
                  (origBytes[nBytes+1] & 0xf8) == 0xe8) {
          // sub r, byte
          nBytes += 3;
        } else if (origBytes[nBytes] == 0x83 &&
                  (origBytes[nBytes+1] & 0xf8) == 0x60) {
          // and [r+d], imm8
          nBytes += 5;
        } else if ((origBytes[nBytes] & 0xfd) == 0x89) {
          // MOV r/m64, r64 | MOV r64, r/m64
          if ((origBytes[nBytes+1] & 0xc0) == 0x40) {
            if ((origBytes[nBytes+1] & 0x7) == 0x04) {
              // R/M=[SIB+disp8], REG=r64
              nBytes += 4;
            } else {
              // R/M=[r64+disp8], REG=r64
              nBytes += 3;
            }
          } else if (((origBytes[nBytes+1] & 0xc0) == 0xc0) ||
                     (((origBytes[nBytes+1] & 0xc0) == 0x00) &&
                      ((origBytes[nBytes+1] & 0x07) != 0x04) && ((origBytes[nBytes+1] & 0x07) != 0x05))) {
            // REG=r64, R/M=r64 or REG=r64, R/M=[r64]
            nBytes += 2;
          } else {
            // complex MOV
            return;
          }
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
          pJmp32 = nBytes - 1;
          // JMP /4
          if ((origBytes[nBytes+1] & 0xc0) == 0x0 &&
              (origBytes[nBytes+1] & 0x07) == 0x5) {
            // [rip+disp32]
            // convert JMP 32bit offset to JMP 64bit direct
            directJmpAddr = (byteptr_t)*((uint64_t*)(origBytes + nBytes + 6 + (*((int32_t*)(origBytes + nBytes + 2)))));
            nBytes += 6;
          } else {
            // not support yet!
            return;
          }
        } else {
          // not support yet!
          return;
        }
      } else if ((origBytes[nBytes] & 0xf0) == 0x50) {
        // 1-byte push/pop
        nBytes++;
      } else if (origBytes[nBytes] == 0x90) {
        // nop
        nBytes++;
      } else if (origBytes[nBytes] == 0xb8) {
        // MOV 0xB8: http://ref.x86asm.net/coder32.html#xB8
        nBytes += 5;
      } else if (origBytes[nBytes] == 0xc3) {
        // ret
        nBytes++;
      } else if (origBytes[nBytes] == 0xe9) {
        pJmp32 = nBytes;
        // convert JMP 32bit offset to JMP 64bit direct
        directJmpAddr = origBytes + pJmp32 + 5 + (*((int32_t*)(origBytes + pJmp32 + 1)));
        // jmp 32bit offset
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
    *((void **)tramp) = origFunction;
    tramp += sizeof(void *);

    memcpy(tramp, origFunction, nBytes);

    // OrigFunction+N, the target of the trampoline
    byteptr_t trampDest = origBytes + nBytes;

#if defined(_M_IX86)
    if (pJmp32 >= 0) {
      // Jump directly to the original target of the jump instead of jumping to the
      // original function.
      // Adjust jump target displacement to jump location in the trampoline.
      *((intptr_t*)(tramp+pJmp32+1)) += origBytes + pJmp32 - tramp;
    } else {
      tramp[nBytes] = 0xE9; // jmp
      *((intptr_t*)(tramp+nBytes+1)) = (intptr_t)trampDest - (intptr_t)(tramp+nBytes+5); // target displacement
    }
#elif defined(_M_X64)
    // If JMP32 opcode found, we don't insert to trampoline jump 
    if (pJmp32 >= 0) {
      // mov r11, address
      tramp[pJmp32]   = 0x49;
      tramp[pJmp32+1] = 0xbb;
      *((intptr_t*)(tramp+pJmp32+2)) = (intptr_t)directJmpAddr;

      // jmp r11
      tramp[pJmp32+10] = 0x41;
      tramp[pJmp32+11] = 0xff;
      tramp[pJmp32+12] = 0xe3;
    } else {
      // mov r11, address
      tramp[nBytes] = 0x49;
      tramp[nBytes+1] = 0xbb;
      *((intptr_t*)(tramp+nBytes+2)) = (intptr_t)trampDest;

      // jmp r11
      tramp[nBytes+10] = 0x41;
      tramp[nBytes+11] = 0xff;
      tramp[nBytes+12] = 0xe3;
    }
#endif

    // The trampoline is now valid.
    *outTramp = tramp;

    // ensure we can modify the original code
    DWORD op;
    if (!VirtualProtectEx(GetCurrentProcess(), origFunction, nBytes, PAGE_EXECUTE_READWRITE, &op)) {
      //printf ("VirtualProtectEx failed! %d\n", GetLastError());
      return;
    }

#if defined(_M_IX86)
    // now modify the original bytes
    origBytes[0] = 0xE9; // jmp
    *((intptr_t*)(origBytes+1)) = dest - (intptr_t)(origBytes+5); // target displacement
#elif defined(_M_X64)
    // mov r11, address
    origBytes[0] = 0x49;
    origBytes[1] = 0xbb;

    *((intptr_t*)(origBytes+2)) = dest;

    // jmp r11
    origBytes[10] = 0x41;
    origBytes[11] = 0xff;
    origBytes[12] = 0xe3;
#endif

    // restore protection; if this fails we can't really do anything about it
    VirtualProtectEx(GetCurrentProcess(), origFunction, nBytes, op, &op);
  }

  byteptr_t FindTrampolineSpace()
  {
    if (mCurHooks >= mMaxHooks)
      return 0;

    byteptr_t p = mHookPage + mCurHooks*kHookSize;

    mCurHooks++;

    return p;
  }
};

} // namespace internal

class WindowsDllInterceptor
{
  internal::WindowsDllNopSpacePatcher mNopSpacePatcher;
  internal::WindowsDllDetourPatcher mDetourPatcher;

  const char *mModuleName;
  int mNHooks;

public:
  WindowsDllInterceptor()
    : mModuleName(nullptr)
    , mNHooks(0)
  {}

  void Init(const char *moduleName, int nhooks = 0)
  {
    if (mModuleName) {
      return;
    }

    mModuleName = moduleName;
    mNHooks = nhooks;
    mNopSpacePatcher.Init(moduleName);

    // Lazily initialize mDetourPatcher, since it allocates memory and we might
    // not need it.
  }

  void LockHooks()
  {
    if (mDetourPatcher.Initialized())
      mDetourPatcher.LockHooks();
  }

  bool AddHook(const char *pname, intptr_t hookDest, void **origFunc)
  {
    if (!mModuleName) {
      // printf("AddHook before initialized?\n");
      return false;
    }

    if (mNopSpacePatcher.AddHook(pname, hookDest, origFunc)) {
      // printf("nopSpacePatcher succeeded.\n");
      return true;
    }

    if (!mDetourPatcher.Initialized()) {
      // printf("Initializing detour patcher.\n");
      mDetourPatcher.Init(mModuleName, mNHooks);
    }

    bool rv = mDetourPatcher.AddHook(pname, hookDest, origFunc);
    // printf("detourPatcher returned %d\n", rv);
    return rv;
  }
};

} // namespace mozilla

#endif /* NS_WINDOWS_DLL_INTERCEPTOR_H_ */
