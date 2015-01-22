// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of PreamblePatcher

#include "sandbox/win/src/sidestep/preamble_patcher.h"

#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sidestep/mini_disassembler.h"

// Definitions of assembly statements we need
#define ASM_JMP32REL 0xE9
#define ASM_INT3 0xCC

namespace {

// Very basic memcpy. We are copying 4 to 12 bytes most of the time, so there
// is no attempt to optimize this code or have a general purpose function.
// We don't want to call the crt from this code.
inline void* RawMemcpy(void* destination, const void* source, size_t bytes) {
  const char* from = reinterpret_cast<const char*>(source);
  char* to = reinterpret_cast<char*>(destination);

  for (size_t i = 0; i < bytes ; i++)
    to[i] = from[i];

  return destination;
}

// Very basic memset. We are filling 1 to 7 bytes most of the time, so there
// is no attempt to optimize this code or have a general purpose function.
// We don't want to call the crt from this code.
inline void* RawMemset(void* destination, int value, size_t bytes) {
  char* to = reinterpret_cast<char*>(destination);

  for (size_t i = 0; i < bytes ; i++)
    to[i] = static_cast<char>(value);

  return destination;
}

}  // namespace

#define ASSERT(a, b) DCHECK_NT(a)

namespace sidestep {

SideStepError PreamblePatcher::RawPatchWithStub(
    void* target_function,
    void* replacement_function,
    unsigned char* preamble_stub,
    size_t stub_size,
    size_t* bytes_needed) {
  if ((NULL == target_function) ||
      (NULL == replacement_function) ||
      (NULL == preamble_stub)) {
    ASSERT(false, (L"Invalid parameters - either pTargetFunction or "
                   L"pReplacementFunction or pPreambleStub were NULL."));
    return SIDESTEP_INVALID_PARAMETER;
  }

  // TODO(V7:joi) Siggi and I just had a discussion and decided that both
  // patching and unpatching are actually unsafe.  We also discussed a
  // method of making it safe, which is to freeze all other threads in the
  // process, check their thread context to see if their eip is currently
  // inside the block of instructions we need to copy to the stub, and if so
  // wait a bit and try again, then unfreeze all threads once we've patched.
  // Not implementing this for now since we're only using SideStep for unit
  // testing, but if we ever use it for production code this is what we
  // should do.
  //
  // NOTE: Stoyan suggests we can write 8 or even 10 bytes atomically using
  // FPU instructions, and on newer processors we could use cmpxchg8b or
  // cmpxchg16b. So it might be possible to do the patching/unpatching
  // atomically and avoid having to freeze other threads.  Note though, that
  // doing it atomically does not help if one of the other threads happens
  // to have its eip in the middle of the bytes you change while you change
  // them.
  unsigned char* target = reinterpret_cast<unsigned char*>(target_function);

  // Let's disassemble the preamble of the target function to see if we can
  // patch, and to see how much of the preamble we need to take.  We need 5
  // bytes for our jmp instruction, so let's find the minimum number of
  // instructions to get 5 bytes.
  MiniDisassembler disassembler;
  unsigned int preamble_bytes = 0;
  while (preamble_bytes < 5) {
    InstructionType instruction_type =
      disassembler.Disassemble(target + preamble_bytes, &preamble_bytes);
    if (IT_JUMP == instruction_type) {
      ASSERT(false, (L"Unable to patch because there is a jump instruction "
                     L"in the first 5 bytes."));
      return SIDESTEP_JUMP_INSTRUCTION;
    } else if (IT_RETURN == instruction_type) {
      ASSERT(false, (L"Unable to patch because function is too short"));
      return SIDESTEP_FUNCTION_TOO_SMALL;
    } else if (IT_GENERIC != instruction_type) {
      ASSERT(false, (L"Disassembler encountered unsupported instruction "
                     L"(either unused or unknown"));
      return SIDESTEP_UNSUPPORTED_INSTRUCTION;
    }
  }

  if (NULL != bytes_needed)
    *bytes_needed = preamble_bytes + 5;

  // Inv: preamble_bytes is the number of bytes (at least 5) that we need to
  // take from the preamble to have whole instructions that are 5 bytes or more
  // in size total. The size of the stub required is cbPreamble + size of
  // jmp (5)
  if (preamble_bytes + 5 > stub_size) {
    NOTREACHED_NT();
    return SIDESTEP_INSUFFICIENT_BUFFER;
  }

  // First, copy the preamble that we will overwrite.
  RawMemcpy(reinterpret_cast<void*>(preamble_stub),
            reinterpret_cast<void*>(target), preamble_bytes);

  // Now, make a jmp instruction to the rest of the target function (minus the
  // preamble bytes we moved into the stub) and copy it into our preamble-stub.
  // find address to jump to, relative to next address after jmp instruction
#pragma warning(push)
#pragma warning(disable:4244)
  // This assignment generates a warning because it is 32 bit specific.
  int relative_offset_to_target_rest
    = ((reinterpret_cast<unsigned char*>(target) + preamble_bytes) -
        (preamble_stub + preamble_bytes + 5));
#pragma warning(pop)
  // jmp (Jump near, relative, displacement relative to next instruction)
  preamble_stub[preamble_bytes] = ASM_JMP32REL;
  // copy the address
  RawMemcpy(reinterpret_cast<void*>(preamble_stub + preamble_bytes + 1),
            reinterpret_cast<void*>(&relative_offset_to_target_rest), 4);

  // Inv: preamble_stub points to assembly code that will execute the
  // original function by first executing the first cbPreamble bytes of the
  // preamble, then jumping to the rest of the function.

  // Overwrite the first 5 bytes of the target function with a jump to our
  // replacement function.
  // (Jump near, relative, displacement relative to next instruction)
  target[0] = ASM_JMP32REL;

  // Find offset from instruction after jmp, to the replacement function.
#pragma warning(push)
#pragma warning(disable:4244)
  int offset_to_replacement_function =
    reinterpret_cast<unsigned char*>(replacement_function) -
    reinterpret_cast<unsigned char*>(target) - 5;
#pragma warning(pop)
  // complete the jmp instruction
  RawMemcpy(reinterpret_cast<void*>(target + 1),
            reinterpret_cast<void*>(&offset_to_replacement_function), 4);
  // Set any remaining bytes that were moved to the preamble-stub to INT3 so
  // as not to cause confusion (otherwise you might see some strange
  // instructions if you look at the disassembly, or even invalid
  // instructions). Also, by doing this, we will break into the debugger if
  // some code calls into this portion of the code.  If this happens, it
  // means that this function cannot be patched using this patcher without
  // further thought.
  if (preamble_bytes > 5) {
    RawMemset(reinterpret_cast<void*>(target + 5), ASM_INT3,
              preamble_bytes - 5);
  }

  // Inv: The memory pointed to by target_function now points to a relative
  // jump instruction that jumps over to the preamble_stub.  The preamble
  // stub contains the first stub_size bytes of the original target
  // function's preamble code, followed by a relative jump back to the next
  // instruction after the first cbPreamble bytes.

  return SIDESTEP_SUCCESS;
}

};  // namespace sidestep

#undef ASSERT
