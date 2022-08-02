/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/GeckoArgs.h"

using namespace mozilla;
using namespace mozilla::geckoargs;

static CommandLineArg<const char*> kCharParam{"-charParam", "charparam"};
static CommandLineArg<uint64_t> kUint64Param{"-Uint64Param", "uint64param"};
static CommandLineArg<bool> kFlag{"-Flag", "flag"};

template <size_t N>
bool CheckArgv(char** aArgv, const char* const (&aExpected)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (aArgv[i] == nullptr && aExpected[i] == nullptr) {
      return true;
    }
    if (aArgv[i] == nullptr || aExpected[i] == nullptr) {
      return false;
    }
    if (strcmp(aArgv[i], aExpected[i]) != 0) {
      return false;
    }
  }
  return true;
}

char kFirefox[] = "$HOME/bin/firefox/firefox-bin";

TEST(GeckoArgs, const_char_ptr)
{
  char kCharParamStr[] = "-charParam";
  char kCharParamValue[] = "paramValue";

  {
    char* argv[] = {kFirefox, kCharParamStr, kCharParamValue, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 4);

    Maybe<const char*> charParam = kCharParam.Get(argc, argv);
    EXPECT_TRUE(charParam.isSome());
    EXPECT_EQ(*charParam, kCharParamValue);

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    char kBlahBlah[] = "-blahblah";
    char* argv[] = {kFirefox, kCharParamStr, kBlahBlah, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 4);

    Maybe<const char*> charParam = kCharParam.Get(argc, argv);
    EXPECT_TRUE(charParam.isNothing());

    const char* const expArgv[] = {kFirefox, kBlahBlah, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    std::vector<std::string> extraArgs;
    EXPECT_EQ(extraArgs.size(), 0U);
    kCharParam.Put("ParamValue", extraArgs);
    EXPECT_EQ(extraArgs.size(), 2U);
    EXPECT_EQ(extraArgs[0], "-charParam");
    EXPECT_EQ(extraArgs[1], "ParamValue");
  }
  { EXPECT_EQ(kCharParam.Name(), "-charParam"); }
}

TEST(GeckoArgs, uint64)
{
  char kUint64ParamStr[] = "-Uint64Param";

  {
    char* argv[] = {kFirefox, kUint64ParamStr, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 3);

    Maybe<uint64_t> uint64Param = kUint64Param.Get(argc, argv);
    EXPECT_TRUE(uint64Param.isNothing());

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    char* argv[] = {kFirefox, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 2);

    Maybe<uint64_t> uint64Param = kUint64Param.Get(argc, argv);
    EXPECT_TRUE(uint64Param.isNothing());

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    char kUint64ParamValue[] = "42";
    char* argv[] = {kFirefox, kUint64ParamStr, kUint64ParamValue, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 4);

    Maybe<uint64_t> uint64Param = kUint64Param.Get(argc, argv);
    EXPECT_TRUE(uint64Param.isSome());
    EXPECT_EQ(*uint64Param, 42U);

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    char kUint64ParamValue[] = "aa";
    char* argv[] = {kFirefox, kUint64ParamStr, kUint64ParamValue, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 4);

    Maybe<uint64_t> uint64Param = kUint64Param.Get(argc, argv);
    EXPECT_TRUE(uint64Param.isNothing());

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    std::vector<std::string> extraArgs;
    EXPECT_EQ(extraArgs.size(), 0U);
    kUint64Param.Put(1234, extraArgs);
    EXPECT_EQ(extraArgs.size(), 2U);
    EXPECT_EQ(extraArgs[0], "-Uint64Param");
    EXPECT_EQ(extraArgs[1], "1234");
  }
  { EXPECT_EQ(kUint64Param.Name(), "-Uint64Param"); }
}

TEST(GeckoArgs, bool)
{
  char kFlagStr[] = "-Flag";

  {
    char* argv[] = {kFirefox, kFlagStr, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 3);

    Maybe<bool> Flag = kFlag.Get(argc, argv);
    EXPECT_TRUE(Flag.isSome());
    EXPECT_TRUE(*Flag);

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    char* argv[] = {kFirefox, nullptr};
    int argc = ArrayLength(argv);
    EXPECT_EQ(argc, 2);

    Maybe<bool> Flag = kFlag.Get(argc, argv);
    EXPECT_TRUE(Flag.isNothing());

    const char* const expArgv[] = {kFirefox, nullptr};
    EXPECT_TRUE(CheckArgv(argv, expArgv));
  }
  {
    std::vector<std::string> extraArgs;
    EXPECT_EQ(extraArgs.size(), 0U);
    kFlag.Put(true, extraArgs);
    EXPECT_EQ(extraArgs.size(), 1U);
    EXPECT_EQ(extraArgs[0], "-Flag");
  }
  {
    std::vector<std::string> extraArgs;
    EXPECT_EQ(extraArgs.size(), 0U);
    kFlag.Put(false, extraArgs);
    EXPECT_EQ(extraArgs.size(), 0U);
  }
  { EXPECT_EQ(kFlag.Name(), "-Flag"); }
}
