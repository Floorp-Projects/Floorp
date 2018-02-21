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
  // Valid syntax. (Other testing of more typical valid syntax and semantics is
  // done in modules/libpref/test/unit/test_parser.js.)
  //-------------------------------------------------------------------------

  // Normal prefs.
  P(R"(
pref("bool", true);
sticky_pref("int", 123);
user_pref("string", "value");
    )",
    ""
  );

  // Totally empty input.
  P("",
    ""
  );

  // Whitespace-only input.
  P(R"(   
		
    )" "\v \t \v \f",
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
pref("int.ok", +2147483647);
pref("int.overflow", +2147483648);
pref("int.ok", -2147483648);
pref("int.overflow", -2147483649);
pref("int.overflow", 4294967296);
pref("int.overflow", +4294967296);
pref("int.overflow", -4294967296);
pref("int.overflow", 4294967297);
pref("int.overflow", 1234567890987654321);
    )",
    "test:3: prefs parse error: integer literal overflowed\n"
    "test:5: prefs parse error: integer literal overflowed\n"
    "test:7: prefs parse error: integer literal overflowed\n"
    "test:8: prefs parse error: integer literal overflowed\n"
    "test:9: prefs parse error: integer literal overflowed\n"
    "test:10: prefs parse error: integer literal overflowed\n"
    "test:11: prefs parse error: integer literal overflowed\n"
    "test:12: prefs parse error: integer literal overflowed\n"
  );

  // Other integer errors.
  P(R"(
pref("int.unexpected", 100foo);
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: unexpected character in integer literal\n"
  );

  // \x00 is not allowed.
  P(R"(
pref("string.bad-x-escape", "foo\x00bar");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: \\x00 is not allowed\n"
  );

  // Various bad things after \x: end of string, punctuation, space, newline,
  // EOF.
  P(R"(
pref("string.bad-x-escape", "foo\x");
pref("string.bad-x-escape", "foo\x,bar");
pref("string.bad-x-escape", "foo\x 12");
pref("string.bad-x-escape", "foo\x
12");
pref("string.bad-x-escape", "foo\x)",
    "test:2: prefs parse error: malformed \\x escape sequence\n"
    "test:3: prefs parse error: malformed \\x escape sequence\n"
    "test:4: prefs parse error: malformed \\x escape sequence\n"
    "test:5: prefs parse error: malformed \\x escape sequence\n"
    "test:7: prefs parse error: malformed \\x escape sequence\n"
  );

  // Not enough hex digits.
  P(R"(
pref("string.bad-x-escape", "foo\x1");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: malformed \\x escape sequence\n"
  );

  // Invalid hex digit.
  P(R"(
pref("string.bad-x-escape", "foo\x1G");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: malformed \\x escape sequence\n"
  );

  // \u0000 is not allowed.
  // (The string literal is broken in two so that MSVC doesn't complain about
  // an invalid universal-character-name.)
  P(R"(
pref("string.bad-u-escape", "foo\)" R"(u0000 bar");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: \\u0000 is not allowed\n"
  );

  // Various bad things after \u: end of string, punctuation, space, newline,
  // EOF.
  P(R"(
pref("string.bad-u-escape", "foo\u");
pref("string.bad-u-escape", "foo\u,bar");
pref("string.bad-u-escape", "foo\u 1234");
pref("string.bad-u-escape", "foo\u
1234");
pref("string.bad-u-escape", "foo\u)",
    "test:2: prefs parse error: malformed \\u escape sequence\n"
    "test:3: prefs parse error: malformed \\u escape sequence\n"
    "test:4: prefs parse error: malformed \\u escape sequence\n"
    "test:5: prefs parse error: malformed \\u escape sequence\n"
    "test:7: prefs parse error: malformed \\u escape sequence\n"
  );

  // Not enough hex digits.
  P(R"(
pref("string.bad-u-escape", "foo\u1");
pref("string.bad-u-escape", "foo\u12");
pref("string.bad-u-escape", "foo\u123");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: malformed \\u escape sequence\n"
    "test:3: prefs parse error: malformed \\u escape sequence\n"
    "test:4: prefs parse error: malformed \\u escape sequence\n"
  );

  // Invalid hex digit.
  P(R"(
pref("string.bad-u-escape", "foo\u1G34");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: malformed \\u escape sequence\n"
  );

  // High surrogate not followed by low surrogate.
  // (The string literal is broken in two so that MSVC doesn't complain about
  // an invalid universal-character-name.)
  P(R"(
pref("string.bad-u-surrogate", "foo\)" R"(ud83c,blah");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: expected low surrogate after high surrogate\n"
  );

  // High surrogate followed by invalid low surrogate value.
  // (The string literal is broken in two so that MSVC doesn't complain about
  // an invalid universal-character-name.)
  P(R"(
pref("string.bad-u-surrogate", "foo\)" R"(ud83c\u1234");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: invalid low surrogate value after high surrogate\n"
  );

  // Unlike in JavaScript, \b, \f, \t, \v aren't allowed.
  P(R"(
pref("string.bad-escape", "foo\b");
pref("string.bad-escape", "foo\f");
pref("string.bad-escape", "foo\t");
pref("string.bad-escape", "foo\v");
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:3: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:4: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:5: prefs parse error: unexpected escape sequence character after '\\'\n"
  );

  // Various bad things after \: non-special letter, number, punctuation,
  // space, newline, EOF.
  P(R"(
pref("string.bad-escape", "foo\Q");
pref("string.bad-escape", "foo\1");
pref("string.bad-escape", "foo\,");
pref("string.bad-escape", "foo\ n");
pref("string.bad-escape", "foo\
n");
pref("string.bad-escape", "foo\)",
    "test:2: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:3: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:4: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:5: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:6: prefs parse error: unexpected escape sequence character after '\\'\n"
    "test:8: prefs parse error: unexpected escape sequence character after '\\'\n"
  );

  // Unterminated string literals.

  // Simple case.
  P(R"(
pref("string.unterminated-string", "foo
    )",
    "test:3: prefs parse error: unterminated string literal\n"
  );

  // Alternative case; `int` comes after the string and is seen as a keyword.
  // The parser then skips to the ';', so no error about the unterminated
  // string is issued.
  P(R"(
pref("string.unterminated-string", "foo);
pref("int.ok", 0);
    )",
    "test:3: prefs parse error: unknown keyword\n"
  );

  // Mismatched quotes (1).
  P(R"(
pref("string.unterminated-string", "foo');
    )",
    "test:3: prefs parse error: unterminated string literal\n"
  );

  // Mismatched quotes (2).
  P(R"(
pref("string.unterminated-string", 'foo");
    )",
    "test:3: prefs parse error: unterminated string literal\n"
  );

  // Unknown keywords.
  P(R"(
foo;
preff("string.bad-keyword", true);
ticky_pref("string.bad-keyword", true);
User_pref("string.bad-keyword", true);
pref("string.bad-keyword", TRUE);
    )",
    "test:2: prefs parse error: unknown keyword\n"
    "test:3: prefs parse error: unknown keyword\n"
    "test:4: prefs parse error: unknown keyword\n"
    "test:5: prefs parse error: unknown keyword\n"
    "test:6: prefs parse error: unknown keyword\n"
  );

  // Unterminated C-style comment.
  P(R"(
/* comment
    )",
    "test:3: prefs parse error: unterminated /* comment\n"
  );

  // Malformed comment.
  P(R"(
/ comment
    )",
    "test:2: prefs parse error: expected '/' or '*' after '/'\n"
  );

  // C++-style comment ending in EOF (1).
  P(R"(
// comment)",
    ""
  );

  // C++-style comment ending in EOF (2).
  P(R"(
//)",
    ""
  );

  // Various unexpected characters.
  P(R"(
pref("unexpected.chars", &true);
pref("unexpected.chars" : true);
@pref("unexpected.chars", true);
pref["unexpected.chars": true];
    )",
    "test:2: prefs parse error: unexpected character\n"
    "test:3: prefs parse error: unexpected character\n"
    "test:4: prefs parse error: unexpected character\n"
    "test:5: prefs parse error: unexpected character\n"
  );

  //-------------------------------------------------------------------------
  // All the parsing errors.
  //-------------------------------------------------------------------------

  P(R"(
"pref"("parse.error": true);
pref1("parse.error": true);
pref(123: true);
pref("parse.error" true);
pref("parse.error", pref);
pref("parse.error", -true);
pref("parse.error", +"value");
pref("parse.error", true;
pref("parse.error", true)
pref("int.ok", 1);
pref("parse.error", true))",
    "test:2: prefs parse error: expected pref specifier at start of pref definition\n"
    "test:3: prefs parse error: expected '(' after pref specifier\n"
    "test:4: prefs parse error: expected pref name after '('\n"
    "test:5: prefs parse error: expected ',' after pref name\n"
    "test:6: prefs parse error: expected pref value after ','\n"
    "test:7: prefs parse error: expected integer literal after '-'\n"
    "test:8: prefs parse error: expected integer literal after '+'\n"
    "test:9: prefs parse error: expected ')' after pref value\n"
    "test:11: prefs parse error: expected ';' after ')'\n"
    "test:12: prefs parse error: expected ';' after ')'\n"
  );

  // Parse errors involving unexpected EOF.

  P(R"(
pref)",
    "test:2: prefs parse error: expected '(' after pref specifier\n"
  );

  P(R"(
pref()",
    "test:2: prefs parse error: expected pref name after '('\n"
  );

  P(R"(
pref("parse.error")",
    "test:2: prefs parse error: expected ',' after pref name\n"
  );

  P(R"(
pref("parse.error",)",
    "test:2: prefs parse error: expected pref value after ','\n"
  );

  P(R"(
pref("parse.error", -)",
    "test:2: prefs parse error: expected integer literal after '-'\n"
  );

  P(R"(
pref("parse.error", +)",
    "test:2: prefs parse error: expected integer literal after '+'\n"
  );

  P(R"(
pref("parse.error", true)",
    "test:2: prefs parse error: expected ')' after pref value\n"
  );

  P(R"(
pref("parse.error", true))",
    "test:2: prefs parse error: expected ';' after ')'\n"
  );

  // This is something we saw in practice with the old parser, which allowed
  // repeated semicolons.
  P(R"(
pref("parse.error", true);;
pref("parse.error", true);;;
pref("parse.error", true);;;;
pref("int.ok", 0);
    )",
    "test:2: prefs parse error: expected pref specifier at start of pref definition\n"
    "test:3: prefs parse error: expected pref specifier at start of pref definition\n"
    "test:3: prefs parse error: expected pref specifier at start of pref definition\n"
    "test:4: prefs parse error: expected pref specifier at start of pref definition\n"
    "test:4: prefs parse error: expected pref specifier at start of pref definition\n"
    "test:4: prefs parse error: expected pref specifier at start of pref definition\n"
  );

  //-------------------------------------------------------------------------
  // Invalid syntax after various newline combinations, for the purpose of
  // testing that line numbers are correct.
  //-------------------------------------------------------------------------

  // In all of the following we have a \n, a \r, a \r\n, and then an error, so
  // the error is on line 4. (Note: these ones don't use raw string literals
  // because MSVC somehow swallows any \r that appears in them.)

  P("\n \r \r\n bad",
    "test:4: prefs parse error: unknown keyword\n"
  );

  P("#\n#\r#\r\n bad",
    "test:4: prefs parse error: unknown keyword\n"
  );

  P("//\n//\r//\r\n bad",
    "test:4: prefs parse error: unknown keyword\n"
  );

  P("/*\n \r \r\n*/ bad",
    "test:4: prefs parse error: unknown keyword\n"
  );

  // Note: the escape sequences do *not* affect the line number.
  P("pref(\"foo\\n\n foo\\r\r foo\\r\\n\r\n foo\", bad);",
    "test:4: prefs parse error: unknown keyword\n"
  );

  // clang-format on
}
