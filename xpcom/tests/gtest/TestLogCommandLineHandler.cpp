/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LogCommandLineHandler.h"

#include <iterator>
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "plstr.h"
#include "gtest/gtest.h"

using namespace mozilla;

template<class T, size_t N>
constexpr size_t array_size(T (&)[N])
{
  return N;
}

TEST(LogCommandLineHandler, Empty)
{
  bool callbackInvoked = false;
  auto callback = [&](nsACString const& env) mutable {
    callbackInvoked = true;
  };

  mozilla::LoggingHandleCommandLineArgs(0, nullptr, callback);
  EXPECT_FALSE(callbackInvoked);

  char const* argv1[] = { "" };
  mozilla::LoggingHandleCommandLineArgs(array_size(argv1), argv1, callback);
  EXPECT_FALSE(callbackInvoked);
}

TEST(LogCommandLineHandler, MOZ_LOG_regular)
{
  nsTArray<nsCString> results;

  auto callback = [&](nsACString const& env) mutable {
    results.AppendElement(env);
  };

  char const* argv1[] = { "", "-MOZ_LOG", "module1:5,module2:4,sync,timestamp" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv1), argv1, callback);
  EXPECT_TRUE(results.Length() == 1);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=module1:5,module2:4,sync,timestamp").Equals(results[0]));

  char const* argv2[] = { "", "-MOZ_LOG=modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv2), argv2, callback);
  EXPECT_TRUE(results.Length() == 1);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));

  char const* argv3[] = { "", "--MOZ_LOG", "modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv3), argv3, callback);
  EXPECT_TRUE(results.Length() == 1);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));

  char const* argv4[] = { "", "--MOZ_LOG=modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv4), argv4, callback);
  EXPECT_TRUE(results.Length() == 1);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));
}

TEST(LogCommandLineHandler, MOZ_LOG_and_FILE_regular)
{
  nsTArray<nsCString> results;

  auto callback = [&](nsACString const& env) mutable {
    results.AppendElement(env);
  };

  char const* argv1[] = { "", "-MOZ_LOG", "modules", "-MOZ_LOG_FILE", "c:\\file/path" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv1), argv1, callback);
  EXPECT_TRUE(results.Length() == 2);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG_FILE=c:\\file/path").Equals(results[1]));

  char const* argv2[] = { "", "-MOZ_LOG=modules", "-MOZ_LOG_FILE=file" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv2), argv2, callback);
  EXPECT_TRUE(results.Length() == 2);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG_FILE=file").Equals(results[1]));

  char const* argv3[] = { "", "--MOZ_LOG", "modules", "--MOZ_LOG_FILE", "file" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv3), argv3, callback);
  EXPECT_TRUE(results.Length() == 2);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG_FILE=file").Equals(results[1]));

  char const* argv4[] = { "", "--MOZ_LOG=modules", "--MOZ_LOG_FILE=file" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv4), argv4, callback);
  EXPECT_TRUE(results.Length() == 2);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG_FILE=file").Equals(results[1]));

  char const* argv5[] = { "", "--MOZ_LOG", "modules", "-P", "foo", "--MOZ_LOG_FILE", "file" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv5), argv5, callback);
  EXPECT_TRUE(results.Length() == 2);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG_FILE=file").Equals(results[1]));
}

TEST(LogCommandLineHandler, MOZ_LOG_fuzzy)
{
  nsTArray<nsCString> results;

  auto callback = [&](nsACString const& env) mutable {
    results.AppendElement(env);
  };

  char const* argv1[] = { "", "-MOZ_LOG" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv1), argv1, callback);
  EXPECT_TRUE(results.Length() == 0);

  char const* argv2[] = { "", "modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv2), argv2, callback);
  EXPECT_TRUE(results.Length() == 0);

  char const* argv3[] = { "", "-MOZ_LOG,modules", "-MOZ_LOG" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv3), argv3, callback);
  EXPECT_TRUE(results.Length() == 0);

  char const* argv4[] = { "", "-MOZ_LOG", "-MOZ_LOG", "-MOZ_LOG" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv4), argv4, callback);
  EXPECT_TRUE(results.Length() == 0);

  char const* argv5[] = { "", "-MOZ_LOG", "-diffent_command", "modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv5), argv5, callback);
  EXPECT_TRUE(results.Length() == 0);
}

TEST(LogCommandLineHandler, MOZ_LOG_overlapping)
{
  nsTArray<nsCString> results;

  auto callback = [&](nsACString const& env) mutable {
    results.AppendElement(env);
  };

  char const* argv1[] = { "", "-MOZ_LOG=modules1", "-MOZ_LOG=modules2" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv1), argv1, callback);
  EXPECT_TRUE(results.Length() == 2);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules1").Equals(results[0]));
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules2").Equals(results[1]));

  char const* argv2[] = { "", "-MOZ_LOG", "--MOZ_LOG", "modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv2), argv2, callback);
  EXPECT_TRUE(results.Length() == 1);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));

  char const* argv3[] = { "", "-MOZ_LOG_FILE", "-MOZ_LOG", "modules" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv3), argv3, callback);
  EXPECT_TRUE(results.Length() == 1);
  EXPECT_TRUE(NS_LITERAL_CSTRING("MOZ_LOG=modules").Equals(results[0]));

  char const* argv4[] = { "", "-MOZ_LOG", "-MOZ_LOG_FILE", "-MOZ_LOG" };
  results.Clear();
  mozilla::LoggingHandleCommandLineArgs(array_size(argv4), argv4, callback);
  EXPECT_TRUE(results.Length() == 0);
}
