// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/codegen.h"

#include <linux/filter.h>

#include <set>
#include <string>
#include <vector>

#include "sandbox/linux/seccomp-bpf/basicblock.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/instruction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// We want to access some of the private methods in the code generator. We
// do so by defining a "friend" that makes these methods public for us.
class CodeGenUnittestHelper : public CodeGen {
 public:
  using CodeGen::CutGraphIntoBasicBlocks;
  using CodeGen::FindBranchTargets;
  using CodeGen::MergeTails;
};

namespace {

enum {
  NO_FLAGS = 0x0000,
  HAS_MERGEABLE_TAILS = 0x0001,
};

using ProgramTestFunc = void (*)(CodeGenUnittestHelper* gen,
                                 Instruction* head,
                                 int flags);

class ProgramTest : public ::testing::TestWithParam<ProgramTestFunc> {
 protected:
  ProgramTest() : gen_() {}

  // RunTest runs the test function argument.  It should be called at
  // the end of each program test case.
  void RunTest(Instruction* head, int flags) { GetParam()(&gen_, head, flags); }

  Instruction* MakeInstruction(uint16_t code,
                               uint32_t k,
                               Instruction* next = nullptr) {
    Instruction* ret = gen_.MakeInstruction(code, k, next);
    EXPECT_NE(nullptr, ret);
    EXPECT_EQ(code, ret->code);
    EXPECT_EQ(k, ret->k);
    if (code == BPF_JMP + BPF_JA) {
      // Annoying inconsistency.
      EXPECT_EQ(nullptr, ret->next);
      EXPECT_EQ(next, ret->jt_ptr);
    } else {
      EXPECT_EQ(next, ret->next);
      EXPECT_EQ(nullptr, ret->jt_ptr);
    }
    EXPECT_EQ(nullptr, ret->jf_ptr);
    return ret;
  }

  Instruction* MakeInstruction(uint16_t code,
                               uint32_t k,
                               Instruction* jt,
                               Instruction* jf) {
    Instruction* ret = gen_.MakeInstruction(code, k, jt, jf);
    EXPECT_NE(nullptr, ret);
    EXPECT_EQ(code, ret->code);
    EXPECT_EQ(k, ret->k);
    EXPECT_EQ(nullptr, ret->next);
    EXPECT_EQ(jt, ret->jt_ptr);
    EXPECT_EQ(jf, ret->jf_ptr);
    return ret;
  }

 private:
  CodeGenUnittestHelper gen_;
};

TEST_P(ProgramTest, OneInstruction) {
  // Create the most basic valid BPF program:
  //    RET 0
  Instruction* head = MakeInstruction(BPF_RET + BPF_K, 0);
  RunTest(head, NO_FLAGS);
}

TEST_P(ProgramTest, SimpleBranch) {
  // Create a program with a single branch:
  //    JUMP if eq 42 then $0 else $1
  // 0: RET 1
  // 1: RET 0
  Instruction* head = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K,
                                      42,
                                      MakeInstruction(BPF_RET + BPF_K, 1),
                                      MakeInstruction(BPF_RET + BPF_K, 0));
  RunTest(head, NO_FLAGS);
}

TEST_P(ProgramTest, AtypicalBranch) {
  // Create a program with a single branch:
  //    JUMP if eq 42 then $0 else $0
  // 0: RET 0

  Instruction* ret = MakeInstruction(BPF_RET + BPF_K, 0);
  Instruction* head = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, ret, ret);

  // N.B.: As the instructions in both sides of the branch are already
  //       the same object, we do not actually have any "mergeable" branches.
  //       This needs to be reflected in our choice of "flags".
  RunTest(head, NO_FLAGS);
}

TEST_P(ProgramTest, Complex) {
  // Creates a basic BPF program that we'll use to test some of the code:
  //    JUMP if eq 42 the $0 else $1     (insn6)
  // 0: LD 23                            (insn5)
  // 1: JUMP if eq 42 then $2 else $4    (insn4)
  // 2: JUMP to $3                       (insn2)
  // 3: LD 42                            (insn1)
  //    RET 42                           (insn0)
  // 4: LD 42                            (insn3)
  //    RET 42                           (insn3+)
  Instruction* insn0 = MakeInstruction(BPF_RET + BPF_K, 42);
  Instruction* insn1 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 42, insn0);
  Instruction* insn2 = MakeInstruction(BPF_JMP + BPF_JA, 0, insn1);

  // We explicitly duplicate instructions so that MergeTails() can coalesce
  // them later.
  Instruction* insn3 = MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, 42, MakeInstruction(BPF_RET + BPF_K, 42));

  Instruction* insn4 =
      MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, insn2, insn3);
  Instruction* insn5 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 23, insn4);

  // Force a basic block that ends in neither a jump instruction nor a return
  // instruction. It only contains "insn5". This exercises one of the less
  // common code paths in the topo-sort algorithm.
  // This also gives us a diamond-shaped pattern in our graph, which stresses
  // another aspect of the topo-sort algorithm (namely, the ability to
  // correctly count the incoming branches for subtrees that are not disjunct).
  Instruction* insn6 =
      MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, insn5, insn4);

  RunTest(insn6, HAS_MERGEABLE_TAILS);
}

TEST_P(ProgramTest, ConfusingTails) {
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
  //  6) RET 0
  //  7) RET 1

  Instruction* i7 = MakeInstruction(BPF_RET + BPF_K, 1);
  Instruction* i6 = MakeInstruction(BPF_RET + BPF_K, 0);
  Instruction* i5 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i6, i7);
  Instruction* i4 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i5);
  Instruction* i3 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  Instruction* i2 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i3);
  Instruction* i1 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  Instruction* i0 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  RunTest(i0, NO_FLAGS);
}

TEST_P(ProgramTest, ConfusingTailsBasic) {
  // Without the fix for https://crbug.com/351103/, (see
  // SampleProgramConfusingTails()), this would generate a cyclic graph and
  // crash as the two "LOAD 0" instructions would get merged.
  //
  // 0) LOAD 1  // ???
  // 1) if A == 0x1; then JMP 2 else JMP 3
  // 2) LOAD 0  // System call number
  // 3) if A == 0x2; then JMP 4 else JMP 5
  // 4) LOAD 0  // System call number
  // 5) RET 1

  Instruction* i5 = MakeInstruction(BPF_RET + BPF_K, 1);
  Instruction* i4 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i5);
  Instruction* i3 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  Instruction* i2 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i3);
  Instruction* i1 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  Instruction* i0 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  RunTest(i0, NO_FLAGS);
}

TEST_P(ProgramTest, ConfusingTailsMergeable) {
  // This is similar to SampleProgramConfusingTails(), except that
  // instructions 2 and 4 are now RET instructions.
  // In PointerCompare(), this exercises the path where two blocks are of the
  // same length and identical and the last instruction is a JMP or RET, so the
  // following blocks don't need to be looked at and the blocks are mergeable.
  //
  // 0) LOAD 1  // ???
  // 1) if A == 0x1; then JMP 2 else JMP 3
  // 2) RET 42
  // 3) if A == 0x2; then JMP 4 else JMP 5
  // 4) RET 42
  // 5) if A == 0x1; then JMP 6 else JMP 7
  // 6) RET 0
  // 7) RET 1

  Instruction* i7 = MakeInstruction(BPF_RET + BPF_K, 1);
  Instruction* i6 = MakeInstruction(BPF_RET + BPF_K, 0);
  Instruction* i5 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i6, i7);
  Instruction* i4 = MakeInstruction(BPF_RET + BPF_K, 42);
  Instruction* i3 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  Instruction* i2 = MakeInstruction(BPF_RET + BPF_K, 42);
  Instruction* i1 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  Instruction* i0 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  RunTest(i0, HAS_MERGEABLE_TAILS);
}

void MakeInstruction(CodeGenUnittestHelper* codegen,
                     Instruction* program, int) {
  // Nothing to do here
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
      ASSERT_TRUE(insn->jt_ptr != NULL);
      ASSERT_TRUE(branch_targets.find(insn->jt_ptr) != end);
      if (BPF_OP(insn->code) != BPF_JA) {
        target_instructions.insert(insn->jf_ptr);
        ASSERT_TRUE(insn->jf_ptr != NULL);
        ASSERT_TRUE(branch_targets.find(insn->jf_ptr) != end);
        stack.push_back(insn->jf_ptr);
      }
      insn = insn->jt_ptr;
    } else if (BPF_CLASS(insn->code) == BPF_RET) {
      ASSERT_TRUE(insn->next == NULL);
      if (stack.empty()) {
        break;
      }
      insn = stack.back();
      stack.pop_back();
    } else {
      ASSERT_TRUE(insn->next != NULL);
      insn = insn->next;
    }
  }
  ASSERT_TRUE(target_instructions.size() == branch_targets.size());

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
    ASSERT_TRUE(branch_targets.find(*iter) == end);
  }
}

void CutGraphIntoBasicBlocks(CodeGenUnittestHelper* codegen,
                             Instruction* prg,
                             int) {
  BranchTargets branch_targets;
  codegen->FindBranchTargets(*prg, &branch_targets);
  TargetsToBlocks all_blocks;
  BasicBlock* first_block =
      codegen->CutGraphIntoBasicBlocks(prg, branch_targets, &all_blocks);
  ASSERT_TRUE(first_block != NULL);
  ASSERT_TRUE(first_block->instructions.size() > 0);
  Instruction* first_insn = first_block->instructions[0];

  // Basic blocks are supposed to start with a branch target and end with
  // either a jump or a return instruction. It can also end, if the next
  // instruction forms the beginning of a new basic block. There should be
  // no other jumps or return instructions in the middle of a basic block.
  for (TargetsToBlocks::const_iterator bb_iter = all_blocks.begin();
       bb_iter != all_blocks.end();
       ++bb_iter) {
    BasicBlock* bb = bb_iter->second;
    ASSERT_TRUE(bb != NULL);
    ASSERT_TRUE(bb->instructions.size() > 0);
    Instruction* insn = bb->instructions[0];
    ASSERT_TRUE(insn == first_insn ||
                branch_targets.find(insn) != branch_targets.end());
    for (Instructions::const_iterator insn_iter = bb->instructions.begin();;) {
      insn = *insn_iter;
      if (++insn_iter != bb->instructions.end()) {
        ASSERT_TRUE(BPF_CLASS(insn->code) != BPF_JMP);
        ASSERT_TRUE(BPF_CLASS(insn->code) != BPF_RET);
      } else {
        ASSERT_TRUE(BPF_CLASS(insn->code) == BPF_JMP ||
                    BPF_CLASS(insn->code) == BPF_RET ||
                    branch_targets.find(insn->next) != branch_targets.end());
        break;
      }
      ASSERT_TRUE(branch_targets.find(*insn_iter) == branch_targets.end());
    }
  }
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
  ASSERT_TRUE(graph[0] == graph[1]);
  if (flags & HAS_MERGEABLE_TAILS) {
    ASSERT_TRUE(edges[0] != edges[1]);
  } else {
    ASSERT_TRUE(edges[0] == edges[1]);
  }
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
  CodeGen::Program bpf;
  codegen->Compile(prg, &bpf);

  // Serialize the resulting BPF instructions.
  std::string assembly;
  std::vector<int> assembly_stack;
  for (int idx = 0; idx >= 0;) {
    ASSERT_TRUE(idx < (int)bpf.size());
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
  ASSERT_TRUE(source == assembly);
}

const ProgramTestFunc kProgramTestFuncs[] = {
    MakeInstruction,
    FindBranchTargets,
    CutGraphIntoBasicBlocks,
    MergeTails,
    CompileAndCompare,
};

INSTANTIATE_TEST_CASE_P(CodeGen,
                        ProgramTest,
                        ::testing::ValuesIn(kProgramTestFuncs));

}  // namespace

}  // namespace sandbox
