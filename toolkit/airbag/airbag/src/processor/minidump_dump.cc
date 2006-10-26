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

#include <stdlib.h>
#include <stdio.h>

#include <string>

#include "processor/minidump.h"


using std::string;
using namespace google_airbag;


int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    exit(1);
  }

  Minidump minidump(argv[1]);
  if (!minidump.Read()) {
    printf("minidump.Read() failed\n");
    exit(1);
  }
  minidump.Print();

  int error = 0;

  MinidumpThreadList* threadList = minidump.GetThreadList();
  if (!threadList) {
    error |= 1 << 2;
    printf("minidump.GetThreadList() failed\n");
  } else {
    threadList->Print();
  }

  MinidumpModuleList* moduleList = minidump.GetModuleList();
  if (!moduleList) {
    error |= 1 << 3;
    printf("minidump.GetModuleList() failed\n");
  } else {
    moduleList->Print();
  }

  MinidumpMemoryList* memoryList = minidump.GetMemoryList();
  if (!memoryList) {
    error |= 1 << 4;
    printf("minidump.GetMemoryList() failed\n");
  } else {
    memoryList->Print();
  }

  MinidumpException* exception = minidump.GetException();
  if (!exception) {
    error |= 1 << 5;
    printf("minidump.GetException() failed\n");
  } else {
    exception->Print();
  }

  MinidumpSystemInfo* systemInfo = minidump.GetSystemInfo();
  if (!systemInfo) {
    error |= 1 << 6;
    printf("minidump.GetSystemInfo() failed\n");
  } else {
    systemInfo->Print();
  }

  MinidumpMiscInfo* miscInfo = minidump.GetMiscInfo();
  if (!miscInfo) {
    error |= 1 << 7;
    printf("minidump.GetMiscInfo() failed\n");
  } else {
    miscInfo->Print();
  }

  // Use return instead of exit to allow destructors to run.
  return(error);
}
