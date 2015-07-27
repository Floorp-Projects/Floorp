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


  Tokenizer p2(NS_LITERAL_CSTRING("a b"));

  EXPECT_FALSE(p2.CheckChar('c'));
  EXPECT_TRUE(p2.HasFailed());
  EXPECT_TRUE(p2.CheckChar(HttpHeaderCharacter));
  EXPECT_FALSE(p2.HasFailed());
  p2.SkipWhites();
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_TRUE(p2.Next(t));
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_TRUE(p2.Next(t));
  EXPECT_FALSE(p2.HasFailed());
  EXPECT_FALSE(p2.CheckChar('c'));
  EXPECT_TRUE(p2.HasFailed());

  while (p2.Next(t) && t.Type() != Tokenizer::TOKEN_CHAR);
  EXPECT_TRUE(p2.HasFailed());
}
