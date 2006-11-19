// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Unit test for MinidumpProcessor.  Uses a pre-generated minidump and
// corresponding symbol file, and checks the stack frames for correctness.

#include <string>
#include "google_airbag/processor/call_stack.h"
#include "google_airbag/processor/minidump.h"
#include "google_airbag/processor/minidump_processor.h"
#include "google_airbag/processor/process_state.h"
#include "google_airbag/processor/stack_frame.h"
#include "google_airbag/processor/symbol_supplier.h"
#include "processor/scoped_ptr.h"

namespace {

using std::string;
using google_airbag::CallStack;
using google_airbag::MinidumpModule;
using google_airbag::MinidumpProcessor;
using google_airbag::ProcessState;
using google_airbag::scoped_ptr;
using google_airbag::SymbolSupplier;

#define ASSERT_TRUE(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

class TestSymbolSupplier : public SymbolSupplier {
 public:
  virtual string GetSymbolFile(MinidumpModule *module);
};

string TestSymbolSupplier::GetSymbolFile(MinidumpModule *module) {
  if (*(module->GetName()) == "c:\\test_app.exe") {
    // The funny-looking pathname is so that the symbol file can also be
    // reached by a SimpleSymbolSupplier.
    return string(getenv("srcdir") ? getenv("srcdir") : ".") +
      "/src/processor/testdata/symbols/"
      "test_app.pdb/8DDB7E9A365748938D6EB08B1DCA31AA1/test_app.sym";
  }

  return "";
}

static bool RunTests() {
  TestSymbolSupplier supplier;
  MinidumpProcessor processor(&supplier);

  string minidump_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                         "/src/processor/testdata/minidump2.dmp";

  scoped_ptr<ProcessState> state(processor.Process(minidump_file));
  ASSERT_TRUE(state.get());
  ASSERT_EQ(state->cpu(), "x86");
  ASSERT_EQ(state->cpu_info(), "GenuineIntel family 6 model 13 stepping 8");
  ASSERT_EQ(state->os(), "Windows NT");
  ASSERT_EQ(state->os_version(), "5.1.2600 Service Pack 2");
  ASSERT_TRUE(state->crashed());
  ASSERT_EQ(state->crash_reason(), "EXCEPTION_ACCESS_VIOLATION");
  ASSERT_EQ(state->crash_address(), 0x45);
  ASSERT_EQ(state->threads()->size(), 1);
  ASSERT_EQ(state->requesting_thread(), 0);
  CallStack *stack = state->threads()->at(0);
  ASSERT_TRUE(stack);
  ASSERT_EQ(stack->frames()->size(), 4);

  ASSERT_EQ(stack->frames()->at(0)->module_base, 0x400000);
  ASSERT_EQ(stack->frames()->at(0)->module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(0)->function_name, "CrashFunction()");
  ASSERT_EQ(stack->frames()->at(0)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(0)->source_line, 51);

  ASSERT_EQ(stack->frames()->at(1)->module_base, 0x400000);
  ASSERT_EQ(stack->frames()->at(1)->module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(1)->function_name, "main");
  ASSERT_EQ(stack->frames()->at(1)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(1)->source_line, 56);

  // This comes from the CRT
  ASSERT_EQ(stack->frames()->at(2)->module_base, 0x400000);
  ASSERT_EQ(stack->frames()->at(2)->module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(2)->function_name, "__tmainCRTStartup");
  ASSERT_EQ(stack->frames()->at(2)->source_file_name,
            "f:\\rtm\\vctools\\crt_bld\\self_x86\\crt\\src\\crt0.c");
  ASSERT_EQ(stack->frames()->at(2)->source_line, 318);

  // No debug info available for kernel32.dll
  ASSERT_EQ(stack->frames()->at(3)->module_base, 0x7c800000);
  ASSERT_EQ(stack->frames()->at(3)->module_name,
            "C:\\WINDOWS\\system32\\kernel32.dll");
  ASSERT_TRUE(stack->frames()->at(3)->function_name.empty());
  ASSERT_TRUE(stack->frames()->at(3)->source_file_name.empty());
  ASSERT_EQ(stack->frames()->at(3)->source_line, 0);

  return true;
}

}  // namespace

int main(int argc, char *argv[]) {
  if (!RunTests()) {
    return 1;
  }

  return 0;
}
