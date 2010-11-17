/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corp
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NS_WINDOWS_DLL_INTERCEPTOR_H_
#define NS_WINDOWS_DLL_INTERCEPTOR_H_
#include <windows.h>
#include <winternl.h>

/*
 * Simple trampoline interception
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
 */

class WindowsDllInterceptor
{
  typedef unsigned char *byteptr_t;
public:
  WindowsDllInterceptor() 
    : mModule(0)
  {
  }

  WindowsDllInterceptor(const char *modulename, int nhooks = 0) {
    Init(modulename, nhooks);
  }

  void Init(const char *modulename, int nhooks = 0) {
    if (mModule)
      return;

    mModule = LoadLibraryExA(modulename, NULL, 0);
    if (!mModule) {
      //printf("LoadLibraryEx for '%s' failed\n", modulename);
      return;
    }

    int hooksPerPage = 4096 / kHookSize;
    if (nhooks == 0)
      nhooks = hooksPerPage;

    mMaxHooks = nhooks + (hooksPerPage % nhooks);
    mCurHooks = 0;

    mHookPage = (byteptr_t) VirtualAllocEx(GetCurrentProcess(), NULL, mMaxHooks * kHookSize,
             MEM_COMMIT | MEM_RESERVE,
             PAGE_EXECUTE_READWRITE);

    if (!mHookPage) {
      mModule = 0;
      return;
    }
  }

  void LockHooks() {
    if (!mModule)
      return;

    DWORD op;
    VirtualProtectEx(GetCurrentProcess(), mHookPage, mMaxHooks * kHookSize, PAGE_EXECUTE_READ, &op);

    mModule = 0;
  }

  bool AddHook(const char *pname,
         void *hookDest,
         void **origFunc)
  {
    if (!mModule)
      return false;

    void *pAddr = (void *) GetProcAddress(mModule, pname);
    if (!pAddr) {
      //printf ("GetProcAddress failed\n");
      return false;
    }

    void *tramp = CreateTrampoline(pAddr, hookDest);
    if (!tramp) {
      //printf ("CreateTrampoline failed\n");
      return false;
    }

    *origFunc = tramp;

    return true;
  }

protected:
  const static int kPageSize = 4096;
  const static int kHookSize = 128;

  HMODULE mModule;
  byteptr_t mHookPage;
  int mMaxHooks;
  int mCurHooks;

  byteptr_t CreateTrampoline(void *origFunction,
           void *dest)
  {
    byteptr_t tramp = FindTrampolineSpace();
    if (!tramp)
      return 0;

    byteptr_t origBytes = (byteptr_t) origFunction;

    int nBytes = 0;
    while (nBytes < 5) {
      // Understand some simple instructions that might be found in a
      // prologue; we might need to extend this as necessary.
      //
      // Note!  If we ever need to understand jump instructions, we'll
      // need to rewrite the displacement argument.
      if (origBytes[nBytes] >= 0x88 && origBytes[nBytes] <= 0x8B) {
        // various MOVs; but only handle the case where it truly is a 2-byte instruction
        unsigned char b = origBytes[nBytes+1];
        if (((b & 0xc0) == 0xc0) ||
            (((b & 0xc0) == 0x00) &&
             ((b & 0x38) != 0x20) && ((b & 0x38) != 0x28)))
        {
          nBytes += 2;
        } else {
          // complex MOV, bail
          return 0;
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
      } else {
        //printf ("Unknown x86 instruction byte 0x%02x, aborting trampoline\n", origBytes[nBytes]);
        return 0;
      }
    }

    if (nBytes > 100) {
      //printf ("Too big!");
      return 0;
    }

    memcpy(tramp, origFunction, nBytes);

    // OrigFunction+N, the target of the trampoline
    byteptr_t trampDest = origBytes + nBytes;

    tramp[nBytes] = 0xE9; // jmp
    *((intptr_t*)(tramp+nBytes+1)) = (intptr_t)trampDest - (intptr_t)(tramp+nBytes+5); // target displacement

    // ensure we can modify the original code
    DWORD op;
    if (!VirtualProtectEx(GetCurrentProcess(), origFunction, nBytes, PAGE_EXECUTE_READWRITE, &op)) {
      //printf ("VirtualProtectEx failed! %d\n", GetLastError());
      return 0;
    }

    // now modify the original bytes
    origBytes[0] = 0xE9; // jmp
    *((intptr_t*)(origBytes+1)) = (intptr_t)dest - (intptr_t)(origBytes+5); // target displacement

    // restore protection; if this fails we can't really do anything about it
    VirtualProtectEx(GetCurrentProcess(), origFunction, nBytes, op, &op);

    return tramp;
  }

  byteptr_t FindTrampolineSpace() {
    if (mCurHooks >= mMaxHooks)
      return 0;

    byteptr_t p = mHookPage + mCurHooks*kHookSize;

    mCurHooks++;

    return p;
  }
};


#endif /* NS_WINDOWS_DLL_INTERCEPTOR_H_ */
