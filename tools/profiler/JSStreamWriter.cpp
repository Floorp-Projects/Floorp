/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSStreamWriter.h"

#include "mozilla/ArrayUtils.h" // for ArrayLength
#include "nsDataHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUTF8Utils.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
 #define snprintf _snprintf
#endif

#define ARRAY (void*)1
#define OBJECT (void*)2

// Escape a UTF8 string to a stream. When an illegal encoding
// is found it will insert "INVALID" and the function will return.
static void EscapeToStream(std::ostream& stream, const char* str) {
  stream << "\"";

  size_t len = strlen(str);
  const char* end = &str[len];
  while (str < end) {
    bool err;
    const char* utf8CharStart = str;
    uint32_t ucs4Char = UTF8CharEnumerator::NextChar(&str, end, &err);

    if (err) {
      // Encoding error
      stream << "INVALID\"";
      return;
    }

    // See http://www.ietf.org/rfc/rfc4627.txt?number=4627
    // characters that must be escaped: quotation mark,
    // reverse solidus, and the control characters
    // (U+0000 through U+001F).
    if (ucs4Char == '\"') {
      stream << "\\\"";
    } else if (ucs4Char == '\\') {
      stream << "\\\\";
    } else if (ucs4Char > 0xFF) {
      char16_t chr[2];
      ConvertUTF8toUTF16 encoder(chr);
      encoder.write(utf8CharStart, uint32_t(str-utf8CharStart));
      char escChar[13];
      snprintf(escChar, mozilla::ArrayLength(escChar), "\\u%04X\\u%04X", chr[0], chr[1]);
      stream << escChar;
    } else if (ucs4Char < 0x1F || ucs4Char > 0xFF) {
      char escChar[7];
      snprintf(escChar, mozilla::ArrayLength(escChar), "\\u%04X", ucs4Char);
      stream << escChar;
    } else {
      stream << char(ucs4Char);
    }
  }
  stream << "\"";
}

JSStreamWriter::JSStreamWriter(std::ostream& aStream)
  : mStream(aStream)
  , mNeedsComma(false)
  , mNeedsName(false)
{ }

JSStreamWriter::~JSStreamWriter()
{
  MOZ_ASSERT(mStack.GetSize() == 0);
}

void
JSStreamWriter::BeginObject()
{
  MOZ_ASSERT(!mNeedsName);
  if (mNeedsComma && mStack.Peek() == ARRAY) {
    mStream << ",";
  }
  mStream << "{";
  mNeedsComma = false;
  mNeedsName = true;
  mStack.Push(OBJECT);
}

void
JSStreamWriter::EndObject()
{
  MOZ_ASSERT(mStack.Peek() == OBJECT);
  mStream << "}";
  mNeedsComma = true;
  mNeedsName = false;
  mStack.Pop();
  if (mStack.GetSize() > 0 && mStack.Peek() == OBJECT) {
    mNeedsName = true;
  }
}

void
JSStreamWriter::BeginArray()
{
  MOZ_ASSERT(!mNeedsName);
  if (mNeedsComma && mStack.Peek() == ARRAY) {
    mStream << ",";
  }
  mStream << "[";
  mNeedsComma = false;
  mStack.Push(ARRAY);
}

void
JSStreamWriter::EndArray()
{
  MOZ_ASSERT(!mNeedsName);
  MOZ_ASSERT(mStack.Peek() == ARRAY);
  mStream << "]";
  mNeedsComma = true;
  mStack.Pop();
  if (mStack.GetSize() > 0 && mStack.Peek() == OBJECT) {
    mNeedsName = true;
  }
}

void
JSStreamWriter::Name(const char *aName)
{
  MOZ_ASSERT(mNeedsName);
  if (mNeedsComma && mStack.Peek() == OBJECT) {
    mStream << ",";
  }
  EscapeToStream(mStream, aName);
  mStream << ":";
  mNeedsName = false;
}

void
JSStreamWriter::Value(int aValue)
{
  MOZ_ASSERT(!mNeedsName);
  if (mNeedsComma && mStack.Peek() == ARRAY) {
    mStream << ",";
  }
  mStream << aValue;
  mNeedsComma = true;
  if (mStack.Peek() == OBJECT) {
    mNeedsName = true;
  }
}

void
JSStreamWriter::Value(unsigned aValue)
{
  MOZ_ASSERT(!mNeedsName);
  if (mNeedsComma && mStack.Peek() == ARRAY) {
    mStream << ",";
  }
  mStream << aValue;
  mNeedsComma = true;
  if (mStack.Peek() == OBJECT) {
    mNeedsName = true;
  }
}

void
JSStreamWriter::Value(double aValue)
{
  MOZ_ASSERT(!mNeedsName);
  if (mNeedsComma && mStack.Peek() == ARRAY) {
    mStream << ",";
  }
  mStream.precision(18);
  mStream << aValue;
  mNeedsComma = true;
  if (mStack.Peek() == OBJECT) {
    mNeedsName = true;
  }
}

void
JSStreamWriter::Value(const char *aValue)
{
  MOZ_ASSERT(!mNeedsName);
  if (mNeedsComma && mStack.Peek() == ARRAY) {
    mStream << ",";
  }
  EscapeToStream(mStream, aValue);
  mNeedsComma = true;
  if (mStack.Peek() == OBJECT) {
    mNeedsName = true;
  }
}
