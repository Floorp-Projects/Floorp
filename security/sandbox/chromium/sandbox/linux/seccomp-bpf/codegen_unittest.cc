// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>

#include <algorithm>
#include <set>
#include <vector>

#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

class SandboxUnittestHelper : public SandboxBPF {
 public:
  typedef SandboxBPF::Program Program;
};

// We want to access some of the private methods in the code generator. We
// do so by defining a "friend" that makes these methods public for us.
class CodeGenUnittestHelper : public CodeGen {
 public:
  void FindBranchTargets(const Instruction& instructions,
                         BranchTargets* branch_targets) {
    CodeGen::FindBranchTargets(instructions, branch_targets);
  }

  BasicBlock* CutGraphIntoBasicBlocks(Instruction* insns,
                                      const BranchTargets& branch_targets,
                                      TargetsToBlocks* blocks) {
    return CodeGen::CutGraphIntoBasicBlocks(insns, branch_targets, blocks);
  }

  void MergeTails(TargetsToBlocks* blocks) { CodeGen::MergeTails(blocks); }
};

enum { NO_FLAGS = 0x0000, HAS_MERGEABLE_TAILS = 0x0001, };

Instruction* SampleProgramOneInstruction(CodeGen* codegen, int* flags) {
  // Create the most basic valid BPF program:
  //    RET ERR_ALLOWED
  *flags = NO_FLAGS;
  return codegen->MakeInstruction(BPF_RET + BPF_K,
                                  ErrorCode(ErrorCode::ERR_ALLOWED));
}

Instruction* SampleProgramSimpleBranch(CodeGen* codegen, int* flags) {
  // Create a program with a single branch:
  //    JUMP if eq 42 then $0 else $1
  // 0: RET EPERM
  // 1: RET ERR_ALLOWED
  *flags = NO_FLAGS;
  return codegen->MakeInstruction(
      BPF_JMP + BPF_JEQ + BPF_K,
      42,
      codegen->MakeInstruction(BPF_RET + BPF_K, ErrorCode(EPERM)),
      codegen->MakeInstruction(BPF_RET + BPF_K,
                               ErrorCode(ErrorCode::ERR_ALLOWED)));
}

Instruction* SampleProgramAtypicalBranch(CodeGen* codegen, int* flags) {
  // Create a program with a single branch:
  //    JUMP if eq 42 then $0 else $0
  // 0: RET ERR_ALLOWED

  // N.B.: As the instructions in both sides of the branch are already
  //       the same object, we do not actually have any "mergeable" branches.
  //       This needs to be reflected in our choice of "flags".
  *flags = NO_FLAGS;

  Instruction* ret = codegen->MakeInstruction(
      BPF_RET + BPF_K, ErrorCode(ErrorCode::ERR_ALLOWED));
  return codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, ret, ret);
}

Instruction* SampleProgramComplex(CodeGen* codegen, int* flags) {
  // Creates a basic BPF program that we'll use to test some of the code:
  //    JUMP if eq 42 the $0 else $1     (insn6)
  // 0: LD 23                            (insn5)
  // 1: JUMP if eq 42 then $2 else $4    (insn4)
  // 2: JUMP to $3                       (insn1)
  // 3: LD 42                            (insn0)
  //    RET ErrorCode(42)                (insn2)
  // 4: LD 42                            (insn3)
  //    RET ErrorCode(42)                (insn3+)
  *flags = HAS_MERGEABLE_TAILS;

  Instruction* insn0 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 42);
  SANDBOX_ASSERT(insn0);
  SANDBOX_ASSERT(insn0->code == BPF_LD + BPF_W + BPF_ABS);
  SANDBOX_ASSERT(insn0->k == 42);
  SANDBOX_ASSERT(insn0->next == NULL);

  Instruction* insn1 = codegen->MakeInstruction(BPF_JMP + BPF_JA, 0, insn0);
  SANDBOX_ASSERT(insn1);
  SANDBOX_ASSERT(insn1->code == BPF_JMP + BPF_JA);
  SANDBOX_ASSERT(insn1->jt_ptr == insn0);

  Instruction* insn2 = codegen->MakeInstruction(BPF_RET + BPF_K, ErrorCode(42));
  SANDBOX_ASSERT(insn2);
  SANDBOX_ASSERT(insn2->code == BPF_RET + BPF_K);
  SANDBOX_ASSERT(insn2->next == NULL);

  // We explicitly duplicate instructions so that MergeTails() can coalesce
  // them later.
  Instruction* insn3 = codegen->MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      42,
      codegen->MakeInstruction(BPF_RET + BPF_K, ErrorCode(42)));

  Instruction* insn4 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, insn1, insn3);
  SANDBOX_ASSERT(insn4);
  SANDBOX_ASSERT(insn4->code == BPF_JMP + BPF_JEQ + BPF_K);
  SANDBOX_ASSERT(insn4->k == 42);
  SANDBOX_ASSERT(insn4->jt_ptr == insn1);
  SANDBOX_ASSERT(insn4->jf_ptr == insn3);

  codegen->JoinInstructions(insn0, insn2);
  SANDBOX_ASSERT(insn0->next == insn2);

  Instruction* insn5 =
      codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 23, insn4);
  SANDBOX_ASSERT(insn5);
  SANDBOX_ASSERT(insn5->code == BPF_LD + BPF_W + BPF_ABS);
  SANDBOX_ASSERT(insn5->k == 23);
  SANDBOX_ASSERT(insn5->next == insn4);

  // Force a basic block that ends in neither a jump instruction nor a return
  // instruction. It only contains "insn5". This exercises one of the less
  // common code paths in the topo-sort algorithm.
  // This also gives us a diamond-shaped pattern in our graph, which stresses
  // another aspect of the topo-sort algorithm (namely, the ability to
  // correctly count the incoming branches for subtrees that are not disjunct).
  Instruction* insn6 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, insn5, insn4);

  return insn6;
}

Instruction* SampleProgramConfusingTails(CodeGen* codegen, int* flags) {
  // This simple program demonstrates https://crbug.com/351103/
  // The two "LOAD 0" instructions are blocks of their own. MergeTails() could
  // be tempted to merge them since they are the same. However, they are
  // not mergeable because they fall-through to non semantically equivalent
  // blocks.
  // Without the fix for this bug, this program should trigger the check in
  // CompileAndCompare: the serialized graphs from the program and its compiled
  // version will differ.
  //
  //  0) LOAD 1  // ???
  //  1) if A == 0x1; then JMP 2 else JMP 3
  //  2) LOAD 0  // System call number
  //  3) if A == 0x2; then JMP 4 else JMP 5
  //  4) LOAD 0  // System call number
  //  5) if A == 0x1; then JMP 6 else JMP 7
  //  6) RET 0x50000  // errno = 0
  //  7) RET 0x50001  // errno = 1
  *flags = NO_FLAGS;

  Instruction* i7 = codegen->MakeInstruction(BPF_RET, ErrorCode(1));
  Instruction* i6 = codegen->MakeInstruction(BPF_RET, ErrorCode(0));
  Instruction* i5 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i6, i7);
  Instruction* i4 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i5);
  Instruction* i3 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  Instruction* i2 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i3);
  Instruction* i1 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  Instruction* i0 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  return i0;
}

Instruction* SampleProgramConfusingTailsBasic(CodeGen* codegen, int* flags) {
  // Without the fix for https://crbug.com/351103/, (see
  // SampleProgramConfusingTails()), this would generate a cyclic graph and
  // crash as the two "LOAD 0" instructions would get merged.
  //
  // 0) LOAD 1  // ???
  // 1) if A == 0x1; then JMP 2 else JMP 3
  // 2) LOAD 0  // System call number
  // 3) if A == 0x2; then JMP 4 else JMP 5
  // 4) LOAD 0  // System call number
  // 5) RET 0x50001  // errno = 1
  *flags = NO_FLAGS;

  Instruction* i5 = codegen->MakeInstruction(BPF_RET, ErrorCode(1));
  Instruction* i4 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i5);
  Instruction* i3 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  Instruction* i2 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i3);
  Instruction* i1 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  Instruction* i0 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  return i0;
}

Instruction* SampleProgramConfusingTailsMergeable(CodeGen* codegen,
                                                  int* flags) {
  // This is similar to SampleProgramConfusingTails(), except that
  // instructions 2 and 4 are now RET instructions.
  // In PointerCompare(), this exercises the path where two blocks are of the
  // same length and identical and the last instruction is a JMP or RET, so the
  // following blocks don't need to be looked at and the blocks are mergeable.
  //
  // 0) LOAD 1  // ???
  // 1) if A == 0x1; then JMP 2 else JMP 3
  // 2) RET 0x5002a  // errno = 42
  // 3) if A == 0x2; then JMP 4 else JMP 5
  // 4) RET 0x5002a  // errno = 42
  // 5) if A == 0x1; then JMP 6 else JMP 7
  // 6) RET 0x50000  // errno = 0
  // 7) RET 0x50001  // errno = 1
  *flags = HAS_MERGEABLE_TAILS;

  Instruction* i7 = codegen->MakeInstruction(BPF_RET, ErrorCode(1));
  Instruction* i6 = codegen->MakeInstruction(BPF_RET, ErrorCode(0));
  Instruction* i5 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i6, i7);
  Instruction* i4 = codegen->MakeInstruction(BPF_RET, ErrorCode(42));
  Instruction* i3 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  Instruction* i2 = codegen->MakeInstruction(BPF_RET, ErrorCode(42));
  Instruction* i1 =
      codegen->MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  Instruction* i0 = codegen->MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  return i0;
}
void ForAllPrograms(void (*test)(CodeGenUnittestHelper*, Instruction*, int)) {
  Instruction* (*function_table[])(CodeGen* codegen, int* flags) = {
    SampleProgramOneInstruction,
    SampleProgramSimpleBranch,
    SampleProgramAtypicalBranch,
    SampleProgramComplex,
    SampleProgramConfusingTails,
    SampleProgramConfusingTailsBasic,
    SampleProgramConfusingTailsMergeable,
  };

  for (size_t i = 0; i < arraysize(function_table); ++i) {
    CodeGenUnittestHelper codegen;
    int flags = NO_FLAGS;
    Instruction *prg = function_table[i](&codegen, &flags);
    test(&codegen, prg, flags);
  }
}

void MakeInstruction(CodeGenUnittestHelper* codegen,
                     Instruction* program, int) {
  // Nothing to do here
}

SANDBOX_TEST(CodeGen, MakeInstruction) {
  ForAllPrograms(MakeInstruction);
}

void FindBranchTargets(CodeGenUnittestHelper* codegen, Instruction* prg, int) {
  BranchTargets branch_targets;
  codegen->FindBranchTargets(*prg, &branch_targets);

  // Verifying the general properties that should be true for every
  // well-formed BPF program.
  // Perform a depth-first traversal of the BPF program an verify that all
  // targets of BPF_JMP instructions are represented in the "branch_targets".
  // At the same time, compute a set of both the branch targets and all the
  // instructions in the program.
  std::vector<Instruction*> stack;
  std::set<Instruction*> all_instructions;
  std::set<Instruction*> target_instructions;
  BranchTargets::const_iterator end = branch_targets.end();
  for (Instruction* insn = prg;;) {
    all_instructions.insert(insn);
    if (BPF_CLASS(insn->code) == BPF_JMP) {
      target_instructions.insert(insn->jt_ptr);
      SANDBOX_ASSERT(insn->jt_ptr != NULL);
      SANDBOX_ASSERT(branch_targets.find(insn->jt_ptr) != end);
      if (BPF_OP(insn->code) != BPF_JA) {
        target_instructions.insert(insn->jf_ptr);
        SANDBOX_ASSERT(insn->jf_ptr != NULL);
        SANDBOX_ASSERT(branch_targets.find(insn->jf_ptr) != end);
        stack.push_back(insn->jf_ptr);
      }
      insn = insn->jt_ptr;
    } else if (BPF_CLASS(insn->code) == BPF_RET) {
      SANDBOX_ASSERT(insn->next == NULL);
      if (stack.empty()) {
        break;
      }
      insn = stack.back();
      stack.pop_back();
    } else {
      SANDBOX_ASSERT(insn->next != NULL);
      insn = insn->next;
    }
  }
  SANDBOX_ASSERT(target_instructions.size() == branch_targets.size());

  // We can now subtract the set of the branch targets from the set of all
  // instructions. This gives us a set with the instructions that nobody
  // ever jumps to. Verify that they are no included in the
  // "branch_targets" that FindBranchTargets() computed for us.
  Instructions non_target_instructions(all_instructions.size() -
                                       target_instructions.size());
  set_difference(all_instructions.begin(),
                 all_instructions.end(),
                 target_instructions.begin(),
                 target_instructions.end(),
                 non_target_instructions.begin());
  for (Instructions::const_iterator iter = non_target_instructions.begin();
       iter != non_target_instructions.end();
       ++iter) {
    SANDBOX_ASSERT(branch_targets.find(*iter) == end);
  }
}

SANDBOX_TEST(CodeGen, FindBranchTargets) { ForAllPrograms(FindBranchTargets); }

void CutGraphIntoBasicBlocks(CodeGenUnittestHelper* codegen,
                             Instruction* prg,
                             int) {
  BranchTargets branch_targets;
  codegen->FindBranchTargets(*prg, &branch_targets);
  TargetsToBlocks all_blocks;
  BasicBlock* first_block =
      codegen->CutGraphIntoBasicBlocks(prg, branch_targets, &all_blocks);
  SANDBOX_ASSERT(first_block != NULL);
  SANDBOX_ASSERT(first_block->instructions.size() > 0);
  Instruction* first_insn = first_block->instructions[0];

  // Basic blocks are supposed to start with a branch target and end with
  // either a jump or a return instruction. It can also end, if the next
  // instruction forms the beginning of a new basic block. There should be
  // no other jumps or return instructions in the middle of a basic block.
  for (TargetsToBlocks::const_iterator bb_iter = all_blocks.begin();
       bb_iter != all_blocks.end();
       ++bb_iter) {
    BasicBlock* bb = bb_iter->second;
    SANDBOX_ASSERT(bb != NULL);
    SANDBOX_ASSERT(bb->instructions.size() > 0);
    Instruction* insn = bb->instructions[0];
    SANDBOX_ASSERT(insn == first_insn ||
                   branch_targets.find(insn) != branch_targets.end());
    for (Instructions::const_iterator insn_iter = bb->instructions.begin();;) {
      insn = *insn_iter;
      if (++insn_iter != bb->instructions.end()) {
        SANDBOX_ASSERT(BPF_CLASS(insn->code) != BPF_JMP);
        SANDBOX_ASSERT(BPF_CLASS(insn->code) != BPF_RET);
      } else {
        SANDBOX_ASSERT(BPF_CLASS(insn->code) == BPF_JMP ||
                       BPF_CLASS(insn->code) == BPF_RET ||
                       branch_targets.find(insn->next) != branch_targets.end());
        break;
      }
      SANDBOX_ASSERT(branch_targets.find(*insn_iter) == branch_targets.end());
    }
  }
}

SANDBOX_TEST(CodeGen, CutGraphIntoBasicBlocks) {
  ForAllPrograms(CutGraphIntoBasicBlocks);
}

void MergeTails(CodeGenUnittestHelper* codegen, Instruction* prg, int flags) {
  BranchTargets branch_targets;
  codegen->FindBranchTargets(*prg, &branch_targets);
  TargetsToBlocks all_blocks;
  BasicBlock* first_block =
      codegen->CutGraphIntoBasicBlocks(prg, branch_targets, &all_blocks);

  // The shape of our graph and thus the function of our program should
  // still be unchanged after we run MergeTails(). We verify this by
  // serializing the graph and verifying that it is still the same.
  // We also verify that at least some of the edges changed because of
  // tail merging.
  std::string graph[2];
  std::string edges[2];

  // The loop executes twice. After the first run, we call MergeTails() on
  // our graph.
  for (int i = 0;;) {
    // Traverse the entire program in depth-first order.
    std::vector<BasicBlock*> stack;
    for (BasicBlock* bb = first_block;;) {
      // Serialize the instructions in this basic block. In general, we only
      // need to serialize "code" and "k"; except for a BPF_JA instruction
      // where "k" isn't set.
      // The stream of instructions should be unchanged after MergeTails().
      for (Instructions::const_iterator iter = bb->instructions.begin();
           iter != bb->instructions.end();
           ++iter) {
        graph[i].append(reinterpret_cast<char*>(&(*iter)->code),
                        sizeof((*iter)->code));
        if (BPF_CLASS((*iter)->code) != BPF_JMP ||
            BPF_OP((*iter)->code) != BPF_JA) {
          graph[i].append(reinterpret_cast<char*>(&(*iter)->k),
                          sizeof((*iter)->k));
        }
      }

      // Also serialize the addresses the basic blocks as we encounter them.
      // This will change as basic blocks are coalesed by MergeTails().
      edges[i].append(reinterpret_cast<char*>(&bb), sizeof(bb));

      // Depth-first traversal of the graph. We only ever need to look at the
      // very last instruction in the basic block, as that is the only one that
      // can change code flow.
      Instruction* insn = bb->instructions.back();
      if (BPF_CLASS(insn->code) == BPF_JMP) {
        // For jump instructions, we need to remember the "false" branch while
        // traversing the "true" branch. This is not necessary for BPF_JA which
        // only has a single branch.
        if (BPF_OP(insn->code) != BPF_JA) {
          stack.push_back(all_blocks[insn->jf_ptr]);
        }
        bb = all_blocks[insn->jt_ptr];
      } else if (BPF_CLASS(insn->code) == BPF_RET) {
        // After a BPF_RET, see if we need to back track.
        if (stack.empty()) {
          break;
        }
        bb = stack.back();
        stack.pop_back();
      } else {
        // For "normal" instructions, just follow to the next basic block.
        bb = all_blocks[insn->next];
      }
    }

    // Our loop runs exactly two times.
    if (++i > 1) {
      break;
    }
    codegen->MergeTails(&all_blocks);
  }
  SANDBOX_ASSERT(graph[0] == graph[1]);
  if (flags & HAS_MERGEABLE_TAILS) {
    SANDBOX_ASSERT(edges[0] != edges[1]);
  } else {
    SANDBOX_ASSERT(edges[0] == edges[1]);
  }
}

SANDBOX_TEST(CodeGen, MergeTails) {
  ForAllPrograms(MergeTails);
}

void CompileAndCompare(CodeGenUnittestHelper* codegen, Instruction* prg, int) {
  // TopoSortBasicBlocks() has internal checks that cause it to fail, if it
  // detects a problem. Typically, if anything goes wrong, this looks to the
  // TopoSort algorithm as if there had been cycles in the input data.
  // This provides a pretty good unittest.
  // We hand-crafted the program returned by SampleProgram() to exercise
  // several of the more interesting code-paths. See comments in
  // SampleProgram() for details.
  // In addition to relying on the internal consistency checks in the compiler,
  // we also serialize the graph and the resulting BPF program and compare
  // them. With the exception of BPF_JA instructions that might have been
  // inserted, both instruction streams should be equivalent.
  // As Compile() modifies the instructions, we have to serialize the graph
  // before calling Compile().
  std::string source;
  Instructions source_stack;
  for (const Instruction* insn = prg, *next; insn; insn = next) {
    if (BPF_CLASS(insn->code) == BPF_JMP) {
      if (BPF_OP(insn->code) == BPF_JA) {
        // Do not serialize BPF_JA instructions (see above).
        next = insn->jt_ptr;
        continue;
      } else {
        source_stack.push_back(insn->jf_ptr);
        next = insn->jt_ptr;
      }
    } else if (BPF_CLASS(insn->code) == BPF_RET) {
      if (source_stack.empty()) {
        next = NULL;
      } else {
        next = source_stack.back();
        source_stack.pop_back();
      }
    } else {
      next = insn->next;
    }
    // Only serialize "code" and "k". That's all the information we need to
    // compare. The rest of the information is encoded in the order of
    // instructions.
    source.append(reinterpret_cast<const char*>(&insn->code),
                  sizeof(insn->code));
    source.append(reinterpret_cast<const char*>(&insn->k), sizeof(insn->k));
  }

  // Compile the program
  SandboxUnittestHelper::Program bpf;
  codegen->Compile(prg, &bpf);

  // Serialize the resulting BPF instructions.
  std::string assembly;
  std::vector<int> assembly_stack;
  for (int idx = 0; idx >= 0;) {
    SANDBOX_ASSERT(idx < (int)bpf.size());
    struct sock_filter& insn = bpf[idx];
    if (BPF_CLASS(insn.code) == BPF_JMP) {
      if (BPF_OP(insn.code) == BPF_JA) {
        // Do not serialize BPF_JA instructions (see above).
        idx += insn.k + 1;
        continue;
      } else {
        assembly_stack.push_back(idx + insn.jf + 1);
        idx += insn.jt + 1;
      }
    } else if (BPF_CLASS(insn.code) == BPF_RET) {
      if (assembly_stack.empty()) {
        idx = -1;
      } else {
        idx = assembly_stack.back();
        assembly_stack.pop_back();
      }
    } else {
      ++idx;
    }
    // Serialize the same information that we serialized before compilation.
    assembly.append(reinterpret_cast<char*>(&insn.code), sizeof(insn.code));
    assembly.append(reinterpret_cast<char*>(&insn.k), sizeof(insn.k));
  }
  SANDBOX_ASSERT(source == assembly);
}

SANDBOX_TEST(CodeGen, All) {
  ForAllPrograms(CompileAndCompare);
}

}  // namespace sandbox
