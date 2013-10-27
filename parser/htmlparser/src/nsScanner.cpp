/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define __INCREMENTAL 1

#include "mozilla/DebugOnly.h"

#include "nsScanner.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsReadableUtils.h"
#include "nsIInputStream.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsUTF8Utils.h" // for LossyConvertEncoding
#include "nsCRT.h"
#include "nsParser.h"
#include "nsCharsetSource.h"

#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

// We replace NUL characters with this character.
static PRUnichar sInvalid = UCS2_REPLACEMENT_CHAR;

nsReadEndCondition::nsReadEndCondition(const PRUnichar* aTerminateChars) :
  mChars(aTerminateChars), mFilter(PRUnichar(~0)) // All bits set
{
  // Build filter that will be used to filter out characters with
  // bits that none of the terminal chars have. This works very well
  // because terminal chars often have only the last 4-6 bits set and
  // normal ascii letters have bit 7 set. Other letters have even higher
  // bits set.
  
  // Calculate filter
  const PRUnichar *current = aTerminateChars;
  PRUnichar terminalChar = *current;
  while (terminalChar) {
    mFilter &= ~terminalChar;
    ++current;
    terminalChar = *current;
  }
}

#ifdef __INCREMENTAL
const int   kBufsize=1;
#else
const int   kBufsize=64;
#endif

/**
 *  Use this constructor if you want i/o to be based on 
 *  a single string you hand in during construction.
 *  This short cut was added for Javascript.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return  
 */
nsScanner::nsScanner(const nsAString& anHTMLString)
{
  MOZ_COUNT_CTOR(nsScanner);

  mSlidingBuffer = nullptr;
  mCountRemaining = 0;
  mFirstNonWhitespacePosition = -1;
  if (AppendToBuffer(anHTMLString)) {
    mSlidingBuffer->BeginReading(mCurrentPosition);
  } else {
    /* XXX see hack below, re: bug 182067 */
    memset(&mCurrentPosition, 0, sizeof(mCurrentPosition));
    mEndPosition = mCurrentPosition;
  }
  mMarkPosition = mCurrentPosition;
  mIncremental = false;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  mHasInvalidCharacter = false;
  mReplacementCharacter = PRUnichar(0x0);
}

/**
 *  Use this constructor if you want i/o to be based on strings 
 *  the scanner receives. If you pass a null filename, you
 *  can still provide data to the scanner via append.
 */
nsScanner::nsScanner(nsString& aFilename, bool aCreateStream)
  : mFilename(aFilename)
{
  MOZ_COUNT_CTOR(nsScanner);
  NS_ASSERTION(!aCreateStream, "This is always true.");

  mSlidingBuffer = nullptr;

  // XXX This is a big hack.  We need to initialize the iterators to something.
  // What matters is that mCurrentPosition == mEndPosition, so that our methods
  // believe that we are at EOF (see bug 182067).  We null out mCurrentPosition
  // so that we have some hope of catching null pointer dereferences associated
  // with this hack. --darin
  memset(&mCurrentPosition, 0, sizeof(mCurrentPosition));
  mMarkPosition = mCurrentPosition;
  mEndPosition = mCurrentPosition;

  mIncremental = true;
  mFirstNonWhitespacePosition = -1;
  mCountRemaining = 0;

  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  mHasInvalidCharacter = false;
  mReplacementCharacter = PRUnichar(0x0);
  // XML defaults to UTF-8 and about:blank is UTF-8, too.
  SetDocumentCharset(NS_LITERAL_CSTRING("UTF-8"), kCharsetFromDocTypeDefault);
}

nsresult nsScanner::SetDocumentCharset(const nsACString& aCharset , int32_t aSource)
{
  if (aSource < mCharsetSource) // priority is lower than the current one
    return NS_OK;

  mCharsetSource = aSource;

  nsCString charsetName;
  mozilla::DebugOnly<bool> valid =
      EncodingUtils::FindEncodingForLabel(aCharset, charsetName);
  MOZ_ASSERT(valid, "Should never call with a bogus aCharset.");

  if (!mCharset.IsEmpty() && charsetName.Equals(mCharset)) {
    return NS_OK; // no difference, don't change it
  }

  // different, need to change it

  mCharset.Assign(charsetName);

  NS_ASSERTION(nsParser::GetCharsetConverterManager(),
               "Must have the charset converter manager!");

  nsresult res = nsParser::GetCharsetConverterManager()->
    GetUnicodeDecoderRaw(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
  if (NS_SUCCEEDED(res) && mUnicodeDecoder)
  {
     // We need to detect conversion error of character to support XML
     // encoding error.
     mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Signal);
  }

  return res;
}


/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsScanner::~nsScanner() {

  delete mSlidingBuffer;

  MOZ_COUNT_DTOR(nsScanner);
}

/**
 *  Resets current offset position of input stream to marked position. 
 *  This allows us to back up to this point if the need should arise, 
 *  such as when tokenization gets interrupted.
 *  NOTE: IT IS REALLY BAD FORM TO CALL RELEASE WITHOUT CALLING MARK FIRST!
 *
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
void nsScanner::RewindToMark(void){
  if (mSlidingBuffer) {
    mCountRemaining += (Distance(mMarkPosition, mCurrentPosition));
    mCurrentPosition = mMarkPosition;
  }
}


/**
 *  Records current offset position in input stream. This allows us
 *  to back up to this point if the need should arise, such as when
 *  tokenization gets interrupted.
 *
 *  @update  gess 7/29/98
 *  @param   
 *  @return  
 */
int32_t nsScanner::Mark() {
  int32_t distance = 0;
  if (mSlidingBuffer) {
    nsScannerIterator oldStart;
    mSlidingBuffer->BeginReading(oldStart);

    distance = Distance(oldStart, mCurrentPosition);

    mSlidingBuffer->DiscardPrefix(mCurrentPosition);
    mSlidingBuffer->BeginReading(mCurrentPosition);
    mMarkPosition = mCurrentPosition;
  }

  return distance;
}

/** 
 * Insert data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  harishd 01/12/99
 * @return  error code 
 */
bool nsScanner::UngetReadable(const nsAString& aBuffer) {
  if (!mSlidingBuffer) {
    return false;
  }

  mSlidingBuffer->UngetReadable(aBuffer,mCurrentPosition);
  mSlidingBuffer->BeginReading(mCurrentPosition); // Insertion invalidated our iterators
  mSlidingBuffer->EndReading(mEndPosition);
 
  uint32_t length = aBuffer.Length();
  mCountRemaining += length; // Ref. bug 117441
  return true;
}

/** 
 * Append data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  gess4/3/98
 * @return  error code 
 */
nsresult nsScanner::Append(const nsAString& aBuffer) {
  if (!AppendToBuffer(aBuffer))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/**
 *  
 *  
 *  @update  gess 5/21/98
 *  @param   
 *  @return  
 */
nsresult nsScanner::Append(const char* aBuffer, uint32_t aLen,
                           nsIRequest *aRequest)
{
  nsresult res = NS_OK;
  if (mUnicodeDecoder) {
    int32_t unicharBufLen = 0;
    mUnicodeDecoder->GetMaxLength(aBuffer, aLen, &unicharBufLen);
    nsScannerString::Buffer* buffer = nsScannerString::AllocBuffer(unicharBufLen + 1);
    NS_ENSURE_TRUE(buffer,NS_ERROR_OUT_OF_MEMORY);
    PRUnichar *unichars = buffer->DataStart();

    int32_t totalChars = 0;
    int32_t unicharLength = unicharBufLen;
    int32_t errorPos = -1;

    do {
      int32_t srcLength = aLen;
      res = mUnicodeDecoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);

      totalChars += unicharLength;
      // Continuation of failure case
      if(NS_FAILED(res)) {
        // if we failed, we consume one byte, replace it with the replacement
        // character and try the conversion again.

        // This is only needed because some decoders don't follow the
        // nsIUnicodeDecoder contract: they return a failure when *aDestLength
        // is 0 rather than the correct NS_OK_UDEC_MOREOUTPUT.  See bug 244177
        if ((unichars + unicharLength) >= buffer->DataEnd()) {
          NS_ERROR("Unexpected end of destination buffer");
          break;
        }

        if (mReplacementCharacter == 0x0 && errorPos == -1) {
          errorPos = totalChars;
        }
        unichars[unicharLength++] = mReplacementCharacter == 0x0 ?
                                    mUnicodeDecoder->GetCharacterForUnMapped() :
                                    mReplacementCharacter;

        unichars = unichars + unicharLength;
        unicharLength = unicharBufLen - (++totalChars);

        mUnicodeDecoder->Reset();

        if(((uint32_t) (srcLength + 1)) > aLen) {
          srcLength = aLen;
        }
        else {
          ++srcLength;
        }

        aBuffer += srcLength;
        aLen -= srcLength;
      }
    } while (NS_FAILED(res) && (aLen > 0));

    buffer->SetDataLength(totalChars);
    // Don't propagate return code of unicode decoder
    // since it doesn't reflect on our success or failure
    // - Ref. bug 87110
    res = NS_OK; 
    if (!AppendToBuffer(buffer, aRequest, errorPos))
      res = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_WARNING("No decoder found.");
    res = NS_ERROR_FAILURE;
  }

  return res;
}

/**
 *  retrieve next char from scanners internal input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code reflecting read status
 */
nsresult nsScanner::GetChar(PRUnichar& aChar) {
  if (!mSlidingBuffer || mCurrentPosition == mEndPosition) {
    aChar = 0;
    return kEOF;
  }

  aChar = *mCurrentPosition++;
  --mCountRemaining;

  return NS_OK;
}


/**
 *  peek ahead to consume next char from scanner's internal
 *  input buffer
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult nsScanner::Peek(PRUnichar& aChar, uint32_t aOffset) {
  aChar = 0;

  if (!mSlidingBuffer || mCurrentPosition == mEndPosition) {
    return kEOF;
  }

  if (aOffset > 0) {
    if (mCountRemaining <= aOffset)
      return kEOF;

    nsScannerIterator pos = mCurrentPosition;
    pos.advance(aOffset);
    aChar=*pos;
  }
  else {
    aChar=*mCurrentPosition;
  }

  return NS_OK;
}

nsresult nsScanner::Peek(nsAString& aStr, int32_t aNumChars, int32_t aOffset)
{
  if (!mSlidingBuffer || mCurrentPosition == mEndPosition) {
    return kEOF;
  }

  nsScannerIterator start, end;

  start = mCurrentPosition;

  if ((int32_t)mCountRemaining <= aOffset) {
    return kEOF;
  }

  if (aOffset > 0) {
    start.advance(aOffset);
  }

  if (mCountRemaining < uint32_t(aNumChars + aOffset)) {
    end = mEndPosition;
  }
  else {
    end = start;
    end.advance(aNumChars);
  }

  CopyUnicodeTo(start, end, aStr);

  return NS_OK;
}


/**
 *  Skip whitespace on scanner input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error status
 */
nsresult nsScanner::SkipWhitespace(int32_t& aNewlinesSkipped) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar = 0;
  nsresult  result = Peek(theChar);
  
  if (NS_FAILED(result)) {
    return result;
  }
  
  nsScannerIterator current = mCurrentPosition;
  bool      done = false;
  bool      skipped = false;
  
  while (!done && current != mEndPosition) {
    switch(theChar) {
      case '\n':
      case '\r': ++aNewlinesSkipped;
      case ' ' :
      case '\t':
        {
          skipped = true;
          PRUnichar thePrevChar = theChar;
          theChar = (++current != mEndPosition) ? *current : '\0';
          if ((thePrevChar == '\r' && theChar == '\n') ||
              (thePrevChar == '\n' && theChar == '\r')) {
            theChar = (++current != mEndPosition) ? *current : '\0'; // CRLF == LFCR => LF
          }
        }
        break;
      default:
        done = true;
        break;
    }
  }

  if (skipped) {
    SetPosition(current);
    if (current == mEndPosition) {
      result = kEOF;
    }
  }

  return result;
}

/**
 *  Skip over chars as long as they equal given char
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::SkipOver(PRUnichar aSkipChar){

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar ch=0;
  nsresult   result=NS_OK;

  while(NS_OK==result) {
    result=Peek(ch);
    if(NS_OK == result) {
      if(ch!=aSkipChar) {
        break;
      }
      GetChar(ch);
    } 
    else break;
  } //while
  return result;

}

#if 0
void DoErrTest(nsString& aString) {
  int32_t pos=aString.FindChar(0);
  if(kNotFound<pos) {
    if(aString.Length()-1!=pos) {
    }
  }
}

void DoErrTest(nsCString& aString) {
  int32_t pos=aString.FindChar(0);
  if(kNotFound<pos) {
    if(aString.Length()-1!=pos) {
    }
  }
}
#endif

/**
 *  Consume characters until you run into space, a '<', a '>', or a '/'.
 *  
 *  @param   aString - receives new data from stream
 *  @return  error code
 */
nsresult nsScanner::ReadTagIdentifier(nsScannerSharedSubstring& aString) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsScannerIterator current, end;
  bool              found=false;  
  
  current = mCurrentPosition;
  end = mEndPosition;

  // Loop until we find an illegal character. Everything is then appended
  // later.
  while(current != end && !found) {
    theChar=*current;

    switch(theChar) {
      case '\n':
      case '\r':
      case ' ' :
      case '\t':
      case '\v':
      case '\f':
      case '<':
      case '>':
      case '/':
        found = true;
        break;

      case '\0':
        ReplaceCharacter(current, sInvalid);
        break;

      default:
        break;
    }

    if (!found) {
      ++current;
    }
  }

  // Don't bother appending nothing.
  if (current != mCurrentPosition) {
    AppendUnicodeTo(mCurrentPosition, current, aString);
  }

  SetPosition(current);  
  if (current == end) {
    result = kEOF;
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume characters until you run into a char that's not valid in an
 *  entity name
 *  
 *  @param   aString - receives new data from stream
 *  @return  error code
 */
nsresult nsScanner::ReadEntityIdentifier(nsString& aString) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsScannerIterator origin, current, end;
  bool              found=false;  

  origin = mCurrentPosition;
  current = mCurrentPosition;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      found=false;
      switch(theChar) {
        case '_':
        case '-':
        case '.':
          // Don't allow ':' in entity names.  See bug 23791
          found = true;
          break;
        default:
          found = ('a'<=theChar && theChar<='z') ||
                  ('A'<=theChar && theChar<='Z') ||
                  ('0'<=theChar && theChar<='9');
          break;
      }

      if(!found) {
        AppendUnicodeTo(mCurrentPosition, current, aString);
        break;
      }
    }
    ++current;
  }
  
  SetPosition(current);
  if (current == end) {
    AppendUnicodeTo(origin, current, aString);
    return kEOF;
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume digits 
 *  
 *  @param   aString - should contain digits
 *  @return  error code
 */
nsresult nsScanner::ReadNumber(nsString& aString,int32_t aBase) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  NS_ASSERTION(aBase == 10 || aBase == 16,"base value not supported");

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsScannerIterator origin, current, end;

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  bool done = false;
  while(current != end) {
    theChar=*current;
    if(theChar) {
      done = (theChar < '0' || theChar > '9') && 
             ((aBase == 16)? (theChar < 'A' || theChar > 'F') &&
                             (theChar < 'a' || theChar > 'f')
                             :true);
      if(done) {
        AppendUnicodeTo(origin, current, aString);
        break;
      }
    }
    ++current;
  }

  SetPosition(current);
  if (current == end) {
    AppendUnicodeTo(origin, current, aString);
    return kEOF;
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume characters until you find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString receives new data from stream
 *  @param   addTerminal tells us whether to append terminal to aString
 *  @return  error code
 */
nsresult nsScanner::ReadWhitespace(nsScannerSharedSubstring& aString,
                                   int32_t& aNewlinesSkipped,
                                   bool& aHaveCR) {

  aHaveCR = false;

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar = 0;
  nsresult  result = Peek(theChar);
  
  if (NS_FAILED(result)) {
    return result;
  }
  
  nsScannerIterator origin, current, end;
  bool done = false;  

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  bool haveCR = false;

  while(!done && current != end) {
    switch(theChar) {
      case '\n':
      case '\r':
        {
          ++aNewlinesSkipped;
          PRUnichar thePrevChar = theChar;
          theChar = (++current != end) ? *current : '\0';
          if ((thePrevChar == '\r' && theChar == '\n') ||
              (thePrevChar == '\n' && theChar == '\r')) {
            theChar = (++current != end) ? *current : '\0'; // CRLF == LFCR => LF
            haveCR = true;
          } else if (thePrevChar == '\r') {
            // Lone CR becomes CRLF; callers should know to remove extra CRs
            AppendUnicodeTo(origin, current, aString);
            aString.writable().Append(PRUnichar('\n'));
            origin = current;
            haveCR = true;
          }
        }
        break;
      case ' ' :
      case '\t':
        theChar = (++current != end) ? *current : '\0';
        break;
      default:
        done = true;
        AppendUnicodeTo(origin, current, aString);
        break;
    }
  }

  SetPosition(current);
  if (current == end) {
    AppendUnicodeTo(origin, current, aString);
    result = kEOF;
  }

  aHaveCR = haveCR;
  return result;
}

//XXXbz callers of this have to manage their lone '\r' themselves if they want
//it to work.  Good thing they're all in view-source and it deals.
nsresult nsScanner::ReadWhitespace(nsScannerIterator& aStart, 
                                   nsScannerIterator& aEnd,
                                   int32_t& aNewlinesSkipped) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar = 0;
  nsresult  result = Peek(theChar);
  
  if (NS_FAILED(result)) {
    return result;
  }
  
  nsScannerIterator origin, current, end;
  bool done = false;  

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  while(!done && current != end) {
    switch(theChar) {
      case '\n':
      case '\r': ++aNewlinesSkipped;
      case ' ' :
      case '\t':
        {
          PRUnichar thePrevChar = theChar;
          theChar = (++current != end) ? *current : '\0';
          if ((thePrevChar == '\r' && theChar == '\n') ||
              (thePrevChar == '\n' && theChar == '\r')) {
            theChar = (++current != end) ? *current : '\0'; // CRLF == LFCR => LF
          }
        }
        break;
      default:
        done = true;
        aStart = origin;
        aEnd = current;
        break;
    }
  }

  SetPosition(current);
  if (current == end) {
    aStart = origin;
    aEnd = current;
    result = kEOF;
  }

  return result;
}

/**
 *  Consume characters until you encounter one contained in given
 *  input set.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will contain the result of this method
 *  @param   aTerminalSet is an ordered string that contains
 *           the set of INVALID characters
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsAString& aString,
                              const nsReadEndCondition& aEndCondition,
                              bool addTerminal)
{  
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsScannerIterator origin, current;
  const PRUnichar* setstart = aEndCondition.mChars;
  const PRUnichar* setcurrent;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);

  if (NS_FAILED(result)) {
    return result;
  }
  
  while (current != mEndPosition) {
    theChar = *current;
    if (theChar == '\0') {
      ReplaceCharacter(current, sInvalid);
      theChar = sInvalid;
    }

    // Filter out completely wrong characters
    // Check if all bits are in the required area
    if(!(theChar & aEndCondition.mFilter)) {
      // They were. Do a thorough check.

      setcurrent = setstart;
      while (*setcurrent) {
        if (*setcurrent == theChar) {
          if(addTerminal)
            ++current;
          AppendUnicodeTo(origin, current, aString);
          SetPosition(current);

          //DoErrTest(aString);

          return NS_OK;
        }
        ++setcurrent;
      }
    }
    
    ++current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  SetPosition(current);
  AppendUnicodeTo(origin, current, aString);
  return kEOF;
}

nsresult nsScanner::ReadUntil(nsScannerSharedSubstring& aString,
                              const nsReadEndCondition& aEndCondition,
                              bool addTerminal)
{  
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsScannerIterator origin, current;
  const PRUnichar* setstart = aEndCondition.mChars;
  const PRUnichar* setcurrent;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);

  if (NS_FAILED(result)) {
    return result;
  }
  
  while (current != mEndPosition) {
    theChar = *current;
    if (theChar == '\0') {
      ReplaceCharacter(current, sInvalid);
      theChar = sInvalid;
    }

    // Filter out completely wrong characters
    // Check if all bits are in the required area
    if(!(theChar & aEndCondition.mFilter)) {
      // They were. Do a thorough check.

      setcurrent = setstart;
      while (*setcurrent) {
        if (*setcurrent == theChar) {
          if(addTerminal)
            ++current;
          AppendUnicodeTo(origin, current, aString);
          SetPosition(current);

          //DoErrTest(aString);

          return NS_OK;
        }
        ++setcurrent;
      }
    }
    
    ++current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  SetPosition(current);
  AppendUnicodeTo(origin, current, aString);
  return kEOF;
}

nsresult nsScanner::ReadUntil(nsScannerIterator& aStart, 
                              nsScannerIterator& aEnd,
                              const nsReadEndCondition &aEndCondition,
                              bool addTerminal)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsScannerIterator origin, current;
  const PRUnichar* setstart = aEndCondition.mChars;
  const PRUnichar* setcurrent;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  
  if (NS_FAILED(result)) {
    aStart = aEnd = current;
    return result;
  }
  
  while (current != mEndPosition) {
    theChar = *current;
    if (theChar == '\0') {
      ReplaceCharacter(current, sInvalid);
      theChar = sInvalid;
    }

    // Filter out completely wrong characters
    // Check if all bits are in the required area
    if(!(theChar & aEndCondition.mFilter)) {
      // They were. Do a thorough check.
      setcurrent = setstart;
      while (*setcurrent) {
        if (*setcurrent == theChar) {
          if(addTerminal)
            ++current;
          aStart = origin;
          aEnd = current;
          SetPosition(current);

          return NS_OK;
        }
        ++setcurrent;
      }
    }

    ++current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  SetPosition(current);
  aStart = origin;
  aEnd = current;
  return kEOF;
}

/**
 *  Consumes chars until you see the given terminalChar
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsAString& aString,
                              PRUnichar aTerminalChar,
                              bool addTerminal)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsScannerIterator origin, current;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar theChar;
  nsresult result = Peek(theChar);

  if (NS_FAILED(result)) {
    return result;
  }

  while (current != mEndPosition) {
    theChar = *current;
    if (theChar == '\0') {
      ReplaceCharacter(current, sInvalid);
      theChar = sInvalid;
    }

    if (aTerminalChar == theChar) {
      if(addTerminal)
        ++current;
      AppendUnicodeTo(origin, current, aString);
      SetPosition(current);
      return NS_OK;
    }
    ++current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  AppendUnicodeTo(origin, current, aString);
  SetPosition(current);
  return kEOF;

}

void nsScanner::BindSubstring(nsScannerSubstring& aSubstring, const nsScannerIterator& aStart, const nsScannerIterator& aEnd)
{
  aSubstring.Rebind(*mSlidingBuffer, aStart, aEnd);
}

void nsScanner::CurrentPosition(nsScannerIterator& aPosition)
{
  aPosition = mCurrentPosition;
}

void nsScanner::EndReading(nsScannerIterator& aPosition)
{
  aPosition = mEndPosition;
}
 
void nsScanner::SetPosition(nsScannerIterator& aPosition, bool aTerminate, bool aReverse)
{
  if (mSlidingBuffer) {
#ifdef DEBUG
    uint32_t origRemaining = mCountRemaining;
#endif

    if (aReverse) {
      mCountRemaining += (Distance(aPosition, mCurrentPosition));
    }
    else {
      mCountRemaining -= (Distance(mCurrentPosition, aPosition));
    }

    NS_ASSERTION((mCountRemaining >= origRemaining && aReverse) ||
                 (mCountRemaining <= origRemaining && !aReverse),
                 "Improper use of nsScanner::SetPosition. Make sure to set the"
                 " aReverse parameter correctly");

    mCurrentPosition = aPosition;
    if (aTerminate && (mCurrentPosition == mEndPosition)) {
      mMarkPosition = mCurrentPosition;
      mSlidingBuffer->DiscardPrefix(mCurrentPosition);
    }
  }
}

void nsScanner::ReplaceCharacter(nsScannerIterator& aPosition,
                                 PRUnichar aChar)
{
  if (mSlidingBuffer) {
    mSlidingBuffer->ReplaceCharacter(aPosition, aChar);
  }
}

bool nsScanner::AppendToBuffer(nsScannerString::Buffer* aBuf,
                                 nsIRequest *aRequest,
                                 int32_t aErrorPos)
{
  uint32_t countRemaining = mCountRemaining;
  if (!mSlidingBuffer) {
    mSlidingBuffer = new nsScannerString(aBuf);
    if (!mSlidingBuffer)
      return false;
    mSlidingBuffer->BeginReading(mCurrentPosition);
    mMarkPosition = mCurrentPosition;
    mSlidingBuffer->EndReading(mEndPosition);
    mCountRemaining = aBuf->DataLength();
  }
  else {
    mSlidingBuffer->AppendBuffer(aBuf);
    if (mCurrentPosition == mEndPosition) {
      mSlidingBuffer->BeginReading(mCurrentPosition);
    }
    mSlidingBuffer->EndReading(mEndPosition);
    mCountRemaining += aBuf->DataLength();
  }

  if (aErrorPos != -1 && !mHasInvalidCharacter) {
    mHasInvalidCharacter = true;
    mFirstInvalidPosition = mCurrentPosition;
    mFirstInvalidPosition.advance(countRemaining + aErrorPos);
  }

  if (mFirstNonWhitespacePosition == -1) {
    nsScannerIterator iter(mCurrentPosition);
    nsScannerIterator end(mEndPosition);

    while (iter != end) {
      if (!nsCRT::IsAsciiSpace(*iter)) {
        mFirstNonWhitespacePosition = Distance(mCurrentPosition, iter);

        break;
      }

      ++iter;
    }
  }
  return true;
}

/**
 *  call this to copy bytes out of the scanner that have not yet been consumed
 *  by the tokenization process.
 *  
 *  @update  gess 5/12/98
 *  @param   aCopyBuffer is where the scanner buffer will be copied to
 *  @return  nada
 */
void nsScanner::CopyUnusedData(nsString& aCopyBuffer) {
  if (!mSlidingBuffer) {
    aCopyBuffer.Truncate();
    return;
  }

  nsScannerIterator start, end;
  start = mCurrentPosition;
  end = mEndPosition;

  CopyUnicodeTo(start, end, aCopyBuffer);
}

/**
 *  Retrieve the name of the file that the scanner is reading from.
 *  In some cases, it's just a given name, because the scanner isn't
 *  really reading from a file.
 *  
 *  @update  gess 5/12/98
 *  @return  
 */
nsString& nsScanner::GetFilename(void) {
  return mFilename;
}

/**
 *  Conduct self test. Actually, selftesting for this class
 *  occurs in the parser selftest.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */

void nsScanner::SelfTest(void) {
#ifdef _DEBUG
#endif
}

void nsScanner::OverrideReplacementCharacter(PRUnichar aReplacementCharacter)
{
  mReplacementCharacter = aReplacementCharacter;

  if (mHasInvalidCharacter) {
    ReplaceCharacter(mFirstInvalidPosition, mReplacementCharacter);
  }
}

