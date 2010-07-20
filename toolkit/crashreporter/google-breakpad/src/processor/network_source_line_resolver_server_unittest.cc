// Copyright (c) 2010, Google Inc.
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

// Full system test for NetworkSourceLineResolver / NetworkSourceLineServer.
// Forks a background process to run a NetworkSourceLineServer, then
// instantiates a MinidumpProcessor with a NetworkSourceLineResolver to
// connect to the background server and process a minidump.

#include <string>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "breakpad_googletest_includes.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "google_breakpad/processor/network_source_line_resolver.h"
#include "processor/simple_symbol_supplier.h"
#include "processor/network_source_line_server.h"
#include "processor/simple_symbol_supplier.h"
#include "processor/udp_network.h"

namespace {

using std::string;
using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::MinidumpProcessor;
using google_breakpad::NetworkSourceLineResolver;
using google_breakpad::NetworkSourceLineServer;
using google_breakpad::ProcessState;
using google_breakpad::SimpleSymbolSupplier;
using google_breakpad::UDPNetwork;

static const char *kSystemInfoOS = "Windows NT";
static const char *kSystemInfoOSShort = "windows";
static const char *kSystemInfoOSVersion = "5.1.2600 Service Pack 2";
static const char *kSystemInfoCPU = "x86";
static const char *kSystemInfoCPUInfo =
    "GenuineIntel family 6 model 13 stepping 8";

bool exitProcess = false;

void signal_handler(int signal) {
  if (signal == SIGINT)
    exitProcess = true;
}

void RunSourceLineServer(int fd) {
  // Set a signal handler so the parent process
  // can signal this process to end.
  signal(SIGINT, signal_handler);

  BasicSourceLineResolver resolver;
  SimpleSymbolSupplier supplier(string(getenv("srcdir") ?
                                       getenv("srcdir") : ".") +
                                "/src/processor/testdata/symbols/");
  UDPNetwork net("localhost",
                 0,    // pick a free port
                 true); // IPv4 only

  NetworkSourceLineServer server(&supplier, &resolver, &net,
                                 0);   // no source line limit
  unsigned short port = -1;
  bool initialized = server.Initialize();
  if (initialized)
    port = net.port();

  // send port number back to parent
  ssize_t written = write(fd, &port, sizeof(port));
  close(fd);

  if (!initialized || written != sizeof(port))
    return;

  while (!exitProcess) {
    server.RunOnce(100);
  }
}

TEST(NetworkSourceLineResolverServer, SystemTest) {
  int fds[2];
  ASSERT_EQ(0, pipe(fds));
  // Fork a background process to run the server.
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    RunSourceLineServer(fds[1]);
    exit(0);
  }
  ASSERT_NE(-1, pid);
  // Wait for the background process to return info about the port.
  close(fds[1]);
  unsigned short port;
  ssize_t nbytes = read(fds[0], &port, sizeof(port));
  ASSERT_EQ(sizeof(port), nbytes);
  ASSERT_NE(-1, port);

  NetworkSourceLineResolver resolver("localhost", port,
                                     5000); // wait at most 5 seconds for reply
  MinidumpProcessor processor(&resolver, &resolver);
  // this is all copied from minidump_processor_unittest.cc
  string minidump_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                         "/src/processor/testdata/minidump2.dmp";

  ProcessState state;
  ASSERT_EQ(processor.Process(minidump_file, &state),
            google_breakpad::PROCESS_OK);
  ASSERT_EQ(state.system_info()->os, kSystemInfoOS);
  ASSERT_EQ(state.system_info()->os_short, kSystemInfoOSShort);
  ASSERT_EQ(state.system_info()->os_version, kSystemInfoOSVersion);
  ASSERT_EQ(state.system_info()->cpu, kSystemInfoCPU);
  ASSERT_EQ(state.system_info()->cpu_info, kSystemInfoCPUInfo);
  ASSERT_TRUE(state.crashed());
  ASSERT_EQ(state.crash_reason(), "EXCEPTION_ACCESS_VIOLATION_WRITE");
  ASSERT_EQ(state.crash_address(), 0x45U);
  ASSERT_EQ(state.threads()->size(), size_t(1));
  ASSERT_EQ(state.requesting_thread(), 0);

  CallStack *stack = state.threads()->at(0);
  ASSERT_TRUE(stack);
  ASSERT_EQ(stack->frames()->size(), 4U);

  ASSERT_TRUE(stack->frames()->at(0)->module);
  ASSERT_EQ(stack->frames()->at(0)->module->base_address(), 0x400000U);
  ASSERT_EQ(stack->frames()->at(0)->module->code_file(), "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(0)->function_name,
            "`anonymous namespace'::CrashFunction");
  ASSERT_EQ(stack->frames()->at(0)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(0)->source_line, 58);

  ASSERT_TRUE(stack->frames()->at(1)->module);
  ASSERT_EQ(stack->frames()->at(1)->module->base_address(), 0x400000U);
  ASSERT_EQ(stack->frames()->at(1)->module->code_file(), "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(1)->function_name, "main");
  ASSERT_EQ(stack->frames()->at(1)->source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack->frames()->at(1)->source_line, 65);

  // This comes from the CRT
  ASSERT_TRUE(stack->frames()->at(2)->module);
  ASSERT_EQ(stack->frames()->at(2)->module->base_address(), 0x400000U);
  ASSERT_EQ(stack->frames()->at(2)->module->code_file(), "c:\\test_app.exe");
  ASSERT_EQ(stack->frames()->at(2)->function_name, "__tmainCRTStartup");
  ASSERT_EQ(stack->frames()->at(2)->source_file_name,
            "f:\\sp\\vctools\\crt_bld\\self_x86\\crt\\src\\crt0.c");
  ASSERT_EQ(stack->frames()->at(2)->source_line, 327);

  // OS frame, kernel32.dll
  ASSERT_TRUE(stack->frames()->at(3)->module);
  ASSERT_EQ(stack->frames()->at(3)->module->base_address(), 0x7c800000U);
  ASSERT_EQ(stack->frames()->at(3)->module->code_file(),
            "C:\\WINDOWS\\system32\\kernel32.dll");
  ASSERT_EQ(stack->frames()->at(3)->function_name, "BaseProcessStart");
  ASSERT_TRUE(stack->frames()->at(3)->source_file_name.empty());
  ASSERT_EQ(stack->frames()->at(3)->source_line, 0);

  ASSERT_EQ(state.modules()->module_count(), 13U);
  ASSERT_TRUE(state.modules()->GetMainModule());
  ASSERT_EQ(state.modules()->GetMainModule()->code_file(), "c:\\test_app.exe");
  ASSERT_FALSE(state.modules()->GetModuleForAddress(0));
  ASSERT_EQ(state.modules()->GetMainModule(),
            state.modules()->GetModuleForAddress(0x400000));
  ASSERT_EQ(state.modules()->GetModuleForAddress(0x7c801234)->debug_file(),
            "kernel32.pdb");
  ASSERT_EQ(state.modules()->GetModuleForAddress(0x77d43210)->version(),
            "5.1.2600.2622");

  // Kill background process
  kill(pid, SIGINT);
  ASSERT_EQ(pid, waitpid(pid, NULL, 0));
}

}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
