/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GTestRunner.h"
#include "gtest/gtest.h"
#include "mozilla/Attributes.h"
#include "nsICrashReporter.h"
#include "testing/TestHarness.h"
#include "prenv.h"
#ifdef ANDROID
#  include <android/log.h>
#endif
#ifdef XP_WIN
#  include "mozilla/ipc/WindowsMessageLoop.h"
#endif

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

namespace mozilla {

#ifdef ANDROID
#  define MOZ_STDOUT_PRINT(...) \
    __android_log_print(ANDROID_LOG_INFO, "gtest", __VA_ARGS__);
#else
#  define MOZ_STDOUT_PRINT(...) printf(__VA_ARGS__);
#endif

#define MOZ_PRINT(...)              \
  MOZ_STDOUT_PRINT(__VA_ARGS__);    \
  if (mLogFile) {                   \
    fprintf(mLogFile, __VA_ARGS__); \
  }

// See gtest.h for method documentation
class MozillaPrinter : public EmptyTestEventListener {
 public:
  MozillaPrinter() : mLogFile(nullptr) {
    char* path = PR_GetEnv("MOZ_GTEST_LOG_PATH");
    if (path) {
      mLogFile = fopen(path, "w");
    }
  }
  virtual void OnTestProgramStart(const UnitTest& /* aUnitTest */) override {
    MOZ_PRINT("TEST-INFO | GTest unit test starting\n");
  }
  virtual void OnTestProgramEnd(const UnitTest& aUnitTest) override {
    MOZ_PRINT("TEST-%s | GTest unit test: %s\n",
              aUnitTest.Passed() ? "PASS" : "UNEXPECTED-FAIL",
              aUnitTest.Passed() ? "passed" : "failed");
    MOZ_PRINT("Passed: %d\n", aUnitTest.successful_test_count());
    MOZ_PRINT("Failed: %d\n", aUnitTest.failed_test_count());
    if (mLogFile) {
      fclose(mLogFile);
      mLogFile = nullptr;
    }
  }
  virtual void OnTestStart(const TestInfo& aTestInfo) override {
    mTestInfo = &aTestInfo;
    MOZ_PRINT("TEST-START | %s.%s\n", mTestInfo->test_case_name(),
              mTestInfo->name());
  }
  virtual void OnTestPartResult(
      const TestPartResult& aTestPartResult) override {
    MOZ_PRINT("TEST-%s | %s.%s | %s @ %s:%i\n",
              !aTestPartResult.failed() ? "PASS" : "UNEXPECTED-FAIL",
              mTestInfo ? mTestInfo->test_case_name() : "?",
              mTestInfo ? mTestInfo->name() : "?", aTestPartResult.summary(),
              aTestPartResult.file_name(), aTestPartResult.line_number());
  }
  virtual void OnTestEnd(const TestInfo& aTestInfo) override {
    MOZ_PRINT("TEST-%s | %s.%s | test completed (time: %" PRIi64 "ms)\n",
              aTestInfo.result()->Passed() ? "PASS" : "UNEXPECTED-FAIL",
              aTestInfo.test_case_name(), aTestInfo.name(),
              aTestInfo.result()->elapsed_time());
    MOZ_ASSERT(&aTestInfo == mTestInfo);
    mTestInfo = nullptr;
  }

  const TestInfo* mTestInfo;
  FILE* mLogFile;
};

static void ReplaceGTestLogger() {
  // Replace the GTest logger so that it can be passed
  // by the mozilla test parsers.
  // Code is based on:
  // http://googletest.googlecode.com/svn/trunk/samples/sample9_unittest.cc
  UnitTest& unitTest = *UnitTest::GetInstance();
  TestEventListeners& listeners = unitTest.listeners();
  delete listeners.Release(listeners.default_result_printer());

  listeners.Append(new MozillaPrinter);
}

int RunGTestFunc(int* argc, char** argv) {
  InitGoogleTest(argc, argv);

  if (getenv("MOZ_TBPL_PARSER")) {
    ReplaceGTestLogger();
  }

  PR_SetEnv("XPCOM_DEBUG_BREAK=stack-and-abort");

  ScopedXPCOM xpcom("GTest");

#ifdef XP_WIN
  mozilla::ipc::windows::InitUIThread();
#endif
#ifdef ANDROID
  // On Android, gtest is running in an application, which uses a
  // current working directory of '/' by default. Desktop tests
  // sometimes assume that support files are in the current
  // working directory. For compatibility with desktop, the Android
  // harness pushes test support files to the device at the location
  // specified by MOZ_GTEST_CWD and gtest changes the cwd to that
  // location.
  char* path = PR_GetEnv("MOZ_GTEST_CWD");
  chdir(path);
#endif
  nsCOMPtr<nsICrashReporter> crashreporter;
  char* crashreporterStr = PR_GetEnv("MOZ_CRASHREPORTER");
  if (crashreporterStr && !strcmp(crashreporterStr, "1")) {
    // TODO: move this to an even-more-common location to use in all
    // C++ unittests
    crashreporter = do_GetService("@mozilla.org/toolkit/crash-reporter;1");
    if (crashreporter) {
      printf_stderr("Setting up crash reporting\n");
      char* path = PR_GetEnv("MOZ_GTEST_MINIDUMPS_PATH");
      nsCOMPtr<nsIFile> file;
      if (path) {
        nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(path), true,
                                      getter_AddRefs(file));
        if (NS_FAILED(rv)) {
          printf_stderr("Ignoring invalid MOZ_GTEST_MINIDUMPS_PATH\n");
        }
      }
      if (!file) {
        nsCOMPtr<nsIProperties> dirsvc =
            do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
        nsresult rv = dirsvc->Get(NS_OS_CURRENT_WORKING_DIR,
                                  NS_GET_IID(nsIFile), getter_AddRefs(file));
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
      }
      crashreporter->SetEnabled(true);
      crashreporter->SetMinidumpPath(file);
    }
  }

  return RUN_ALL_TESTS();
}

// We use a static var 'RunGTest' defined in nsAppRunner.cpp.
// RunGTest is initialized to nullptr but if GTest (this file)
// is linked in then RunGTest will be set here indicating
// GTest is supported.
class _InitRunGTest {
 public:
  _InitRunGTest() { RunGTest = RunGTestFunc; }
} InitRunGTest;

}  // namespace mozilla
