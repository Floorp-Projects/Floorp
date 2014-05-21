// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BASICBLOCK_H__
#define SANDBOX_LINUX_SECCOMP_BPF_BASICBLOCK_H__

#include <vector>

#include "sandbox/linux/seccomp-bpf/instruction.h"

namespace sandbox {

struct BasicBlock {
  BasicBlock();
  ~BasicBlock();

  // Our implementation of the code generator uses a "Less" operator to
  // identify common sequences of basic blocks. This would normally be
  // really easy to do, but STL requires us to wrap the comparator into
  // a class. We begrudgingly add some code here that provides this wrapping.
  template <class T>
  class Less {
   public:
    Less(const T& data,
         int (*cmp)(const BasicBlock*, const BasicBlock*, const T& data))
        : data_(data), cmp_(cmp) {}

    bool operator()(const BasicBlock* a, const BasicBlock* b) const {
      return cmp_(a, b, data_) < 0;
    }

   private:
    const T& data_;
    int (*cmp_)(const BasicBlock*, const BasicBlock*, const T&);
  };

  // Basic blocks are essentially nothing more than a set of instructions.
  std::vector<Instruction*> instructions;

  // In order to compute relative branch offsets we need to keep track of
  // how far our block is away from the very last basic block. The "offset_"
  // is measured in number of BPF instructions.
  int offset;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BASICBLOCK_H__
