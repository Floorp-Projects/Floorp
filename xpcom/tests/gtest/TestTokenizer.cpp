#include "mozilla/Tokenizer.h"
#include "gtest/gtest.h"

using namespace mozilla;

static bool IsOperator(char const c)
{
  return c == '+' || c == '*';
}

static bool HttpHeaderCharacter(char const c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         (c == '_') ||
         (c == '-');
}

TEST(Tokenizer, HTTPResponse)
{
  Tokenizer::Token t;

  // Real life test, HTTP response

  Tokenizer p(NS_LITERAL_CSTRING(
    "HTTP/1.0 304 Not modified\r\n"
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
  while (p.Next(t) && t.Type() != Tokenizer::TOKEN_EOL);
  EXPECT_FALSE(p.HasFailed());
  nsAutoCString h;
  p.Claim(h);
  EXPECT_TRUE(h == "Not modified");

  p.Record();
  while (p.CheckChar(HttpHeaderCharacter));
  p.Claim(h, Tokenizer::INCLUDE_LAST);
  EXPECT_TRUE(h == "ETag");
  p.SkipWhites();
  EXPECT_TRUE(p.CheckChar(':'));
  p.SkipWhites();
  p.Record();
  while (p.Next(t) && t.Type() != Tokenizer::TOKEN_EOL);
  EXPECT_FALSE(p.HasFailed());
  p.Claim(h);
  EXPECT_TRUE(h == "hallo");

  p.Record();
  while (p.CheckChar(HttpHeaderCharacter));
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
  while (p.Next(t) && t.Type() != Tokenizer::TOKEN_EOF);
  nsAutoCString b;
  p.Claim(b);
  EXPECT_TRUE(b == "This is the body");
}

TEST(Tokenizer, Main)
{
  Tokenizer::Token t;

  // Synthetic code-specific test

  Tokenizer p(NS_LITERAL_CSTRING("test123 ,15  \t*\r\n%xx,-15\r\r"));

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

TEST(Tokenizer, SingleWord)
{
  // Single word with numbers in it test

  Tokenizer p(NS_LITERAL_CSTRING("test123"));

  EXPECT_TRUE(p.CheckWord("test123"));
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, EndingAfterNumber)
{
  // An end handling after a number

  Tokenizer p(NS_LITERAL_CSTRING("123"));

  EXPECT_FALSE(p.CheckWord("123"));
  EXPECT_TRUE(p.Check(Tokenizer::Token::Number(123)));
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, BadInteger)
{
  Tokenizer::Token t;

  // A bad integer test

  Tokenizer p(NS_LITERAL_CSTRING("189234891274981758617846178651647620587135"));

  EXPECT_TRUE(p.Next(t));
  EXPECT_TRUE(t.Type() == Tokenizer::TOKEN_ERROR);
  EXPECT_TRUE(p.CheckEOF());
}

TEST(Tokenizer, CheckExpectedTokenValue)
{
  Tokenizer::Token t;

  // Check expected token value test

  Tokenizer p(NS_LITERAL_CSTRING("blue velvet"));

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

  Tokenizer p1(NS_LITERAL_CSTRING("a b"));

  while (p1.Next(t) && t.Type() != Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(p1.HasFailed());


  Tokenizer p2(NS_LITERAL_CSTRING("a b ?!c"));

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

  while (p2.Next(t) && t.Type() != Tokenizer::TOKEN_CHAR);
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
    Tokenizer p1(NS_LITERAL_CSTRING("test"));
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
  Tokenizer p1(NS_LITERAL_CSTRING("test-custom*words and\tdefault-whites"), nullptr, "-*");
  EXPECT_TRUE(p1.CheckWord("test-custom*words"));
  EXPECT_TRUE(p1.CheckWhite());
  EXPECT_TRUE(p1.CheckWord("and"));
  EXPECT_TRUE(p1.CheckWhite());
  EXPECT_TRUE(p1.CheckWord("default-whites"));

  Tokenizer p2(NS_LITERAL_CSTRING("test, custom,whites"), ", ");
  EXPECT_TRUE(p2.CheckWord("test"));
  EXPECT_TRUE(p2.CheckWhite());
  EXPECT_TRUE(p2.CheckWhite());
  EXPECT_TRUE(p2.CheckWord("custom"));
  EXPECT_TRUE(p2.CheckWhite());
  EXPECT_TRUE(p2.CheckWord("whites"));

  Tokenizer p3(NS_LITERAL_CSTRING("test, custom, whites-and#word-chars"), ",", "-#");
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
  nsDependentCString test2;
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

static bool ABChar(const char aChar)
{
  return aChar == 'a' || aChar == 'b';
}

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

TEST(Tokenizer, IntegerReading)
{
#define INT_6_BITS                 64U
#define INT_30_BITS                1073741824UL
#define INT_32_BITS                4294967295UL
#define INT_50_BITS                1125899906842624ULL
#define STR_INT_MORE_THAN_64_BITS "922337203685477580899"

  {
    Tokenizer p(NS_STRINGIFY(INT_6_BITS));
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
    Tokenizer p(NS_STRINGIFY(INT_30_BITS));
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
    Tokenizer p(NS_STRINGIFY(INT_32_BITS));
    uint32_t u32;
    int32_t s32;
    EXPECT_FALSE(p.ReadInteger(&s32));
    EXPECT_TRUE(p.ReadInteger(&u32));
    EXPECT_TRUE(u32 == INT_32_BITS);
    EXPECT_TRUE(p.CheckWord("UL"));
    EXPECT_TRUE(p.CheckEOF());
  }

  {
    Tokenizer p(NS_STRINGIFY(INT_50_BITS));
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
