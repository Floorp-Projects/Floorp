/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecurityHeaderParser.h"
#include "mozilla/Logging.h"

// The character classes in this file are informed by [RFC2616], Section 2.2.
// signed char is a signed data type one byte (8 bits) wide, so its value can
// never be greater than 127. The following implicitly makes use of this.

// A token is one or more CHAR except CTLs or separators.
// A CHAR is any US-ASCII character (octets 0 - 127).
// A CTL is any US-ASCII control character (octets 0 - 31) and DEL (127).
// A separator is one of ()<>@,;:\"/[]?={} as well as space and
// horizontal-tab (32 and 9, respectively).
// So, this returns true if chr is any octet 33-126 except ()<>@,;:\"/[]?={}
bool
IsTokenSymbol(signed char chr) {
  if (chr < 33 || chr == 127 ||
      chr == '(' || chr == ')' || chr == '<' || chr == '>' ||
      chr == '@' || chr == ',' || chr == ';' || chr == ':' ||
      chr == '"' || chr == '/' || chr == '[' || chr == ']' ||
      chr == '?' || chr == '=' || chr == '{' || chr == '}' || chr == '\\') {
    return false;
  }
  return true;
}

// A quoted-string consists of a quote (") followed by any amount of
// qdtext or quoted-pair, followed by a quote.
// qdtext is any TEXT except a quote.
// TEXT is any 8-bit octet except CTLs, but including LWS.
// quoted-pair is a backslash (\) followed by a CHAR.
// So, it turns out, \ can't really be a qdtext symbol for our purposes.
// This returns true if chr is any octet 9,10,13,32-126 except <"> or "\"
bool
IsQuotedTextSymbol(signed char chr) {
  return ((chr >= 32 && chr != '"' && chr != '\\' && chr != 127) ||
          chr == 0x9 || chr == 0xa || chr == 0xd);
}

// The octet following the "\" in a quoted pair can be anything 0-127.
bool
IsQuotedPairSymbol(signed char chr) {
  return (chr >= 0);
}

static mozilla::LazyLogModule sSHParserLog("nsSecurityHeaderParser");

#define SHPARSERLOG(args) MOZ_LOG(sSHParserLog, mozilla::LogLevel::Debug, args)

nsSecurityHeaderParser::nsSecurityHeaderParser(const nsCString& aHeader)
  : mCursor(aHeader.get())
  , mDirective(nullptr)
  , mError(false)
{
}

nsSecurityHeaderParser::~nsSecurityHeaderParser() {
  nsSecurityHeaderDirective *directive;
  while ((directive = mDirectives.popFirst())) {
    delete directive;
  }
}

mozilla::LinkedList<nsSecurityHeaderDirective> *
nsSecurityHeaderParser::GetDirectives() {
  return &mDirectives;
}

nsresult
nsSecurityHeaderParser::Parse() {
  MOZ_ASSERT(mDirectives.isEmpty());
  SHPARSERLOG(("trying to parse '%s'", mCursor));

  Header();

  // if we didn't consume the entire input, we were unable to parse it => error
  if (mError || *mCursor) {
    return NS_ERROR_FAILURE;
  } else {
    return NS_OK;
  }
}

bool
nsSecurityHeaderParser::Accept(char aChr)
{
  if (*mCursor == aChr) {
    Advance();
    return true;
  }

  return false;
}

bool
nsSecurityHeaderParser::Accept(bool (*aClassifier) (signed char))
{
  if (aClassifier(*mCursor)) {
    Advance();
    return true;
  }

  return false;
}

void
nsSecurityHeaderParser::Expect(char aChr)
{
  if (*mCursor != aChr) {
    mError = true;
  } else {
    Advance();
  }
}

void
nsSecurityHeaderParser::Advance()
{
  // Technically, 0 is valid in quoted-pair, but we were handed a
  // null-terminated const char *, so this doesn't handle that.
  if (*mCursor) {
    mOutput.Append(*mCursor);
    mCursor++;
  } else {
    mError = true;
  }
}

void
nsSecurityHeaderParser::Header()
{
  Directive();
  while (Accept(';')) {
    Directive();
  }
}

void
nsSecurityHeaderParser::Directive()
{
  mDirective = new nsSecurityHeaderDirective();
  LWSMultiple();
  DirectiveName();
  LWSMultiple();
  if (Accept('=')) {
    LWSMultiple();
    DirectiveValue();
    LWSMultiple();
  }
  mDirectives.insertBack(mDirective);
  SHPARSERLOG(("read directive name '%s', value '%s'",
               mDirective->mName.Data(), mDirective->mValue.Data()));
}

void
nsSecurityHeaderParser::DirectiveName()
{
  mOutput.Truncate(0);
  Token();
  mDirective->mName.Assign(mOutput);
}

void
nsSecurityHeaderParser::DirectiveValue()
{
  mOutput.Truncate(0);
  if (Accept(IsTokenSymbol)) {
    Token();
    mDirective->mValue.Assign(mOutput);
  } else if (Accept('"')) {
    // Accept advances the cursor if successful, which appends a character to
    // mOutput. The " is not part of what we want to capture, so truncate
    // mOutput again.
    mOutput.Truncate(0);
    QuotedString();
    mDirective->mValue.Assign(mOutput);
    Expect('"');
  }
}

void
nsSecurityHeaderParser::Token()
{
  while (Accept(IsTokenSymbol));
}

void
nsSecurityHeaderParser::QuotedString()
{
  while (true) {
    if (Accept(IsQuotedTextSymbol)) {
      QuotedText();
    } else if (Accept('\\')) {
      QuotedPair();
    } else {
      break;
    }
  }
}

void
nsSecurityHeaderParser::QuotedText()
{
  while (Accept(IsQuotedTextSymbol));
}

void
nsSecurityHeaderParser::QuotedPair()
{
  Accept(IsQuotedPairSymbol);
}

void
nsSecurityHeaderParser::LWSMultiple()
{
  while (true) {
    if (Accept('\r')) {
      LWSCRLF();
    } else if (Accept(' ') || Accept('\t')) {
      LWS();
    } else {
      break;
    }
  }
}

void
nsSecurityHeaderParser::LWSCRLF() {
  Expect('\n');
  if (!(Accept(' ') || Accept('\t'))) {
    mError = true;
  }
  LWS();
}

void
nsSecurityHeaderParser::LWS()
{
  // Note that becaue of how we're called, we don't have to check for
  // the mandatory presense of at least one of SP or HT.
  while (Accept(' ') || Accept('\t'));
}
