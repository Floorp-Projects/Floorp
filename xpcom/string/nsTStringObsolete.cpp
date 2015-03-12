/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



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
  if (!EnsureMutable())
    AllocFailed(mLength);

  mLength = nsBufferRoutines<CharT>::strip_chars(mData, mLength, aSet);
}

void
nsTString_CharT::StripWhitespace()
{
  StripChars(kWhitespace);
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

void ReleaseData(void* aData, uint32_t aFlags);

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
  uint32_t t = aTarget.Length();
  if (t == 0)
    return true;

  int32_t gap = FindSubstring(mData, mLength, aTarget.Data(), t, false);
  if (gap == kNotFound)
    return true;

  // Build up a list of gaps between matches.
  nsAutoTArray<uint32_t, 16> gaps;
  int32_t pos = 0;
  do {
    gaps.AppendElement(gap);
    pos += gap + t;
    gap = FindSubstring(mData + pos, mLength - pos, aTarget.Data(), t, false);
  } while (gap != kNotFound);

  uint32_t n = aNewValue.Length();
  uint32_t l = mLength + (n - t) * gaps.Length();
  char_type* oldData;
  uint32_t oldFlags;
  if (!MutatePrep(XPCOM_MAX(mLength, l), &oldData, &oldFlags))
    return false;

  // The rest of the string includes the null terminator.
  gaps.AppendElement(Length() + 1 - pos);
  // Save the old end of the string.
  const char_type* src = mData + mLength + 1;
  mLength = l;
  if (!oldData && t < n) {
    // We are growing the string in-place. Work back from the end.
    char_type* dest = mData + l + 1;
    for (size_t i = gaps.Length() - 1; i > 0; i--) {
      if (gaps[i] > 0) {
        src -= gaps[i];
        dest -= gaps[i];
        char_traits::move(dest, src, gaps[i]);
      }
      src -= t;
      dest -= n;
      char_traits::copy(dest, aNewValue.Data(), n);
    }
    return true;
  }

  src = mData;
  if (oldData) {
    // We are copying to a new buffer, so explicitly copy the first gap.
    src = oldData;
    if (gaps[0] > 0)
      char_traits::copy(mData, src, gaps[0]);
  }

  char_type* dest = mData + gaps[0];
  src += gaps[0];
  for (size_t i = 1; i < gaps.Length(); i++) {
    char_traits::copy(dest, aNewValue.Data(), n);
    dest += n;
    src += t;
    if (gaps[i] > 0) {
      char_traits::move(dest, src, gaps[i]);
      dest += gaps[i];
      src += gaps[i];
    }
  }

  if (oldData)
    ::ReleaseData(oldData, oldFlags);
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
 * nsTString::CompressWhitespace
 */

void
nsTString_CharT::CompressWhitespace( bool aTrimLeading, bool aTrimTrailing )
{
  const char* set = kWhitespace;

  ReplaceChar(set, ' ');
  Trim(set, aTrimLeading, aTrimTrailing);

  // this one does some questionable fu... just copying the old code!
  mLength = nsBufferRoutines<char_type>::compress_chars(mData, mLength, set);
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
