/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//#define __INCREMENTAL 1

#define NS_IMPL_IDS
#include "nsScanner.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsFileSpec.h"
#include "nsReadableUtils.h"

nsScannerString::nsScannerString(PRUnichar* aStorageStart, 
                                 PRUnichar* aDataEnd, 
                                 PRUnichar* aStorageEnd) : nsSlidingString(aStorageStart, aDataEnd, aStorageEnd)
{
}

void
nsScannerString::ReplaceCharacter(nsReadingIterator<PRUnichar>& aPosition,
                                  PRUnichar aChar)
{
  // XXX Casting a const to non-const. Unless the base class
  // provides support for writing iterators, this is the best
  // that can be done.
  PRUnichar* pos = (PRUnichar*)aPosition.get();
  *pos = aChar;
}

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

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

const char* kBadHTMLText="<H3>Oops...</H3>You just tried to read a non-existent document: <BR>";
const char* kUnorderedStringError = "String argument must be ordered. Don't you read API's?";

#ifdef __INCREMENTAL
const int   kBufsize=1;
#else
const int   kBufsize=64;
#endif

MOZ_DECL_CTOR_COUNTER(nsScanner)

/**
 *  Use this constructor if you want i/o to be based on 
 *  a single string you hand in during construction.
 *  This short cut was added for Javascript.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return  
 */
nsScanner::nsScanner(nsString& anHTMLString, const nsString& aCharset, nsCharsetSource aSource)
{
  MOZ_COUNT_CTOR(nsScanner);

  PRUnichar* buffer = ToNewUnicode(anHTMLString);
  mTotalRead = anHTMLString.Length();
  mSlidingBuffer = nsnull;
  mCountRemaining = 0;
  AppendToBuffer(buffer, buffer+mTotalRead, buffer+mTotalRead);
  mSlidingBuffer->BeginReading(mCurrentPosition);
  mMarkPosition = mCurrentPosition;
  mIncremental=PR_FALSE;
  mOwnsStream=PR_FALSE;
  mInputStream=0;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
  mNewlinesSkipped=0;
}

/**
 *  Use this constructor if you want i/o to be based on strings 
 *  the scanner receives. If you pass a null filename, you
 *  can still provide data to the scanner via append.
 *
 *  @update  gess 5/12/98
 *  @param   aFilename --
 *  @return  
 */
nsScanner::nsScanner(nsString& aFilename,PRBool aCreateStream, const nsString& aCharset, nsCharsetSource aSource) : 
    mFilename(aFilename)
{
  MOZ_COUNT_CTOR(nsScanner);

  mSlidingBuffer = nsnull;
  mIncremental=PR_TRUE;
  mCountRemaining = 0;
  mTotalRead=0;
  mOwnsStream=aCreateStream;
  mInputStream=0;
  if(aCreateStream) {
		mInputStream = new nsInputFileStream(nsFileSpec(aFilename));
  } //if
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
  mNewlinesSkipped=0;
}

/**
 *  Use this constructor if you want i/o to be stream based.
 *
 *  @update  gess 5/12/98
 *  @param   aStream --
 *  @param   assumeOwnership --
 *  @param   aFilename --
 *  @return  
 */
nsScanner::nsScanner(nsString& aFilename,nsInputStream& aStream,const nsString& aCharset, nsCharsetSource aSource) :
    mFilename(aFilename)
{  
  MOZ_COUNT_CTOR(nsScanner);

  mSlidingBuffer = nsnull;
  mIncremental=PR_FALSE;
  mCountRemaining = 0;
  mTotalRead=0;
  mOwnsStream=PR_FALSE;
  mInputStream=&aStream;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
  mNewlinesSkipped=0;
}


nsresult nsScanner::SetDocumentCharset(const nsString& aCharset , nsCharsetSource aSource) {

  nsresult res = NS_OK;

  if( aSource < mCharsetSource) // priority is lower the the current one , just
    return res;

  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res));
  NS_ASSERTION( nsnull != calias, "cannot find charset alias");
  nsAutoString charsetName; charsetName.Assign(aCharset);
  if( NS_SUCCEEDED(res) && (nsnull != calias))
  {
    PRBool same = PR_FALSE;
    res = calias->Equals(aCharset, mCharset, &same);
    if(NS_SUCCEEDED(res) && same)
    {
      return NS_OK; // no difference, don't change it
    }
    // different, need to change it
    res = calias->GetPreferred(aCharset, charsetName);

    if(NS_FAILED(res) && (kCharsetUninitialized == mCharsetSource) )
    {
       // failed - unknown alias , fallback to ISO-8859-1
      charsetName.AssignWithConversion("ISO-8859-1");
    }
    mCharset = charsetName;
    mCharsetSource = aSource;

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &res);
    if(NS_SUCCEEDED(res) && (nsnull != ccm))
    {
      nsIUnicodeDecoder * decoder = nsnull;
      res = ccm->GetUnicodeDecoder(&mCharset, &decoder);
      if(NS_SUCCEEDED(res) && (nsnull != decoder))
      {
         NS_IF_RELEASE(mUnicodeDecoder);

         mUnicodeDecoder = decoder;
      }    
    }
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

  if (mSlidingBuffer) {
    delete mSlidingBuffer;
  }

  MOZ_COUNT_DTOR(nsScanner);

  if(mInputStream) {
    mInputStream->close();
    if(mOwnsStream)
      delete mInputStream;
  }
  mInputStream=0;
  NS_IF_RELEASE(mUnicodeDecoder);
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
  mCountRemaining += (Distance(mMarkPosition, mCurrentPosition));
  mCurrentPosition = mMarkPosition;
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
void nsScanner::Mark() {
  if (mSlidingBuffer) {
    mMarkPosition = mCurrentPosition;
    mSlidingBuffer->DiscardPrefix(mMarkPosition);
  }
}
 

/** 
 * Insert data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  harishd 01/12/99
 * @return  error code 
 */
PRBool nsScanner::UngetReadable(const nsAReadableString& aBuffer) {

  mSlidingBuffer->UngetReadable(aBuffer,mCurrentPosition);
  mSlidingBuffer->BeginReading(mCurrentPosition); // Insertion invalidated our iterators
  mSlidingBuffer->EndReading(mEndPosition);
  mTotalRead += aBuffer.Length();
      
  return PR_TRUE;
}

/** 
 * Append data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  gess4/3/98
 * @return  error code 
 */
nsresult nsScanner::Append(const nsAReadableString& aBuffer) {
  
  PRUnichar* buffer = ToNewUnicode(aBuffer);
  PRUint32 bufLen = aBuffer.Length();
  mTotalRead += bufLen;

  AppendToBuffer(buffer, buffer+bufLen, buffer+bufLen);

  return NS_OK;
}

/**
 *  
 *  
 *  @update  gess 5/21/98
 *  @param   
 *  @return  
 */
nsresult nsScanner::Append(const char* aBuffer, PRUint32 aLen){
  nsresult res=NS_OK;
  PRUnichar *unichars, *start;
  if(mUnicodeDecoder) {
    PRInt32 unicharBufLen = 0;
    mUnicodeDecoder->GetMaxLength(aBuffer, aLen, &unicharBufLen);
    start = unichars = (PRUnichar*)nsMemory::Alloc((unicharBufLen+1) * sizeof(PRUnichar));
    NS_ENSURE_TRUE(unichars,NS_ERROR_OUT_OF_MEMORY);
	  
    PRInt32 totalChars = 0;
    PRInt32 unicharLength = unicharBufLen;
    do {
      PRInt32 srcLength = aLen;
		  res = mUnicodeDecoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);

      totalChars += unicharLength;
      // Continuation of failure case
		  if(NS_FAILED(res)) {
        // if we failed, we consume one byte, replace it with U+FFFD
        // and try the conversion again.
        unichars[unicharLength++] = (PRUnichar)0xFFFD;
        unichars = unichars + unicharLength;
        unicharLength = unicharBufLen - (++totalChars);

			  mUnicodeDecoder->Reset();

        if(((PRUint32) (srcLength + 1)) > aLen) {
          srcLength = aLen;
        }
        else {
          srcLength++;
        }

        aBuffer += srcLength;
        aLen -= srcLength;
		  }
	  } while (NS_FAILED(res) && (aLen > 0));

    AppendToBuffer(start, 
                   start+totalChars, 
                   start+unicharBufLen);
    mTotalRead += totalChars;

    // Don't propagate return code of unicode decoder
    // since it doesn't reflect on our success or failure
    // - Ref. bug 87110
    res = NS_OK; 
  }
  else {
    nsDependentCString str(aBuffer, aLen);
    unichars = ToNewUnicode(str);
    AppendToBuffer(unichars, unichars+aLen, unichars+aLen);
    mTotalRead+=aLen;
  }

  return res;
}


/** 
 * Grab data from underlying stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
nsresult nsScanner::FillBuffer(void) {
  nsresult result=NS_OK;

  if(!mInputStream) {
#if 0
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    if(0==mTotalRead) {
      mBuffer.Append((const char*)kBadHTMLText);
      mBuffer.Append(mFilename);
      mTotalRead+=mBuffer.Length();
    }
    else 
#endif
    result=kEOF;
  }
  else {
    PRInt32 numread=0;
    char* buf = new char[kBufsize+1];
    buf[kBufsize]=0;

    if(mInputStream) {
    	numread = mInputStream->read(buf, kBufsize);
      if (0 == numread) {
        delete [] buf;
        return kEOF;
      }
    }

    if((0<numread) && (0==result)) {
      nsDependentCString str(buf, numread);
      PRUnichar* unichars = ToNewUnicode(str);
      AppendToBuffer(unichars, unichars+numread, unichars+kBufsize+1);
    }
    delete [] buf;
    mTotalRead+=numread;
  }

  return result;
}

/**
 *  determine if the scanner has reached EOF
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  0=!eof 1=eof 
 */
nsresult nsScanner::Eof() {
  nsresult theError=NS_OK;
  
  if (!mSlidingBuffer) {
    return kEOF;
  }

  theError=FillBuffer();  

  if(NS_OK==theError) {
    if (0==(PRUint32)mSlidingBuffer->Length()) {
      return kEOF;
    }
  }

  return theError;
}

/**
 *  retrieve next char from scanners internal input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code reflecting read status
 */
nsresult nsScanner::GetChar(PRUnichar& aChar) {
  nsresult result=NS_OK;
  aChar=0;  

  if (!mSlidingBuffer) {
    return kEOF;
  }

  if (mCurrentPosition == mEndPosition) {
    result=Eof();
  }

  if(NS_OK == result){
    aChar=*mCurrentPosition++;
    mCountRemaining--;
  }
  return result;
}


/**
 *  peek ahead to consume next char from scanner's internal
 *  input buffer
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsresult nsScanner::Peek(PRUnichar& aChar, PRUint32 aOffset) {
  nsresult result=NS_OK;
  aChar=0;  
  
  if (!mSlidingBuffer) {
    return kEOF;
  }

  if (mCurrentPosition == mEndPosition) {
    result=Eof();
  }

  if(NS_OK == result){
    if (aOffset) {
      while ((NS_OK == result) && (mCountRemaining <= aOffset)) {
        result = Eof();
      }

      if (NS_OK == result) {
        nsReadingIterator<PRUnichar> pos = mCurrentPosition;
        pos.advance(aOffset);
        aChar=*pos;
      }
    }
    else {
      aChar=*mCurrentPosition;
    }
  }

  return result;
}

nsresult nsScanner::Peek(nsAWritableString& aStr, PRInt32 aNumChars)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  if (mCurrentPosition == mEndPosition) {
    return Eof();
  }    
  
  nsReadingIterator<PRUnichar> start, end;

  start = mCurrentPosition;

  if (mCountRemaining < PRUint32(aNumChars)) {
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
nsresult nsScanner::SkipWhitespace(void) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsReadingIterator<PRUnichar> current;
  PRBool            found;
  PRBool            skipped = PR_FALSE;

  mNewlinesSkipped = 0;
  current = mCurrentPosition;

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  
  if (result == kEOF) {
    return Eof();
  }
  
  while (current != mEndPosition) {
    switch(theChar) {
      case '\n': mNewlinesSkipped++;
      case ' ' :
      case '\r':
      case '\b':
      case '\t':
        found=PR_TRUE;
        break;
      default:
        found=PR_FALSE;
        break;
    }
    if(!found) {
      break;
    }
    ++current;
    theChar = *current;
    skipped = PR_TRUE;
  }

  if (skipped) {
    SetPosition(current);
    if (current == mEndPosition) {
      return Eof();
    }
  }

  return NS_OK;
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

/**
 *  Skip over chars as long as they're in aSkipSet
 *  
 *  @update  gess 3/25/98
 *  @param   aSkipSet is an ordered string.
 *  @return  error code
 */
nsresult nsScanner::SkipOver(nsString& aSkipSet){

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=Peek(theChar);
    if(NS_OK == result) {
      PRInt32 pos=aSkipSet.FindChar(theChar);
      if(kNotFound==pos) {
        break;
      }
      GetChar(theChar);
    } 
    else break;
  } //while
  return result;

}


/**
 *  Skip over chars until they're in aValidSet
 *  
 *  @update  gess 3/25/98
 *  @param   aValid set is an ordered string that 
 *           contains chars you're looking for
 *  @return  error code
 */
nsresult nsScanner::SkipTo(nsString& aValidSet){
  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar ch=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=Peek(ch);
    if(NS_OK == result) {
      PRInt32 pos=aValidSet.FindChar(ch);
      if(kNotFound!=pos) {
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
  PRInt32 pos=aString.FindChar(0);
  if(kNotFound<pos) {
    if(aString.Length()-1!=pos) {
    }
  }
}

void DoErrTest(nsCString& aString) {
  PRInt32 pos=aString.FindChar(0);
  if(kNotFound<pos) {
    if(aString.Length()-1!=pos) {
    }
  }
}
#endif

/**
 *  Skip over chars as long as they're in aValidSet
 *  
 *  @update  gess 3/25/98
 *  @param   aValidSet is an ordered string containing the 
 *           characters you want to skip
 *  @return  error code
 */
nsresult nsScanner::SkipPast(nsString& aValidSet){
  NS_NOTYETIMPLEMENTED("Error: SkipPast not yet implemented.");
  return NS_OK;
}

/**
 *  Consume characters until you did not find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString - receives new data from stream
 *  @param   aIgnore - If set ignores ':','-','_'
 *  @return  error code
 */
nsresult nsScanner::GetIdentifier(nsString& aString,PRBool allowPunct) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> current, end;
  PRBool            found=PR_FALSE;  
  
  current = mCurrentPosition;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      found=PR_FALSE;
      switch(theChar) {
        case ':':
        case '_':
        case '-':
          found=allowPunct;
          break;
        default:
          if(('a'<=theChar) && (theChar<='z'))
            found=PR_TRUE;
          else if(('A'<=theChar) && (theChar<='Z'))
            found=PR_TRUE;
          else if(('0'<=theChar) && (theChar<='9'))
            found=PR_TRUE;
          break;
      }

      if(!found) {
        // If we the current character isn't a valid character for
        // the identifier, we're done. Copy the results into
        // the string passed in.
        CopyUnicodeTo(mCurrentPosition, current, aString);
        break;
      }
    }
    ++current;
  }

  SetPosition(current);  
  if (current == end) {
    result = Eof();
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume characters until you did not find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString - receives new data from stream
 *  @param   allowPunct - If set ignores ':','-','_'
 *  @return  error code
 */
nsresult nsScanner::ReadIdentifier(nsString& aString,PRBool allowPunct) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;
  PRBool            found=PR_FALSE;  

  origin = mCurrentPosition;
  current = mCurrentPosition;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      found=PR_FALSE;
      switch(theChar) {
        case ':':
        case '_':
        case '-':
          found=allowPunct;
          break;
        default:
          if(('a'<=theChar) && (theChar<='z'))
            found=PR_TRUE;
          else if(('A'<=theChar) && (theChar<='Z'))
            found=PR_TRUE;
          else if(('0'<=theChar) && (theChar<='9'))
            found=PR_TRUE;
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
    return Eof();
  }

  //DoErrTest(aString);

  return result;
}

nsresult nsScanner::ReadIdentifier(nsReadingIterator<PRUnichar>& aStart,
                                   nsReadingIterator<PRUnichar>& aEnd,
                                   PRBool allowPunct) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;
  PRBool            found=PR_FALSE;  

  origin = mCurrentPosition;
  current = mCurrentPosition;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      found=PR_FALSE;
      switch(theChar) {
        case ':':
        case '_':
        case '-':
          found=allowPunct;
          break;
        default:
          if(('a'<=theChar) && (theChar<='z'))
            found=PR_TRUE;
          else if(('A'<=theChar) && (theChar<='Z'))
            found=PR_TRUE;
          else if(('0'<=theChar) && (theChar<='9'))
            found=PR_TRUE;
          break;
      }

      if(!found) {
        aStart = mCurrentPosition;
        aEnd = current;
        break;
      }
    }
    ++current;
  }
  
  SetPosition(current);
  if (current == end) {
    aStart = origin;
    aEnd = current;
    return Eof();
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
nsresult nsScanner::ReadNumber(nsString& aString,PRInt32 aBase) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  NS_ASSERTION(aBase == 10 || aBase == 16,"base value not supported");

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  PRBool done = PR_FALSE;
  while(current != end) {
    theChar=*current;
    if(theChar) {
      done = (theChar < '0' || theChar > '9') && 
             ((aBase == 16)? (theChar < 'A' || theChar > 'F') &&
                             (theChar < 'a' || theChar > 'f')
                             :PR_TRUE);
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
    return Eof();
  }

  //DoErrTest(aString);

  return result;
}

nsresult nsScanner::ReadNumber(nsReadingIterator<PRUnichar>& aStart,
                               nsReadingIterator<PRUnichar>& aEnd,
                               PRInt32 aBase) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  NS_ASSERTION(aBase == 10 || aBase == 16,"base value not supported");

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  PRBool done = PR_FALSE;
  while(current != end) {
    theChar=*current;
    if(theChar) {
      done = (theChar < '0' || theChar > '9') && 
             ((aBase == 16)? (theChar < 'A' || theChar > 'F') &&
                             (theChar < 'a' || theChar > 'f')
                             :PR_TRUE);
      if(done) {
        aStart = origin;
        aEnd = current;
        break;
      }
    }
    ++current;
  }

  SetPosition(current);
  if (current == end) {
    aStart = origin;
    aEnd = current;
    return Eof();
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
nsresult nsScanner::ReadWhitespace(nsString& aString) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;
  PRBool            found=PR_FALSE;  

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      switch(theChar) {
        case ' ':
        case '\b':
        case '\t':
        case kLF:
        case kCR:
          found=PR_TRUE;
          break;
        default:
          found=PR_FALSE;
          break;
      }
      if(!found) {
        AppendUnicodeTo(origin, current, aString);
        break;
      }
    }
    ++current;
  }

  SetPosition(current);
  if (current == end) {
    AppendUnicodeTo(origin, current, aString);
    return Eof();
  }

  //DoErrTest(aString);

  return result;
}

nsresult nsScanner::ReadWhitespace(nsReadingIterator<PRUnichar>& aStart, 
                                   nsReadingIterator<PRUnichar>& aEnd) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;
  PRBool            found=PR_FALSE;  

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      switch(theChar) {
        case ' ':
        case '\b':
        case '\t':
        case kLF:
        case kCR:
          found=PR_TRUE;
          break;
        default:
          found=PR_FALSE;
          break;
      }
      if(!found) {
        aStart = origin;
        aEnd = current;
        break;
      }

      ++current;
    }
  }

  SetPosition(current);
  if (current == end) {
    aStart = origin;
    aEnd = current;
    return Eof();
  }

  //DoErrTest(aString);

  return result;
}

/**
 *  Consume chars as long as they are <i>in</i> the 
 *  given validSet of input chars.
 *  
 *  @update  gess 3/25/98
 *  @param   aString will contain the result of this method
 *  @param   aValidSet is an ordered string that contains the
 *           valid characters
 *  @return  error code
 */
nsresult nsScanner::ReadWhile(nsString& aString,
                             nsString& aValidSet,
                             PRBool addTerminal){

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsReadingIterator<PRUnichar> origin, current, end;

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      PRInt32 pos=aValidSet.FindChar(theChar);
      if(kNotFound==pos) {
        if(addTerminal)
          ++current;
        AppendUnicodeTo(origin, current, aString);
        break;
      }
    }
    ++current;
  }

  SetPosition(current);
  if (current == end) {
    AppendUnicodeTo(origin, current, aString);
    return Eof();
  }

  //DoErrTest(aString);

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
nsresult nsScanner::ReadUntil(nsAWritableString& aString,
                              const nsReadEndCondition& aEndCondition,
                              PRBool addTerminal)
{  
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsReadingIterator<PRUnichar> origin, current;
  const PRUnichar* setstart = aEndCondition.mChars;
  const PRUnichar* setcurrent;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);

  if (result == kEOF) {
    return Eof();
  }
  
  while (current != mEndPosition) {
    // Filter out completely wrong characters
    // Check if all bits are in the required area
    if(!(theChar & aEndCondition.mFilter)) {
      // They were. Do a thorough check.

      setcurrent = setstart;
      while (*setcurrent) {
        if (*setcurrent == theChar) {
          goto found;
        }
        ++setcurrent;
      }
    }
    
    ++current;
    theChar = *current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  SetPosition(current);
  AppendUnicodeTo(origin, current, aString);
  return Eof();

found:
  if(addTerminal)
    ++current;
  AppendUnicodeTo(origin, current, aString);
  SetPosition(current);

  //DoErrTest(aString);

  return NS_OK;
}

nsresult nsScanner::ReadUntil(nsReadingIterator<PRUnichar>& aStart, 
                              nsReadingIterator<PRUnichar>& aEnd,
                              const nsReadEndCondition &aEndCondition,
                              PRBool addTerminal)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsReadingIterator<PRUnichar> origin, current;
  const PRUnichar* setstart = aEndCondition.mChars;
  const PRUnichar* setcurrent;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  
  if (result == kEOF) {
    aStart = aEnd = current;
    return Eof();
  }
  
  while (current != mEndPosition) {
    // Filter out completely wrong characters
    // Check if all bits are in the required area
    if(!(theChar & aEndCondition.mFilter)) {
      // They were. Do a thorough check.
      setcurrent = setstart;
      while (*setcurrent) {
        if (*setcurrent == theChar) {
          goto found;
        }
      ++setcurrent;
      }
    }
    
    ++current;
    theChar = *current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  SetPosition(current);
  aStart = origin;
  aEnd = current;
  return Eof();

 found:
  if(addTerminal)
    ++current;
  aStart = origin;
  aEnd = current;
  SetPosition(current);

  return NS_OK; 
}

/**
 *  Consumes chars until you see the given terminalChar
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsAWritableString& aString,
                              PRUnichar aTerminalChar,
                              PRBool addTerminal)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsReadingIterator<PRUnichar> origin, current;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar theChar;
  nsresult  result=Peek(theChar);
  
  while (current != mEndPosition) {
    if (aTerminalChar == theChar) {
      if(addTerminal)
        ++current;
      AppendUnicodeTo(origin, current, aString);
      SetPosition(current);
      return NS_OK;
    }
    ++current;
    theChar = *current;
  }

  // If we are here, we didn't find any terminator in the string and
  // current = mEndPosition
  AppendUnicodeTo(origin, current, aString);
  SetPosition(current);
  return Eof();

}

void nsScanner::BindSubstring(nsSlidingSubstring& aSubstring, const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd)
{
  aSubstring.Rebind(*mSlidingBuffer, aStart, aEnd);
}

void nsScanner::CurrentPosition(nsReadingIterator<PRUnichar>& aPosition)
{
  aPosition = mCurrentPosition;
}

void nsScanner::EndReading(nsReadingIterator<PRUnichar>& aPosition)
{
  aPosition = mEndPosition;
}
 
void nsScanner::SetPosition(nsReadingIterator<PRUnichar>& aPosition, PRBool aTerminate, PRBool aReverse)
{
  if (mSlidingBuffer) {
    if (aReverse) {
      mCountRemaining += (Distance(aPosition, mCurrentPosition));
    }
    else {
      mCountRemaining -= (Distance(mCurrentPosition, aPosition));
    }
    mCurrentPosition = aPosition;
    if (aTerminate && (mCurrentPosition == mEndPosition)) {
      mMarkPosition = mCurrentPosition;
      mSlidingBuffer->DiscardPrefix(mCurrentPosition);
    }
  }
}

void nsScanner::ReplaceCharacter(nsReadingIterator<PRUnichar>& aPosition,
                                 PRUnichar aChar)
{
  if (mSlidingBuffer) {
    mSlidingBuffer->ReplaceCharacter(aPosition, aChar);
  }
}

void nsScanner::AppendToBuffer(PRUnichar* aStorageStart, 
                               PRUnichar* aDataEnd, 
                               PRUnichar* aStorageEnd)
{
  if (!mSlidingBuffer) {
    mSlidingBuffer = new nsScannerString(aStorageStart, aDataEnd, aStorageEnd);
    mSlidingBuffer->BeginReading(mCurrentPosition);
    mMarkPosition = mCurrentPosition;
    mSlidingBuffer->EndReading(mEndPosition);
    mCountRemaining = (aDataEnd - aStorageStart);
  }
  else {
    mSlidingBuffer->AppendBuffer(aStorageStart, aDataEnd, aStorageEnd);
    if (mCurrentPosition == mEndPosition) {
      mSlidingBuffer->BeginReading(mCurrentPosition);
    }
    mSlidingBuffer->EndReading(mEndPosition);
    mCountRemaining += (aDataEnd - aStorageStart);
  }
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
  nsReadingIterator<PRUnichar> start, end;
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



