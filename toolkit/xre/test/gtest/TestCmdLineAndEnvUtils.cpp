/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

#include "mozilla/CmdLineAndEnvUtils.h"

// Insulate these test-classes from the outside world.
//
// (In particular, distinguish this `CommandLine` from the one in ipc/chromium.
// The compiler has no issues here, but the MSVC debugger gets somewhat
// confused.)
namespace testzilla {

// Auxiliary class to simplify the declaration of test-cases for
// EnsureCommandlineSafe.
class CommandLine {
  const size_t size;
  std::vector<std::string> _data_8;
  std::vector<std::wstring> _data_16;
  // _should_ be const all the way through, but...
  char const** argv_8;
  wchar_t const** argv_16;

 public:
  inline int argc() const { return (int)(unsigned int)(size); }

  template <typename CharT>
  CharT const** argv() const;

  CommandLine(CommandLine const&) = delete;
  CommandLine(CommandLine&& that) = delete;

  template <typename Container>
  explicit CommandLine(Container const& container)
      : size{std::size(container) + 1},     // plus one for argv[0]
        argv_8(new const char*[size + 1]),  // plus one again for argv[argc]
        argv_16(new const wchar_t*[size + 1]) {
    _data_8.reserve(size + 1);
    _data_16.reserve(size + 1);
    size_t pos = 0;

    const auto append = [&](const char* val) {
      size_t const len = ::strlen(val);
      _data_8.emplace_back(val, val + len);
      _data_16.emplace_back(val, val + len);
      argv_8[pos] = _data_8.back().data();
      argv_16[pos] = _data_16.back().data();
      ++pos;
    };

    append("GECKOAPP.COM");  // argv[0] may be anything
    for (const char* item : container) {
      append(item);
    }
    assert(pos == size);

    // C99 §5.1.2.2.1 states "argv[argc] shall be a null pointer", and
    // `CheckArg` implicitly relies on this.
    argv_8[pos] = nullptr;
    argv_16[pos] = nullptr;
  }

  // debug output
  friend std::ostream& operator<<(std::ostream& o, CommandLine const& cl) {
    for (const auto& item : cl._data_8) {
      o << '"';
      for (const char c : item) {
        if (c == '"') {
          o << "\\\"";
        } else {
          o << c;
        }
      }
      o << "\", ";
    }
    return o;
  }
};

template <>
char const** CommandLine::argv<char>() const {
  return argv_8;
}
template <>
wchar_t const** CommandLine::argv<wchar_t>() const {
  return argv_16;
}

enum TestCaseState : bool {
  FAIL = false,
  PASS = true,
};
constexpr TestCaseState operator!(TestCaseState s) {
  return TestCaseState(!bool(s));
}

#ifdef XP_WIN
constexpr static const TestCaseState WIN_ONLY = PASS;
#else
constexpr static const TestCaseState WIN_ONLY = FAIL;
#endif

using NarrowTestCase = std::pair<const char*, const char*>;
constexpr std::pair<TestCaseState, NarrowTestCase> kStrMatches8[] = {
    {PASS, {"", ""}},

    {PASS, {"i", "i"}},
    {PASS, {"i", "I"}},

    {FAIL, {"", "i"}},
    {FAIL, {"i", ""}},
    {FAIL, {"i", "j"}},

    {PASS, {"mozilla", "mozilla"}},
    {PASS, {"mozilla", "Mozilla"}},
    {PASS, {"mozilla", "MOZILLA"}},
    {PASS, {"mozilla", "mOZILLA"}},
    {PASS, {"mozilla", "mOZIlLa"}},
    {PASS, {"mozilla", "MoZiLlA"}},

    {FAIL, {"mozilla", ""}},
    {FAIL, {"mozilla", "mozill"}},
    {FAIL, {"mozilla", "mozillo"}},
    {FAIL, {"mozilla", "mozillam"}},
    {FAIL, {"mozilla", "mozilla-"}},
    {FAIL, {"mozilla", "-mozilla"}},
    {FAIL, {"-mozilla", "mozilla"}},

    // numbers are permissible
    {PASS, {"m0zi11a", "m0zi11a"}},
    {PASS, {"m0zi11a", "M0ZI11A"}},
    {PASS, {"m0zi11a", "m0Zi11A"}},

    {FAIL, {"mozilla", "m0zi11a"}},
    {FAIL, {"m0zi11a", "mozilla"}},

    // capital letters are not accepted in the left comparand
    {FAIL, {"I", "i"}},
    {FAIL, {"I", "i"}},
    {FAIL, {"Mozilla", "mozilla"}},
    {FAIL, {"Mozilla", "Mozilla"}},

    // punctuation other than `-` is rejected
    {FAIL, {"*", "*"}},
    {FAIL, {"*", "word"}},
    {FAIL, {".", "."}},
    {FAIL, {".", "a"}},
    {FAIL, {"_", "_"}},

    // spaces are rejected
    {FAIL, {" ", " "}},
    {FAIL, {"two words", "two words"}},

    // non-ASCII characters are rejected
    //
    // (the contents of this test case may differ depending on the source and
    // execution character sets, but the result should always be failure)
    {FAIL, {"à", "a"}},
    {FAIL, {"a", "à"}},
    {FAIL, {"à", "à"}},
};

#ifdef XP_WIN
using WideTestCase = std::pair<const char*, const wchar_t*>;
std::pair<TestCaseState, WideTestCase> kStrMatches16[] = {
    // (Turkish 'İ' may lowercase to 'i' in some locales, but
    // we explicitly prevent that from being relevant)
    {FAIL, {"i", L"İ"}},
    {FAIL, {"mozilla", L"ṁozilla"}},
};
#endif

constexpr const char* kRequiredArgs[] = {"aleph", "beth"};

std::pair<TestCaseState, std::vector<const char*>> const kCommandLines[] = {
    // the basic admissible forms
    {PASS, {"-osint", "-aleph", "http://www.example.com/"}},
    {PASS, {"-osint", "-beth", "http://www.example.com/"}},

    // GNU-style double-dashes are also allowed
    {PASS, {"--osint", "-aleph", "http://www.example.com/"}},
    {PASS, {"--osint", "-beth", "http://www.example.com/"}},
    {PASS, {"--osint", "--aleph", "http://www.example.com/"}},
    {PASS, {"--osint", "--beth", "http://www.example.com/"}},

    // on Windows, slashes are also permitted
    {WIN_ONLY, {"-osint", "/aleph", "http://www.example.com/"}},
    {WIN_ONLY, {"--osint", "/aleph", "http://www.example.com/"}},
    // - These should pass on Windows because they're well-formed.
    // - These should pass elsewhere because the first parameter is
    //     just a local filesystem path, not an option.
    {PASS, {"/osint", "/aleph", "http://www.example.com/"}},
    {PASS, {"/osint", "-aleph", "http://www.example.com/"}},
    {PASS, {"/osint", "--aleph", "http://www.example.com/"}},

    // the required argument's argument may not itself be a flag
    {FAIL, {"-osint", "-aleph", "-anything"}},
    {FAIL, {"-osint", "-aleph", "--anything"}},
    // - On Windows systems this is a switch and should be rejected.
    // - On non-Windows systems this is potentially a valid local path.
    {!WIN_ONLY, {"-osint", "-aleph", "/anything"}},

    // wrong order
    {FAIL, {"-osint", "http://www.example.com/", "-aleph"}},
    {FAIL, {"-aleph", "-osint", "http://www.example.com/"}},
    {FAIL, {"-aleph", "http://www.example.com/", "-osint"}},
    {FAIL, {"http://www.example.com/", "-osint", "-aleph"}},
    {FAIL, {"http://www.example.com/", "-aleph", "-osint"}},

    // missing arguments
    {FAIL, {"-osint", "http://www.example.com/"}},
    {FAIL, {"-osint", "-aleph"}},
    {FAIL, {"-osint"}},

    // improper arguments
    {FAIL, {"-osint", "-other-argument", "http://www.example.com/"}},
    {FAIL, {"-osint", "", "http://www.example.com/"}},

    // additional arguments
    {FAIL, {"-osint", "-aleph", "http://www.example.com/", "-other-argument"}},
    {FAIL, {"-osint", "-other-argument", "-aleph", "http://www.example.com/"}},
    {FAIL, {"-osint", "-aleph", "-other-argument", "http://www.example.com/"}},
    {FAIL, {"-osint", "-aleph", "http://www.example.com/", "-a", "-b"}},
    {FAIL, {"-osint", "-aleph", "-a", "http://www.example.com/", "-b"}},
    {FAIL, {"-osint", "-a", "-aleph", "http://www.example.com/", "-b"}},

    // multiply-occurring otherwise-licit arguments
    {FAIL, {"-osint", "-aleph", "-beth", "http://www.example.com/"}},
    {FAIL, {"-osint", "-aleph", "-aleph", "http://www.example.com/"}},

    // if -osint is not supplied, anything goes
    {PASS, {}},
    {PASS, {"http://www.example.com"}},
    {PASS, {"-aleph", "http://www.example.com/"}},
    {PASS, {"-beth", "http://www.example.com/"}},
    {PASS, {"-aleph", "http://www.example.com/", "-other-argument"}},
    {PASS, {"-aleph", "-aleph", "http://www.example.com/"}},
};

constexpr static char const* const kOptionalArgs[] = {"mozilla", "allizom"};

// Additional test cases for optional parameters.
//
// Test cases marked PASS should pass iff the above optional parameters are
// permitted. (Test cases marked FAIL should fail regardless, and are only
// grouped here for convenience and semantic coherence.)
std::pair<TestCaseState, std::vector<const char*>> kCommandLinesOpt[] = {
    // one permitted optional argument
    {PASS, {"-osint", "-mozilla", "-aleph", "http://www.example.com/"}},
    {PASS, {"-osint", "-allizom", "-aleph", "http://www.example.com/"}},

    // multiple permitted optional arguments
    {PASS,
     {"-osint", "-mozilla", "-allizom", "-aleph", "http://www.example.com/"}},
    {PASS,
     {"-osint", "-allizom", "-mozilla", "-aleph", "http://www.example.com/"}},

    // optional arguments in the wrong place
    {FAIL, {"-mozilla", "-osint", "-aleph", "http://www.example.com/"}},
    {FAIL, {"-osint", "-aleph", "-mozilla", "http://www.example.com/"}},
    {FAIL, {"-osint", "-aleph", "http://www.example.com/", "-mozilla"}},

    // optional arguments in both right and wrong places
    {FAIL,
     {"-mozilla", "-osint", "-allizom", "-aleph", "http://www.example.com/"}},
    {FAIL,
     {"-osint", "-allizom", "-aleph", "-mozilla", "http://www.example.com/"}},
    {FAIL,
     {"-osint", "-allizom", "-aleph", "http://www.example.com/", "-mozilla"}},
};

enum WithOptionalState : bool {
  NoOptionalArgs = false,
  WithOptionalArgs = true,
};

template <typename CharT>
bool TestCommandLineImpl(CommandLine const& cl,
                         WithOptionalState withOptional) {
  int argc = cl.argc();
  // EnsureCommandlineSafe's signature isn't const-correct here for annoying
  // reasons, but it is indeed non-mutating.
  CharT** argv = const_cast<CharT**>(cl.argv<CharT>());
  if (withOptional) {
    return mozilla::internal::EnsureCommandlineSafeImpl(
        argc, argv, kRequiredArgs, kOptionalArgs);
  }
  return mozilla::internal::EnsureCommandlineSafeImpl(argc, argv,
                                                      kRequiredArgs);
}

// Test that `args` produces `expectation`. On Windows, test against both
// wide-character and narrow-character implementations.
void TestCommandLine(TestCaseState expectation, CommandLine const& cl,
                     WithOptionalState withOptional) {
  EXPECT_EQ(TestCommandLineImpl<char>(cl, withOptional), expectation)
      << "cl is: " << cl << std::endl
      << "withOptional is: " << bool(withOptional);
#ifdef XP_WIN
  EXPECT_EQ(TestCommandLineImpl<wchar_t>(cl, withOptional), expectation)
      << "cl is: " << cl << std::endl
      << "withOptional is: " << bool(withOptional);
#endif
}
}  // namespace testzilla

/***********************
 * Test cases
 */

TEST(CmdLineAndEnvUtils, strimatch)
{
  using namespace testzilla;
  using mozilla::strimatch;
  for (auto const& [result, data] : kStrMatches8) {
    auto const& [left, right] = data;
    EXPECT_EQ(strimatch(left, right), result)
        << '<' << left << "> !~ <" << right << '>';

#ifdef XP_WIN
    wchar_t right_wide[200];
    ::mbstowcs(right_wide, right, 200);
    EXPECT_EQ(strimatch(left, right_wide), result)
        << '<' << left << "> !~ L<" << right << '>';
#endif
  }

#ifdef XP_WIN
  for (auto const& [result, data] : kStrMatches16) {
    auto const& [left, right] = data;
    EXPECT_EQ(strimatch(left, right), result)
        << '<' << left << "> !~ L<" << right << '>';
  }
#endif
}

TEST(CmdLineAndEnvUtils, ensureSafe)
{
  using namespace testzilla;
  for (auto const& [result, data] : kCommandLines) {
    CommandLine const cl(data);
    TestCommandLine(result, cl, NoOptionalArgs);
  }
  for (auto const& [_unused, data] : kCommandLinesOpt) {
    MOZ_UNUSED(_unused);  // silence gcc
    CommandLine const cl(data);
    TestCommandLine(FAIL, cl, NoOptionalArgs);
  }
}

TEST(CmdLineAndEnvUtils, ensureSafeWithOptional)
{
  using namespace testzilla;
  for (auto const& [result, data] : kCommandLines) {
    CommandLine const cl(data);
    TestCommandLine(result, cl, WithOptionalArgs);
  }
  for (auto const& [result, data] : kCommandLinesOpt) {
    CommandLine const cl(data);
    TestCommandLine(result, cl, WithOptionalArgs);
  }
}
