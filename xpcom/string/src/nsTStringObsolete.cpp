/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



  /**
   * nsTString::Find
   *
   * aOffset specifies starting index
   * aCount specifies number of string compares (iterations)
   */

PRInt32
nsTString_CharT::Find( const nsCString& aString, bool aIgnoreCase, PRInt32 aOffset, PRInt32 aCount) const
  {
    // this method changes the meaning of aOffset and aCount:
    Find_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

    PRInt32 result = FindSubstring(mData + aOffset, aCount, aString.get(), aString.Length(), aIgnoreCase);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

PRInt32
nsTString_CharT::Find( const char* aString, bool aIgnoreCase, PRInt32 aOffset, PRInt32 aCount) const
  {
    return Find(nsDependentCString(aString), aIgnoreCase, aOffset, aCount);
  }


  /**
   * nsTString::RFind
   *
   * aOffset specifies starting index
   * aCount specifies number of string compares (iterations)
   */

PRInt32
nsTString_CharT::RFind( const nsCString& aString, bool aIgnoreCase, PRInt32 aOffset, PRInt32 aCount) const
  {
    // this method changes the meaning of aOffset and aCount:
    RFind_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

    PRInt32 result = RFindSubstring(mData + aOffset, aCount, aString.get(), aString.Length(), aIgnoreCase);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

PRInt32
nsTString_CharT::RFind( const char* aString, bool aIgnoreCase, PRInt32 aOffset, PRInt32 aCount) const
  {
    return RFind(nsDependentCString(aString), aIgnoreCase, aOffset, aCount);
  }


  /**
   * nsTString::RFindChar
   */

PRInt32
nsTString_CharT::RFindChar( PRUnichar aChar, PRInt32 aOffset, PRInt32 aCount) const
  {
    return nsBufferRoutines<CharT>::rfind_char(mData, mLength, aOffset, aChar, aCount);
  }


  /**
   * nsTString::FindCharInSet
   */

PRInt32
nsTString_CharT::FindCharInSet( const char* aSet, PRInt32 aOffset ) const
  {
    if (aOffset < 0)
      aOffset = 0;
    else if (aOffset >= PRInt32(mLength))
      return kNotFound;
    
    PRInt32 result = ::FindCharInSet(mData + aOffset, mLength - aOffset, aSet);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }


  /**
   * nsTString::RFindCharInSet
   */

PRInt32
nsTString_CharT::RFindCharInSet( const CharT* aSet, PRInt32 aOffset ) const
  {
    // We want to pass a "data length" to ::RFindCharInSet
    if (aOffset < 0 || aOffset > PRInt32(mLength))
      aOffset = mLength;
    else
      ++aOffset;

    return ::RFindCharInSet(mData, aOffset, aSet);
  }


  // it's a shame to replicate this code.  it was done this way in the past
  // to help performance.  this function also gets to keep the rickg style
  // indentation :-/
PRInt32
nsTString_CharT::ToInteger( PRInt32* aErrorCode, PRUint32 aRadix ) const
{
  CharT*  cp=mData;
  PRInt32 theRadix=10; // base 10 unless base 16 detected, or overriden (aRadix != kAutoDetect)
  PRInt32 result=0;
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
        PRInt32 oldresult = result;

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

PRUint32
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
nsTString_CharT::SetCharAt( PRUnichar aChar, PRUint32 aIndex )
  {
    if (aIndex >= mLength)
      return false;

    if (!EnsureMutable())
      NS_RUNTIMEABORT("OOM");

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
      NS_RUNTIMEABORT("OOM");

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
      NS_RUNTIMEABORT("OOM");

    for (PRUint32 i=0; i<mLength; ++i)
      {
        if (mData[i] == aOldChar)
          mData[i] = aNewChar;
      }
  }

void
nsTString_CharT::ReplaceChar( const char* aSet, char_type aNewChar )
  {
    if (!EnsureMutable()) // XXX do this lazily?
      NS_RUNTIMEABORT("OOM");

    char_type* data = mData;
    PRUint32 lenRemaining = mLength;

    while (lenRemaining)
      {
        PRInt32 i = ::FindCharInSet(data, lenRemaining, aSet);
        if (i == kNotFound)
          break;

        data[i++] = aNewChar;
        data += i;
        lenRemaining -= i;
      }
  }

void
nsTString_CharT::ReplaceSubstring( const char_type* aTarget, const char_type* aNewValue )
  {
    ReplaceSubstring(nsTDependentString_CharT(aTarget),
                     nsTDependentString_CharT(aNewValue));
  }

void
nsTString_CharT::ReplaceSubstring( const self_type& aTarget, const self_type& aNewValue )
  {
    if (aTarget.Length() == 0)
      return;

    PRUint32 i = 0;
    while (i < mLength)
      {
        PRInt32 r = FindSubstring(mData + i, mLength - i, aTarget.Data(), aTarget.Length(), false);
        if (r == kNotFound)
          break;

        Replace(i + r, aTarget.Length(), aNewValue);
        i += r + aNewValue.Length();
      }
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

    PRUint32 setLen = nsCharTraits<char>::length(aSet);

    if (aTrimLeading)
      {
        PRUint32 cutStart = start - mData;
        PRUint32 cutLength = 0;

          // walk forward from start to end
        for (; start != end; ++start, ++cutLength)
          {
            PRInt32 pos = FindChar1(aSet, setLen, 0, *start, setLen);
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
        PRUint32 cutEnd = end - mData;
        PRUint32 cutLength = 0;

          // walk backward from end to start
        --end;
        for (; end >= start; --end, ++cutLength)
          {
            PRInt32 pos = FindChar1(aSet, setLen, 0, *end, setLen);
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
nsTString_CharT::AssignWithConversion( const incompatible_char_type* aData, PRInt32 aLength )
  {
      // for compatibility with the old string implementation, we need to allow
      // for a NULL input buffer :-(
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
