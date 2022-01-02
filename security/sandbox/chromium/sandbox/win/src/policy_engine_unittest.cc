// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

#define POLPARAMS_BEGIN(x) sandbox::ParameterSet x[] = {
#define POLPARAM(p) sandbox::ParamPickerMake(p),
#define POLPARAMS_END }

namespace sandbox {

bool SetupNtdllImports();

TEST(PolicyEngineTest, Rules1) {
  SetupNtdllImports();

  // Construct two policy rules that say:
  //
  // #1
  // If the path is c:\\documents and settings\\* AND
  // If the creation mode is 'open existing' AND
  // If the security descriptor is null THEN
  // Ask the broker.
  //
  // #2
  // If the security descriptor is null AND
  // If the path ends with *.txt AND
  // If the creation mode is not 'create new' THEN
  // return Access Denied.

  enum FileCreateArgs {
    FileNameArg,
    CreationDispositionArg,
    FlagsAndAttributesArg,
    SecurityAttributes
  };

  const size_t policy_sz = 1024;
  PolicyBuffer* policy = reinterpret_cast<PolicyBuffer*>(new char[policy_sz]);
  OpcodeFactory opcode_maker(policy, policy_sz - 0x40);

  // Add rule set #1
  opcode_maker.MakeOpWStringMatch(FileNameArg, L"c:\\documents and settings\\",
                                  0, CASE_INSENSITIVE, kPolNone);
  opcode_maker.MakeOpNumberMatch(CreationDispositionArg, OPEN_EXISTING,
                                 kPolNone);
  opcode_maker.MakeOpVoidPtrMatch(SecurityAttributes, nullptr, kPolNone);
  opcode_maker.MakeOpAction(ASK_BROKER, kPolNone);

  // Add rule set #2
  opcode_maker.MakeOpWStringMatch(FileNameArg, L".TXT", kSeekToEnd,
                                  CASE_INSENSITIVE, kPolNone);
  opcode_maker.MakeOpNumberMatch(CreationDispositionArg, CREATE_NEW,
                                 kPolNegateEval);
  opcode_maker.MakeOpAction(FAKE_ACCESS_DENIED, kPolNone);
  policy->opcode_count = 7;

  const wchar_t* filename = L"c:\\Documents and Settings\\Microsoft\\BLAH.txt";
  uint32_t creation_mode = OPEN_EXISTING;
  uint32_t flags = FILE_ATTRIBUTE_NORMAL;
  void* security_descriptor = nullptr;

  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)
    POLPARAM(creation_mode)
    POLPARAM(flags)
    POLPARAM(security_descriptor)
  POLPARAMS_END;

  PolicyResult pr;
  PolicyProcessor pol_ev(policy);

  // Test should match the first rule set.
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, pr);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  // Test should still match the first rule set.
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, pr);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  // Changing creation_mode such that evaluation should not match any rule.
  creation_mode = CREATE_NEW;
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, pr);

  // Changing creation_mode such that evaluation should match rule #2.
  creation_mode = OPEN_ALWAYS;
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, pr);
  EXPECT_EQ(FAKE_ACCESS_DENIED, pol_ev.GetAction());

  delete[] reinterpret_cast<char*>(policy);
}

}  // namespace sandbox
