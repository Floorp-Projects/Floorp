/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReadableUtils.h"

#include <algorithm>

#include "mozilla/CheckedInt.h"
#include "mozilla/Utf8.h"

#include "nscore.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUTF8Utils.h"

using mozilla::MakeSpan;

/**
 * A helper function that allocates a buffer of the desired character type big
 * enough to hold a copy of the supplied string (plus a zero terminator).
 *
 * @param aSource an string you will eventually be making a copy of
 * @return a new buffer (of the type specified by the second parameter) which
 * you must free with |free|.
 *
 */
template <class FromStringT, class ToCharT>
inline ToCharT* AllocateStringCopy(const FromStringT& aSource, ToCharT*) {
  // Can't overflow due to the definition of nsTSubstring<T>::kMaxCapacity
  return static_cast<ToCharT*>(
      moz_xmalloc((size_t(aSource.Length()) + 1) * sizeof(ToCharT)));
}

char* ToNewCString(const nsAString& aSource) {
  char* dest = AllocateStringCopy(aSource, (char*)nullptr);
  if (!dest) {
    return nullptr;
  }

  auto len = aSource.Length();
  LossyConvertUtf16toLatin1(aSource, MakeSpan(dest, len));
  dest[len] = 0;
  return dest;
}

char* ToNewUTF8String(const nsAString& aSource, uint32_t* aUTF8Count) {
  auto len = aSource.Length();
  // The uses of this function seem temporary enough that it's not
  // worthwhile to be fancy about the allocation size. Let's just use
  // the worst case.
  // Times 3 plus 1, because ConvertUTF16toUTF8 requires times 3 and
  // then we have the terminator.
  // Using CheckedInt<uint32_t>, because aUTF8Count is uint32_t* for
  // historical reasons.
  mozilla::CheckedInt<uint32_t> destLen(len);
  destLen *= 3;
  destLen += 1;
  if (!destLen.isValid()) {
    return nullptr;
  }
  size_t destLenVal = destLen.value();
  char* dest = static_cast<char*>(moz_xmalloc(destLenVal));

  size_t written = ConvertUtf16toUtf8(aSource, MakeSpan(dest, destLenVal));
  dest[written] = 0;

  if (aUTF8Count) {
    *aUTF8Count = written;
  }

  return dest;
}

char* ToNewCString(const nsACString& aSource) {
  // no conversion needed, just allocate a buffer of the correct length and copy
  // into it

  char* dest = AllocateStringCopy(aSource, (char*)nullptr);
  if (!dest) {
    return nullptr;
  }

  auto len = aSource.Length();
  memcpy(dest, aSource.BeginReading(), len * sizeof(char));
  dest[len] = 0;
  return dest;
}

char16_t* ToNewUnicode(const nsAString& aSource) {
  // no conversion needed, just allocate a buffer of the correct length and copy
  // into it

  char16_t* dest = AllocateStringCopy(aSource, (char16_t*)nullptr);
  if (!dest) {
    return nullptr;
  }

  auto len = aSource.Length();
  memcpy(dest, aSource.BeginReading(), len * sizeof(char16_t));
  dest[len] = 0;
  return dest;
}

char16_t* ToNewUnicode(const nsACString& aSource) {
  char16_t* dest = AllocateStringCopy(aSource, (char16_t*)nullptr);
  if (!dest) {
    return nullptr;
  }

  auto len = aSource.Length();
  ConvertLatin1toUtf16(aSource, MakeSpan(dest, len));
  dest[len] = 0;
  return dest;
}

char16_t* UTF8ToNewUnicode(const nsACString& aSource, uint32_t* aUTF16Count) {
  // Compute length plus one as required by ConvertUTF8toUTF16
  uint32_t lengthPlusOne = aSource.Length() + 1;  // Can't overflow

  mozilla::CheckedInt<size_t> allocLength(lengthPlusOne);
  // Add space for zero-termination
  allocLength += 1;
  // We need UTF-16 units
  allocLength *= sizeof(char16_t);

  if (!allocLength.isValid()) {
    return nullptr;
  }

  char16_t* dest = (char16_t*)moz_xmalloc(allocLength.value());

  size_t written = ConvertUtf8toUtf16(aSource, MakeSpan(dest, lengthPlusOne));
  dest[written] = 0;

  if (aUTF16Count) {
    *aUTF16Count = written;
  }

  return dest;
}

char16_t* CopyUnicodeTo(const nsAString& aSource, uint32_t aSrcOffset,
                        char16_t* aDest, uint32_t aLength) {
  MOZ_ASSERT(aSrcOffset + aLength <= aSource.Length());
  memcpy(aDest, aSource.BeginReading() + aSrcOffset,
         size_t(aLength) * sizeof(char16_t));
  return aDest;
}

void ToUpperCase(nsACString& aCString) {
  char* cp = aCString.BeginWriting();
  char* end = cp + aCString.Length();
  while (cp != end) {
    char ch = *cp;
    if (ch >= 'a' && ch <= 'z') {
      *cp = ch - ('a' - 'A');
    }
    ++cp;
  }
}

void ToUpperCase(const nsACString& aSource, nsACString& aDest) {
  aDest.SetLength(aSource.Length());
  const char* src = aSource.BeginReading();
  const char* end = src + aSource.Length();
  char* dst = aDest.BeginWriting();
  while (src != end) {
    char ch = *src;
    if (ch >= 'a' && ch <= 'z') {
      *dst = ch - ('a' - 'A');
    } else {
      *dst = ch;
    }
    ++src;
    ++dst;
  }
}

void ToLowerCase(nsACString& aCString) {
  char* cp = aCString.BeginWriting();
  char* end = cp + aCString.Length();
  while (cp != end) {
    char ch = *cp;
    if (ch >= 'A' && ch <= 'Z') {
      *cp = ch + ('a' - 'A');
    }
    ++cp;
  }
}

void ToLowerCase(const nsACString& aSource, nsACString& aDest) {
  aDest.SetLength(aSource.Length());
  const char* src = aSource.BeginReading();
  const char* end = src + aSource.Length();
  char* dst = aDest.BeginWriting();
  while (src != end) {
    char ch = *src;
    if (ch >= 'A' && ch <= 'Z') {
      *dst = ch + ('a' - 'A');
    } else {
      *dst = ch;
    }
    ++src;
    ++dst;
  }
}

void ParseString(const nsACString& aSource, char aDelimiter,
                 nsTArray<nsCString>& aArray) {
  nsACString::const_iterator start, end;
  aSource.BeginReading(start);
  aSource.EndReading(end);

  for (;;) {
    nsACString::const_iterator delimiter = start;
    FindCharInReadable(aDelimiter, delimiter, end);

    if (delimiter != start) {
      aArray.AppendElement(Substring(start, delimiter));
    }

    if (delimiter == end) {
      break;
    }
    start = ++delimiter;
    if (start == end) {
      break;
    }
  }
}

template <class StringT, class IteratorT, class Comparator>
bool FindInReadable_Impl(const StringT& aPattern, IteratorT& aSearchStart,
                         IteratorT& aSearchEnd, const Comparator& aCompare) {
  bool found_it = false;

  // only bother searching at all if we're given a non-empty range to search
  if (aSearchStart != aSearchEnd) {
    IteratorT aPatternStart, aPatternEnd;
    aPattern.BeginReading(aPatternStart);
    aPattern.EndReading(aPatternEnd);

    // outer loop keeps searching till we find it or run out of string to search
    while (!found_it) {
      // fast inner loop (that's what it's called, not what it is) looks for a
      // potential match
      while (aSearchStart != aSearchEnd &&
             aCompare(aPatternStart.get(), aSearchStart.get(), 1, 1)) {
        ++aSearchStart;
      }

      // if we broke out of the `fast' loop because we're out of string ...
      // we're done: no match
      if (aSearchStart == aSearchEnd) {
        break;
      }

      // otherwise, we're at a potential match, let's see if we really hit one
      IteratorT testPattern(aPatternStart);
      IteratorT testSearch(aSearchStart);

      // slow inner loop verifies the potential match (found by the `fast' loop)
      // at the current position
      for (;;) {
        // we already compared the first character in the outer loop,
        //  so we'll advance before the next comparison
        ++testPattern;
        ++testSearch;

        // if we verified all the way to the end of the pattern, then we found
        // it!
        if (testPattern == aPatternEnd) {
          found_it = true;
          aSearchEnd = testSearch;  // return the exact found range through the
                                    // parameters
          break;
        }

        // if we got to end of the string we're searching before we hit the end
        // of the
        //  pattern, we'll never find what we're looking for
        if (testSearch == aSearchEnd) {
          aSearchStart = aSearchEnd;
          break;
        }

        // else if we mismatched ... it's time to advance to the next search
        // position
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
 * This searches the entire string from right to left, and returns the first
 * match found, if any.
 */
template <class StringT, class IteratorT, class Comparator>
bool RFindInReadable_Impl(const StringT& aPattern, IteratorT& aSearchStart,
                          IteratorT& aSearchEnd, const Comparator& aCompare) {
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
        // if we verified all the way to the end of the pattern, then we found
        // it!
        if (testPattern == patternStart) {
          aSearchStart = testSearch;  // point to start of match
          aSearchEnd = ++searchEnd;   // point to end of match
          return true;
        }

        // if we got to end of the string we're searching before we hit the end
        // of the
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

bool FindInReadable(const nsAString& aPattern,
                    nsAString::const_iterator& aSearchStart,
                    nsAString::const_iterator& aSearchEnd,
                    const nsStringComparator& aComparator) {
  return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool FindInReadable(const nsACString& aPattern,
                    nsACString::const_iterator& aSearchStart,
                    nsACString::const_iterator& aSearchEnd,
                    const nsCStringComparator& aComparator) {
  return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool CaseInsensitiveFindInReadable(const nsACString& aPattern,
                                   nsACString::const_iterator& aSearchStart,
                                   nsACString::const_iterator& aSearchEnd) {
  return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd,
                             nsCaseInsensitiveCStringComparator());
}

bool RFindInReadable(const nsAString& aPattern,
                     nsAString::const_iterator& aSearchStart,
                     nsAString::const_iterator& aSearchEnd,
                     const nsStringComparator& aComparator) {
  return RFindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool RFindInReadable(const nsACString& aPattern,
                     nsACString::const_iterator& aSearchStart,
                     nsACString::const_iterator& aSearchEnd,
                     const nsCStringComparator& aComparator) {
  return RFindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
}

bool FindCharInReadable(char16_t aChar, nsAString::const_iterator& aSearchStart,
                        const nsAString::const_iterator& aSearchEnd) {
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

bool FindCharInReadable(char aChar, nsACString::const_iterator& aSearchStart,
                        const nsACString::const_iterator& aSearchEnd) {
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

bool StringBeginsWith(const nsAString& aSource, const nsAString& aSubstring) {
  nsAString::size_type src_len = aSource.Length(),
                       sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, 0, sub_len).Equals(aSubstring);
}

bool StringBeginsWith(const nsAString& aSource, const nsAString& aSubstring,
                      const nsStringComparator& aComparator) {
  nsAString::size_type src_len = aSource.Length(),
                       sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, 0, sub_len).Equals(aSubstring, aComparator);
}

bool StringBeginsWith(const nsACString& aSource, const nsACString& aSubstring) {
  nsACString::size_type src_len = aSource.Length(),
                        sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, 0, sub_len).Equals(aSubstring);
}

bool StringBeginsWith(const nsACString& aSource, const nsACString& aSubstring,
                      const nsCStringComparator& aComparator) {
  nsACString::size_type src_len = aSource.Length(),
                        sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, 0, sub_len).Equals(aSubstring, aComparator);
}

bool StringEndsWith(const nsAString& aSource, const nsAString& aSubstring) {
  nsAString::size_type src_len = aSource.Length(),
                       sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, src_len - sub_len, sub_len).Equals(aSubstring);
}

bool StringEndsWith(const nsAString& aSource, const nsAString& aSubstring,
                    const nsStringComparator& aComparator) {
  nsAString::size_type src_len = aSource.Length(),
                       sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, src_len - sub_len, sub_len)
      .Equals(aSubstring, aComparator);
}

bool StringEndsWith(const nsACString& aSource, const nsACString& aSubstring) {
  nsACString::size_type src_len = aSource.Length(),
                        sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, src_len - sub_len, sub_len).Equals(aSubstring);
}

bool StringEndsWith(const nsACString& aSource, const nsACString& aSubstring,
                    const nsCStringComparator& aComparator) {
  nsACString::size_type src_len = aSource.Length(),
                        sub_len = aSubstring.Length();
  if (sub_len > src_len) {
    return false;
  }
  return Substring(aSource, src_len - sub_len, sub_len)
      .Equals(aSubstring, aComparator);
}

static const char16_t empty_buffer[1] = {'\0'};

const nsString& EmptyString() {
  static const nsDependentString sEmpty(empty_buffer);

  return sEmpty;
}

const nsCString& EmptyCString() {
  static const nsDependentCString sEmpty((const char*)empty_buffer);

  return sEmpty;
}

const nsString& VoidString() {
  static const nsString sNull(mozilla::detail::StringDataFlags::VOIDED);

  return sNull;
}

const nsCString& VoidCString() {
  static const nsCString sNull(mozilla::detail::StringDataFlags::VOIDED);

  return sNull;
}

int32_t CompareUTF8toUTF16(const nsACString& aUTF8String,
                           const nsAString& aUTF16String, bool* aErr) {
  const char* u8;
  const char* u8end;
  aUTF8String.BeginReading(u8);
  aUTF8String.EndReading(u8end);

  const char16_t* u16;
  const char16_t* u16end;
  aUTF16String.BeginReading(u16);
  aUTF16String.EndReading(u16end);

  for (;;) {
    if (u8 == u8end) {
      if (u16 == u16end) {
        return 0;
      }
      return -1;
    }
    if (u16 == u16end) {
      return 1;
    }
    // No need for ASCII optimization, since both NextChar()
    // calls get inlined.
    uint32_t scalar8 = UTF8CharEnumerator::NextChar(&u8, u8end, aErr);
    uint32_t scalar16 = UTF16CharEnumerator::NextChar(&u16, u16end, aErr);
    if (scalar16 == scalar8) {
      continue;
    }
    if (scalar8 < scalar16) {
      return -1;
    }
    return 1;
  }
}

void AppendUCS4ToUTF16(const uint32_t aSource, nsAString& aDest) {
  NS_ASSERTION(IS_VALID_CHAR(aSource), "Invalid UCS4 char");
  if (IS_IN_BMP(aSource)) {
    aDest.Append(char16_t(aSource));
  } else {
    aDest.Append(H_SURROGATE(aSource));
    aDest.Append(L_SURROGATE(aSource));
  }
}
