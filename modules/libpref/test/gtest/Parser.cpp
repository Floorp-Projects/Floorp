/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"

// Keep this in sync with the declaration in Preferences.cpp.
//
// It's declared here to avoid polluting Preferences.h with test-only stuff.
void
TestParseError(const char* aText, nsCString& aErrorMsg);

TEST(PrefsParser, Errors)
{
  nsAutoCStringN<128> actualErrorMsg;

// Use a macro rather than a function so that the line number reported by
// gtest on failure is useful.
#define P(text_, expectedErrorMsg_)                                            \
  do {                                                                         \
    TestParseError(text_, actualErrorMsg);                                     \
    ASSERT_STREQ(expectedErrorMsg_, actualErrorMsg.get());                     \
  } while (0)

  // clang-format off

  //-------------------------------------------------------------------------
  // Valid syntax, just as a sanity test. (More thorough testing of valid syntax
  // and semantics is done in modules/libpref/test/unit/test_parser.js.)
  //-------------------------------------------------------------------------

  P(R"(
pref("bool", true);
sticky_pref("int", 123);
user_pref("string", "value");
    )",
    ""
  );

  //-------------------------------------------------------------------------
  // All the lexing errors. (To be pedantic, some of the integer literal
  // overflows are triggered in the parser, but put them all here so they're all
  // in the one spot.)
  //-------------------------------------------------------------------------

  // Integer overflow errors.

  P(R"(
pref("int.ok", 2147483647);
pref("int.overflow", 2147483648);
    )",
    "test:3: prefs parse error: integer literal overflowed");

  P(R"(
pref("int.ok", +2147483647);
pref("int.overflow", +2147483648);
    )",
    "test:3: prefs parse error: integer literal overflowed"
  );

  P(R"(
pref("int.ok", -2147483648);
pref("int.overflow", -2147483649);
    )",
    "test:3: prefs parse error: integer literal overflowed"
  );

  P(R"(
pref("int.overflow", 4294967296);
    )",
    "test:2: prefs parse error: integer literal overflowed"
  );

  P(R"(
pref("int.overflow", +4294967296);
    )",
    "test:2: prefs parse error: integer literal overflowed"
  );

  P(R"(
pref("int.overflow", -4294967296);
    )",
    "test:2: prefs parse error: integer literal overflowed"
  );

  P(R"(
pref("int.overflow", 4294967297);
    )",
    "test:2: prefs parse error: integer literal overflowed"
  );

  P(R"(
pref("int.overflow", 1234567890987654321);
    )",
    "test:2: prefs parse error: integer literal overflowed"
  );

  // Other integer errors.

  P(R"(
pref("int.unexpected", 100foo);
    )",
    "test:2: prefs parse error: unexpected character in integer literal"
  );

  // \x escape errors.

  // \x00 is not allowed.
  P(R"(
pref("string.bad-x-escape", "foo\x00bar");
    )",
    "test:2: prefs parse error: \\x00 is not allowed"
  );

  // End of string after \x.
  P(R"(
pref("string.bad-x-escape", "foo\x");
    )",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // Punctuation after \x.
  P(R"(
pref("string.bad-x-escape", "foo\x,bar");
    )",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // Space after \x.
  P(R"(
pref("string.bad-x-escape", "foo\x 12");
    )",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // Newline after \x.
  P(R"(
pref("string.bad-x-escape", "foo\x
12");
    )",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // EOF after \x.
  P(R"(
pref("string.bad-x-escape", "foo\x)",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // Not enough hex digits.
  P(R"(
pref("string.bad-x-escape", "foo\x1");
    )",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // Invalid hex digit.
  P(R"(
pref("string.bad-x-escape", "foo\x1G");
    )",
    "test:2: prefs parse error: malformed \\x escape sequence"
  );

  // \u escape errors.

  // \u0000 is not allowed.
  // (The string literal is broken in two so that MSVC doesn't complain about
  // an invalid universal-character-name.)
  P(R"(
pref("string.bad-u-escape", "foo\)" R"(u0000 bar");
    )",
    "test:2: prefs parse error: \\u0000 is not allowed"
  );

  // End of string after \u.
  P(R"(
pref("string.bad-u-escape", "foo\u");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Punctuation after \u.
  P(R"(
pref("string.bad-u-escape", "foo\u,bar");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Space after \u.
  P(R"(
pref("string.bad-u-escape", "foo\u 1234");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Newline after \u.
  P(R"(
pref("string.bad-u-escape", "foo\u
1234");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // EOF after \u.
  P(R"(
pref("string.bad-u-escape", "foo\u)",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Not enough hex digits.
  P(R"(
pref("string.bad-u-escape", "foo\u1");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Not enough hex digits.
  P(R"(
pref("string.bad-u-escape", "foo\u12");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Not enough hex digits.
  P(R"(
pref("string.bad-u-escape", "foo\u123");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // Invalid hex digit.
  P(R"(
pref("string.bad-u-escape", "foo\u1G34");
    )",
    "test:2: prefs parse error: malformed \\u escape sequence"
  );

  // High surrogate not followed by low surrogate.
  // (The string literal is broken in two so that MSVC doesn't complain about
  // an invalid universal-character-name.)
  P(R"(
pref("string.bad-u-surrogate", "foo\)" R"(ud83c,blah");
    )",
    "test:2: prefs parse error: expected low surrogate after high surrogate"
  );

  // High surrogate followed by invalid low surrogate value.
  // (The string literal is broken in two so that MSVC doesn't complain about
  // an invalid universal-character-name.)
  P(R"(
pref("string.bad-u-surrogate", "foo\)" R"(ud83c\u1234");
    )",
    "test:2: prefs parse error: invalid low surrogate value after high surrogate"
  );

  // Bad escape characters.

  // Unlike in JavaScript, \b isn't allowed.
  P(R"(
pref("string.bad-escape", "foo\v");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Unlike in JavaScript, \f isn't allowed.
  P(R"(
pref("string.bad-escape", "foo\f");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Unlike in JavaScript, \t isn't allowed.
  P(R"(
pref("string.bad-escape", "foo\t");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Unlike in JavaScript, \v isn't allowed.
  P(R"(
pref("string.bad-escape", "foo\v");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Non-special letter after \.
  P(R"(
pref("string.bad-escape", "foo\Q");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Number after \.
  P(R"(
pref("string.bad-escape", "foo\1");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Punctuation after \.
  P(R"(
pref("string.bad-escape", "foo\,");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Space after \.
  P(R"(
pref("string.bad-escape", "foo\ n");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Newline after \.
  P(R"(
pref("string.bad-escape", "foo\
n");
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // EOF after \.
  P(R"(
pref("string.bad-escape", "foo\)",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'"
  );

  // Unterminated string literals.

  // Simple case.
  P(R"(
pref("string.unterminated-string", "foo
    )",
    "test:3: prefs parse error: unterminated string literal"
  );

  // Mismatched quotes (1).
  P(R"(
pref("string.unterminated-string", "foo');
    )",
    "test:3: prefs parse error: unterminated string literal"
  );

  // Mismatched quotes (2).
  P(R"(
pref("string.unterminated-string", 'foo");
    )",
    "test:3: prefs parse error: unterminated string literal"
  );

  // Unknown keywords

  P(R"(
foo
    )",
    "test:2: prefs parse error: unknown keyword"
  );

  P(R"(
preff("string.bad-keyword", true);
    )",
    "test:2: prefs parse error: unknown keyword"
  );

  P(R"(
ticky_pref("string.bad-keyword", true);
    )",
    "test:2: prefs parse error: unknown keyword"
  );

  P(R"(
User_pref("string.bad-keyword", true);
    )",
    "test:2: prefs parse error: unknown keyword"
  );

  P(R"(
pref("string.bad-keyword", TRUE);
    )",
    "test:2: prefs parse error: unknown keyword"
  );

  // Unterminated C-style comment
  P(R"(
/* comment
    )",
    "test:3: prefs parse error: unterminated /* comment"
  );

  // Malformed comments.

  P(R"(
/ comment
    )",
    "test:2: prefs parse error: expected '/' or '*' after '/'"
  );

  // Unexpected characters

  P(R"(
pref("unexpected.chars", &true);
    )",
    "test:2: prefs parse error: unexpected character"
  );

  P(R"(
pref("unexpected.chars" : true);
    )",
    "test:2: prefs parse error: unexpected character"
  );

  P(R"(
@pref("unexpected.chars", true);
    )",
    "test:2: prefs parse error: unexpected character"
  );

  P(R"(
pref["unexpected.chars": true];
    )",
    "test:2: prefs parse error: unexpected character"
  );

  //-------------------------------------------------------------------------
  // All the parsing errors.
  //-------------------------------------------------------------------------

  P(R"(
"pref"("parse.error": true);
    )",
    "test:2: prefs parse error: expected pref specifier at start of pref definition"
  );

  P(R"(
pref1("parse.error": true);
    )",
    "test:2: prefs parse error: expected '(' after pref specifier"
  );

  P(R"(
pref(123: true);
    )",
    "test:2: prefs parse error: expected pref name after '('"
  );

  P(R"(
pref("parse.error" true);
    )",
    "test:2: prefs parse error: expected ',' after pref name"
  );

  P(R"(
pref("parse.error", -true);
    )",
    "test:2: prefs parse error: expected integer literal after '-'"
  );

  P(R"(
pref("parse.error", +"value");
    )",
    "test:2: prefs parse error: expected integer literal after '+'"
  );

  P(R"(
pref("parse.error", pref);
    )",
    "test:2: prefs parse error: expected pref value after ','"
  );

  P(R"(
pref("parse.error", true;
    )",
    "test:2: prefs parse error: expected ')' after pref value"
  );

  P(R"(
pref("parse.error", true)
pref("parse.error", true)
    )",
    "test:3: prefs parse error: expected ';' after ')'"
  );

  // This is something we saw in practice with the old parser, which allowed
  // repeated semicolons.
  P(R"(
pref("parse.error", true);;
    )",
    "test:2: prefs parse error: expected pref specifier at start of pref definition"
  );

  //-------------------------------------------------------------------------
  // Invalid syntax after various newline combinations, for the purpose of
  // testing that line numbers are correct.
  //-------------------------------------------------------------------------

  // In all of the following we have a \n, a \r, a \r\n, and then an error, so
  // the error is on line 4.

  P(R"(
  
bad
    )",
    "test:4: prefs parse error: unknown keyword"
  );

  P(R"(#
##
bad
    )",
    "test:4: prefs parse error: unknown keyword"
  );

  P(R"(//
////
bad
    )",
    "test:4: prefs parse error: unknown keyword"
  );

  P(R"(/*
 
*/ bad
    )",
    "test:4: prefs parse error: unknown keyword"
  );

  // Note: the escape sequences do *not* affect the line number.
  P(R"(pref("foo\n
\rfoo\r\n
foo", bad
    )",
    "test:4: prefs parse error: unknown keyword"
  );

  // clang-format on
}
