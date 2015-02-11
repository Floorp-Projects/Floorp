// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/dump_bpf.h"

#include <stdio.h>

#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace sandbox {
namespace bpf_dsl {

void DumpBPF::PrintProgram(const CodeGen::Program& program) {
  for (CodeGen::Program::const_iterator iter = program.begin();
       iter != program.end();
       ++iter) {
    int ip = (int)(iter - program.begin());
    fprintf(stderr, "%3d) ", ip);
    switch (BPF_CLASS(iter->code)) {
      case BPF_LD:
        if (iter->code == BPF_LD + BPF_W + BPF_ABS) {
          fprintf(stderr, "LOAD %d  // ", (int)iter->k);
          if (iter->k == offsetof(struct arch_seccomp_data, nr)) {
            fprintf(stderr, "System call number\n");
          } else if (iter->k == offsetof(struct arch_seccomp_data, arch)) {
            fprintf(stderr, "Architecture\n");
          } else if (iter->k ==
                     offsetof(struct arch_seccomp_data, instruction_pointer)) {
            fprintf(stderr, "Instruction pointer (LSB)\n");
          } else if (iter->k ==
                     offsetof(struct arch_seccomp_data, instruction_pointer) +
                         4) {
            fprintf(stderr, "Instruction pointer (MSB)\n");
          } else if (iter->k >= offsetof(struct arch_seccomp_data, args) &&
                     iter->k < offsetof(struct arch_seccomp_data, args) + 48 &&
                     (iter->k - offsetof(struct arch_seccomp_data, args)) % 4 ==
                         0) {
            fprintf(
                stderr,
                "Argument %d (%cSB)\n",
                (int)(iter->k - offsetof(struct arch_seccomp_data, args)) / 8,
                (iter->k - offsetof(struct arch_seccomp_data, args)) % 8 ? 'M'
                                                                         : 'L');
          } else {
            fprintf(stderr, "???\n");
          }
        } else {
          fprintf(stderr, "LOAD ???\n");
        }
        break;
      case BPF_JMP:
        if (BPF_OP(iter->code) == BPF_JA) {
          fprintf(stderr, "JMP %d\n", ip + iter->k + 1);
        } else {
          fprintf(stderr, "if A %s 0x%x; then JMP %d else JMP %d\n",
              BPF_OP(iter->code) == BPF_JSET ? "&" :
              BPF_OP(iter->code) == BPF_JEQ ? "==" :
              BPF_OP(iter->code) == BPF_JGE ? ">=" :
              BPF_OP(iter->code) == BPF_JGT ? ">"  : "???",
              (int)iter->k,
              ip + iter->jt + 1, ip + iter->jf + 1);
        }
        break;
      case BPF_RET:
        fprintf(stderr, "RET 0x%x  // ", iter->k);
        if ((iter->k & SECCOMP_RET_ACTION) == SECCOMP_RET_TRAP) {
          fprintf(stderr, "Trap #%d\n", iter->k & SECCOMP_RET_DATA);
        } else if ((iter->k & SECCOMP_RET_ACTION) == SECCOMP_RET_ERRNO) {
          fprintf(stderr, "errno = %d\n", iter->k & SECCOMP_RET_DATA);
        } else if ((iter->k & SECCOMP_RET_ACTION) == SECCOMP_RET_TRACE) {
          fprintf(stderr, "Trace #%d\n", iter->k & SECCOMP_RET_DATA);
        } else if (iter->k == SECCOMP_RET_ALLOW) {
          fprintf(stderr, "Allowed\n");
        } else {
          fprintf(stderr, "???\n");
        }
        break;
      case BPF_ALU:
        fprintf(stderr, BPF_OP(iter->code) == BPF_NEG
            ? "A := -A\n" : "A := A %s 0x%x\n",
            BPF_OP(iter->code) == BPF_ADD ? "+"  :
            BPF_OP(iter->code) == BPF_SUB ? "-"  :
            BPF_OP(iter->code) == BPF_MUL ? "*"  :
            BPF_OP(iter->code) == BPF_DIV ? "/"  :
            BPF_OP(iter->code) == BPF_MOD ? "%"  :
            BPF_OP(iter->code) == BPF_OR  ? "|"  :
            BPF_OP(iter->code) == BPF_XOR ? "^"  :
            BPF_OP(iter->code) == BPF_AND ? "&"  :
            BPF_OP(iter->code) == BPF_LSH ? "<<" :
            BPF_OP(iter->code) == BPF_RSH ? ">>" : "???",
            (int)iter->k);
        break;
      default:
        fprintf(stderr, "???\n");
        break;
    }
  }
  return;
}

}  // namespace bpf_dsl
}  // namespace sandbox
