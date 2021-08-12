/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Tokenizer.h"
#include "mozilla/IncrementalTokenizer.h"
#include "mozilla/Unused.h"
#include "gtest/gtest.h"

using namespace mozilla;

template <typename Char>
static bool IsOperator(Char const c) {
  return c == '+' || c == '*';
}

static bool HttpHeaderCharacter(char const c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || (c == '_') || (c == '-');
}

TEST(Tokenizer, HTTPResponse)
{
  Tokenizer::Token t;

  // Real life test, HTTP response

  Tokenizer p(
      nsLiteralCString("HTTP/1.0 304 Not modified\r\n"
                       "ETag: hallo\r\n"
                       "Content-Length: 16\r\n"
                       "\r\n"
                       "This is the body"));

  EXPECT_TRUE(p.CheckWord("HTTP"));
  EXPECT_TRUE(p.CheckChar('/'));
  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_INTEGER, t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 1);
  EXPECT_TRUE(p.CheckChar('.'));
  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_INTEGER, t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 0);
  p.SkipWhites();

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_INTEGER, t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 304);
  p.SkipWhites();

  p.Record();
  while (p.Next(t) && t.Type() != Tokenizer::TOKEN_EOL) {
    ;
  }
  EXPECT_FALSE(p.HasFailed());
  nsAutoCString h;
  p.Claim(h);
  EXPECT_TRUE(h == "Not modified");

  p.Record();
  while (p.CheckChar(HttpHeaderCharacter)) {
    ;
  }
  p.Claim(h, Tokenizer::INCLUDE_LAST);
  EXPECT_TRUE(h == "ETag");
  p.SkipWhites();
  EXPECT_TRUE(p.CheckChar(':'));
  p.SkipWhites();
  p.Record();
  while (p.Next(t) && t.Type() != Tokenizer::TOKEN_EOL) {
    ;
  }
  EXPECT_FALSE(p.HasFailed());
  p.Claim(h);
  EXPECT_TRUE(h == "hallo");

  p.Record();
  while (p.CheckChar(HttpHeaderCharacter)) {
    ;
  }
  p.Claim(h, Tokenizer::INCLUDE_LAST);
  EXPECT_TRUE(h == "Content-Length");
  p.SkipWhites();
  EXPECT_TRUE(p.CheckChar(':'));
  p.SkipWhites();
  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_INTEGER, t));
  EXPECT_TRUE(t.AsInteger() == 16);
  EXPECT_TRUE(p.CheckEOL());

  EXPECT_TRUE(p.CheckEOL());

  p.Record();
  while (p.Next(t) && t.Type() != Tokenizer::TOKEN_EOF) {
    ;
  }
  nsAutoCString b;
  p.Claim(b);
  EXPECT_TRUE(b == "This is the body");
}

TEST(Tokenizer, Main)
{
  Tokenizer::Token t;

  // Synthetic code-specific test

  Tokenizer p("test123 ,15  \t*\r\n%xx,-15\r\r"_ns);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_WORD);
  EXPECT_TRUE(t.AsString() == "test123");

  Tokenizer::Token u;
  EXPECT_FALSE(p.Check(u));

  EXPECT_FALSE(p.CheckChar('!'));

  EXPECT_FALSE(p.Check(Tokenizer::Token::Number(123)));

  EXPECT_TRUE(p.CheckWhite());

  EXPECT_TRUE(p.CheckChar(','));

  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(15)));

  p.Rollback();
  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(15)));

  p.Rollback();
  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 15);

  EXPECT_FALSE(p.CheckChar(IsOperator));

  EXPECT_TRUE(p.CheckWhite());

  p.SkipWhites();

  EXPECT_FALSE(p.CheckWhite());

  p.Rollback();

  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckWhite());

  p.Record(Tokenizer::EXCLUDE_LAST);

  EXPECT_TRUE(p.CheckChar(IsOperator));

  p.Rollback();

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(t.AsChar() == '*');

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_EOL);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(t.AsChar() == '%');

  nsAutoCString claim;
  p.Claim(claim, Tokenizer::EXCLUDE_LAST);
  EXPECT_TRUE(claim == "*\r\n");
  p.Claim(claim, Tokenizer::INCLUDE_LAST);
  EXPECT_TRUE(claim == "*\r\n%");

  p.Rollback();
  EXPECT_TRUE(p.CheckChar('%'));

  p.Record(Tokenizer::INCLUDE_LAST);

  EXPECT_FALSE(p.CheckWord("xy"));

  EXPECT_TRUE(p.CheckWord("xx"));

  p.Claim(claim, Tokenizer::INCLUDE_LAST);
  EXPECT_TRUE(claim == "%xx");

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(t.AsChar() == ',');

  EXPECT_TRUE(p.CheckChar('-'));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 15);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_EOL);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_EOL);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_EOF);

  EXPECT_FALSE(p.Next(t));

  p.Rollback();
  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_EOF);

  EXPECT_FALSE(p.Next(t));

  p.Rollback();
  EXPECT_TRUE(p.CheckEOF());

  EXPECT_FALSE(p.CheckEOF());
}

TEST(Tokenizer, Main16)
{
  Tokenizer16::Token t;

  // Synthetic code-specific test

  Tokenizer16 p(u"test123 ,15  \t*\r\n%xx,-15\r\r"_ns);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_WORD);
  EXPECT_TRUE(t.AsString() == u"test123"_ns);

  Tokenizer16::Token u;
  EXPECT_FALSE(p.Check(u));

  EXPECT_FALSE(p.CheckChar('!'));

  EXPECT_FALSE(p.Check(Tokenizer16::Token::Number(123)));

  EXPECT_TRUE(p.CheckWhite());

  EXPECT_TRUE(p.CheckChar(','));

  EXPECT_TRUE(p.Check(Tokenizer16::Token::Number(15)));

  p.Rollback();
  EXPECT_TRUE(p.Check(Tokenizer16::Token::Number(15)));

  p.Rollback();
  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 15);

  EXPECT_FALSE(p.CheckChar(IsOperator));

  EXPECT_TRUE(p.CheckWhite());

  p.SkipWhites();

  EXPECT_FALSE(p.CheckWhite());

  p.Rollback();

  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckWhite());

  p.Record(Tokenizer16::EXCLUDE_LAST);

  EXPECT_TRUE(p.CheckChar(IsOperator));

  p.Rollback();

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_CHAR);
  EXPECT_TRUE(t.AsChar() == '*');

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_EOL);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_CHAR);
  EXPECT_TRUE(t.AsChar() == '%');

  nsAutoString claim;
  p.Claim(claim, Tokenizer16::EXCLUDE_LAST);
  EXPECT_TRUE(claim == u"*\r\n"_ns);
  p.Claim(claim, Tokenizer16::INCLUDE_LAST);
  EXPECT_TRUE(claim == u"*\r\n%"_ns);

  p.Rollback();
  EXPECT_TRUE(p.CheckChar('%'));

  p.Record(Tokenizer16::INCLUDE_LAST);

  EXPECT_FALSE(p.CheckWord(u"xy"_ns));

  EXPECT_TRUE(p.CheckWord(u"xx"_ns));

  p.Claim(claim, Tokenizer16::INCLUDE_LAST);
  EXPECT_TRUE(claim == u"%xx"_ns);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_CHAR);
  EXPECT_TRUE(t.AsChar() == ',');

  EXPECT_TRUE(p.CheckChar('-'));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_INTEGER);
  EXPECT_TRUE(t.AsInteger() == 15);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_EOL);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_EOL);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_EOF);

  EXPECT_FALSE(p.Next(t));

  p.Rollback();
  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer16::TOKEN_EOF);

  EXPECT_FALSE(p.Next(t));

  p.Rollback();
  EXPECT_TRUE(p.CheckEOF());

  EXPECT_FALSE(p.CheckEOF());
}

TEST(Tokenizer, SingleWord)
{
  // Single word with numbers in it test

  Tokenizer p("test123"_ns);

  EXPECT_TRUE(p.CheckWord("test123"));
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, EndingAfterNumber)
{
  // An end handling after a number

  Tokenizer p("123"_ns);

  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(123)));
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, BadInteger)
{
  Tokenizer::Token t;

  // A bad integer test

  Tokenizer p("189234891274981758617846178651647620587135"_ns);

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_ERROR);
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, CheckExpectedTokenValue)
{
  Tokenizer::Token t;

  // Check expected token value test

  Tokenizer p("blue velvet"_ns);

  EXPECT_FALSE(p.Check(Tokenizer::TOKEN_INTEGER, t));

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_WORD, t));
  EXPECT_TRUE(t.AsString() == "blue");

  EXPECT_FALSE(p.Check(Tokenizer::TOKEN_WORD, t));

  EXPECT_TRUE(p.CheckWhite());

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_WORD, t));
  EXPECT_TRUE(t.AsString() == "velvet");

  EXPECT_TRUE(p.CheckEOF());

  EXPECT_FALSE(p.Next(t));
}

TEST(Tokenizer, HasFailed)
{
  Tokenizer::Token t;

  // HasFailed test

  Tokenizer p1("a b"_ns);

  while (p1.Next(t) && t.Type() != Tokenizer::TOKEN_CHAR) {
    ;
  }
  EXPECT_TRUE(p1.HasFailed());

  Tokenizer p2("a b ?!c"_ns);

  EXPECT_FALSE(p2.CheckChar('c'));
  EXPECT_TRUE(p2.HasFailed());
  EXPECT_TRUE(p2.CheckChar(HttpHeaderCharacter));
  EXPECT_FALSE(p2.HasFailed());
  p2.SkipWhites();
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_FALSE(p2.CheckChar('c'));
  EXPECT_TRUE(p2.HasFailed());
  EXPECT_TRUE(p2.Next(t));
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_TRUE(p2.Next(t));
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_FALSE(p2.CheckChar('c'));
  EXPECT_TRUE(p2.HasFailed());
  EXPECT_TRUE(p2.Check(Tokenizer::TOKEN_CHAR, t));
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_FALSE(p2.CheckChar('#'));
  EXPECT_TRUE(p2.HasFailed());
  t = Tokenizer::Token::Char('!');
  EXPECT_TRUE(p2.Check(t));
  EXPECT_FALSE(p2.HasFailed());

  while (p2.Next(t) && t.Type() != Tokenizer::TOKEN_CHAR) {
    ;
  }
  EXPECT_TRUE(p2.HasFailed());
}

TEST(Tokenizer, Construction)
{
  {
    nsCString a("test");
    Tokenizer p1(a);
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }

  {
    nsAutoCString a("test");
    Tokenizer p1(a);
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }

  {
    static const char _a[] = "test";
    nsDependentCString a(_a);
    Tokenizer p1(a);
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }

  {
    static const char* _a = "test";
    nsDependentCString a(_a);
    Tokenizer p1(a);
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }

  {
    Tokenizer p1(nsDependentCString("test"));
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }

  {
    Tokenizer p1("test"_ns);
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }

  {
    Tokenizer p1("test");
    EXPECT_TRUE(p1.CheckWord("test"));
    EXPECT_TRUE(p1.CheckEOF());
  }
}

TEST(Tokenizer, Customization)
{
  Tokenizer p1("test-custom*words and\tdefault-whites"_ns, nullptr, "-*");
  EXPECT_TRUE(p1.CheckWord("test-custom*words"));
  EXPECT_TRUE(p1.CheckWhite());
  EXPECT_TRUE(p1.CheckWord("and"));
  EXPECT_TRUE(p1.CheckWhite());
  EXPECT_TRUE(p1.CheckWord("default-whites"));

  Tokenizer p2("test, custom,whites"_ns, ", ");
  EXPECT_TRUE(p2.CheckWord("test"));
  EXPECT_TRUE(p2.CheckWhite());
  EXPECT_TRUE(p2.CheckWhite());
  EXPECT_TRUE(p2.CheckWord("custom"));
  EXPECT_TRUE(p2.CheckWhite());
  EXPECT_TRUE(p2.CheckWord("whites"));

  Tokenizer p3("test, custom, whites-and#word-chars"_ns, ",", "-#");
  EXPECT_TRUE(p3.CheckWord("test"));
  EXPECT_TRUE(p3.CheckWhite());
  EXPECT_FALSE(p3.CheckWhite());
  EXPECT_TRUE(p3.CheckChar(' '));
  EXPECT_TRUE(p3.CheckWord("custom"));
  EXPECT_TRUE(p3.CheckWhite());
  EXPECT_FALSE(p3.CheckWhite());
  EXPECT_TRUE(p3.CheckChar(' '));
  EXPECT_TRUE(p3.CheckWord("whites-and#word-chars"));
}

TEST(Tokenizer, ShortcutChecks)
{
  Tokenizer p("test1 test2,123");

  nsAutoCString test1;
  nsDependentCSubstring test2;
  char comma;
  uint32_t integer;

  EXPECT_TRUE(p.ReadWord(test1));
  EXPECT_TRUE(test1 == "test1");
  p.SkipWhites();
  EXPECT_TRUE(p.ReadWord(test2));
  EXPECT_TRUE(test2 == "test2");
  EXPECT_TRUE(p.ReadChar(&comma));
  EXPECT_TRUE(comma == ',');
  EXPECT_TRUE(p.ReadInteger(&integer));
  EXPECT_TRUE(integer == 123);
  EXPECT_TRUE(p.CheckEOF());
}

static bool ABChar(const char aChar) { return aChar == 'a' || aChar == 'b'; }

TEST(Tokenizer, ReadCharClassified)
{
  Tokenizer p("abc");

  char c;
  EXPECT_TRUE(p.ReadChar(ABChar, &c));
  EXPECT_TRUE(c == 'a');
  EXPECT_TRUE(p.ReadChar(ABChar, &c));
  EXPECT_TRUE(c == 'b');
  EXPECT_FALSE(p.ReadChar(ABChar, &c));
  nsDependentCSubstring w;
  EXPECT_TRUE(p.ReadWord(w));
  EXPECT_TRUE(w == "c");
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, ClaimSubstring)
{
  Tokenizer p(" abc ");

  EXPECT_TRUE(p.CheckWhite());

  p.Record();
  EXPECT_TRUE(p.CheckWord("abc"));
  nsDependentCSubstring v;
  p.Claim(v, Tokenizer::INCLUDE_LAST);
  EXPECT_TRUE(v == "abc");
  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, Fragment)
{
  const char str[] = "ab;cd:10 ";
  Tokenizer p(str);
  nsDependentCSubstring f;

  Tokenizer::Token t1, t2;

  EXPECT_TRUE(p.Next(t1));
  EXPECT_TRUE(t1.Type() == Tokenizer::TOKEN_WORD);
  EXPECT_TRUE(t1.Fragment() == "ab");
  EXPECT_TRUE(t1.Fragment().BeginReading() == &str[0]);

  p.Rollback();
  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_WORD, t2));
  EXPECT_TRUE(t2.Fragment() == "ab");
  EXPECT_TRUE(t2.Fragment().BeginReading() == &str[0]);

  EXPECT_TRUE(p.Next(t1));
  EXPECT_TRUE(t1.Type() == Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(t1.Fragment() == ";");
  EXPECT_TRUE(t1.Fragment().BeginReading() == &str[2]);

  p.Rollback();
  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_CHAR, t2));
  EXPECT_TRUE(t2.Fragment() == ";");
  EXPECT_TRUE(t2.Fragment().BeginReading() == &str[2]);

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_WORD, t2));
  EXPECT_TRUE(t2.Fragment() == "cd");
  EXPECT_TRUE(t2.Fragment().BeginReading() == &str[3]);

  p.Rollback();
  EXPECT_TRUE(p.Next(t1));
  EXPECT_TRUE(t1.Type() == Tokenizer::TOKEN_WORD);
  EXPECT_TRUE(t1.Fragment() == "cd");
  EXPECT_TRUE(t1.Fragment().BeginReading() == &str[3]);

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_CHAR, t2));
  EXPECT_TRUE(t2.Fragment() == ":");
  EXPECT_TRUE(t2.Fragment().BeginReading() == &str[5]);

  p.Rollback();
  EXPECT_TRUE(p.Next(t1));
  EXPECT_TRUE(t1.Type() == Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(t1.Fragment() == ":");
  EXPECT_TRUE(t1.Fragment().BeginReading() == &str[5]);

  EXPECT_TRUE(p.Next(t1));
  EXPECT_TRUE(t1.Type() == Tokenizer::TOKEN_INTEGER);
  EXPECT_TRUE(t1.Fragment() == "10");
  EXPECT_TRUE(t1.Fragment().BeginReading() == &str[6]);

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_WS, t2));
  EXPECT_TRUE(t2.Fragment() == " ");
  EXPECT_TRUE(t2.Fragment().BeginReading() == &str[8]);

  EXPECT_TRUE(p.Check(Tokenizer::TOKEN_EOF, t1));
  EXPECT_TRUE(t1.Fragment() == "");
  EXPECT_TRUE(t1.Fragment().BeginReading() == &str[9]);
}

TEST(Tokenizer, SkipWhites)
{
  Tokenizer p("Text1 \nText2 \nText3\n Text4\n ");

  EXPECT_TRUE(p.CheckWord("Text1"));
  p.SkipWhites();
  EXPECT_TRUE(p.CheckEOL());

  EXPECT_TRUE(p.CheckWord("Text2"));
  p.SkipWhites(Tokenizer::INCLUDE_NEW_LINE);

  EXPECT_TRUE(p.CheckWord("Text3"));
  p.SkipWhites();
  EXPECT_TRUE(p.CheckEOL());
  p.SkipWhites();

  EXPECT_TRUE(p.CheckWord("Text4"));
  p.SkipWhites(Tokenizer::INCLUDE_NEW_LINE);
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, SkipCustomWhites)
{
  Tokenizer p("Text1 \n\r\t.Text2 \n\r\t.", " \n\r\t.");

  EXPECT_TRUE(p.CheckWord("Text1"));
  p.SkipWhites();
  EXPECT_TRUE(p.CheckWord("Text2"));
  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckWhite());
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, IntegerReading)
{
#define INT_6_BITS 64U
#define INT_30_BITS 1073741824UL
#define INT_32_BITS 4294967295UL
#define INT_50_BITS 1125899906842624ULL
#define STR_INT_MORE_THAN_64_BITS "922337203685477580899"

  {
    Tokenizer p(MOZ_STRINGIFY(INT_6_BITS));
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    EXPECT_TRUE(p.ReadInteger(&u8));
    EXPECT_TRUE(u8 == INT_6_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&u16));
    EXPECT_TRUE(u16 == INT_6_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&u32));
    EXPECT_TRUE(u32 == INT_6_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&u64));
    EXPECT_TRUE(u64 == INT_6_BITS);

    p.Rollback();

    int8_t s8;
    int16_t s16;
    int32_t s32;
    int64_t s64;
    EXPECT_TRUE(p.ReadInteger(&s8));
    EXPECT_TRUE(s8 == INT_6_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&s16));
    EXPECT_TRUE(s16 == INT_6_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&s32));
    EXPECT_TRUE(s32 == INT_6_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&s64));
    EXPECT_TRUE(s64 == INT_6_BITS);

    EXPECT_TRUE(p.CheckWord("U"));
    EXPECT_TRUE(p.CheckEOF());
  }

  {
    Tokenizer p(MOZ_STRINGIFY(INT_30_BITS));
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    EXPECT_FALSE(p.ReadInteger(&u8));
    EXPECT_FALSE(p.ReadInteger(&u16));
    EXPECT_TRUE(p.ReadInteger(&u32));
    EXPECT_TRUE(u32 == INT_30_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&u64));
    EXPECT_TRUE(u64 == INT_30_BITS);

    p.Rollback();

    int8_t s8;
    int16_t s16;
    int32_t s32;
    int64_t s64;
    EXPECT_FALSE(p.ReadInteger(&s8));
    EXPECT_FALSE(p.ReadInteger(&s16));
    EXPECT_TRUE(p.ReadInteger(&s32));
    EXPECT_TRUE(s32 == INT_30_BITS);
    p.Rollback();
    EXPECT_TRUE(p.ReadInteger(&s64));
    EXPECT_TRUE(s64 == INT_30_BITS);
    EXPECT_TRUE(p.CheckWord("UL"));
    EXPECT_TRUE(p.CheckEOF());
  }

  {
    Tokenizer p(MOZ_STRINGIFY(INT_32_BITS));
    uint32_t u32;
    int32_t s32;
    EXPECT_FALSE(p.ReadInteger(&s32));
    EXPECT_TRUE(p.ReadInteger(&u32));
    EXPECT_TRUE(u32 == INT_32_BITS);
    EXPECT_TRUE(p.CheckWord("UL"));
    EXPECT_TRUE(p.CheckEOF());
  }

  {
    Tokenizer p(MOZ_STRINGIFY(INT_50_BITS));
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    EXPECT_FALSE(p.ReadInteger(&u8));
    EXPECT_FALSE(p.ReadInteger(&u16));
    EXPECT_FALSE(p.ReadInteger(&u32));
    EXPECT_TRUE(p.ReadInteger(&u64));
    EXPECT_TRUE(u64 == INT_50_BITS);
    EXPECT_TRUE(p.CheckWord("ULL"));
    EXPECT_TRUE(p.CheckEOF());
  }

  {
    Tokenizer p(STR_INT_MORE_THAN_64_BITS);
    int64_t i;
    EXPECT_FALSE(p.ReadInteger(&i));
    uint64_t u;
    EXPECT_FALSE(p.ReadInteger(&u));
    EXPECT_FALSE(p.CheckEOF());
  }
}

TEST(Tokenizer, ReadUntil)
{
  Tokenizer p("Hello;test 4,");
  nsDependentCSubstring f;
  EXPECT_TRUE(p.ReadUntil(Tokenizer::Token::Char(';'), f));
  EXPECT_TRUE(f == "Hello");
  p.Rollback();

  EXPECT_TRUE(
      p.ReadUntil(Tokenizer::Token::Char(';'), f, Tokenizer::INCLUDE_LAST));
  EXPECT_TRUE(f == "Hello;");
  p.Rollback();

  EXPECT_FALSE(p.ReadUntil(Tokenizer::Token::Char('!'), f));
  EXPECT_TRUE(f == "Hello;test 4,");
  p.Rollback();

  EXPECT_TRUE(p.ReadUntil(Tokenizer::Token::Word("test"_ns), f));
  EXPECT_TRUE(f == "Hello;");
  p.Rollback();

  EXPECT_TRUE(p.ReadUntil(Tokenizer::Token::Word("test"_ns), f,
                          Tokenizer::INCLUDE_LAST));
  EXPECT_TRUE(f == "Hello;test");
  EXPECT_TRUE(p.ReadUntil(Tokenizer::Token::Char(','), f));
  EXPECT_TRUE(f == " 4");
}

TEST(Tokenizer, SkipUntil)
{
  {
    Tokenizer p("test1,test2,,,test3");

    p.SkipUntil(Tokenizer::Token::Char(','));
    EXPECT_TRUE(p.CheckChar(','));
    EXPECT_TRUE(p.CheckWord("test2"));

    p.SkipUntil(Tokenizer::Token::Char(','));  // must not move
    EXPECT_TRUE(p.CheckChar(','));  // check the first comma of the ',,,' string

    p.Rollback();  // moves cursor back to the first comma of the ',,,' string

    p.SkipUntil(
        Tokenizer::Token::Char(','));  // must not move, we are on the ',' char
    EXPECT_TRUE(p.CheckChar(','));
    EXPECT_TRUE(p.CheckChar(','));
    EXPECT_TRUE(p.CheckChar(','));
    EXPECT_TRUE(p.CheckWord("test3"));
    p.Rollback();

    p.SkipUntil(Tokenizer::Token::Char(','));
    EXPECT_TRUE(p.CheckEOF());
  }

  {
    Tokenizer p("test0,test1,test2");

    p.SkipUntil(Tokenizer::Token::Char(','));
    EXPECT_TRUE(p.CheckChar(','));

    p.SkipUntil(Tokenizer::Token::Char(','));
    p.Rollback();

    EXPECT_TRUE(p.CheckWord("test1"));
    EXPECT_TRUE(p.CheckChar(','));

    p.SkipUntil(Tokenizer::Token::Char(','));
    p.Rollback();

    EXPECT_TRUE(p.CheckWord("test2"));
    EXPECT_TRUE(p.CheckEOF());
  }
}

TEST(Tokenizer, Custom)
{
  Tokenizer p(
      "aaaaaacustom-1\r,custom-1,Custom-1,Custom-1,00custom-2xxxx,CUSTOM-2");

  Tokenizer::Token c1 =
      p.AddCustomToken("custom-1", Tokenizer::CASE_INSENSITIVE);
  Tokenizer::Token c2 = p.AddCustomToken("custom-2", Tokenizer::CASE_SENSITIVE);

  // It's expected to NOT FIND the custom token if it's not on an edge
  // between other recognizable tokens.
  EXPECT_TRUE(p.CheckWord("aaaaaacustom"));
  EXPECT_TRUE(p.CheckChar('-'));
  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(1)));
  EXPECT_TRUE(p.CheckEOL());
  EXPECT_TRUE(p.CheckChar(','));

  EXPECT_TRUE(p.Check(c1));
  EXPECT_TRUE(p.CheckChar(','));

  EXPECT_TRUE(p.Check(c1));
  EXPECT_TRUE(p.CheckChar(','));

  p.EnableCustomToken(c1, false);
  EXPECT_TRUE(p.CheckWord("Custom"));
  EXPECT_TRUE(p.CheckChar('-'));
  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(1)));
  EXPECT_TRUE(p.CheckChar(','));

  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(0)));
  EXPECT_TRUE(p.Check(c2));
  EXPECT_TRUE(p.CheckWord("xxxx"));
  EXPECT_TRUE(p.CheckChar(','));

  EXPECT_TRUE(p.CheckWord("CUSTOM"));
  EXPECT_TRUE(p.CheckChar('-'));
  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(2)));

  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, CustomRaw)
{
  Tokenizer p(
      "aaaaaacustom-1\r,custom-1,Custom-1,Custom-1,00custom-2xxxx,CUSTOM-2");

  Tokenizer::Token c1 =
      p.AddCustomToken("custom-1", Tokenizer::CASE_INSENSITIVE);
  Tokenizer::Token c2 = p.AddCustomToken("custom-2", Tokenizer::CASE_SENSITIVE);

  // In this mode it's expected to find all custom tokens among any kind of
  // input.
  p.SetTokenizingMode(Tokenizer::Mode::CUSTOM_ONLY);

  Tokenizer::Token t;

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_RAW);
  EXPECT_TRUE(t.Fragment().EqualsLiteral("aaaaaa"));

  EXPECT_TRUE(p.Check(c1));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_RAW);
  EXPECT_TRUE(t.Fragment().EqualsLiteral("\r,"));

  EXPECT_TRUE(p.Check(c1));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_RAW);
  EXPECT_TRUE(t.Fragment().EqualsLiteral(","));

  EXPECT_TRUE(p.Check(c1));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_RAW);
  EXPECT_TRUE(t.Fragment().EqualsLiteral(","));

  EXPECT_TRUE(p.Check(c1));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_RAW);
  EXPECT_TRUE(t.Fragment().EqualsLiteral(",00"));

  EXPECT_TRUE(p.Check(c2));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_RAW);
  EXPECT_TRUE(t.Fragment().EqualsLiteral("xxxx,CUSTOM-2"));

  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, Incremental)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Equals(Token::Word("test1"_ns)));
            break;
          case 2:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 3:
            EXPECT_TRUE(t.Equals(Token::Word("test2"_ns)));
            break;
          case 4:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 5:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 6:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 7:
            EXPECT_TRUE(t.Equals(Token::Word("test3"_ns)));
            break;
          case 8:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }

        return NS_OK;
      });

  constexpr auto input = "test1,test2,,,test3"_ns;
  const auto* cur = input.BeginReading();
  const auto* end = input.EndReading();
  for (; cur < end; ++cur) {
    i.FeedInput(nsDependentCSubstring(cur, 1));
  }

  EXPECT_TRUE(test == 6);
  i.FinishInput();
  EXPECT_TRUE(test == 8);
}

TEST(Tokenizer, IncrementalRollback)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Equals(Token::Word("test1"_ns)));
            break;
          case 2:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 3:
            EXPECT_TRUE(t.Equals(Token::Word("test2"_ns)));
            i.Rollback();  // so that we get the token again
            break;
          case 4:
            EXPECT_TRUE(t.Equals(Token::Word("test2"_ns)));
            break;
          case 5:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 6:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 7:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 8:
            EXPECT_TRUE(t.Equals(Token::Word("test3"_ns)));
            break;
          case 9:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }

        return NS_OK;
      });

  constexpr auto input = "test1,test2,,,test3"_ns;
  const auto* cur = input.BeginReading();
  const auto* end = input.EndReading();
  for (; cur < end; ++cur) {
    i.FeedInput(nsDependentCSubstring(cur, 1));
  }

  EXPECT_TRUE(test == 7);
  i.FinishInput();
  EXPECT_TRUE(test == 9);
}

TEST(Tokenizer, IncrementalNeedMoreInput)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        Token t2;
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Equals(Token::Word("a"_ns)));
            break;
          case 2:
          case 3:
          case 4:
          case 5:
            EXPECT_TRUE(t.Equals(Token::Whitespace()));
            if (i.Next(t2)) {
              EXPECT_TRUE(test == 5);
              EXPECT_TRUE(t2.Equals(Token::Word("bb"_ns)));
            } else {
              EXPECT_TRUE(test < 5);
              i.NeedMoreInput();
            }
            break;
          case 6:
            EXPECT_TRUE(t.Equals(Token::Char(',')));
            break;
          case 7:
            EXPECT_TRUE(t.Equals(Token::Word("c"_ns)));
            return NS_ERROR_FAILURE;
          default:
            EXPECT_TRUE(false);
            break;
        }

        return NS_OK;
      });

  constexpr auto input = "a bb,c"_ns;
  const auto* cur = input.BeginReading();
  const auto* end = input.EndReading();

  nsresult rv;
  for (; cur < end; ++cur) {
    rv = i.FeedInput(nsDependentCSubstring(cur, 1));
    if (NS_FAILED(rv)) {
      break;
    }
  }

  EXPECT_TRUE(rv == NS_OK);
  EXPECT_TRUE(test == 6);

  rv = i.FinishInput();
  EXPECT_TRUE(rv == NS_ERROR_FAILURE);
  EXPECT_TRUE(test == 7);
}

TEST(Tokenizer, IncrementalCustom)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  Token custom;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Equals(custom));
            break;
          case 2:
            EXPECT_TRUE(t.Equals(Token::Word("bla"_ns)));
            break;
          case 3:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }

        return NS_OK;
      },
      nullptr, "-");

  custom = i.AddCustomToken("some-test", Tokenizer::CASE_SENSITIVE);
  i.FeedInput("some-"_ns);
  EXPECT_TRUE(test == 0);
  i.FeedInput("tes"_ns);
  EXPECT_TRUE(test == 0);
  i.FeedInput("tbla"_ns);
  EXPECT_TRUE(test == 1);
  i.FinishInput();
  EXPECT_TRUE(test == 3);
}

TEST(Tokenizer, IncrementalCustomRaw)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  Token custom;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("test1,"));
            break;
          case 2:
            EXPECT_TRUE(t.Equals(custom));
            break;
          case 3:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("!,,test3"));
            i.Rollback();
            i.SetTokenizingMode(Tokenizer::Mode::FULL);
            break;
          case 4:
            EXPECT_TRUE(t.Equals(Token::Char('!')));
            i.SetTokenizingMode(Tokenizer::Mode::CUSTOM_ONLY);
            break;
          case 5:
            EXPECT_TRUE(t.Fragment().EqualsLiteral(",,test3"));
            break;
          case 6:
            EXPECT_TRUE(t.Equals(custom));
            break;
          case 7:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("tes"));
            break;
          case 8:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }

        return NS_OK;
      });

  custom = i.AddCustomToken("test2", Tokenizer::CASE_SENSITIVE);
  i.SetTokenizingMode(Tokenizer::Mode::CUSTOM_ONLY);

  constexpr auto input = "test1,test2!,,test3test2tes"_ns;
  const auto* cur = input.BeginReading();
  const auto* end = input.EndReading();
  for (; cur < end; ++cur) {
    i.FeedInput(nsDependentCSubstring(cur, 1));
  }

  EXPECT_TRUE(test == 6);
  i.FinishInput();
  EXPECT_TRUE(test == 8);
}

TEST(Tokenizer, IncrementalCustomRemove)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  Token custom;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Equals(custom));
            i.RemoveCustomToken(custom);
            break;
          case 2:
            EXPECT_FALSE(t.Equals(custom));
            break;
          case 3:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }

        return NS_OK;
      });

  custom = i.AddCustomToken("custom1", Tokenizer::CASE_SENSITIVE);

  constexpr auto input = "custom1custom1"_ns;
  i.FeedInput(input);
  EXPECT_TRUE(test == 1);
  i.FinishInput();
  EXPECT_TRUE(test == 3);
}

TEST(Tokenizer, IncrementalBuffering1)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  Token custom;
  nsDependentCSubstring observedFragment;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("012"));
            break;
          case 2:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("3456789"));
            break;
          case 3:
            EXPECT_TRUE(t.Equals(custom));
            break;
          case 4:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("qwe"));
            break;
          case 5:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("rt"));
            break;
          case 6:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }

        observedFragment.Rebind(t.Fragment().BeginReading(),
                                t.Fragment().Length());
        return NS_OK;
      },
      nullptr, nullptr, 3);

  custom = i.AddCustomToken("aaa", Tokenizer::CASE_SENSITIVE);
  // This externally unused token is added only to check the internal algorithm
  // does work correctly as expected when there are two different length tokens.
  Unused << i.AddCustomToken("bb", Tokenizer::CASE_SENSITIVE);
  i.SetTokenizingMode(Tokenizer::Mode::CUSTOM_ONLY);

  i.FeedInput("01234"_ns);
  EXPECT_TRUE(test == 1);
  EXPECT_TRUE(observedFragment.EqualsLiteral("012"));

  i.FeedInput("5"_ns);
  EXPECT_TRUE(test == 1);
  i.FeedInput("6789aa"_ns);
  EXPECT_TRUE(test == 2);
  EXPECT_TRUE(observedFragment.EqualsLiteral("3456789"));

  i.FeedInput("aqwert"_ns);
  EXPECT_TRUE(test == 4);
  EXPECT_TRUE(observedFragment.EqualsLiteral("qwe"));

  i.FinishInput();
  EXPECT_TRUE(test == 6);
}

TEST(Tokenizer, IncrementalBuffering2)
{
  using Token = IncrementalTokenizer::Token;

  int test = 0;
  Token custom;
  IncrementalTokenizer i(
      [&](Token const& t, IncrementalTokenizer& i) -> nsresult {
        switch (++test) {
          case 1:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("01"));
            break;
          case 2:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("234567"));
            break;
          case 3:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("89"));
            break;
          case 4:
            EXPECT_TRUE(t.Equals(custom));
            break;
          case 5:
            EXPECT_TRUE(t.Fragment().EqualsLiteral("qwert"));
            break;
          case 6:
            EXPECT_TRUE(t.Equals(Token::EndOfFile()));
            break;
        }
        return NS_OK;
      },
      nullptr, nullptr, 3);

  custom = i.AddCustomToken("aaa", Tokenizer::CASE_SENSITIVE);
  // This externally unused token is added only to check the internal algorithm
  // does work correctly as expected when there are two different length tokens.
  Unused << i.AddCustomToken("bbbbb", Tokenizer::CASE_SENSITIVE);
  i.SetTokenizingMode(Tokenizer::Mode::CUSTOM_ONLY);

  i.FeedInput("01234"_ns);
  EXPECT_TRUE(test == 0);
  i.FeedInput("5"_ns);
  EXPECT_TRUE(test == 1);
  i.FeedInput("6789aa"_ns);
  EXPECT_TRUE(test == 2);
  i.FeedInput("aqwert"_ns);
  EXPECT_TRUE(test == 4);
  i.FinishInput();
  EXPECT_TRUE(test == 6);
}

TEST(Tokenizer, RecordAndReadUntil)
{
  Tokenizer t("aaaa,bbbb");
  t.SkipWhites();
  nsDependentCSubstring subject;

  EXPECT_TRUE(t.ReadUntil(mozilla::Tokenizer::Token::Char(','), subject));
  EXPECT_FALSE(t.CheckChar(','));
  EXPECT_TRUE(subject.Length() == 4);
  EXPECT_TRUE(subject == "aaaa");

  EXPECT_FALSE(t.ReadUntil(mozilla::Tokenizer::Token::Char(','), subject));
  EXPECT_TRUE(subject.Length() == 4);
  EXPECT_TRUE(subject == "bbbb");

  EXPECT_FALSE(t.ReadUntil(mozilla::Tokenizer::Token::Char(','), subject));
  EXPECT_TRUE(subject.Length() == 0);

  EXPECT_TRUE(t.CheckEOF());
}

TEST(Tokenizer, ReadIntegers)
{
  // Make sure that adding dash (the 'minus' sign) as an additional char
  // doesn't break reading negative numbers.
  Tokenizer t("100,-100,200,-200,4294967295,-4294967295,-2147483647", nullptr,
              "-");

  uint32_t unsigned_value32;
  int32_t signed_value32;
  int64_t signed_value64;

  // "100,"
  EXPECT_TRUE(t.ReadInteger(&unsigned_value32));
  EXPECT_TRUE(unsigned_value32 == 100);
  EXPECT_TRUE(t.CheckChar(','));

  // "-100,"
  EXPECT_FALSE(t.ReadInteger(&unsigned_value32));
  EXPECT_FALSE(t.CheckChar(','));

  EXPECT_TRUE(t.ReadSignedInteger(&signed_value32));
  EXPECT_TRUE(signed_value32 == -100);
  EXPECT_TRUE(t.CheckChar(','));

  // "200,"
  EXPECT_TRUE(t.ReadSignedInteger(&signed_value32));
  EXPECT_TRUE(signed_value32 == 200);
  EXPECT_TRUE(t.CheckChar(','));

  // "-200,"
  EXPECT_TRUE(t.ReadSignedInteger(&signed_value32));
  EXPECT_TRUE(signed_value32 == -200);
  EXPECT_TRUE(t.CheckChar(','));

  // "4294967295,"
  EXPECT_FALSE(t.ReadSignedInteger(&signed_value32));
  EXPECT_FALSE(t.CheckChar(','));

  EXPECT_TRUE(t.ReadInteger(&unsigned_value32));
  EXPECT_TRUE(unsigned_value32 == 4294967295UL);
  EXPECT_TRUE(t.CheckChar(','));

  // "-4294967295,"
  EXPECT_FALSE(t.ReadSignedInteger(&signed_value32));
  EXPECT_FALSE(t.CheckChar(','));

  EXPECT_FALSE(t.ReadInteger(&unsigned_value32));
  EXPECT_FALSE(t.CheckChar(','));

  EXPECT_TRUE(t.ReadSignedInteger(&signed_value64));
  EXPECT_TRUE(signed_value64 == -4294967295LL);
  EXPECT_TRUE(t.CheckChar(','));

  // "-2147483647"
  EXPECT_FALSE(t.ReadInteger(&unsigned_value32));
  EXPECT_FALSE(t.CheckChar(','));

  EXPECT_TRUE(t.ReadSignedInteger(&signed_value32));
  EXPECT_TRUE(signed_value32 == -2147483647L);
  EXPECT_TRUE(t.CheckEOF());
}

TEST(Tokenizer, CheckPhrase)
{
  Tokenizer t("foo bar baz");

  EXPECT_TRUE(t.CheckPhrase("foo "));

  EXPECT_FALSE(t.CheckPhrase("barr"));
  EXPECT_FALSE(t.CheckPhrase("BAR BAZ"));
  EXPECT_FALSE(t.CheckPhrase("bar baz "));
  EXPECT_FALSE(t.CheckPhrase("b"));
  EXPECT_FALSE(t.CheckPhrase("ba"));
  EXPECT_FALSE(t.CheckPhrase("??"));

  EXPECT_TRUE(t.CheckPhrase("bar baz"));

  t.Rollback();
  EXPECT_TRUE(t.CheckPhrase("bar"));
  EXPECT_TRUE(t.CheckPhrase(" baz"));

  t.Rollback();
  EXPECT_FALSE(t.CheckPhrase("\tbaz"));
  EXPECT_TRUE(t.CheckPhrase(" baz"));
  EXPECT_TRUE(t.CheckEOF());
}
