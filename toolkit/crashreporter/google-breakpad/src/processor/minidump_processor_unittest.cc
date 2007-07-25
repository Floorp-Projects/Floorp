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

#include <cstdlib>
#include <string>
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "google_breakpad/processor/stack_frame.h"
#include "google_breakpad/processor/symbol_supplier.h"
#include "processor/scoped_ptr.h"

namespace {

using std::string;
using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::MinidumpProcessor;
using google_breakpad::ProcessState;
using google_breakpad::scoped_ptr;
using google_breakpad::SymbolSupplier;
using google_breakpad::SystemInfo;

static const char *kSystemInfoOS = "Windows NT";
static const char *kSystemInfoOSShort = "windows";
static const char *kSystemInfoOSVersion = "5.1.2600 Service Pack 2";
static const char *kSystemInfoCPU = "x86";
static const char *kSystemInfoCPUInfo =
    "GenuineIntel family 6 model 13 stepping 8";

#define ASSERT_TRUE(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

// Use ASSERT_*_ABORT in functions that can't return a boolean.
#define ASSERT_TRUE_ABORT(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    abort(); \
  }

#define ASSERT_EQ_ABORT(e1, e2) ASSERT_TRUE_ABORT((e1) == (e2))

class TestSymbolSupplier : public SymbolSupplier {
 public:
  TestSymbolSupplier() : interrupt_(false) {}

  virtual SymbolResult GetSymbolFile(const CodeModule *module,
                                     const SystemInfo *system_info,
                                     string *symbol_file);

  // When set to true, causes the SymbolSupplier to return INTERRUPT
  void set_interrupt(bool interrupt) { interrupt_ = interrupt; }

 private:
  bool interrupt_;
};

SymbolSupplier::SymbolResult TestSymbolSupplier::GetSymbolFile(
    const CodeModule *module,
    const SystemInfo *system_info,
    string *symbol_file) {
  ASSERT_TRUE_ABORT(module);
  ASSERT_TRUE_ABORT(system_info);
  ASSERT_EQ_ABORT(system_info->cpu, kSystemInfoCPU);
  ASSERT_EQ_ABORT(system_info->cpu_info, kSystemInfoCPUInfo);
  ASSERT_EQ_ABORT(system_info->os, kSystemInfoOS);
  ASSERT_EQ_ABORT(system_info->os_short, kSystemInfoOSShort);
  ASSERT_EQ_ABORT(system_info->os_version, kSystemInfoOSVersion);

  if (interrupt_) {
    return INTERRUPT;
  }

  if (module && module->code_file() == "c:\\test_app.exe") {
      *symbol_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                     "/src/processor/testdata/symbols/test_app.pdb/" +
                     module->debug_identifier() +
                     "/test_app.sym";
    return FOUND;
  }

  return NOT_FOUND;
}

static bool RunTests() {
  TestSymbolSupplier supplier;
  BasicSourceLineResolver resolver;
  MinidumpProcessor processor(&supplier, &resolver);

  string minidump_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                         "/src/processor/testdata/minidump2.dmp";

  ProcessState state;
  ASSERT_EQ(processor.Process(minidump_file, &state),
            MinidumpProcessor::PROCESS_OK);
  ASSERT_EQ(state.system_info()->os, kSystemInfoOS);
  ASSERT_EQ(state.system_info()->os_short, kSystemInfoOSShort);
  ASSERT_EQ(state.system_info()->os_version, kSystemInfoOSVersion);
  ASSERT_EQ(state.system_info()->cpu, kSystemInfoCPU);
  ASSERT_EQ(state.system_info()->cpu_info, kSystemInfoCPUInfo);
  ASSERT_TRUE(state.crashed());
  ASSERT_EQ(state.crash_reason(), "EXCEPTION_ACCESS_VIOLATION");
  ASSERT_EQ(state.crash_address(), 0x45);
  ASSERT_EQ(state.threads()->size(), 1);
  ASSERT_EQ(state.requesting_thread(), 0);

  CallStack *stack = state.threads()->at(0);
  ASSERT_TRUE(stack);
  ASSERT_EQ(stack->frames()->size(), 4);

  ASSERT_TRUE(stack->frames()->at(0)->module);
  ASSERT_EQ(stack->frames()->at(0)->module->base_address(), 0x400000);
  ASSERT_EQ(stack->frames()->at(0)->module->code_file(), "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(0)->function_name,
            "`anonymous namespace'::CrashFunction");
  ASSERT_EQ(stack->frames()->at(0)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(0)->source_line, 58);

  ASSERT_TRUE(stack->frames()->at(1)->module);
  ASSERT_EQ(stack->frames()->at(1)->module->base_address(), 0x400000);
  ASSERT_EQ(stack->frames()->at(1)->module->code_file(), "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(1)->function_name, "main");
  ASSERT_EQ(stack->frames()->at(1)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(1)->source_line, 65);

  // This comes from the CRT
  ASSERT_TRUE(stack->frames()->at(2)->module);
  ASSERT_EQ(stack->frames()->at(2)->module->base_address(), 0x400000);
  ASSERT_EQ(stack->frames()->at(2)->module->code_file(), "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(2)->function_name, "__tmainCRTStartup");
  ASSERT_EQ(stack->frames()->at(2)->source_file_name,
            "f:\\sp\\vctools\\crt_bld\\self_x86\\crt\\src\\crt0.c");
  ASSERT_EQ(stack->frames()->at(2)->source_line, 327);

  // No debug info available for kernel32.dll
  ASSERT_TRUE(stack->frames()->at(3)->module);
  ASSERT_EQ(stack->frames()->at(3)->module->base_address(), 0x7c800000);
  ASSERT_EQ(stack->frames()->at(3)->module->code_file(),
            "C:\\WINDOWS\\system32\\kernel32.dll");
  ASSERT_TRUE(stack->frames()->at(3)->function_name.empty());
  ASSERT_TRUE(stack->frames()->at(3)->source_file_name.empty());
  ASSERT_EQ(stack->frames()->at(3)->source_line, 0);

  ASSERT_EQ(state.modules()->module_count(), 13);
  ASSERT_TRUE(state.modules()->GetMainModule());
  ASSERT_EQ(state.modules()->GetMainModule()->code_file(), "c:\\test_app.exe");
  ASSERT_FALSE(state.modules()->GetModuleForAddress(0));
  ASSERT_EQ(state.modules()->GetMainModule(),
            state.modules()->GetModuleForAddress(0x400000));
  ASSERT_EQ(state.modules()->GetModuleForAddress(0x7c801234)->debug_file(),
            "kernel32.pdb");
  ASSERT_EQ(state.modules()->GetModuleForAddress(0x77d43210)->version(),
            "5.1.2600.2622");

  // Test that the symbol supplier can interrupt processing
  state.Clear();
  supplier.set_interrupt(true);
  ASSERT_EQ(processor.Process(minidump_file, &state),
            MinidumpProcessor::PROCESS_INTERRUPTED);

  return true;
}

}  // namespace

int main(int argc, char *argv[]) {
  if (!RunTests()) {
    return 1;
  }

  return 0;
}
