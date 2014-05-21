// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/logging.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"

namespace {

// Helper function for Traverse().
void TraverseRecursively(std::set<sandbox::Instruction*>* visited,
                         sandbox::Instruction* instruction) {
  if (visited->find(instruction) == visited->end()) {
    visited->insert(instruction);
    switch (BPF_CLASS(instruction->code)) {
      case BPF_JMP:
        if (BPF_OP(instruction->code) != BPF_JA) {
          TraverseRecursively(visited, instruction->jf_ptr);
        }
        TraverseRecursively(visited, instruction->jt_ptr);
        break;
      case BPF_RET:
        break;
      default:
        TraverseRecursively(visited, instruction->next);
        break;
    }
  }
}

}  // namespace

namespace sandbox {

CodeGen::CodeGen() : compiled_(false) {}

CodeGen::~CodeGen() {
  for (Instructions::iterator iter = instructions_.begin();
       iter != instructions_.end();
       ++iter) {
    delete *iter;
  }
  for (BasicBlocks::iterator iter = basic_blocks_.begin();
       iter != basic_blocks_.end();
       ++iter) {
    delete *iter;
  }
}

void CodeGen::PrintProgram(const SandboxBPF::Program& program) {
  for (SandboxBPF::Program::const_iterator iter = program.begin();
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

Instruction* CodeGen::MakeInstruction(uint16_t code,
                                      uint32_t k,
                                      Instruction* next) {
  // We can handle non-jumping instructions and "always" jumps. Both of
  // them are followed by exactly one "next" instruction.
  // We allow callers to defer specifying "next", but then they must call
  // "joinInstructions" later.
  if (BPF_CLASS(code) == BPF_JMP && BPF_OP(code) != BPF_JA) {
    SANDBOX_DIE(
        "Must provide both \"true\" and \"false\" branch "
        "for a BPF_JMP");
  }
  if (next && BPF_CLASS(code) == BPF_RET) {
    SANDBOX_DIE("Cannot append instructions after a return statement");
  }
  if (BPF_CLASS(code) == BPF_JMP) {
    // "Always" jumps use the "true" branch target, only.
    Instruction* insn = new Instruction(code, 0, next, NULL);
    instructions_.push_back(insn);
    return insn;
  } else {
    // Non-jumping instructions do not use any of the branch targets.
    Instruction* insn = new Instruction(code, k, next);
    instructions_.push_back(insn);
    return insn;
  }
}

Instruction* CodeGen::MakeInstruction(uint16_t code, const ErrorCode& err) {
  if (BPF_CLASS(code) != BPF_RET) {
    SANDBOX_DIE("ErrorCodes can only be used in return expressions");
  }
  if (err.error_type_ != ErrorCode::ET_SIMPLE &&
      err.error_type_ != ErrorCode::ET_TRAP) {
    SANDBOX_DIE("ErrorCode is not suitable for returning from a BPF program");
  }
  return MakeInstruction(code, err.err_);
}

Instruction* CodeGen::MakeInstruction(uint16_t code,
                                      uint32_t k,
                                      Instruction* jt,
                                      Instruction* jf) {
  // We can handle all conditional jumps. They are followed by both a
  // "true" and a "false" branch.
  if (BPF_CLASS(code) != BPF_JMP || BPF_OP(code) == BPF_JA) {
    SANDBOX_DIE("Expected a BPF_JMP instruction");
  }
  if (!jt && !jf) {
    // We allow callers to defer specifying exactly one of the branch
    // targets. It must then be set later by calling "JoinInstructions".
    SANDBOX_DIE("Branches must jump to a valid instruction");
  }
  Instruction* insn = new Instruction(code, k, jt, jf);
  instructions_.push_back(insn);
  return insn;
}

void CodeGen::JoinInstructions(Instruction* head, Instruction* tail) {
  // Merge two instructions, or set the branch target for an "always" jump.
  // This function should be called, if the caller didn't initially provide
  // a value for "next" when creating the instruction.
  if (BPF_CLASS(head->code) == BPF_JMP) {
    if (BPF_OP(head->code) == BPF_JA) {
      if (head->jt_ptr) {
        SANDBOX_DIE("Cannot append instructions in the middle of a sequence");
      }
      head->jt_ptr = tail;
    } else {
      if (!head->jt_ptr && head->jf_ptr) {
        head->jt_ptr = tail;
      } else if (!head->jf_ptr && head->jt_ptr) {
        head->jf_ptr = tail;
      } else {
        SANDBOX_DIE("Cannot append instructions after a jump");
      }
    }
  } else if (BPF_CLASS(head->code) == BPF_RET) {
    SANDBOX_DIE("Cannot append instructions after a return statement");
  } else if (head->next) {
    SANDBOX_DIE("Cannot append instructions in the middle of a sequence");
  } else {
    head->next = tail;
  }
  return;
}

void CodeGen::Traverse(Instruction* instruction,
                       void (*fnc)(Instruction*, void*),
                       void* aux) {
  std::set<Instruction*> visited;
  TraverseRecursively(&visited, instruction);
  for (std::set<Instruction*>::const_iterator iter = visited.begin();
       iter != visited.end();
       ++iter) {
    fnc(*iter, aux);
  }
}

void CodeGen::FindBranchTargets(const Instruction& instructions,
                                BranchTargets* branch_targets) {
  // Follow all possible paths through the "instructions" graph and compute
  // a list of branch targets. This will later be needed to compute the
  // boundaries of basic blocks.
  // We maintain a set of all instructions that we have previously seen. This
  // set ultimately converges on all instructions in the program.
  std::set<const Instruction*> seen_instructions;
  Instructions stack;
  for (const Instruction* insn = &instructions; insn;) {
    seen_instructions.insert(insn);
    if (BPF_CLASS(insn->code) == BPF_JMP) {
      // Found a jump. Increase count of incoming edges for each of the jump
      // targets.
      ++(*branch_targets)[insn->jt_ptr];
      if (BPF_OP(insn->code) != BPF_JA) {
        ++(*branch_targets)[insn->jf_ptr];
        stack.push_back(const_cast<Instruction*>(insn));
      }
      // Start a recursive decent for depth-first traversal.
      if (seen_instructions.find(insn->jt_ptr) == seen_instructions.end()) {
        // We haven't seen the "true" branch yet. Traverse it now. We have
        // already remembered the "false" branch on the stack and will
        // traverse it later.
        insn = insn->jt_ptr;
        continue;
      } else {
        // Now try traversing the "false" branch.
        insn = NULL;
      }
    } else {
      // This is a non-jump instruction, just continue to the next instruction
      // (if any). It's OK if "insn" becomes NULL when reaching a return
      // instruction.
      if (!insn->next != (BPF_CLASS(insn->code) == BPF_RET)) {
        SANDBOX_DIE(
            "Internal compiler error; return instruction must be at "
            "the end of the BPF program");
      }
      if (seen_instructions.find(insn->next) == seen_instructions.end()) {
        insn = insn->next;
      } else {
        // We have seen this instruction before. That could happen if it is
        // a branch target. No need to continue processing.
        insn = NULL;
      }
    }
    while (!insn && !stack.empty()) {
      // We are done processing all the way to a leaf node, backtrack up the
      // stack to any branches that we haven't processed yet. By definition,
      // this has to be a "false" branch, as we always process the "true"
      // branches right away.
      insn = stack.back();
      stack.pop_back();
      if (seen_instructions.find(insn->jf_ptr) == seen_instructions.end()) {
        // We haven't seen the "false" branch yet. So, that's where we'll
        // go now.
        insn = insn->jf_ptr;
      } else {
        // We have seen both the "true" and the "false" branch, continue
        // up the stack.
        if (seen_instructions.find(insn->jt_ptr) == seen_instructions.end()) {
          SANDBOX_DIE(
              "Internal compiler error; cannot find all "
              "branch targets");
        }
        insn = NULL;
      }
    }
  }
  return;
}

BasicBlock* CodeGen::MakeBasicBlock(Instruction* head, Instruction* tail) {
  // Iterate over all the instructions between "head" and "tail" and
  // insert them into a new basic block.
  BasicBlock* bb = new BasicBlock;
  for (;; head = head->next) {
    bb->instructions.push_back(head);
    if (head == tail) {
      break;
    }
    if (BPF_CLASS(head->code) == BPF_JMP) {
      SANDBOX_DIE("Found a jump inside of a basic block");
    }
  }
  basic_blocks_.push_back(bb);
  return bb;
}

void CodeGen::AddBasicBlock(Instruction* head,
                            Instruction* tail,
                            const BranchTargets& branch_targets,
                            TargetsToBlocks* basic_blocks,
                            BasicBlock** firstBlock) {
  // Add a new basic block to "basic_blocks". Also set "firstBlock", if it
  // has not been set before.
  BranchTargets::const_iterator iter = branch_targets.find(head);
  if ((iter == branch_targets.end()) != !*firstBlock ||
      !*firstBlock != basic_blocks->empty()) {
    SANDBOX_DIE(
        "Only the very first basic block should have no "
        "incoming jumps");
  }
  BasicBlock* bb = MakeBasicBlock(head, tail);
  if (!*firstBlock) {
    *firstBlock = bb;
  }
  (*basic_blocks)[head] = bb;
  return;
}

BasicBlock* CodeGen::CutGraphIntoBasicBlocks(
    Instruction* instructions,
    const BranchTargets& branch_targets,
    TargetsToBlocks* basic_blocks) {
  // Textbook implementation of a basic block generator. All basic blocks
  // start with a branch target and end with either a return statement or
  // a jump (or are followed by an instruction that forms the beginning of a
  // new block). Both conditional and "always" jumps are supported.
  BasicBlock* first_block = NULL;
  std::set<const Instruction*> seen_instructions;
  Instructions stack;
  Instruction* tail = NULL;
  Instruction* head = instructions;
  for (Instruction* insn = head; insn;) {
    if (seen_instructions.find(insn) != seen_instructions.end()) {
      // We somehow went in a circle. This should never be possible. Not even
      // cyclic graphs are supposed to confuse us this much.
      SANDBOX_DIE("Internal compiler error; cannot compute basic blocks");
    }
    seen_instructions.insert(insn);
    if (tail && branch_targets.find(insn) != branch_targets.end()) {
      // We reached a branch target. Start a new basic block (this means,
      // flushing the previous basic block first).
      AddBasicBlock(head, tail, branch_targets, basic_blocks, &first_block);
      head = insn;
    }
    if (BPF_CLASS(insn->code) == BPF_JMP) {
      // We reached a jump instruction, this completes our current basic
      // block. Flush it and continue by traversing both the true and the
      // false branch of the jump. We need to maintain a stack to do so.
      AddBasicBlock(head, insn, branch_targets, basic_blocks, &first_block);
      if (BPF_OP(insn->code) != BPF_JA) {
        stack.push_back(insn->jf_ptr);
      }
      insn = insn->jt_ptr;

      // If we are jumping to an instruction that we have previously
      // processed, we are done with this branch. Continue by backtracking
      // up the stack.
      while (seen_instructions.find(insn) != seen_instructions.end()) {
      backtracking:
        if (stack.empty()) {
          // We successfully traversed all reachable instructions.
          return first_block;
        } else {
          // Going up the stack.
          insn = stack.back();
          stack.pop_back();
        }
      }
      // Starting a new basic block.
      tail = NULL;
      head = insn;
    } else {
      // We found a non-jumping instruction, append it to current basic
      // block.
      tail = insn;
      insn = insn->next;
      if (!insn) {
        // We reached a return statement, flush the current basic block and
        // backtrack up the stack.
        AddBasicBlock(head, tail, branch_targets, basic_blocks, &first_block);
        goto backtracking;
      }
    }
  }
  return first_block;
}

// We define a comparator that inspects the sequence of instructions in our
// basic block and any blocks referenced by this block. This function can be
// used in a "less" comparator for the purpose of storing pointers to basic
// blocks in STL containers; this gives an easy option to use STL to find
// shared tail  sequences of basic blocks.
static int PointerCompare(const BasicBlock* block1,
                          const BasicBlock* block2,
                          const TargetsToBlocks& blocks) {
  // Return <0, 0, or >0 depending on the ordering of "block1" and "block2".
  // If we are looking at the exact same block, this is trivial and we don't
  // need to do a full comparison.
  if (block1 == block2) {
    return 0;
  }

  // We compare the sequence of instructions in both basic blocks.
  const Instructions& insns1 = block1->instructions;
  const Instructions& insns2 = block2->instructions;
  // Basic blocks should never be empty.
  CHECK(!insns1.empty());
  CHECK(!insns2.empty());

  Instructions::const_iterator iter1 = insns1.begin();
  Instructions::const_iterator iter2 = insns2.begin();
  for (;; ++iter1, ++iter2) {
    // If we have reached the end of the sequence of instructions in one or
    // both basic blocks, we know the relative ordering between the two blocks
    // and can return.
    if (iter1 == insns1.end()) {
      if (iter2 == insns2.end()) {
        // If the two blocks are the same length (and have elementwise-equal
        // code and k fields, which is the only way we can reach this point),
        // and the last instruction isn't a JMP or a RET, then we must compare
        // their successors.
        Instruction* const insns1_last = insns1.back();
        Instruction* const insns2_last = insns2.back();
        if (BPF_CLASS(insns1_last->code) != BPF_JMP &&
            BPF_CLASS(insns1_last->code) != BPF_RET) {
          // Non jumping instructions will always have a valid next instruction.
          CHECK(insns1_last->next);
          CHECK(insns2_last->next);
          return PointerCompare(blocks.find(insns1_last->next)->second,
                                blocks.find(insns2_last->next)->second,
                                blocks);
        } else {
          return 0;
        }
      }
      return -1;
    } else if (iter2 == insns2.end()) {
      return 1;
    }

    // Compare the individual fields for both instructions.
    const Instruction& insn1 = **iter1;
    const Instruction& insn2 = **iter2;
    if (insn1.code == insn2.code) {
      if (insn1.k == insn2.k) {
        // Only conditional jump instructions use the jt_ptr and jf_ptr
        // fields.
        if (BPF_CLASS(insn1.code) == BPF_JMP) {
          if (BPF_OP(insn1.code) != BPF_JA) {
            // Recursively compare the "true" and "false" branches.
            // A well-formed BPF program can't have any cycles, so we know
            // that our recursive algorithm will ultimately terminate.
            // In the unlikely event that the programmer made a mistake and
            // went out of the way to give us a cyclic program, we will crash
            // with a stack overflow. We are OK with that.
            int c = PointerCompare(blocks.find(insn1.jt_ptr)->second,
                                   blocks.find(insn2.jt_ptr)->second,
                                   blocks);
            if (c == 0) {
              c = PointerCompare(blocks.find(insn1.jf_ptr)->second,
                                 blocks.find(insn2.jf_ptr)->second,
                                 blocks);
              if (c == 0) {
                continue;
              } else {
                return c;
              }
            } else {
              return c;
            }
          } else {
            int c = PointerCompare(blocks.find(insn1.jt_ptr)->second,
                                   blocks.find(insn2.jt_ptr)->second,
                                   blocks);
            if (c == 0) {
              continue;
            } else {
              return c;
            }
          }
        } else {
          continue;
        }
      } else {
        return insn1.k - insn2.k;
      }
    } else {
      return insn1.code - insn2.code;
    }
  }
}

void CodeGen::MergeTails(TargetsToBlocks* blocks) {
  // We enter all of our basic blocks into a set using the BasicBlock::Less()
  // comparator. This naturally results in blocks with identical tails of
  // instructions to map to the same entry in the set. Whenever we discover
  // that a particular chain of instructions is already in the set, we merge
  // the basic blocks and update the pointer in the "blocks" map.
  // Returns the number of unique basic blocks.
  // N.B. We don't merge instructions on a granularity that is finer than
  //      a basic block. In practice, this is sufficiently rare that we don't
  //      incur a big cost.
  //      Similarly, we currently don't merge anything other than tails. In
  //      the future, we might decide to revisit this decision and attempt to
  //      merge arbitrary sub-sequences of instructions.
  BasicBlock::Less<TargetsToBlocks> less(*blocks, PointerCompare);
  typedef std::set<BasicBlock*, BasicBlock::Less<TargetsToBlocks> > Set;
  Set seen_basic_blocks(less);
  for (TargetsToBlocks::iterator iter = blocks->begin(); iter != blocks->end();
       ++iter) {
    BasicBlock* bb = iter->second;
    Set::const_iterator entry = seen_basic_blocks.find(bb);
    if (entry == seen_basic_blocks.end()) {
      // This is the first time we see this particular sequence of
      // instructions. Enter the basic block into the set of known
      // basic blocks.
      seen_basic_blocks.insert(bb);
    } else {
      // We have previously seen another basic block that defines the same
      // sequence of instructions. Merge the two blocks and update the
      // pointer in the "blocks" map.
      iter->second = *entry;
    }
  }
}

void CodeGen::ComputeIncomingBranches(BasicBlock* block,
                                      const TargetsToBlocks& targets_to_blocks,
                                      IncomingBranches* incoming_branches) {
  // We increment the number of incoming branches each time we encounter a
  // basic block. But we only traverse recursively the very first time we
  // encounter a new block. This is necessary to make topological sorting
  // work correctly.
  if (++(*incoming_branches)[block] == 1) {
    Instruction* last_insn = block->instructions.back();
    if (BPF_CLASS(last_insn->code) == BPF_JMP) {
      ComputeIncomingBranches(targets_to_blocks.find(last_insn->jt_ptr)->second,
                              targets_to_blocks,
                              incoming_branches);
      if (BPF_OP(last_insn->code) != BPF_JA) {
        ComputeIncomingBranches(
            targets_to_blocks.find(last_insn->jf_ptr)->second,
            targets_to_blocks,
            incoming_branches);
      }
    } else if (BPF_CLASS(last_insn->code) != BPF_RET) {
      ComputeIncomingBranches(targets_to_blocks.find(last_insn->next)->second,
                              targets_to_blocks,
                              incoming_branches);
    }
  }
}

void CodeGen::TopoSortBasicBlocks(BasicBlock* first_block,
                                  const TargetsToBlocks& blocks,
                                  BasicBlocks* basic_blocks) {
  // Textbook implementation of a toposort. We keep looking for basic blocks
  // that don't have any incoming branches (initially, this is just the
  // "first_block") and add them to the topologically sorted list of
  // "basic_blocks". As we do so, we remove outgoing branches. This potentially
  // ends up making our descendants eligible for the sorted list. The
  // sorting algorithm terminates when there are no more basic blocks that have
  // no incoming branches. If we didn't move all blocks from the set of
  // "unordered_blocks" to the sorted list of "basic_blocks", there must have
  // been a cyclic dependency. This should never happen in a BPF program, as
  // well-formed BPF programs only ever have forward branches.
  IncomingBranches unordered_blocks;
  ComputeIncomingBranches(first_block, blocks, &unordered_blocks);

  std::set<BasicBlock*> heads;
  for (;;) {
    // Move block from "unordered_blocks" to "basic_blocks".
    basic_blocks->push_back(first_block);

    // Inspect last instruction in the basic block. This is typically either a
    // jump or a return statement. But it could also be a "normal" instruction
    // that is followed by a jump target.
    Instruction* last_insn = first_block->instructions.back();
    if (BPF_CLASS(last_insn->code) == BPF_JMP) {
      // Remove outgoing branches. This might end up moving our descendants
      // into set of "head" nodes that no longer have any incoming branches.
      TargetsToBlocks::const_iterator iter;
      if (BPF_OP(last_insn->code) != BPF_JA) {
        iter = blocks.find(last_insn->jf_ptr);
        if (!--unordered_blocks[iter->second]) {
          heads.insert(iter->second);
        }
      }
      iter = blocks.find(last_insn->jt_ptr);
      if (!--unordered_blocks[iter->second]) {
        first_block = iter->second;
        continue;
      }
    } else if (BPF_CLASS(last_insn->code) != BPF_RET) {
      // We encountered an instruction that doesn't change code flow. Try to
      // pick the next "first_block" from "last_insn->next", if possible.
      TargetsToBlocks::const_iterator iter;
      iter = blocks.find(last_insn->next);
      if (!--unordered_blocks[iter->second]) {
        first_block = iter->second;
        continue;
      } else {
        // Our basic block is supposed to be followed by "last_insn->next",
        // but dependencies prevent this from happening. Insert a BPF_JA
        // instruction to correct the code flow.
        Instruction* ja = MakeInstruction(BPF_JMP + BPF_JA, 0, last_insn->next);
        first_block->instructions.push_back(ja);
        last_insn->next = ja;
      }
    }
    if (heads.empty()) {
      if (unordered_blocks.size() != basic_blocks->size()) {
        SANDBOX_DIE("Internal compiler error; cyclic graph detected");
      }
      return;
    }
    // Proceed by picking an arbitrary node from the set of basic blocks that
    // do not have any incoming branches.
    first_block = *heads.begin();
    heads.erase(heads.begin());
  }
}

void CodeGen::ComputeRelativeJumps(BasicBlocks* basic_blocks,
                                   const TargetsToBlocks& targets_to_blocks) {
  // While we previously used pointers in jt_ptr and jf_ptr to link jump
  // instructions to their targets, we now convert these jumps to relative
  // jumps that are suitable for loading the BPF program into the kernel.
  int offset = 0;

  // Since we just completed a toposort, all jump targets are guaranteed to
  // go forward. This means, iterating over the basic blocks in reverse makes
  // it trivial to compute the correct offsets.
  BasicBlock* bb = NULL;
  BasicBlock* last_bb = NULL;
  for (BasicBlocks::reverse_iterator iter = basic_blocks->rbegin();
       iter != basic_blocks->rend();
       ++iter) {
    last_bb = bb;
    bb = *iter;
    Instruction* insn = bb->instructions.back();
    if (BPF_CLASS(insn->code) == BPF_JMP) {
      // Basic block ended in a jump instruction. We can now compute the
      // appropriate offsets.
      if (BPF_OP(insn->code) == BPF_JA) {
        // "Always" jumps use the 32bit "k" field for the offset, instead
        // of the 8bit "jt" and "jf" fields.
        int jmp = offset - targets_to_blocks.find(insn->jt_ptr)->second->offset;
        insn->k = jmp;
        insn->jt = insn->jf = 0;
      } else {
        // The offset computations for conditional jumps are just the same
        // as for "always" jumps.
        int jt = offset - targets_to_blocks.find(insn->jt_ptr)->second->offset;
        int jf = offset - targets_to_blocks.find(insn->jf_ptr)->second->offset;

        // There is an added complication, because conditional relative jumps
        // can only jump at most 255 instructions forward. If we have to jump
        // further, insert an extra "always" jump.
        Instructions::size_type jmp = bb->instructions.size();
        if (jt > 255 || (jt == 255 && jf > 255)) {
          Instruction* ja = MakeInstruction(BPF_JMP + BPF_JA, 0, insn->jt_ptr);
          bb->instructions.push_back(ja);
          ja->k = jt;
          ja->jt = ja->jf = 0;

          // The newly inserted "always" jump, of course, requires us to adjust
          // the jump targets in the original conditional jump.
          jt = 0;
          ++jf;
        }
        if (jf > 255) {
          Instruction* ja = MakeInstruction(BPF_JMP + BPF_JA, 0, insn->jf_ptr);
          bb->instructions.insert(bb->instructions.begin() + jmp, ja);
          ja->k = jf;
          ja->jt = ja->jf = 0;

          // Again, we have to adjust the jump targets in the original
          // conditional jump.
          ++jt;
          jf = 0;
        }

        // Now we can finally set the relative jump targets in the conditional
        // jump instruction. Afterwards, we must no longer access the jt_ptr
        // and jf_ptr fields.
        insn->jt = jt;
        insn->jf = jf;
      }
    } else if (BPF_CLASS(insn->code) != BPF_RET &&
               targets_to_blocks.find(insn->next)->second != last_bb) {
      SANDBOX_DIE("Internal compiler error; invalid basic block encountered");
    }

    // Proceed to next basic block.
    offset += bb->instructions.size();
    bb->offset = offset;
  }
  return;
}

void CodeGen::ConcatenateBasicBlocks(const BasicBlocks& basic_blocks,
                                     SandboxBPF::Program* program) {
  // Our basic blocks have been sorted and relative jump offsets have been
  // computed. The last remaining step is for all the instructions in our
  // basic blocks to be concatenated into a BPF program.
  program->clear();
  for (BasicBlocks::const_iterator bb_iter = basic_blocks.begin();
       bb_iter != basic_blocks.end();
       ++bb_iter) {
    const BasicBlock& bb = **bb_iter;
    for (Instructions::const_iterator insn_iter = bb.instructions.begin();
         insn_iter != bb.instructions.end();
         ++insn_iter) {
      const Instruction& insn = **insn_iter;
      program->push_back(
          (struct sock_filter) {insn.code, insn.jt, insn.jf, insn.k});
    }
  }
  return;
}

void CodeGen::Compile(Instruction* instructions, SandboxBPF::Program* program) {
  if (compiled_) {
    SANDBOX_DIE(
        "Cannot call Compile() multiple times. Create a new code "
        "generator instead");
  }
  compiled_ = true;

  BranchTargets branch_targets;
  FindBranchTargets(*instructions, &branch_targets);
  TargetsToBlocks all_blocks;
  BasicBlock* first_block =
      CutGraphIntoBasicBlocks(instructions, branch_targets, &all_blocks);
  MergeTails(&all_blocks);
  BasicBlocks basic_blocks;
  TopoSortBasicBlocks(first_block, all_blocks, &basic_blocks);
  ComputeRelativeJumps(&basic_blocks, all_blocks);
  ConcatenateBasicBlocks(basic_blocks, program);
  return;
}

}  // namespace sandbox
