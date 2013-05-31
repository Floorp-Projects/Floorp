/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * * This Source Code Form is subject to the terms of the Mozilla Public
 * * License, v. 2.0. If a copy of the MPL was not distributed with this
 * * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Attributes.h"
#include "mozilla/NullPtr.h"

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestCase;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

namespace mozilla {

// See gtest.h for method documentation
class MozillaPrinter : public EmptyTestEventListener
{
public:
  virtual void OnTestProgramStart(const UnitTest& /* aUnitTest */) MOZ_OVERRIDE {
    printf("TEST-INFO | GTest unit test starting\n");
  }
  virtual void OnTestProgramEnd(const UnitTest& aUnitTest) MOZ_OVERRIDE {
    printf("TEST-%s | GTest unit test: %s\n",
           aUnitTest.Passed() ? "PASS" : "UNEXPECTED-FAIL",
           aUnitTest.Passed() ? "passed" : "failed");
  }
  virtual void OnTestStart(const TestInfo& aTestInfo) MOZ_OVERRIDE {
    mTestInfo = &aTestInfo;
    printf("TEST-START | %s.%s\n",
        mTestInfo->test_case_name(), mTestInfo->name());
  }
  virtual void OnTestPartResult(const TestPartResult& aTestPartResult) MOZ_OVERRIDE {
    printf("TEST-%s | %s.%s | %s @ %s:%i\n",
           aTestPartResult.failed() ? "PASS" : "UNEXPECTED-FAIL",
           mTestInfo->test_case_name(), mTestInfo->name(),
           aTestPartResult.summary(),
           aTestPartResult.file_name(), aTestPartResult.line_number());
  }
  virtual void OnTestEnd(const TestInfo& aTestInfo) MOZ_OVERRIDE {
    printf("TEST-%s | %s.%s | test completed (time: %llims)\n",
           mTestInfo->result()->Passed() ? "PASS": "UNEXPECTED-FAIL",
           mTestInfo->test_case_name(), mTestInfo->name(),
           mTestInfo->result()->elapsed_time());
    mTestInfo = nullptr;
  }

  const TestInfo* mTestInfo;
};

static void ReplaceGTestLogger()
{
  // Replace the GTest logger so that it can be passed
  // by the mozilla test parsers.
  // Code is based on: http://googletest.googlecode.com/svn/trunk/samples/sample9_unittest.cc
  UnitTest& unitTest = *UnitTest::GetInstance();
  TestEventListeners& listeners = unitTest.listeners();
  delete listeners.Release(listeners.default_result_printer());

  listeners.Append(new MozillaPrinter);
}

int RunGTest()
{
  int c = 0;
  InitGoogleTest(&c, static_cast<char**>(nullptr));

  if (getenv("MOZ_TBPL_PARSER")) {
    ReplaceGTestLogger();
  }

  setenv("XPCOM_DEBUG_BREAK", "stack-and-abort", false);

  return RUN_ALL_TESTS();
}

}
