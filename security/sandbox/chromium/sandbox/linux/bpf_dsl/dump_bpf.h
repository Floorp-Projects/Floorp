// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {

class SANDBOX_EXPORT DumpBPF {
 public:
  // PrintProgram writes |program| in a human-readable format to stderr.
  static void PrintProgram(const CodeGen::Program& program);
};

}  // namespace bpf_dsl
}  // namespace sandbox
