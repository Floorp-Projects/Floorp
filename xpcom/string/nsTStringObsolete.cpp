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

int32_t
nsTString_CharT::Find( const nsCString& aString, bool aIgnoreCase, int32_t aOffset, int32_t aCount) const
{
  // this method changes the meaning of aOffset and aCount:
  Find_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

  int32_t result = FindSubstring(mData + aOffset, aCount, aString.get(), aString.Length(), aIgnoreCase);
  if (result != kNotFound)
    result += aOffset;
  return result;
}

int32_t
nsTString_CharT::Find( const char* aString, bool aIgnoreCase, int32_t aOffset, int32_t aCount) const
{
  return Find(nsDependentCString(aString), aIgnoreCase, aOffset, aCount);
}


/**
 * nsTString::RFind
 *
 * aOffset specifies starting index
 * aCount specifies number of string compares (iterations)
 */

int32_t
nsTString_CharT::RFind( const nsCString& aString, bool aIgnoreCase, int32_t aOffset, int32_t aCount) const
{
  // this method changes the meaning of aOffset and aCount:
  RFind_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

  int32_t result = RFindSubstring(mData + aOffset, aCount, aString.get(), aString.Length(), aIgnoreCase);
  if (result != kNotFound)
    result += aOffset;
  return result;
}

int32_t
nsTString_CharT::RFind( const char* aString, bool aIgnoreCase, int32_t aOffset, int32_t aCount) const
{
  return RFind(nsDependentCString(aString), aIgnoreCase, aOffset, aCount);
}


/**
 * nsTString::RFindChar
 */

int32_t
nsTString_CharT::RFindChar( char16_t aChar, int32_t aOffset, int32_t aCount) const
{
  return nsBufferRoutines<CharT>::rfind_char(mData, mLength, aOffset, aChar, aCount);
}


/**
 * nsTString::FindCharInSet
 */

int32_t
nsTString_CharT::FindCharInSet( const char* aSet, int32_t aOffset ) const
{
  if (aOffset < 0)
    aOffset = 0;
  else if (aOffset >= int32_t(mLength))
    return kNotFound;

  int32_t result = ::FindCharInSet(mData + aOffset, mLength - aOffset, aSet);
  if (result != kNotFound)
    result += aOffset;
  return result;
}


/**
 * nsTString::RFindCharInSet
 */

int32_t
nsTString_CharT::RFindCharInSet( const CharT* aSet, int32_t aOffset ) const
{
  // We want to pass a "data length" to ::RFindCharInSet
  if (aOffset < 0 || aOffset > int32_t(mLength))
    aOffset = mLength;
  else
    ++aOffset;

  return ::RFindCharInSet(mData, aOffset, aSet);
}


// it's a shame to replicate this code.  it was done this way in the past
// to help performance.  this function also gets to keep the rickg style
// indentation :-/
int32_t
nsTString_CharT::ToInteger( nsresult* aErrorCode, uint32_t aRadix ) const
{
  CharT*  cp=mData;
  int32_t theRadix=10; // base 10 unless base 16 detected, or overriden (aRadix != kAutoDetect)
  int32_t result=0;
  bool    negate=false;
  CharT   theChar=0;

  //initial value, override if we find an integer
  *aErrorCode=NS_ERROR_ILLEGAL_VALUE;

  if(cp) {

    //begin by skipping over leading chars that shouldn't be part of the number...

    CharT*  endcp=cp+mLength;
    bool    done=false;

    while((cp<endcp) && (!done)){
      switch(*cp++) {
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          theRadix=16;
          done=true;
          break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          done=true;
          break;
        case '-':
          negate=true; //fall through...
          break;
        case 'X': case 'x':
          theRadix=16;
          break;
        default:
          break;
      } //switch
    }

    if (done) {

      //integer found
      *aErrorCode = NS_OK;

      if (aRadix!=kAutoDetect) theRadix = aRadix; // override

      //now iterate the numeric chars and build our result
      CharT* first=--cp;  //in case we have to back up.
      bool haveValue = false;

      while(cp<endcp){
        int32_t oldresult = result;

        theChar=*cp++;
        if(('0'<=theChar) && (theChar<='9')){
          result = (theRadix * result) + (theChar-'0');
          haveValue = true;
        }
        else if((theChar>='A') && (theChar<='F')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
              haveValue = false;
            }
            else {
              *aErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'A')+10);
            haveValue = true;
          }
        }
        else if((theChar>='a') && (theChar<='f')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
              haveValue = false;
            }
            else {
              *aErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'a')+10);
            haveValue = true;
          }
        }
        else if((('X'==theChar) || ('x'==theChar)) && (!haveValue || result == 0)) {
          continue;
        }
        else if((('#'==theChar) || ('+'==theChar)) && !haveValue) {
          continue;
        }
        else {
          //we've encountered a char that's not a legal number or sign
          break;
        }

        if (result < oldresult) {
          // overflow!
          *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
          result = 0;
          break;
        }
      } //while
      if(negate)
        result=-result;
    } //if
  }
  return result;
}


/**
 * nsTString::ToInteger64
 */
int64_t
nsTString_CharT::ToInteger64( nsresult* aErrorCode, uint32_t aRadix ) const
{
  CharT*  cp=mData;
  int32_t theRadix=10; // base 10 unless base 16 detected, or overriden (aRadix != kAutoDetect)
  int64_t result=0;
  bool    negate=false;
  CharT   theChar=0;

  //initial value, override if we find an integer
  *aErrorCode=NS_ERROR_ILLEGAL_VALUE;

  if(cp) {

    //begin by skipping over leading chars that shouldn't be part of the number...

    CharT*  endcp=cp+mLength;
    bool    done=false;

    while((cp<endcp) && (!done)){
      switch(*cp++) {
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          theRadix=16;
          done=true;
          break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          done=true;
          break;
        case '-':
          negate=true; //fall through...
          break;
        case 'X': case 'x':
          theRadix=16;
          break;
        default:
          break;
      } //switch
    }

    if (done) {

      //integer found
      *aErrorCode = NS_OK;

      if (aRadix!=kAutoDetect) theRadix = aRadix; // override

      //now iterate the numeric chars and build our result
      CharT* first=--cp;  //in case we have to back up.
      bool haveValue = false;

      while(cp<endcp){
        int64_t oldresult = result;

        theChar=*cp++;
        if(('0'<=theChar) && (theChar<='9')){
          result = (theRadix * result) + (theChar-'0');
          haveValue = true;
        }
        else if((theChar>='A') && (theChar<='F')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
              haveValue = false;
            }
            else {
              *aErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'A')+10);
            haveValue = true;
          }
        }
        else if((theChar>='a') && (theChar<='f')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
              haveValue = false;
            }
            else {
              *aErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'a')+10);
            haveValue = true;
          }
        }
        else if((('X'==theChar) || ('x'==theChar)) && (!haveValue || result == 0)) {
          continue;
        }
        else if((('#'==theChar) || ('+'==theChar)) && !haveValue) {
          continue;
        }
        else {
          //we've encountered a char that's not a legal number or sign
          break;
        }

        if (result < oldresult) {
          // overflow!
          *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
          result = 0;
          break;
        }
      } //while
      if(negate)
        result=-result;
    } //if
  }
  return result;
}


/**
   * nsTString::Mid
   */

uint32_t
nsTString_CharT::Mid( self_type& aResult, index_type aStartPos, size_type aLengthToCopy ) const
{
  if (aStartPos == 0 && aLengthToCopy >= mLength)
    aResult = *this;
  else
    aResult = Substring(*this, aStartPos, aLengthToCopy);

  return aResult.mLength;
}


/**
 * nsTString::SetCharAt
 */

bool
nsTString_CharT::SetCharAt( char16_t aChar, uint32_t aIndex )
{
  if (aIndex >= mLength)
    return false;

  if (!EnsureMutable())
    AllocFailed(mLength);

  mData[aIndex] = CharT(aChar);
  return true;
}


/**
 * nsTString::StripChars,StripChar,StripWhitespace
 */

void
nsTString_CharT::StripChars( const char* aSet )
{
  if (!StripChars(aSet, mozilla::fallible)) {
    AllocFailed(mLength);
  }
}

bool
nsTString_CharT::StripChars( const char* aSet, const fallible_t& )
{
  if (!EnsureMutable()) {
    return false;
  }

  mLength = nsBufferRoutines<CharT>::strip_chars(mData, mLength, aSet);
  return true;
}

void
nsTString_CharT::StripWhitespace()
{
  if (!StripWhitespace(mozilla::fallible)) {
    AllocFailed(mLength);
  }
}

bool
nsTString_CharT::StripWhitespace( const fallible_t& )
{
  if (!EnsureMutable()) {
    return false;
  }

  StripTaggedASCII(mozilla::ASCIIMask::MaskWhitespace());
  return true;
}

/**
 * nsTString::ReplaceChar,ReplaceSubstring
 */

void
nsTString_CharT::ReplaceChar( char_type aOldChar, char_type aNewChar )
{
  if (!EnsureMutable()) // XXX do this lazily?
    AllocFailed(mLength);

  for (uint32_t i=0; i<mLength; ++i)
  {
    if (mData[i] == aOldChar)
      mData[i] = aNewChar;
  }
}

void
nsTString_CharT::ReplaceChar( const char* aSet, char_type aNewChar )
{
  if (!EnsureMutable()) // XXX do this lazily?
    AllocFailed(mLength);

  char_type* data = mData;
  uint32_t lenRemaining = mLength;

  while (lenRemaining)
  {
    int32_t i = ::FindCharInSet(data, lenRemaining, aSet);
    if (i == kNotFound)
      break;

    data[i++] = aNewChar;
    data += i;
    lenRemaining -= i;
  }
}

void ReleaseData(void* aData, nsAString::DataFlags aFlags);

void
nsTString_CharT::ReplaceSubstring(const char_type* aTarget,
                                  const char_type* aNewValue)
{
  ReplaceSubstring(nsTDependentString_CharT(aTarget),
                   nsTDependentString_CharT(aNewValue));
}

bool
nsTString_CharT::ReplaceSubstring(const char_type* aTarget,
                                  const char_type* aNewValue,
                                  const fallible_t& aFallible)
{
  return ReplaceSubstring(nsTDependentString_CharT(aTarget),
                          nsTDependentString_CharT(aNewValue),
                          aFallible);
}

void
nsTString_CharT::ReplaceSubstring(const self_type& aTarget,
                                  const self_type& aNewValue)
{
  if (!ReplaceSubstring(aTarget, aNewValue, mozilla::fallible)) {
    // Note that this may wildly underestimate the allocation that failed, as
    // we could have been replacing multiple copies of aTarget.
    AllocFailed(mLength + (aNewValue.Length() - aTarget.Length()));
  }
}

bool
nsTString_CharT::ReplaceSubstring(const self_type& aTarget,
                                  const self_type& aNewValue,
                                  const fallible_t&)
{
  if (aTarget.Length() == 0)
    return true;

  // Remember all of the non-matching parts.
  AutoTArray<Segment, 16> nonMatching;
  uint32_t i = 0;
  mozilla::CheckedUint32 newLength;
  while (true)
  {
    int32_t r = FindSubstring(mData + i, mLength - i, static_cast<const char_type*>(aTarget.Data()), aTarget.Length(), false);
    int32_t until = (r == kNotFound) ? mLength - i : r;
    nonMatching.AppendElement(Segment(i, until));
    newLength += until;
    if (r == kNotFound) {
      break;
    }

    newLength += aNewValue.Length();
    i += r + aTarget.Length();
    if (i >= mLength) {
      // Add an auxiliary entry at the end of the list to help as an edge case
      // for the algorithms below.
      nonMatching.AppendElement(Segment(mLength, 0));
      break;
    }
  }

  if (!newLength.isValid()) {
    return false;
  }

  // If there's only one non-matching segment, then the target string was not
  // found, and there's nothing to do.
  if (nonMatching.Length() == 1) {
    MOZ_ASSERT(nonMatching[0].mBegin == 0 && nonMatching[0].mLength == mLength,
               "We should have the correct non-matching segment.");
    return true;
  }

  // Make sure that we can mutate our buffer.
  // Note that we always allocate at least an mLength sized buffer, because the
  // rest of the algorithm relies on having access to all of the original
  // string.  In other words, we over-allocate in the shrinking case.
  char_type* oldData;
  DataFlags oldFlags;
  if (!MutatePrep(XPCOM_MAX(mLength, newLength.value()), &oldData, &oldFlags))
    return false;
  if (oldData) {
    // Copy all of the old data to the new buffer.
    char_traits::copy(mData, oldData, mLength);
    ::ReleaseData(oldData, oldFlags);
  }

  if (aTarget.Length() >= aNewValue.Length()) {
    // In the shrinking case, start filling the buffer from the beginning.
    const uint32_t delta = (aTarget.Length() - aNewValue.Length());
    for (i = 1; i < nonMatching.Length(); ++i) {
      // When we move the i'th non-matching segment into position, we need to
      // account for the characters deleted by the previous |i| replacements by
      // subtracting |i * delta|.
      const char_type* sourceSegmentPtr = mData + nonMatching[i].mBegin;
      char_type* destinationSegmentPtr = mData + nonMatching[i].mBegin - i * delta;
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
      const char_type* sourceSegmentPtr = mData + nonMatching[i].mBegin;
      char_type* destinationSegmentPtr = mData + nonMatching[i].mBegin + i * delta;
      char_traits::move(destinationSegmentPtr, sourceSegmentPtr,
                        nonMatching[i].mLength);
      // Write the i'th replacement immediately before the new i'th non-matching
      // segment.
      char_traits::copy(destinationSegmentPtr - aNewValue.Length(),
                        aNewValue.Data(), aNewValue.Length());
    }
  }

  // Adjust the length and make sure the string is null terminated.
  mLength = newLength.value();
  mData[mLength] = char_type(0);

  return true;
}

/**
 * nsTString::Trim
 */

void
nsTString_CharT::Trim( const char* aSet, bool aTrimLeading, bool aTrimTrailing, bool aIgnoreQuotes )
{
  // the old implementation worried about aSet being null :-/
  if (!aSet)
    return;

  char_type* start = mData;
  char_type* end   = mData + mLength;

  // skip over quotes if requested
  if (aIgnoreQuotes && mLength > 2 && mData[0] == mData[mLength - 1] &&
      (mData[0] == '\'' || mData[0] == '"'))
  {
    ++start;
    --end;
  }

  uint32_t setLen = nsCharTraits<char>::length(aSet);

  if (aTrimLeading)
  {
    uint32_t cutStart = start - mData;
    uint32_t cutLength = 0;

    // walk forward from start to end
    for (; start != end; ++start, ++cutLength)
    {
      int32_t pos = FindChar1(aSet, setLen, 0, *start, setLen);
      if (pos == kNotFound)
        break;
    }

    if (cutLength)
    {
      Cut(cutStart, cutLength);

      // reset iterators
      start = mData + cutStart;
      end   = mData + mLength - cutStart;
    }
  }

  if (aTrimTrailing)
  {
    uint32_t cutEnd = end - mData;
    uint32_t cutLength = 0;

    // walk backward from end to start
    --end;
    for (; end >= start; --end, ++cutLength)
    {
      int32_t pos = FindChar1(aSet, setLen, 0, *end, setLen);
      if (pos == kNotFound)
        break;
    }

    if (cutLength)
      Cut(cutEnd - cutLength, cutLength);
  }
}


/**
 * nsTString::CompressWhitespace.
 */

void
nsTString_CharT::CompressWhitespace( bool aTrimLeading, bool aTrimTrailing )
{
  // Quick exit
  if (mLength == 0) {
    return;
  }

  if (!EnsureMutable())
    AllocFailed(mLength);

  const ASCIIMaskArray& mask = mozilla::ASCIIMask::MaskWhitespace();

  char_type* to   = mData;
  char_type* from = mData;
  char_type* end  = mData + mLength;

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
  if (aTrimTrailing && skipWS && to > mData) {
    to--;
  }

  *to = char_type(0); // add the null
  mLength = to - mData;
}


/**
 * nsTString::AssignWithConversion
 */

void
nsTString_CharT::AssignWithConversion( const incompatible_char_type* aData, int32_t aLength )
{
  // for compatibility with the old string implementation, we need to allow
  // for a nullptr input buffer :-(
  if (!aData)
  {
    Truncate();
  }
  else
  {
    if (aLength < 0)
      aLength = nsCharTraits<incompatible_char_type>::length(aData);

    AssignWithConversion(Substring(aData, aLength));
  }
}
