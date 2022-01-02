// Copyright 2019 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "gtest/gtest.h"

#include <string>

#include "src/option-parser.h"

using namespace wabt;

#define ERROR_ENDING "\nTry '--help' for more information."

TEST(OptionParser, LongFlag) {
  bool flag = false;
  OptionParser parser("prog", "desc");
  parser.AddOption("flag", "help", [&]() { flag = true; });
  const char* args[] = {"prog name", "--flag"};
  parser.Parse(2, const_cast<char**>(args));
  EXPECT_EQ(true, flag);
}

TEST(OptionParser, ShortAndLongFlag) {
  int count = 0;
  OptionParser parser("prog", "desc");
  parser.AddOption('f', "flag", "help", [&]() { ++count; });
  const char* args[] = {"prog name", "-f", "--flag", "-f", "--flag"};
  parser.Parse(5, const_cast<char**>(args));
  EXPECT_EQ(4, count);
}

TEST(OptionParser, ShortFlagCombined) {
  int count = 0;
  OptionParser parser("prog", "desc");
  parser.AddOption('a', "a", "help", [&]() { count += 1; });
  parser.AddOption('b', "b", "help", [&]() { count += 2; });
  const char* args[] = {"prog name", "-aa", "-abb"};
  parser.Parse(3, const_cast<char**>(args));
  EXPECT_EQ(7, count);
}

TEST(OptionParser, UnknownShortOption) {
  std::string error;
  OptionParser parser("prog", "desc");
  parser.SetErrorCallback([&](const char* msg) { error = msg; });
  const char* args[] = {"prog name", "-f"};
  parser.Parse(2, const_cast<char**>(args));
  EXPECT_EQ(error, "prog: unknown option '-f'" ERROR_ENDING);
}

TEST(OptionParser, UnknownLongOption) {
  std::string error;
  OptionParser parser("prog", "desc");
  parser.SetErrorCallback([&](const char* msg) { error = msg; });
  const char* args[] = {"prog name", "--foo"};
  parser.Parse(2, const_cast<char**>(args));
  EXPECT_EQ(error, "prog: unknown option '--foo'" ERROR_ENDING);
}

TEST(OptionParser, ShortAndLongParam) {
  std::string param;
  OptionParser parser("prog", "desc");
  parser.AddOption('p', "param", "metavar", "help",
                   [&](const char* arg) { param += arg; });
  const char* args[] = {"prog name", "-p", "h", "--param", "el", "--param=lo"};
  parser.Parse(6, const_cast<char**>(args));
  EXPECT_EQ("hello", param);
}

TEST(OptionParser, MissingParam) {
  std::string error;
  std::string param;
  OptionParser parser("prog", "desc");
  parser.SetErrorCallback([&](const char* msg) { error = msg; });
  parser.AddOption('p', "param", "metavar", "help",
                   [&](const char* arg) { param = arg; });
  const char* args[] = {"prog name", "--param"};
  parser.Parse(2, const_cast<char**>(args));
  EXPECT_EQ("", param);
  EXPECT_EQ(error, "prog: option '--param' requires argument" ERROR_ENDING);
}

TEST(OptionParser, MissingArgument) {
  std::string error;
  OptionParser parser("prog", "desc");
  parser.AddArgument("arg", OptionParser::ArgumentCount::One,
                     [&](const char* arg) {});
  parser.SetErrorCallback([&](const char* msg) { error = msg; });
  const char* args[] = {"prog name"};
  parser.Parse(1, const_cast<char**>(args));
  EXPECT_EQ(error, "prog: expected arg argument." ERROR_ENDING);
}

TEST(OptionParser, FlagCombinedAfterShortParam) {
  std::string error;
  std::string param;
  bool has_x = false;

  OptionParser parser("prog", "desc");
  parser.SetErrorCallback([&](const char* msg) { error = msg; });
  parser.AddOption('p', "p", "metavar", "help",
                   [&](const char* arg) { param = arg; });
  parser.AddOption('x', "x", "help", [&]() { has_x = true; });
  const char* args[] = {"prog name", "-px", "stuff"};
  parser.Parse(3, const_cast<char**>(args));
  EXPECT_EQ("", param);
  EXPECT_TRUE(has_x);
  EXPECT_EQ(error, "prog: unexpected argument 'stuff'" ERROR_ENDING);
}


TEST(OptionParser, OneArgument) {
  std::string argument;
  OptionParser parser("prog", "desc");
  parser.AddArgument("arg", OptionParser::ArgumentCount::One,
                     [&](const char* arg) { argument = arg; });
  const char* args[] = {"prog name", "hello"};
  parser.Parse(2, const_cast<char**>(args));
  EXPECT_EQ("hello", argument);
}

TEST(OptionParser, TooManyArguments) {
  std::string error;
  OptionParser parser("prog", "desc");
  parser.SetErrorCallback([&](const char* msg) { error = msg; });
  parser.AddArgument("arg", OptionParser::ArgumentCount::One,
                     [&](const char* arg) {});
  const char* args[] = {"prog name", "hello", "goodbye"};
  parser.Parse(3, const_cast<char**>(args));
  EXPECT_EQ(error, "prog: unexpected argument 'goodbye'" ERROR_ENDING);
}

TEST(OptionParser, OneOrMoreArguments) {
  std::string argument;
  OptionParser parser("prog", "desc");
  parser.AddArgument("arg", OptionParser::ArgumentCount::OneOrMore,
                     [&](const char* arg) { argument += arg; });
  const char* args[] = {"prog name", "hello", "goodbye"};
  parser.Parse(3, const_cast<char**>(args));
  EXPECT_EQ("hellogoodbye", argument);
}

TEST(OptionParser, ZeroOrMoreArguments) {
  std::string argument;
  OptionParser parser("prog", "desc");
  parser.AddArgument("arg", OptionParser::ArgumentCount::ZeroOrMore,
                     [&](const char* arg) { argument += arg; });

  const char* args_none[] = {"prog name"};
  parser.Parse(1, const_cast<char**>(args_none));
  EXPECT_EQ("", argument);

  const char* args_many[] = {"prog name", "hello", "goodbye"};
  parser.Parse(3, const_cast<char**>(args_many));
  EXPECT_EQ("hellogoodbye", argument);
}

TEST(OptionParser, StopProccessing) {
  std::string argument;
  bool has_x = false;
  OptionParser parser("prog", "desc");
  parser.AddArgument("arg", OptionParser::ArgumentCount::ZeroOrMore,
                     [&](const char* arg) { argument += arg; });
  parser.AddOption('x', "x", "help", [&]() { has_x = true; });

  const char* args_many[] = {"prog name", "-x", "--", "foo", "-x", "-y", "bar"};
  parser.Parse(7, const_cast<char**>(args_many));
  EXPECT_TRUE(has_x);
  EXPECT_EQ("foo-x-ybar", argument);
}
