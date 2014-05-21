// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_INSTRUCTION_H__
#define SANDBOX_LINUX_SECCOMP_BPF_INSTRUCTION_H__

#include <stdint.h>

namespace sandbox {

// The fields in this structure have the same meaning as the corresponding
// fields in "struct sock_filter". See <linux/filter.h> for a lot more
// detail.
// code     -- Opcode of the instruction. This is typically a bitwise
//             combination BPF_XXX values.
// k        -- Operand; BPF instructions take zero or one operands. Operands
//             are 32bit-wide constants, if present. They can be immediate
//             values (if BPF_K is present in "code_"), addresses (if BPF_ABS
//             is present in "code_"), or relative jump offsets (if BPF_JMP
//             and BPF_JA are present in "code_").
// jt, jf   -- all conditional jumps have a 8bit-wide jump offset that allows
//             jumps of up to 256 instructions forward. Conditional jumps are
//             identified by BPF_JMP in "code_", but the lack of BPF_JA.
//             Conditional jumps have a "t"rue and "f"alse branch.
struct Instruction {
  // Constructor for an non-jumping instruction or for an unconditional
  // "always" jump.
  Instruction(uint16_t c, uint32_t parm, Instruction* n)
      : code(c), next(n), k(parm) {}

  // Constructor for a conditional jump instruction.
  Instruction(uint16_t c, uint32_t parm, Instruction* jt, Instruction* jf)
      : code(c), jt_ptr(jt), jf_ptr(jf), k(parm) {}

  uint16_t code;
  union {
    // When code generation is complete, we will have computed relative
    // branch targets that are in the range 0..255.
    struct {
      uint8_t jt, jf;
    };

    // While assembling the BPF program, we use pointers for branch targets.
    // Once we have computed basic blocks, these pointers will be entered as
    // keys in a TargetsToBlocks map and should no longer be dereferenced
    // directly.
    struct {
      Instruction* jt_ptr, *jf_ptr;
    };

    // While assembling the BPF program, non-jumping instructions are linked
    // by the "next_" pointer. This field is no longer needed when we have
    // computed basic blocks.
    Instruction* next;
  };
  uint32_t k;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_INSTRUCTION_H__
