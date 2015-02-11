// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_CODEGEN_H__
#define SANDBOX_LINUX_SECCOMP_BPF_CODEGEN_H__

#include <stdint.h>

#include <map>
#include <vector>

#include "sandbox/sandbox_export.h"

struct sock_filter;

namespace sandbox {
struct BasicBlock;
struct Instruction;

typedef std::vector<Instruction*> Instructions;
typedef std::vector<BasicBlock*> BasicBlocks;
typedef std::map<const Instruction*, int> BranchTargets;
typedef std::map<const Instruction*, BasicBlock*> TargetsToBlocks;
typedef std::map<const BasicBlock*, int> IncomingBranches;

// The code generator instantiates a basic compiler that can convert a
// graph of BPF instructions into a well-formed stream of BPF instructions.
// Most notably, it ensures that jumps are always forward and don't exceed
// the limit of 255 instructions imposed by the instruction set.
//
// Callers would typically create a new CodeGen object and then use it to
// build a DAG of Instructions. They'll eventually call Compile() to convert
// this DAG to a Program.
//
//   CodeGen gen;
//   Instruction *allow, *branch, *dag;
//
//   allow =
//     gen.MakeInstruction(BPF_RET+BPF_K,
//                         ErrorCode(ErrorCode::ERR_ALLOWED).err()));
//   branch =
//     gen.MakeInstruction(BPF_JMP+BPF_EQ+BPF_K, __NR_getpid,
//                         Trap(GetPidHandler, NULL), allow);
//   dag =
//     gen.MakeInstruction(BPF_LD+BPF_W+BPF_ABS,
//                         offsetof(struct arch_seccomp_data, nr), branch);
//
//   // Simplified code follows; in practice, it is important to avoid calling
//   // any C++ destructors after starting the sandbox.
//   CodeGen::Program program;
//   gen.Compile(dag, program);
//   const struct sock_fprog prog = {
//     static_cast<unsigned short>(program->size()), &program[0] };
//   prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
//
class SANDBOX_EXPORT CodeGen {
 public:
  // A vector of BPF instructions that need to be installed as a filter
  // program in the kernel.
  typedef std::vector<struct sock_filter> Program;

  CodeGen();
  ~CodeGen();

  // Create a new instruction. Instructions form a DAG. The instruction objects
  // are owned by the CodeGen object. They do not need to be explicitly
  // deleted.
  // For details on the possible parameters refer to <linux/filter.h>
  Instruction* MakeInstruction(uint16_t code,
                               uint32_t k,
                               Instruction* next = nullptr);
  Instruction* MakeInstruction(uint16_t code,
                               uint32_t k,
                               Instruction* jt,
                               Instruction* jf);

  // Compiles the graph of instructions into a BPF program that can be passed
  // to the kernel. Please note that this function modifies the graph in place
  // and must therefore only be called once per graph.
  void Compile(Instruction* instructions, Program* program);

 private:
  friend class CodeGenUnittestHelper;

  // Find all the instructions that are the target of BPF_JMPs.
  void FindBranchTargets(const Instruction& instructions,
                         BranchTargets* branch_targets);

  // Combine instructions between "head" and "tail" into a new basic block.
  // Basic blocks are defined as sequences of instructions whose only branch
  // target is the very first instruction; furthermore, any BPF_JMP or BPF_RET
  // instruction must be at the very end of the basic block.
  BasicBlock* MakeBasicBlock(Instruction* head, Instruction* tail);

  // Creates a basic block and adds it to "basic_blocks"; sets "first_block"
  // if it is still NULL.
  void AddBasicBlock(Instruction* head,
                     Instruction* tail,
                     const BranchTargets& branch_targets,
                     TargetsToBlocks* basic_blocks,
                     BasicBlock** first_block);

  // Cuts the DAG of instructions into basic blocks.
  BasicBlock* CutGraphIntoBasicBlocks(Instruction* instructions,
                                      const BranchTargets& branch_targets,
                                      TargetsToBlocks* blocks);

  // Find common tail sequences of basic blocks and coalesce them.
  void MergeTails(TargetsToBlocks* blocks);

  // For each basic block, compute the number of incoming branches.
  void ComputeIncomingBranches(BasicBlock* block,
                               const TargetsToBlocks& targets_to_blocks,
                               IncomingBranches* incoming_branches);

  // Topologically sort the basic blocks so that all jumps are forward jumps.
  // This is a requirement for any well-formed BPF program.
  void TopoSortBasicBlocks(BasicBlock* first_block,
                           const TargetsToBlocks& blocks,
                           BasicBlocks* basic_blocks);

  // Convert jt_ptr_ and jf_ptr_ fields in BPF_JMP instructions to valid
  // jt_ and jf_ jump offsets. This can result in BPF_JA instructions being
  // inserted, if we need to jump over more than 256 instructions.
  void ComputeRelativeJumps(BasicBlocks* basic_blocks,
                            const TargetsToBlocks& targets_to_blocks);

  // Concatenate instructions from all basic blocks into a BPF program that
  // can be passed to the kernel.
  void ConcatenateBasicBlocks(const BasicBlocks&, Program* program);

  // We stick all instructions and basic blocks into pools that get destroyed
  // when the CodeGen object is destroyed. This way, we neither need to worry
  // about explicitly managing ownership, nor do we need to worry about using
  // smart pointers in the presence of circular references.
  Instructions instructions_;
  BasicBlocks basic_blocks_;

  // Compile() must only ever be called once as it makes destructive changes
  // to the DAG.
  bool compiled_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_CODEGEN_H__
