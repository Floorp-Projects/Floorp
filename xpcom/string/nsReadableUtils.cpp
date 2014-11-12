/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReadableUtils.h"

#include "nsMemory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUTF8Utils.h"

void
LossyCopyUTF16toASCII(const nsAString& aSource, nsACString& aDest)
{
  aDest.Truncate();
  LossyAppendUTF16toASCII(aSource, aDest);
}

void
CopyASCIItoUTF16(const nsACString& aSource, nsAString& aDest)
{
  aDest.Truncate();
  AppendASCIItoUTF16(aSource, aDest);
}

void
LossyCopyUTF16toASCII(const char16_t* aSource, nsACString& aDest)
{
  aDest.Truncate();
  if (aSource) {
    LossyAppendUTF16toASCII(nsDependentString(aSource), aDest);
  }
}

void
CopyASCIItoUTF16(const char* aSource, nsAString& aDest)
{
  aDest.Truncate();
  if (aSource) {
    AppendASCIItoUTF16(nsDependentCString(aSource), aDest);
  }
}

void
CopyUTF16toUTF8(const nsAString& aSource, nsACString& aDest)
{
  aDest.Truncate();
  AppendUTF16toUTF8(aSource, aDest);
}

void
CopyUTF8toUTF16(const nsACString& aSource, nsAString& aDest)
{
  aDest.Truncate();
  AppendUTF8toUTF16(aSource, aDest);
}

void
CopyUTF16toUTF8(const char16_t* aSource, nsACString& aDest)
{
  aDest.Truncate();
  AppendUTF16toUTF8(aSource, aDest);
}

void
CopyUTF8toUTF16(const char* aSource, nsAString& aDest)
{
  aDest.Truncate();
  AppendUTF8toUTF16(aSource, aDest);
}

void
LossyAppendUTF16toASCII(const nsAString& aSource, nsACString& aDest)
{
  uint32_t old_dest_length = aDest.Length();
  aDest.SetLength(old_dest_length + aSource.Length());

  nsAString::const_iterator fromBegin, fromEnd;

  nsACString::iterator dest;
  aDest.BeginWriting(dest);

  dest.advance(old_dest_length);

  // right now, this won't work on multi-fragment destinations
  LossyConvertEncoding16to8 converter(dest.get());

  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter);
}

void
AppendASCIItoUTF16(const nsACString& aSource, nsAString& aDest)
{
  if (!AppendASCIItoUTF16(aSource, aDest, mozilla::fallible_t())) {
    NS_ABORT_OOM(aDest.Length() + aSource.Length());
  }
}

bool
AppendASCIItoUTF16(const nsACString& aSource, nsAString& aDest,
                   const mozilla::fallible_t&)
{
  uint32_t old_dest_length = aDest.Length();
  if (!aDest.SetLength(old_dest_length + aSource.Length(),
                       mozilla::fallible_t())) {
    return false;
  }

  nsACString::const_iterator fromBegin, fromEnd;

  nsAString::iterator dest;
  aDest.BeginWriting(dest);

  dest.advance(old_dest_length);

  // right now, this won't work on multi-fragment destinations
  LossyConvertEncoding8to16 converter(dest.get());

  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter);
  return true;
}

void
LossyAppendUTF16toASCII(const char16_t* aSource, nsACString& aDest)
{
  if (aSource) {
    LossyAppendUTF16toASCII(nsDependentString(aSource), aDest);
  }
}

void
AppendASCIItoUTF16(const char* aSource, nsAString& aDest)
{
  if (aSource) {
    AppendASCIItoUTF16(nsDependentCString(aSource), aDest);
  }
}

void
AppendUTF16toUTF8(const nsAString& aSource, nsACString& aDest)
{
  if (!AppendUTF16toUTF8(aSource, aDest, mozilla::fallible_t())) {
    NS_ABORT_OOM(aDest.Length() + aSource.Length());
  }
}

bool
AppendUTF16toUTF8(const nsAString& aSource, nsACString& aDest,
                  const mozilla::fallible_t&)
{
  nsAString::const_iterator source_start, source_end;
  CalculateUTF8Size calculator;
  copy_string(aSource.BeginReading(source_start),
              aSource.EndReading(source_end), calculator);

  uint32_t count = calculator.Size();

  if (count) {
    uint32_t old_dest_length = aDest.Length();

    // Grow the buffer if we need to.
    if (!aDest.SetLength(old_dest_length + count, mozilla::fallible_t())) {
      return false;
    }

    // All ready? Time to convert

    ConvertUTF16toUTF8 converter(aDest.BeginWriting() + old_dest_length);
    copy_string(aSource.BeginReading(source_start),
                aSource.EndReading(source_end), converter);

    NS_ASSERTION(converter.Size() == count,
                 "Unexpected disparity between CalculateUTF8Size and "
                 "ConvertUTF16toUTF8");
  }

  return true;
}

void
AppendUTF8toUTF16(const nsACString& aSource, nsAString& aDest)
{
  if (!AppendUTF8toUTF16(aSource, aDest, mozilla::fallible_t())) {
    NS_ABORT_OOM(aDest.Length() + aSource.Length());
  }
}

bool
AppendUTF8toUTF16(const nsACString& aSource, nsAString& aDest,
                  const mozilla::fallible_t&)
{
  nsACString::const_iterator source_start, source_end;
  CalculateUTF8Length calculator;
  copy_string(aSource.BeginReading(source_start),
              aSource.EndReading(source_end), calculator);

  uint32_t count = calculator.Length();

  // Avoid making the string mutable if we're appending an empty string
  if (count) {
    uint32_t old_dest_length = aDest.Length();

    // Grow the buffer if we need to.
    if (!aDest.SetLength(old_dest_length + count, mozilla::fallible_t())) {
      return false;
    }

    // All ready? Time to convert

    ConvertUTF8toUTF16 converter(aDest.BeginWriting() + old_dest_length);
    copy_string(aSource.BeginReading(source_start),
                aSource.EndReading(source_end), converter);

    NS_ASSERTION(converter.ErrorEncountered() ||
                 converter.Length() == count,
                 "CalculateUTF8Length produced the wrong length");

    if (converter.ErrorEncountered()) {
      NS_ERROR("Input wasn't UTF8 or incorrect length was calculated");
      aDest.SetLength(old_dest_length);
    }
  }

  return true;
}

void
AppendUTF16toUTF8(const char16_t* aSource, nsACString& aDest)
{
  if (aSource) {
    AppendUTF16toUTF8(nsDependentString(aSource), aDest);
  }
}

void
AppendUTF8toUTF16(const char* aSource, nsAString& aDest)
{
  if (aSource) {
    AppendUTF8toUTF16(nsDependentCString(aSource), aDest);
  }
}


/**
 * A helper function that allocates a buffer of the desired character type big enough to hold a copy of the supplied string (plus a zero terminator).
 *
 * @param aSource an string you will eventually be making a copy of
 * @return a new buffer (of the type specified by the second parameter) which you must free with |nsMemory::Free|.
 *
 */
template <class FromStringT, class ToCharT>
inline
ToCharT*
AllocateStringCopy(const FromStringT& aSource, ToCharT*)
{
  return static_cast<ToCharT*>(nsMemory::Alloc(
    (aSource.Length() + 1) * sizeof(ToCharT)));
}


char*
ToNewCString(const nsAString& aSource)
{
  char* result = AllocateStringCopy(aSource, (char*)0);
  if (!result) {
    return nullptr;
  }

  nsAString::const_iterator fromBegin, fromEnd;
  LossyConvertEncoding16to8 converter(result);
  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter).write_terminator();
  return result;
}

char*
ToNewUTF8String(const nsAString& aSource, uint32_t* aUTF8Count)
{
  nsAString::const_iterator start, end;
  CalculateUTF8Size calculator;
  copy_string(aSource.BeginReading(start), aSource.EndReading(end),
              calculator);

  if (aUTF8Count) {
    *aUTF8Count = calculator.Size();
  }

  char* result = static_cast<char*>
                 (nsMemory::Alloc(calculator.Size() + 1));
  if (!result) {
    return nullptr;
  }

  ConvertUTF16toUTF8 converter(result);
  copy_string(aSource.BeginReading(start), aSource.EndReading(end),
              converter).write_terminator();
  NS_ASSERTION(calculator.Size() == converter.Size(), "length mismatch");

  return result;
}

char*
ToNewCString(const nsACString& aSource)
{
  // no conversion needed, just allocate a buffer of the correct length and copy into it

  char* result = AllocateStringCopy(aSource, (char*)0);
  if (!result) {
    return nullptr;
  }

  nsACString::const_iterator fromBegin, fromEnd;
  char* toBegin = result;
  *copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
               toBegin) = char(0);
  return result;
}

char16_t*
ToNewUnicode(const nsAString& aSource)
{
  // no conversion needed, just allocate a buffer of the correct length and copy into it

  char16_t* result = AllocateStringCopy(aSource, (char16_t*)0);
  if (!result) {
    return nullptr;
  }

  nsAString::const_iterator fromBegin, fromEnd;
  char16_t* toBegin = result;
  *copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
               toBegin) = char16_t(0);
  return result;
}

char16_t*
ToNewUnicode(const nsACString& aSource)
{
  char16_t* result = AllocateStringCopy(aSource, (char16_t*)0);
  if (!result) {
    return nullptr;
  }

  nsACString::const_iterator fromBegin, fromEnd;
  LossyConvertEncoding8to16 converter(result);
  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter).write_terminator();
  return result;
}

uint32_t
CalcUTF8ToUnicodeLength(const nsACString& aSource)
{
  nsACString::const_iterator start, end;
  CalculateUTF8Length calculator;
  copy_string(aSource.BeginReading(start), aSource.EndReading(end),
              calculator);
  return calculator.Length();
}

char16_t*
UTF8ToUnicodeBuffer(const nsACString& aSource, char16_t* aBuffer,
                    uint32_t* aUTF16Count)
{
  nsACString::const_iterator start, end;
  ConvertUTF8toUTF16 converter(aBuffer);
  copy_string(aSource.BeginReading(start),
              aSource.EndReading(end),
              converter).write_terminator();
  if (aUTF16Count) {
    *aUTF16Count = converter.Length();
  }
  return aBuffer;
}

char16_t*
UTF8ToNewUnicode(const nsACString& aSource, uint32_t* aUTF16Count)
{
  const uint32_t length = CalcUTF8ToUnicodeLength(aSource);
  const size_t buffer_size = (length + 1) * sizeof(char16_t);
  char16_t* buffer = static_cast<char16_t*>(nsMemory::Alloc(buffer_size));
  if (!buffer) {
    return nullptr;
  }

  uint32_t copied;
  UTF8ToUnicodeBuffer(aSource, buffer, &copied);
  NS_ASSERTION(length == copied, "length mismatch");

  if (aUTF16Count) {
    *aUTF16Count = copied;
  }
  return buffer;
}

char16_t*
CopyUnicodeTo(const nsAString& aSource, uint32_t aSrcOffset, char16_t* aDest,
              uint32_t aLength)
{
  nsAString::const_iterator fromBegin, fromEnd;
  char16_t* toBegin = aDest;
  copy_string(aSource.BeginReading(fromBegin).advance(int32_t(aSrcOffset)),
              aSource.BeginReading(fromEnd).advance(int32_t(aSrcOffset + aLength)),
              toBegin);
  return aDest;
}

void
CopyUnicodeTo(const nsAString::const_iterator& aSrcStart,
              const nsAString::const_iterator& aSrcEnd,
              nsAString& aDest)
{
  nsAString::iterator writer;
  aDest.SetLength(Distance(aSrcStart, aSrcEnd));

  aDest.BeginWriting(writer);
  nsAString::const_iterator fromBegin(aSrcStart);

  copy_string(fromBegin, aSrcEnd, writer);
}

void
AppendUnicodeTo(const nsAString::const_iterator& aSrcStart,
                const nsAString::const_iterator& aSrcEnd,
                nsAString& aDest)
{
  nsAString::iterator writer;
  uint32_t oldLength = aDest.Length();
  aDest.SetLength(oldLength + Distance(aSrcStart, aSrcEnd));

  aDest.BeginWriting(writer).advance(oldLength);
  nsAString::const_iterator fromBegin(aSrcStart);

  copy_string(fromBegin, aSrcEnd, writer);
}

bool
IsASCII(const nsAString& aString)
{
  static const char16_t NOT_ASCII = char16_t(~0x007F);


  // Don't want to use |copy_string| for this task, since we can stop at the first non-ASCII character

  nsAString::const_iterator iter, done_reading;
  aString.BeginReading(iter);
  aString.EndReading(done_reading);

  const char16_t* c = iter.get();
  const char16_t* end = done_reading.get();

  while (c < end) {
    if (*c++ & NOT_ASCII) {
      return false;
    }
  }

  return true;
}

bool
IsASCII(const nsACString& aString)
{
  static const char NOT_ASCII = char(~0x7F);


  // Don't want to use |copy_string| for this task, since we can stop at the first non-ASCII character

  nsACString::const_iterator iter, done_reading;
  aString.BeginReading(iter);
  aString.EndReading(done_reading);

  const char* c = iter.get();
  const char* end = done_reading.get();

  while (c < end) {
    if (*c++ & NOT_ASCII) {
      return false;
    }
  }

  return true;
}

bool
IsUTF8(const nsACString& aString, bool aRejectNonChar)
{
  nsReadingIterator<char> done_reading;
  aString.EndReading(done_reading);

  int32_t state = 0;
  bool overlong = false;
  bool surrogate = false;
  bool nonchar = false;
  uint16_t olupper = 0; // overlong byte upper bound.
  uint16_t slower = 0;  // surrogate byte lower bound.

  nsReadingIterator<char> iter;
  aString.BeginReading(iter);

  const char* ptr = iter.get();
  const char* end = done_reading.get();
  while (ptr < end) {
    uint8_t c;

    if (0 == state) {
      c = *ptr++;

      if (UTF8traits::isASCII(c)) {
        continue;
      }

      if (c <= 0xC1) { // [80-BF] where not expected, [C0-C1] for overlong.
        return false;
      } else if (UTF8traits::is2byte(c)) {
        state = 1;
      } else if (UTF8traits::is3byte(c)) {
        state = 2;
        if (c == 0xE0) { // to exclude E0[80-9F][80-BF]
          overlong = true;
          olupper = 0x9F;
        } else if (c == 0xED) { // ED[A0-BF][80-BF] : surrogate codepoint
          surrogate = true;
          slower = 0xA0;
        } else if (c == 0xEF) { // EF BF [BE-BF] : non-character
          nonchar = true;
        }
      } else if (c <= 0xF4) { // XXX replace /w UTF8traits::is4byte when it's updated to exclude [F5-F7].(bug 199090)
        state = 3;
        nonchar = true;
        if (c == 0xF0) { // to exclude F0[80-8F][80-BF]{2}
          overlong = true;
          olupper = 0x8F;
        } else if (c == 0xF4) { // to exclude F4[90-BF][80-BF]
          // actually not surrogates but codepoints beyond 0x10FFFF
          surrogate = true;
          slower = 0x90;
        }
      } else {
        return false;  // Not UTF-8 string
      }
    }

    if (nonchar && !aRejectNonChar) {
      nonchar = false;
    }

    while (ptr < end && state) {
      c = *ptr++;
      --state;

      // non-character : EF BF [BE-BF] or F[0-7] [89AB]F BF [BE-BF]
      if (nonchar &&
          ((!state && c < 0xBE) ||
           (state == 1 && c != 0xBF)  ||
           (state == 2 && 0x0F != (0x0F & c)))) {
        nonchar = false;
      }

      if (!UTF8traits::isInSeq(c) || (overlong && c <= olupper) ||
          (surrogate && slower <= c) || (nonchar && !state)) {
        return false;  // Not UTF-8 string
      }

      overlong = surrogate = false;
    }
  }
  return !state; // state != 0 at the end indicates an invalid UTF-8 seq.
}

/**
 * A character sink for in-place case conversion.
 */
class ConvertToUpperCase
{
public:
  typedef char value_type;

  uint32_t
  write(const char* aSource, uint32_t aSourceLength)
  {
    char* cp = const_cast<char*>(aSource);
    const char* end = aSource + aSourceLength;
    while (cp != end) {
      char ch = *cp;
      if (ch >= 'a' && ch <= 'z') {
        *cp = ch - ('a' - 'A');
      }
      ++cp;
    }
    return aSourceLength;
  }
};

void
ToUpperCase(nsCSubstring& aCString)
{
  ConvertToUpperCase converter;
  char* start;
  converter.write(aCString.BeginWriting(start), aCString.Length());
}

/**
 * A character sink for copying with case conversion.
 */
class CopyToUpperCase
{
public:
  typedef char value_type;

  explicit CopyToUpperCase(nsACString::iterator& aDestIter)
    : mIter(aDestIter)
  {
  }

  uint32_t
  write(const char* aSource, uint32_t aSourceLength)
  {
    uint32_t len = XPCOM_MIN(uint32_t(mIter.size_forward()), aSourceLength);
    char* cp = mIter.get();
    const char* end = aSource + len;
    while (aSource != end) {
      char ch = *aSource;
      if ((ch >= 'a') && (ch <= 'z')) {
        *cp = ch - ('a' - 'A');
      } else {
        *cp = ch;
      }
      ++aSource;
      ++cp;
    }
    mIter.advance(len);
    return len;
  }

protected:
  nsACString::iterator& mIter;
};

void
ToUpperCase(const nsACString& aSource, nsACString& aDest)
{
  nsACString::const_iterator fromBegin, fromEnd;
  nsACString::iterator toBegin;
  aDest.SetLength(aSource.Length());

  CopyToUpperCase converter(aDest.BeginWriting(toBegin));
  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter);
}

/**
 * A character sink for case conversion.
 */
class ConvertToLowerCase
{
public:
  typedef char value_type;

  uint32_t
  write(const char* aSource, uint32_t aSourceLength)
  {
    char* cp = const_cast<char*>(aSource);
    const char* end = aSource + aSourceLength;
    while (cp != end) {
      char ch = *cp;
      if ((ch >= 'A') && (ch <= 'Z')) {
        *cp = ch + ('a' - 'A');
      }
      ++cp;
    }
    return aSourceLength;
  }
};

void
ToLowerCase(nsCSubstring& aCString)
{
  ConvertToLowerCase converter;
  char* start;
  converter.write(aCString.BeginWriting(start), aCString.Length());
}

/**
 * A character sink for copying with case conversion.
 */
class CopyToLowerCase
{
public:
  typedef char value_type;

  explicit CopyToLowerCase(nsACString::iterator& aDestIter)
    : mIter(aDestIter)
  {
  }

  uint32_t
  write(const char* aSource, uint32_t aSourceLength)
  {
    uint32_t len = XPCOM_MIN(uint32_t(mIter.size_forward()), aSourceLength);
    char* cp = mIter.get();
    const char* end = aSource + len;
    while (aSource != end) {
      char ch = *aSource;
      if ((ch >= 'A') && (ch <= 'Z')) {
        *cp = ch + ('a' - 'A');
      } else {
        *cp = ch;
      }
      ++aSource;
      ++cp;
    }
    mIter.advance(len);
    return len;
  }

protected:
  nsACString::iterator& mIter;
};

void
ToLowerCase(const nsACString& aSource, nsACString& aDest)
{
  nsACString::const_iterator fromBegin, fromEnd;
  nsACString::iterator toBegin;
  aDest.SetLength(aSource.Length());

  CopyToLowerCase converter(aDest.BeginWriting(toBegin));
  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter);
}

bool
ParseString(const nsACString& aSource, char aDelimiter,
            nsTArray<nsCString>& aArray)
{
  nsACString::const_iterator start, end;
  aSource.BeginReading(start);
  aSource.EndReading(end);

  uint32_t oldLength = aArray.Length();

  for (;;) {
    nsACString::const_iterator delimiter = start;
    FindCharInReadable(aDelimiter, delimiter, end);

    if (delimiter != start) {
      if (!aArray.AppendElement(Substring(start, delimiter))) {
        aArray.RemoveElementsAt(oldLength, aArray.Length() - oldLength);
        return false;
      }
    }

    if (delimiter == end) {
      break;
    }
    start = ++delimiter;
    if (start == end) {
      break;
    }
  }

  return true;
}

template <class StringT, class IteratorT, class Comparator>
bool
FindInReadable_Impl(const StringT& aPattern, IteratorT& aSearchStart,
                    IteratorT& aSearchEnd, const Comparator& aCompare)
{
  bool found_it = false;

  // only bother searching at all if we're given a non-empty range to search
  if (aSearchStart != aSearchEnd) {
    IteratorT aPatternStart, aPatternEnd;
    aPattern.BeginReading(aPatternStart);
    aPattern.EndReading(aPatternEnd);

    // outer loop keeps searching till we find it or run out of string to search
    while (!found_it) {
      // fast inner loop (that's what it's called, not what it is) looks for a potential match
      while (aSearchStart != aSearchEnd &&
             aCompare(aPatternStart.get(), aSearchStart.get(), 1, 1)) {
        ++aSearchStart;
      }

      // if we broke out of the `fast' loop because we're out of string ... we're done: no match
      if (aSearchStart == aSearchEnd) {
        break;
      }

      // otherwise, we're at a potential match, let's see if we really hit one
      IteratorT testPattern(aPatternStart);
      IteratorT testSearch(aSearchStart);

      // slow inner loop verifies the potential match (found by the `fast' loop) at the current position
      for (;;) {
        // we already compared the first character in the outer loop,
        //  so we'll advance before the next comparison
        ++testPattern;
        ++testSearch;

        // if we verified all the way to the end of the pattern, then we found it!
        if (testPattern == aPatternEnd) {
          found_it = true;
          aSearchEnd = testSearch; // return the exact found range through the parameters
          break;
        }

        // if we got to end of the string we're searching before we hit the end of the
        //  pattern, we'll never find what we're looking for
        if (testSearch == aSearchEnd) {
          aSearchStart = aSearchEnd;
          break;
        }

        // else if we mismatched ... it's time to advance to the next search position
        //  and get back into the `fast' loop
        if (aCompare(testPattern.get(), testSearch.get(), 1, 1)) {
          ++aSearchStart;
          break;
        }
      }
    }
  }

  return found_it;
}

/**
 * This searches the entire string from right to left, and returns the first match found, if any.
 */
template <class StringT, class IteratorT, class Comparator>
bool
RFindInReadable_Impl(const StringT& aPattern, IteratorT& aSearchStart,
                     IteratorT& aSearchEnd, const Comparator& aCompare)
{
  IteratorT patternStart, patternEnd, searchEnd = aSearchEnd;
  aPattern.BeginReading(patternStart);
  aPattern.EndReading(patternEnd);

  // Point to the last character in the pattern
  --patternEnd;
  // outer loop keeps searching till we run out of string to search
  while (aSearchStart != searchEnd) {
    // Point to the end position of the next possible match
    --searchEnd;

    // Check last character, if a match, explore further from here
    if (aCompare(patternEnd.get(), searchEnd.get(), 1, 1) == 0) {
      // We're at a potential match, let's see if we really hit one
      IteratorT testPattern(patternEnd);
      IteratorT testSearch(searchEnd);

      // inner loop verifies the potential match at the current position
      do {
        // if we verified all the way to the end of the pattern, then we found it!
        if (testPattern == patternStart) {
          aSearchStart = testSearch;  // point to start of match
          aSearchEnd = ++searchEnd;   // point to end of match
          return true;
        }

        // if we got to end of the string we're searching before we hit the end of the
        //  pattern, we'll never find what we're looking for
        if (testSearch == aSearchStart) {
          aSearchStart = aSearchEnd;
          return false;
        }

        // test previous character for a match
        --testPattern;
        --testSearch;
      } while (aCompare(testPattern.get(), testSearch.get(), 1, 1) == 0);
    }
  }

  aSearchStart = aSearchEnd;
  return false;
}

bool
FindInReadable(const nsAString& aPattern,
               nsAString::const_iterator& aSearchStart,
               nsAString::const_iterator& aSearchEnd,
               const nsStringComparator& aComparator)
{
  return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool
FindInReadable(const nsACString& aPattern,
               nsACString::const_iterator& aSearchStart,
               nsACString::const_iterator& aSearchEnd,
               const nsCStringComparator& aComparator)
{
  return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool
CaseInsensitiveFindInReadable(const nsACString& aPattern,
                              nsACString::const_iterator& aSearchStart,
                              nsACString::const_iterator& aSearchEnd)
{
  return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd,
                             nsCaseInsensitiveCStringComparator());
}

bool
RFindInReadable(const nsAString& aPattern,
                nsAString::const_iterator& aSearchStart,
                nsAString::const_iterator& aSearchEnd,
                const nsStringComparator& aComparator)
{
  return RFindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool
RFindInReadable(const nsACString& aPattern,
                nsACString::const_iterator& aSearchStart,
                nsACString::const_iterator& aSearchEnd,
                const nsCStringComparator& aComparator)
{
  return RFindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool
FindCharInReadable(char16_t aChar, nsAString::const_iterator& aSearchStart,
                   const nsAString::const_iterator& aSearchEnd)
{
  int32_t fragmentLength = aSearchEnd.get() - aSearchStart.get();

  const char16_t* charFoundAt =
    nsCharTraits<char16_t>::find(aSearchStart.get(), fragmentLength, aChar);
  if (charFoundAt) {
    aSearchStart.advance(charFoundAt - aSearchStart.get());
    return true;
  }

  aSearchStart.advance(fragmentLength);
  return false;
}

bool
FindCharInReadable(char aChar, nsACString::const_iterator& aSearchStart,
                   const nsACString::const_iterator& aSearchEnd)
{
  int32_t fragmentLength = aSearchEnd.get() - aSearchStart.get();

  const char* charFoundAt =
    nsCharTraits<char>::find(aSearchStart.get(), fragmentLength, aChar);
  if (charFoundAt) {
    aSearchStart.advance(charFoundAt - aSearchStart.get());
    return true;
  }

  aSearchStart.advance(fragmentLength);
  return false;
}

uint32_t
CountCharInReadable(const nsAString& aStr, char16_t aChar)
{
  uint32_t count = 0;
  nsAString::const_iterator begin, end;

  aStr.BeginReading(begin);
  aStr.EndReading(end);

  while (begin != end) {
    if (*begin == aChar) {
      ++count;
    }
    ++begin;
  }

  return count;
}

uint32_t
CountCharInReadable(const nsACString& aStr, char aChar)
{
  uint32_t count = 0;
  nsACString::const_iterator begin, end;

  aStr.BeginReading(begin);
  aStr.EndReading(end);

  while (begin != end) {
    if (*begin == aChar) {
      ++count;
    }
    ++begin;
  }

  return count;
}

bool
StringBeginsWith(const nsAString& aSource, const nsAString& aSubstring,
                 const nsStringComparator& aComparator)
{
  nsAString::size_type src_len = aSource.Length(),
                       sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, 0, sub_len).Equals(aSubstring, aComparator);
}

bool
StringBeginsWith(const nsACString& aSource, const nsACString& aSubstring,
                 const nsCStringComparator& aComparator)
{
  nsACString::size_type src_len = aSource.Length(),
                        sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, 0, sub_len).Equals(aSubstring, aComparator);
}

bool
StringEndsWith(const nsAString& aSource, const nsAString& aSubstring,
               const nsStringComparator& aComparator)
{
  nsAString::size_type src_len = aSource.Length(),
                       sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, src_len - sub_len, sub_len).Equals(aSubstring,
                                                               aComparator);
}

bool
StringEndsWith(const nsACString& aSource, const nsACString& aSubstring,
               const nsCStringComparator& aComparator)
{
  nsACString::size_type src_len = aSource.Length(),
                        sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, src_len - sub_len, sub_len).Equals(aSubstring,
                                                               aComparator);
}



static const char16_t empty_buffer[1] = { '\0' };

const nsAFlatString&
EmptyString()
{
  static const nsDependentString sEmpty(empty_buffer);

  return sEmpty;
}

const nsAFlatCString&
EmptyCString()
{
  static const nsDependentCString sEmpty((const char*)empty_buffer);

  return sEmpty;
}

const nsAFlatString&
NullString()
{
  static const nsXPIDLString sNull;

  return sNull;
}

const nsAFlatCString&
NullCString()
{
  static const nsXPIDLCString sNull;

  return sNull;
}

int32_t
CompareUTF8toUTF16(const nsASingleFragmentCString& aUTF8String,
                   const nsASingleFragmentString& aUTF16String)
{
  static const uint32_t NOT_ASCII = uint32_t(~0x7F);

  const char* u8;
  const char* u8end;
  aUTF8String.BeginReading(u8);
  aUTF8String.EndReading(u8end);

  const char16_t* u16;
  const char16_t* u16end;
  aUTF16String.BeginReading(u16);
  aUTF16String.EndReading(u16end);

  while (u8 != u8end && u16 != u16end) {
    // Cast away the signedness of *u8 to prevent signextension when
    // converting to uint32_t
    uint32_t c8_32 = (uint8_t)*u8;

    if (c8_32 & NOT_ASCII) {
      bool err;
      c8_32 = UTF8CharEnumerator::NextChar(&u8, u8end, &err);
      if (err) {
        return INT32_MIN;
      }

      uint32_t c16_32 = UTF16CharEnumerator::NextChar(&u16, u16end);
      // The above UTF16CharEnumerator::NextChar() calls can
      // fail, but if it does for anything other than no data to
      // look at (which can't happen here), it returns the
      // Unicode replacement character 0xFFFD for the invalid
      // data they were fed. Ignore that error and treat invalid
      // UTF16 as 0xFFFD.
      //
      // This matches what our UTF16 to UTF8 conversion code
      // does, and thus a UTF8 string that came from an invalid
      // UTF16 string will compare equal to the invalid UTF16
      // string it came from. Same is true for any other UTF16
      // string differs only in the invalid part of the string.

      if (c8_32 != c16_32) {
        return c8_32 < c16_32 ? -1 : 1;
      }
    } else {
      if (c8_32 != *u16) {
        return c8_32 > *u16 ? 1 : -1;
      }

      ++u8;
      ++u16;
    }
  }

  if (u8 != u8end) {
    // We get to the end of the UTF16 string, but no to the end of
    // the UTF8 string. The UTF8 string is longer than the UTF16
    // string

    return 1;
  }

  if (u16 != u16end) {
    // We get to the end of the UTF8 string, but no to the end of
    // the UTF16 string. The UTF16 string is longer than the UTF8
    // string

    return -1;
  }

  // The two strings match.

  return 0;
}

void
AppendUCS4ToUTF16(const uint32_t aSource, nsAString& aDest)
{
  NS_ASSERTION(IS_VALID_CHAR(aSource), "Invalid UCS4 char");
  if (IS_IN_BMP(aSource)) {
    aDest.Append(char16_t(aSource));
  } else {
    aDest.Append(H_SURROGATE(aSource));
    aDest.Append(L_SURROGATE(aSource));
  }
}
