/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "nsASCIIMask.h"
#include "mozilla/CheckedInt.h"

/**
 * nsTString::Find
 *
 * aOffset specifies starting index
 * aCount specifies number of string compares (iterations)
 */
template <typename T>
int32_t nsTString<T>::Find(const nsTString<char>& aString, bool aIgnoreCase,
                           int32_t aOffset, int32_t aCount) const {
  // this method changes the meaning of aOffset and aCount:
  Find_ComputeSearchRange(this->mLength, aString.Length(), aOffset, aCount);

  int32_t result = FindSubstring(this->mData + aOffset, aCount, aString.get(),
                                 aString.Length(), aIgnoreCase);
  if (result != kNotFound) result += aOffset;
  return result;
}

template <typename T>
int32_t nsTString<T>::Find(const char* aString, bool aIgnoreCase,
                           int32_t aOffset, int32_t aCount) const {
  return Find(nsTDependentString<char>(aString), aIgnoreCase, aOffset, aCount);
}

/**
 * nsTString::RFind
 *
 * aOffset specifies starting index
 * aCount specifies number of string compares (iterations)
 */
template <typename T>
int32_t nsTString<T>::RFind(const nsTString<char>& aString, bool aIgnoreCase,
                            int32_t aOffset, int32_t aCount) const {
  // this method changes the meaning of aOffset and aCount:
  RFind_ComputeSearchRange(this->mLength, aString.Length(), aOffset, aCount);

  int32_t result = RFindSubstring(this->mData + aOffset, aCount, aString.get(),
                                  aString.Length(), aIgnoreCase);
  if (result != kNotFound) result += aOffset;
  return result;
}

template <typename T>
int32_t nsTString<T>::RFind(const char* aString, bool aIgnoreCase,
                            int32_t aOffset, int32_t aCount) const {
  return RFind(nsTDependentString<char>(aString), aIgnoreCase, aOffset, aCount);
}

/**
 * nsTString::RFindChar
 */
template <typename T>
int32_t nsTString<T>::RFindChar(char16_t aChar, int32_t aOffset,
                                int32_t aCount) const {
  return nsBufferRoutines<T>::rfind_char(this->mData, this->mLength, aOffset,
                                         aChar, aCount);
}

/**
 * nsTString::FindCharInSet
 */

template <typename T>
int32_t nsTString<T>::FindCharInSet(const char_type* aSet,
                                    int32_t aOffset) const {
  if (aOffset < 0)
    aOffset = 0;
  else if (aOffset >= int32_t(this->mLength))
    return kNotFound;

  int32_t result =
      ::FindCharInSet(this->mData + aOffset, this->mLength - aOffset, aSet);
  if (result != kNotFound) result += aOffset;
  return result;
}

/**
 * nsTString::RFindCharInSet
 */

template <typename T>
int32_t nsTString<T>::RFindCharInSet(const char_type* aSet,
                                     int32_t aOffset) const {
  // We want to pass a "data length" to ::RFindCharInSet
  if (aOffset < 0 || aOffset > int32_t(this->mLength))
    aOffset = this->mLength;
  else
    ++aOffset;

  return ::RFindCharInSet(this->mData, aOffset, aSet);
}

/**
 * nsTString::Mid
 */

template <typename T>
typename nsTString<T>::size_type nsTString<T>::Mid(
    self_type& aResult, index_type aStartPos, size_type aLengthToCopy) const {
  if (aStartPos == 0 && aLengthToCopy >= this->mLength)
    aResult = *this;
  else
    aResult = Substring(*this, aStartPos, aLengthToCopy);

  return aResult.mLength;
}

/**
 * nsTString::SetCharAt
 */

template <typename T>
bool nsTString<T>::SetCharAt(char16_t aChar, uint32_t aIndex) {
  if (aIndex >= this->mLength) return false;

  if (!this->EnsureMutable()) this->AllocFailed(this->mLength);

  this->mData[aIndex] = char_type(aChar);
  return true;
}

/**
 * nsTString::StripChars,StripChar,StripWhitespace
 */

template <typename T>
template <typename Q, typename EnableIfChar16>
void nsTString<T>::StripChars(const incompatible_char_type* aSet) {
  if (!StripChars(aSet, mozilla::fallible)) {
    this->AllocFailed(this->mLength);
  }
}

template void nsTString<char16_t>::StripChars(const incompatible_char_type*);

template <typename T>
template <typename Q, typename EnableIfChar16>
bool nsTString<T>::StripChars(const incompatible_char_type* aSet,
                              const fallible_t&) {
  if (!this->EnsureMutable()) {
    return false;
  }

  this->mLength =
      nsBufferRoutines<T>::strip_chars(this->mData, this->mLength, aSet);
  return true;
}

template bool nsTString<char16_t>::StripChars(const incompatible_char_type*,
                                              const fallible_t&);

template <typename T>
void nsTString<T>::StripChars(const char_type* aSet) {
  nsTSubstring<T>::StripChars(aSet);
}

template <typename T>
void nsTString<T>::StripWhitespace() {
  if (!StripWhitespace(mozilla::fallible)) {
    this->AllocFailed(this->mLength);
  }
}

template <typename T>
bool nsTString<T>::StripWhitespace(const fallible_t&) {
  if (!this->EnsureMutable()) {
    return false;
  }

  this->StripTaggedASCII(mozilla::ASCIIMask::MaskWhitespace());
  return true;
}

/**
 * nsTString::ReplaceChar,ReplaceSubstring
 */

template <typename T>
void nsTString<T>::ReplaceChar(char_type aOldChar, char_type aNewChar) {
  if (!this->EnsureMutable())  // XXX do this lazily?
    this->AllocFailed(this->mLength);

  for (uint32_t i = 0; i < this->mLength; ++i) {
    if (this->mData[i] == aOldChar) this->mData[i] = aNewChar;
  }
}

template <typename T>
void nsTString<T>::ReplaceChar(const char_type* aSet, char_type aNewChar) {
  if (!this->EnsureMutable())  // XXX do this lazily?
    this->AllocFailed(this->mLength);

  char_type* data = this->mData;
  uint32_t lenRemaining = this->mLength;

  while (lenRemaining) {
    int32_t i = ::FindCharInSet(data, lenRemaining, aSet);
    if (i == kNotFound) break;

    data[i++] = aNewChar;
    data += i;
    lenRemaining -= i;
  }
}

template void nsTString<char16_t>::ReplaceChar(const char*, char16_t);

void ReleaseData(void* aData, nsAString::DataFlags aFlags);

template <typename T>
void nsTString<T>::ReplaceSubstring(const char_type* aTarget,
                                    const char_type* aNewValue) {
  ReplaceSubstring(nsTDependentString<T>(aTarget),
                   nsTDependentString<T>(aNewValue));
}

template <typename T>
bool nsTString<T>::ReplaceSubstring(const char_type* aTarget,
                                    const char_type* aNewValue,
                                    const fallible_t& aFallible) {
  return ReplaceSubstring(nsTDependentString<T>(aTarget),
                          nsTDependentString<T>(aNewValue), aFallible);
}

template <typename T>
void nsTString<T>::ReplaceSubstring(const self_type& aTarget,
                                    const self_type& aNewValue) {
  if (!ReplaceSubstring(aTarget, aNewValue, mozilla::fallible)) {
    // Note that this may wildly underestimate the allocation that failed, as
    // we could have been replacing multiple copies of aTarget.
    this->AllocFailed(this->mLength + (aNewValue.Length() - aTarget.Length()));
  }
}

template <typename T>
bool nsTString<T>::ReplaceSubstring(const self_type& aTarget,
                                    const self_type& aNewValue,
                                    const fallible_t&) {
  if (aTarget.Length() == 0) return true;

  // Remember all of the non-matching parts.
  AutoTArray<Segment, 16> nonMatching;
  uint32_t i = 0;
  mozilla::CheckedUint32 newLength;
  while (true) {
    int32_t r = FindSubstring(this->mData + i, this->mLength - i,
                              static_cast<const char_type*>(aTarget.Data()),
                              aTarget.Length(), false);
    int32_t until = (r == kNotFound) ? this->mLength - i : r;
    nonMatching.AppendElement(Segment(i, until));
    newLength += until;
    if (r == kNotFound) {
      break;
    }

    newLength += aNewValue.Length();
    i += r + aTarget.Length();
    if (i >= this->mLength) {
      // Add an auxiliary entry at the end of the list to help as an edge case
      // for the algorithms below.
      nonMatching.AppendElement(Segment(this->mLength, 0));
      break;
    }
  }

  if (!newLength.isValid()) {
    return false;
  }

  // If there's only one non-matching segment, then the target string was not
  // found, and there's nothing to do.
  if (nonMatching.Length() == 1) {
    MOZ_ASSERT(
        nonMatching[0].mBegin == 0 && nonMatching[0].mLength == this->mLength,
        "We should have the correct non-matching segment.");
    return true;
  }

  // Make sure that we can mutate our buffer.
  // Note that we always allocate at least an this->mLength sized buffer,
  // because the rest of the algorithm relies on having access to all of the
  // original string.  In other words, we over-allocate in the shrinking case.
  uint32_t oldLen = this->mLength;
  mozilla::Result<uint32_t, nsresult> r =
      this->StartBulkWriteImpl(XPCOM_MAX(oldLen, newLength.value()), oldLen);
  if (r.isErr()) {
    return false;
  }

  if (aTarget.Length() >= aNewValue.Length()) {
    // In the shrinking case, start filling the buffer from the beginning.
    const uint32_t delta = (aTarget.Length() - aNewValue.Length());
    for (i = 1; i < nonMatching.Length(); ++i) {
      // When we move the i'th non-matching segment into position, we need to
      // account for the characters deleted by the previous |i| replacements by
      // subtracting |i * delta|.
      const char_type* sourceSegmentPtr = this->mData + nonMatching[i].mBegin;
      char_type* destinationSegmentPtr =
          this->mData + nonMatching[i].mBegin - i * delta;
      // Write the i'th replacement immediately before the new i'th non-matching
      // segment.
      char_traits::copy(destinationSegmentPtr - aNewValue.Length(),
                        aNewValue.Data(), aNewValue.Length());
      char_traits::move(destinationSegmentPtr, sourceSegmentPtr,
                        nonMatching[i].mLength);
    }
  } else {
    // In the growing case, start filling the buffer from the end.
    const uint32_t delta = (aNewValue.Length() - aTarget.Length());
    for (i = nonMatching.Length() - 1; i > 0; --i) {
      // When we move the i'th non-matching segment into position, we need to
      // account for the characters added by the previous |i| replacements by
      // adding |i * delta|.
      const char_type* sourceSegmentPtr = this->mData + nonMatching[i].mBegin;
      char_type* destinationSegmentPtr =
          this->mData + nonMatching[i].mBegin + i * delta;
      char_traits::move(destinationSegmentPtr, sourceSegmentPtr,
                        nonMatching[i].mLength);
      // Write the i'th replacement immediately before the new i'th non-matching
      // segment.
      char_traits::copy(destinationSegmentPtr - aNewValue.Length(),
                        aNewValue.Data(), aNewValue.Length());
    }
  }

  // Adjust the length and make sure the string is null terminated.
  this->FinishBulkWriteImpl(newLength.value());

  return true;
}

/**
 * nsTString::Trim
 */

template <typename T>
void nsTString<T>::Trim(const char* aSet, bool aTrimLeading, bool aTrimTrailing,
                        bool aIgnoreQuotes) {
  // the old implementation worried about aSet being null :-/
  if (!aSet) return;

  char_type* start = this->mData;
  char_type* end = this->mData + this->mLength;

  // skip over quotes if requested
  if (aIgnoreQuotes && this->mLength > 2 &&
      this->mData[0] == this->mData[this->mLength - 1] &&
      (this->mData[0] == '\'' || this->mData[0] == '"')) {
    ++start;
    --end;
  }

  uint32_t setLen = nsCharTraits<char>::length(aSet);

  if (aTrimLeading) {
    uint32_t cutStart = start - this->mData;
    uint32_t cutLength = 0;

    // walk forward from start to end
    for (; start != end; ++start, ++cutLength) {
      int32_t pos = FindChar1(aSet, setLen, 0, *start, setLen);
      if (pos == kNotFound) break;
    }

    if (cutLength) {
      this->Cut(cutStart, cutLength);

      // reset iterators
      start = this->mData + cutStart;
      end = this->mData + this->mLength - cutStart;
    }
  }

  if (aTrimTrailing) {
    uint32_t cutEnd = end - this->mData;
    uint32_t cutLength = 0;

    // walk backward from end to start
    --end;
    for (; end >= start; --end, ++cutLength) {
      int32_t pos = FindChar1(aSet, setLen, 0, *end, setLen);
      if (pos == kNotFound) break;
    }

    if (cutLength) this->Cut(cutEnd - cutLength, cutLength);
  }
}

/**
 * nsTString::CompressWhitespace.
 */

template <typename T>
void nsTString<T>::CompressWhitespace(bool aTrimLeading, bool aTrimTrailing) {
  // Quick exit
  if (this->mLength == 0) {
    return;
  }

  if (!this->EnsureMutable()) this->AllocFailed(this->mLength);

  const ASCIIMaskArray& mask = mozilla::ASCIIMask::MaskWhitespace();

  char_type* to = this->mData;
  char_type* from = this->mData;
  char_type* end = this->mData + this->mLength;

  // Compresses runs of whitespace down to a normal space ' ' and convert
  // any whitespace to a normal space.  This assumes that whitespace is
  // all standard 7-bit ASCII.
  bool skipWS = aTrimLeading;
  while (from < end) {
    uint32_t theChar = *from++;
    if (mozilla::ASCIIMask::IsMasked(mask, theChar)) {
      if (!skipWS) {
        *to++ = ' ';
        skipWS = true;
      }
    } else {
      *to++ = theChar;
      skipWS = false;
    }
  }

  // If we need to trim the trailing whitespace, back up one character.
  if (aTrimTrailing && skipWS && to > this->mData) {
    to--;
  }

  *to = char_type(0);  // add the null
  this->mLength = to - this->mData;
}
