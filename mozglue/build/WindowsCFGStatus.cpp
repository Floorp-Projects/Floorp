/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winternl.h>
#include <io.h>
#include <intrin.h>

#include <setjmp.h>

#include "WindowsCFGStatus.h"

// Inspired by https://github.com/trailofbits/cfg-showcase/blob/master/cfg_icall.cpp

jmp_buf env;

#pragma optimize("", off )
void not_entry_point() {
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();
  __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); __nop();

  longjmp(env, 1);
  return;
}
#pragma optimize("", on )

typedef void (*fn_ptr)();

/*
 * Tests for Microsoft's Control Flow Guard compiler security feature.
 * This function will crash if CFG is enabled, and return false if it is
 * not enabled.
 *
 * CFG protects indirect function calls and ensures they call a valid entry
 * point. We create a function pointer that calls an invalid entry point.
 * That invalid entry point is a nop sled.
 *
 * Jumping into the nop sled skips the preamble that a function normally
 * performs, so if we hit the return (ret) we would mess up the stack.
 * To 'return' from the function safely we jump back to our original
 * function - no preamble and no return.
 *
 * We use setjmp/longjmp because inline asm instructions aren't supported
 * in x64 by MSVC.
 */
MFBT_API bool
CFG_DisabledOrCrash()
{
  // setjmp returns 0 on the initial call and whatever value is given
  // to longjmp (here it is 1) when it is jumped back to.
  int val = setjmp(env);

  if (val == 0) {
    fn_ptr slide_to_the_left = (fn_ptr)((uintptr_t)(not_entry_point) + 0x20);

    // If CFG is enabled, we're going to crash on the next line
    slide_to_the_left();
  }

  return false;
}
