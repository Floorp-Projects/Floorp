// Copyright 2009, Google Inc.
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

#include <windows.h>
#include <dbghelp.h>
#include <strsafe.h>
#include <objbase.h>
#include <shellapi.h>

#include "../../../breakpad_googletest_includes.h"
#include "../crash_generation/crash_generation_server.h"
#include "../handler/exception_handler.h"

namespace {
const wchar_t kPipeName[] = L"\\\\.\\pipe\\BreakpadCrashTest\\TestCaseServer";
const char kSuccessIndicator[] = "success";
const char kFailureIndicator[] = "failure";

// Utility function to test for a path's existence.
BOOL DoesPathExist(const TCHAR *path_name);

class ExceptionHandlerDeathTest : public ::testing::Test {
 protected:
  // Member variable for each test that they can use
  // for temporary storage.
  TCHAR temp_path_[MAX_PATH];
  // Actually constructs a temp path name.
  virtual void SetUp();
  // A helper method that tests can use to crash.
  void DoCrashAccessViolation();
  void DoCrashPureVirtualCall();
};

void ExceptionHandlerDeathTest::SetUp() {
  const ::testing::TestInfo* const test_info =
    ::testing::UnitTest::GetInstance()->current_test_info();
  TCHAR temp_path[MAX_PATH] = { '\0' };
  TCHAR test_name_wide[MAX_PATH] = { '\0' };
  // We want the temporary directory to be what the OS returns
  // to us, + the test case name.
  GetTempPath(MAX_PATH, temp_path);
  // THe test case name is exposed to use as a c-style string,
  // But we might be working in UNICODE here on Windows.
  int dwRet = MultiByteToWideChar(CP_ACP, 0, test_info->name(),
                                  strlen(test_info->name()),
                                  test_name_wide,
                                  MAX_PATH);
  if (!dwRet) {
    assert(false);
  }
  StringCchPrintfW(temp_path_, MAX_PATH, L"%s%s", temp_path, test_name_wide);
  CreateDirectory(temp_path_, NULL);
}

BOOL DoesPathExist(const TCHAR *path_name) {
  DWORD flags = GetFileAttributes(path_name);
  if (flags == INVALID_FILE_ATTRIBUTES) {
    return FALSE;
  }
  return TRUE;
}

bool MinidumpWrittenCallback(const wchar_t* dump_path,
                             const wchar_t* minidump_id,
                             void* context,
                             EXCEPTION_POINTERS* exinfo,
                             MDRawAssertionInfo* assertion,
                             bool succeeded) {
  if (succeeded && DoesPathExist(dump_path)) {
    fprintf(stderr, kSuccessIndicator);
  } else {
    fprintf(stderr, kFailureIndicator);
  }
  // If we don't flush, the output doesn't get sent before
  // this process dies.
  fflush(stderr);
  return succeeded;
}

TEST_F(ExceptionHandlerDeathTest, InProcTest) {
  // For the in-proc test, we just need to instantiate an exception
  // handler in in-proc mode, and crash.   Since the entire test is
  // reexecuted in the child process, we don't have to worry about
  // the semantics of the exception handler being inherited/not
  // inherited across CreateProcess().
  ASSERT_TRUE(DoesPathExist(temp_path_));
  google_breakpad::ExceptionHandler *exc =
    new google_breakpad::ExceptionHandler(
    temp_path_, NULL, &MinidumpWrittenCallback, NULL,
    google_breakpad::ExceptionHandler::HANDLER_ALL);
  int *i = NULL;
  ASSERT_DEATH((*i)++, kSuccessIndicator);
  delete exc;
}

static bool gDumpCallbackCalled = false;

void clientDumpCallback(void *dump_context,
                        const google_breakpad::ClientInfo *client_info,
                        const std::wstring *dump_path) {
  gDumpCallbackCalled = true;
}

void ExceptionHandlerDeathTest::DoCrashAccessViolation() {
  google_breakpad::ExceptionHandler *exc =
    new google_breakpad::ExceptionHandler(
    temp_path_, NULL, NULL, NULL,
    google_breakpad::ExceptionHandler::HANDLER_ALL, MiniDumpNormal, kPipeName,
    NULL);
  // Although this is executing in the child process of the death test,
  // if it's not true we'll still get an error rather than the crash
  // being expected.
  ASSERT_TRUE(exc->IsOutOfProcess());
  int *i = NULL;
  printf("%d\n", (*i)++);
}

TEST_F(ExceptionHandlerDeathTest, OutOfProcTest) {
  // We can take advantage of a detail of google test here to save some
  // complexity in testing: when you do a death test, it actually forks.
  // So we can make the main test harness the crash generation server,
  // and call ASSERT_DEATH on a NULL dereference, it to expecting test
  // the out of process scenario, since it's happening in a different
  // process!  This is different from the above because, above, we pass
  // a NULL pipe name, and we also don't start a crash generation server.

  ASSERT_TRUE(DoesPathExist(temp_path_));
  std::wstring dump_path(temp_path_);
  google_breakpad::CrashGenerationServer server(
    kPipeName, NULL, NULL, NULL, &clientDumpCallback, NULL, NULL, NULL, true,
    &dump_path);

  // This HAS to be EXPECT_, because when this test case is executed in the
  // child process, the server registration will fail due to the named pipe
  // being the same.
  EXPECT_TRUE(server.Start());
  EXPECT_FALSE(gDumpCallbackCalled);
  ASSERT_DEATH(this->DoCrashAccessViolation(), "");
  EXPECT_TRUE(gDumpCallbackCalled);
}

TEST_F(ExceptionHandlerDeathTest, InvalidParameterTest) {
  using google_breakpad::ExceptionHandler;

  ASSERT_TRUE(DoesPathExist(temp_path_));
  ExceptionHandler handler(temp_path_, NULL, NULL, NULL,
                           ExceptionHandler::HANDLER_INVALID_PARAMETER);

  // Disable the message box for assertions
  _CrtSetReportMode(_CRT_ASSERT, 0);

  // Call with a bad argument. The invalid parameter will be swallowed
  // and a dump will be generated, the process will exit(0).
  ASSERT_EXIT(printf(NULL), ::testing::ExitedWithCode(0), "");
}


struct PureVirtualCallBase {
  PureVirtualCallBase() {
    // We have to reinterpret so the linker doesn't get confused because the
    // method isn't defined.
    reinterpret_cast<PureVirtualCallBase*>(this)->PureFunction();
  }
  virtual ~PureVirtualCallBase() {}
  virtual void PureFunction() const = 0;
};
struct PureVirtualCall : public PureVirtualCallBase {
  PureVirtualCall() { PureFunction(); }
  virtual void PureFunction() const {}
};

void ExceptionHandlerDeathTest::DoCrashPureVirtualCall() {
  PureVirtualCall instance;
}

TEST_F(ExceptionHandlerDeathTest, PureVirtualCallTest) {
  using google_breakpad::ExceptionHandler;

  ASSERT_TRUE(DoesPathExist(temp_path_));
  ExceptionHandler handler(temp_path_, NULL, NULL, NULL,
                           ExceptionHandler::HANDLER_PURECALL);

  // Disable the message box for assertions
  _CrtSetReportMode(_CRT_ASSERT, 0);

  // Calls a pure virtual function.
  EXPECT_EXIT(DoCrashPureVirtualCall(), ::testing::ExitedWithCode(0), "");
}
}
