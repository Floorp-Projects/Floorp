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

// This file is used to generate minidump2.dmp and minidump2.sym.
// cl /Zi /Fetest_app.exe test_app.cc dbghelp.lib
// Then run test_app to generate a dump, and dump_syms to create the .sym file.

#include <windows.h>
#include <dbghelp.h>

static LONG HandleException(EXCEPTION_POINTERS *exinfo) {
  HANDLE dump_file = CreateFile("dump.dmp",
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                NULL,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

  MINIDUMP_EXCEPTION_INFORMATION except_info;
  except_info.ThreadId = GetCurrentThreadId();
  except_info.ExceptionPointers = exinfo;
  except_info.ClientPointers = false;
  
  MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    dump_file,
                    MiniDumpNormal,
                    &except_info,
                    NULL,
                    NULL);

  CloseHandle(dump_file);
  return EXCEPTION_EXECUTE_HANDLER;
}

void CrashFunction() {
  int *i = NULL;
  *i = 5;  // crash!
}

int main(int argc, char *argv[]) {
  __try {
    CrashFunction();
  } __except(HandleException(GetExceptionInformation())) {
  }
  return 0;
}
