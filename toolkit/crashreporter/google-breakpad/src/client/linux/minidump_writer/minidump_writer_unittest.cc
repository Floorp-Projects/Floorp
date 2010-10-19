// Copyright (c) 2010 Google Inc.
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

#include <unistd.h>
#include <sys/syscall.h>

#include "client/linux/handler/exception_handler.h"
#include "client/linux/minidump_writer/minidump_writer.h"
#include "common/linux/eintr_wrapper.h"
#include "breakpad_googletest_includes.h"

using namespace google_breakpad;

namespace {
typedef testing::Test MinidumpWriterTest;
}

TEST(MinidumpWriterTest, Setup) {
  int fds[2];
  ASSERT_NE(-1, pipe(fds));

  const pid_t child = fork();
  if (child == 0) {
    close(fds[1]);
    char b;
    HANDLE_EINTR(read(fds[0], &b, sizeof(b)));
    close(fds[0]);
    syscall(__NR_exit);
  }
  close(fds[0]);

  ExceptionHandler::CrashContext context;
  memset(&context, 0, sizeof(context));

  char templ[] = "/tmp/minidump-writer-unittest-XXXXXX";
  mktemp(templ);
  ASSERT_TRUE(WriteMinidump(templ, child, &context, sizeof(context)));
  struct stat st;
  ASSERT_EQ(stat(templ, &st), 0);
  ASSERT_GT(st.st_size, 0u);
  unlink(templ);

  close(fds[1]);
}

TEST(MinidumpWriterTest, MappingInfo) {
  int fds[2];
  ASSERT_NE(-1, pipe(fds));

  // These are defined here so the parent can use them to check the
  // data from the minidump afterwards.
  const u_int32_t kMemorySize = sysconf(_SC_PAGESIZE);
  const char* kMemoryName = "a fake module";
  const u_int8_t kModuleGUID[sizeof(MDGUID)] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
  };
  char module_identifier_buffer[37];
  FileID::ConvertIdentifierToString(kModuleGUID,
                                    module_identifier_buffer,
                                    sizeof(module_identifier_buffer));
  string module_identifier(module_identifier_buffer);
  // Strip out dashes
  size_t pos;
  while ((pos = module_identifier.find('-')) != string::npos) {
    module_identifier.erase(pos, 1);
  }
  // And append a zero, because module IDs include an "age" field
  // which is always zero on Linux.
  module_identifier += "0";
  
  // Get some memory.
  char* memory =
    reinterpret_cast<char*>(mmap(NULL,
                                 kMemorySize,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON,
                                 -1,
                                 0));
  const u_int64_t kMemoryAddress = reinterpret_cast<u_int64_t>(memory);
  ASSERT_TRUE(memory);

  const pid_t child = fork();
  if (child == 0) {
    close(fds[1]);
    char b;
    HANDLE_EINTR(read(fds[0], &b, sizeof(b)));
    close(fds[0]);
    syscall(__NR_exit);
  }
  close(fds[0]);

  ExceptionHandler::CrashContext context;
  memset(&context, 0, sizeof(context));

  char templ[] = TEMPDIR "/minidump-writer-unittest-XXXXXX";
  mktemp(templ);

  // Add information about the mapped memory.
  MappingInfo info;
  info.start_addr = kMemoryAddress;
  info.size = kMemorySize;
  info.offset = 0;
  strcpy(info.name, kMemoryName);
  
  MappingList mappings;
  std::pair<MappingInfo, u_int8_t[sizeof(MDGUID)]> mapping;
  mapping.first = info;
  memcpy(mapping.second, kModuleGUID, sizeof(MDGUID));
  mappings.push_back(mapping);
  ASSERT_TRUE(WriteMinidump(templ, child, &context, sizeof(context), mappings));

  // Read the minidump. Load the module list, and ensure that
  // the mmap'ed |memory| is listed with the given module name
  // and debug ID.
  Minidump minidump(templ);
  ASSERT_TRUE(minidump.Read());

  MinidumpModuleList* module_list = minidump.GetModuleList();
  ASSERT_TRUE(module_list);
  const MinidumpModule* module =
    module_list->GetModuleForAddress(kMemoryAddress);
  ASSERT_TRUE(module);

  EXPECT_EQ(kMemoryAddress, module->base_address());
  EXPECT_EQ(kMemorySize, module->size());
  EXPECT_EQ(kMemoryName, module->code_file());
  EXPECT_EQ(module_identifier, module->debug_identifier());

  unlink(templ);
  close(fds[1]);
}
