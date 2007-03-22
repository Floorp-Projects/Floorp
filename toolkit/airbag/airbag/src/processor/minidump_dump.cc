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

// minidump_dump.cc: Print the contents of a minidump file in somewhat
// readable text.
//
// Author: Mark Mentovai

#include <cstdio>

#include "google_airbag/processor/minidump.h"

namespace {

using google_airbag::Minidump;
using google_airbag::MinidumpThreadList;
using google_airbag::MinidumpModuleList;
using google_airbag::MinidumpMemoryList;
using google_airbag::MinidumpException;
using google_airbag::MinidumpSystemInfo;
using google_airbag::MinidumpMiscInfo;
using google_airbag::MinidumpAirbagInfo;

static bool PrintMinidumpDump(const char *minidump_file) {
  Minidump minidump(minidump_file);
  if (!minidump.Read()) {
    fprintf(stderr, "minidump.Read() failed\n");
    return false;
  }
  minidump.Print();

  int errors = 0;

  MinidumpThreadList *thread_list = minidump.GetThreadList();
  if (!thread_list) {
    ++errors;
    printf("minidump.GetThreadList() failed\n");
  } else {
    thread_list->Print();
  }

  MinidumpModuleList *module_list = minidump.GetModuleList();
  if (!module_list) {
    ++errors;
    printf("minidump.GetModuleList() failed\n");
  } else {
    module_list->Print();
  }

  MinidumpMemoryList *memory_list = minidump.GetMemoryList();
  if (!memory_list) {
    ++errors;
    printf("minidump.GetMemoryList() failed\n");
  } else {
    memory_list->Print();
  }

  MinidumpException *exception = minidump.GetException();
  if (!exception) {
    // Exception info is optional, so don't treat this as an error.
    printf("minidump.GetException() failed\n");
  } else {
    exception->Print();
  }

  MinidumpSystemInfo *system_info = minidump.GetSystemInfo();
  if (!system_info) {
    ++errors;
    printf("minidump.GetSystemInfo() failed\n");
  } else {
    system_info->Print();
  }

  MinidumpMiscInfo *misc_info = minidump.GetMiscInfo();
  if (!misc_info) {
    ++errors;
    printf("minidump.GetMiscInfo() failed\n");
  } else {
    misc_info->Print();
  }

  MinidumpAirbagInfo *airbag_info = minidump.GetAirbagInfo();
  if (!airbag_info) {
    // Airbag info is optional, so don't treat this as an error.
    printf("minidump.GetAirbagInfo() failed\n");
  } else {
    airbag_info->Print();
  }

  return errors == 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    return 1;
  }

  return PrintMinidumpDump(argv[1]) ? 0 : 1;
}
