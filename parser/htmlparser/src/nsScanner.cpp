/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//#define __INCREMENTAL 1

#include "nsScanner.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsReadableUtils.h"
#include "nsIInputStream.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsUTF8Utils.h" // for LossyConvertEncoding
#include "nsCRT.h"

static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

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

static const char kBadHTMLText[] ="<H3>Oops...</H3>You just tried to read a non-existent document: <BR>";
static const char kUnorderedStringError[] = "String argument must be ordered. Don't you read API's?";

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
nsScanner::nsScanner(const nsAString& anHTMLString, const nsACString& aCharset, PRInt32 aSource)
{
  MOZ_COUNT_CTOR(nsScanner);

  mTotalRead = anHTMLString.Length();
  mSlidingBuffer = nsnull;
  mCountRemaining = 0;
  mFirstNonWhitespacePosition = -1;
  AppendToBuffer(anHTMLString);
  mSlidingBuffer->BeginReading(mCurrentPosition);
  mMarkPosition = mCurrentPosition;
  mIncremental = PR_FALSE;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
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
nsScanner::nsScanner(nsString& aFilename,PRBool aCreateStream, const nsACString& aCharset, PRInt32 aSource) : 
    mFilename(aFilename)
{
  MOZ_COUNT_CTOR(nsScanner);

  mSlidingBuffer = nsnull;

  // XXX This is a big hack.  We need to initialize the iterators to something.
  // What matters is that mCurrentPosition == mEndPosition, so that our methods
  // believe that we are at EOF (see bug 182067).  We null out mCurrentPosition
  // so that we have some hope of catching null pointer dereferences associated
  // with this hack. --darin
  memset(&mCurrentPosition, 0, sizeof(mCurrentPosition));
  mMarkPosition = mCurrentPosition;
  mEndPosition = mCurrentPosition;

  mIncremental = PR_TRUE;
  mFirstNonWhitespacePosition = -1;
  mCountRemaining = 0;
  mTotalRead=0;

  if(aCreateStream) {
    nsCOMPtr<nsILocalFile> file;
    nsCOMPtr<nsIInputStream> fileStream;
    
    NS_NewLocalFile(aFilename, PR_TRUE, getter_AddRefs(file));
    if (file)
      NS_NewLocalFileInputStream(getter_AddRefs(mInputStream), file);

  } //if
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
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
nsScanner::nsScanner(const nsAString& aFilename,nsIInputStream* aStream,const nsACString& aCharset, PRInt32 aSource) :
    mFilename(aFilename)
{  
  MOZ_COUNT_CTOR(nsScanner);

  mSlidingBuffer = nsnull;

  // XXX This is a big hack.  We need to initialize the iterators to something.
  // What matters is that mCurrentPosition == mEndPosition, so that our methods
  // believe that we are at EOF (see bug 182067).  We null out mCurrentPosition
  // so that we have some hope of catching null pointer dereferences associated
  // with this hack. --darin
  memset(&mCurrentPosition, 0, sizeof(mCurrentPosition));
  mMarkPosition = mCurrentPosition;
  mEndPosition = mCurrentPosition;

  mIncremental = PR_FALSE;
  mFirstNonWhitespacePosition = -1;
  mCountRemaining = 0;
  mTotalRead=0;
  mInputStream=aStream;
  mUnicodeDecoder = 0;
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
}


nsresult nsScanner::SetDocumentCharset(const nsACString& aCharset , PRInt32 aSource) {

  nsresult res = NS_OK;

  if( aSource < mCharsetSource) // priority is lower the the current one , just
    return res;

  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res));
  NS_ASSERTION( nsnull != calias, "cannot find charset alias");
  if( NS_SUCCEEDED(res) && (nsnull != calias))
  {
    PRBool same = PR_FALSE;
    res = calias->Equals(aCharset, mCharset, &same);
    if(NS_SUCCEEDED(res) && same)
    {
      return NS_OK; // no difference, don't change it
    }
    // different, need to change it
    nsCAutoString charsetName;
    res = calias->GetPreferred(aCharset, charsetName);

    if(NS_FAILED(res) && (kCharsetUninitialized == mCharsetSource) )
    {
       // failed - unknown alias , fallback to ISO-8859-1
      charsetName.AssignLiteral("ISO-8859-1");
    }
    mCharset = charsetName;
    mCharsetSource = aSource;

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &res);
    if(NS_SUCCEEDED(res) && (nsnull != ccm))
    {
      nsIUnicodeDecoder * decoder = nsnull;
      res = ccm->GetUnicodeDecoderRaw(mCharset.get(), &decoder);
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
    mInputStream->Close();
    mInputStream = 0;
  }

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
void nsScanner::Mark() {
  if (mSlidingBuffer) {
    mSlidingBuffer->DiscardPrefix(mCurrentPosition);
    mSlidingBuffer->BeginReading(mCurrentPosition);
    mMarkPosition = mCurrentPosition;
  }
}
 

/** 
 * Insert data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  harishd 01/12/99
 * @return  error code 
 */
PRBool nsScanner::UngetReadable(const nsAString& aBuffer) {
  if (!mSlidingBuffer) {
    return PR_FALSE;
  }

  mSlidingBuffer->UngetReadable(aBuffer,mCurrentPosition);
  mSlidingBuffer->BeginReading(mCurrentPosition); // Insertion invalidated our iterators
  mSlidingBuffer->EndReading(mEndPosition);
 
  PRUint32 length = aBuffer.Length();
  mCountRemaining += length; // Ref. bug 117441
  mTotalRead += length;
  return PR_TRUE;
}

/** 
 * Append data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  gess4/3/98
 * @return  error code 
 */
nsresult nsScanner::Append(const nsAString& aBuffer) {
  mTotalRead += aBuffer.Length();
  AppendToBuffer(aBuffer);
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
    nsScannerString::Buffer* buffer = nsScannerString::AllocBuffer(unicharBufLen + 1);
    NS_ENSURE_TRUE(buffer,NS_ERROR_OUT_OF_MEMORY);
    start = unichars = buffer->DataStart();
	  
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
          ++srcLength;
        }

        aBuffer += srcLength;
        aLen -= srcLength;
		  }
	  } while (NS_FAILED(res) && (aLen > 0));

    buffer->SetDataLength(totalChars);
    AppendToBuffer(buffer);
    mTotalRead += totalChars;

    // Don't propagate return code of unicode decoder
    // since it doesn't reflect on our success or failure
    // - Ref. bug 87110
    res = NS_OK; 
  }
  else {
    AppendASCIItoBuffer(aBuffer, aLen);
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
    PRUint32 numread=0;
    char buf[kBufsize+1];
    buf[kBufsize]=0;

    // XXX use ReadSegments to avoid extra buffer copy? --darin

    result = mInputStream->Read(buf, kBufsize, &numread);
    if (0 == numread) {
      return kEOF;
    }

    if((0<numread) && (0==result)) {
      AppendASCIItoBuffer(buf, numread);
    }
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
    --mCountRemaining;
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
        nsScannerIterator pos = mCurrentPosition;
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

nsresult nsScanner::Peek(nsAString& aStr, PRInt32 aNumChars, PRInt32 aOffset)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  if (mCurrentPosition == mEndPosition) {
    return Eof();
  }    
  
  nsScannerIterator start, end;

  start = mCurrentPosition;

  if ((PRInt32)mCountRemaining <= aOffset) {
    return kEOF;
  }

  if (aOffset > 0) {
    start.advance(aOffset);
  }

  if (mCountRemaining < PRUint32(aNumChars + aOffset)) {
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
nsresult nsScanner::SkipWhitespace(PRInt32& aNewlinesSkipped) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar = 0;
  nsresult  result = Peek(theChar);
  
  if (result == kEOF) {
    // XXX why wouldn't Eof() return kEOF?? --darin
    return Eof();
  }
  
  nsScannerIterator current = mCurrentPosition;
  PRBool    done = PR_FALSE;
  PRBool    skipped = PR_FALSE;
  
  while (!done && current != mEndPosition) {
    switch(theChar) {
      case '\n':
      case '\r': ++aNewlinesSkipped;
      case ' ' :
      case '\b':
      case '\t':
        {
          skipped = PR_TRUE;
          PRUnichar thePrevChar = theChar;
          theChar = (++current != mEndPosition) ? *current : '\0';
          if ((thePrevChar == '\r' && theChar == '\n') ||
              (thePrevChar == '\n' && theChar == '\r')) {
            theChar = (++current != mEndPosition) ? *current : '\0'; // CRLF == LFCR => LF
          }
        }
        break;
      default:
        done = PR_TRUE;
        break;
    }
  }

  if (skipped) {
    SetPosition(current);
    if (current == mEndPosition) {
      result = Eof();
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
 *  Consume characters until you run into space, a '<', a '>', or a '/'.
 *  
 *  @param   aString - receives new data from stream
 *  @return  error code
 */
nsresult nsScanner::ReadTagIdentifier(nsString& aString) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar         theChar=0;
  nsresult          result=Peek(theChar);
  nsScannerIterator current, end;
  PRBool            found=PR_FALSE;  
  
  current = mCurrentPosition;
  end = mEndPosition;

  while(current != end) {
    theChar=*current;

    found = PR_TRUE;
    switch(theChar) {
      case '\n':
      case '\r':
      case ' ' :
      case '\b':
      case '\t':
      case '\v':
      case '\f':
      case '<':
      case '>':
      case '/':
      case '\0':
        found = PR_FALSE;
        break;
      default:
        break;
    }

    if(!found) {
      // If the current character isn't a valid character for
      // the identifier, we're done. Append the results to
      // the string passed in.
      AppendUnicodeTo(mCurrentPosition, current, aString);
      break;
    }
    ++current;
  }

  // Drop NULs on the floor since nobody really likes them.
  while (current != end && !*current) {
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
  PRBool            found=PR_FALSE;  

  origin = mCurrentPosition;
  current = mCurrentPosition;
  end = mEndPosition;

  while(current != end) {
 
    theChar=*current;
    if(theChar) {
      found=PR_FALSE;
      switch(theChar) {
        case '_':
        case '-':
        case '.':
          // Don't allow ':' in entity names.  See bug 23791
          found = PR_TRUE;
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
  nsScannerIterator origin, current, end;

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

/**
 *  Consume characters until you find the terminal char
 *  
 *  @update  gess 3/25/98
 *  @param   aString receives new data from stream
 *  @param   addTerminal tells us whether to append terminal to aString
 *  @return  error code
 */
nsresult nsScanner::ReadWhitespace(nsString& aString,
                                   PRInt32& aNewlinesSkipped) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar = 0;
  nsresult  result = Peek(theChar);
  
  if (result == kEOF) {
    return Eof();
  }
  
  nsScannerIterator origin, current, end;
  PRBool done = PR_FALSE;  

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

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
          } else if (thePrevChar == '\r') {
            // Lone CR becomes CRLF; callers should know to remove extra CRs
            AppendUnicodeTo(origin, current, aString);
            aString.Append(PRUnichar('\n'));
            origin = current;
          }
        }
        break;
      case ' ' :
      case '\b':
      case '\t':
        theChar = (++current != end) ? *current : '\0';
        break;
      default:
        done = PR_TRUE;
        AppendUnicodeTo(origin, current, aString);
        break;
    }
  }

  SetPosition(current);
  if (current == end) {
    AppendUnicodeTo(origin, current, aString);
    result = Eof();
  }

  return result;
}

//XXXbz callers of this have to manage their lone '\r' themselves if they want
//it to work.  Good thing they're all in view-source and it deals.
nsresult nsScanner::ReadWhitespace(nsScannerIterator& aStart, 
                                   nsScannerIterator& aEnd,
                                   PRInt32& aNewlinesSkipped) {

  if (!mSlidingBuffer) {
    return kEOF;
  }

  PRUnichar theChar = 0;
  nsresult  result = Peek(theChar);
  
  if (result == kEOF) {
    return Eof();
  }
  
  nsScannerIterator origin, current, end;
  PRBool done = PR_FALSE;  

  origin = mCurrentPosition;
  current = origin;
  end = mEndPosition;

  while(!done && current != end) {
    switch(theChar) {
      case '\n':
      case '\r': ++aNewlinesSkipped;
      case ' ' :
      case '\b':
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
        done = PR_TRUE;
        aStart = origin;
        aEnd = current;
        break;
    }
  }

  SetPosition(current);
  if (current == end) {
    aStart = origin;
    aEnd = current;
    result = Eof();
  }

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
  nsScannerIterator origin, current, end;

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
nsresult nsScanner::ReadUntil(nsAString& aString,
                              const nsReadEndCondition& aEndCondition,
                              PRBool addTerminal)
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

  if (result == kEOF) {
    return Eof();
  }
  
  while (current != mEndPosition) {
    theChar = *current;

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

nsresult nsScanner::ReadUntil(nsScannerIterator& aStart, 
                              nsScannerIterator& aEnd,
                              const nsReadEndCondition &aEndCondition,
                              PRBool addTerminal)
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
nsresult nsScanner::ReadUntil(nsAString& aString,
                              PRUnichar aTerminalChar,
                              PRBool addTerminal)
{
  if (!mSlidingBuffer) {
    return kEOF;
  }

  nsScannerIterator origin, current;

  origin = mCurrentPosition;
  current = origin;

  PRUnichar theChar;
  Peek(theChar);
  
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
 
void nsScanner::SetPosition(nsScannerIterator& aPosition, PRBool aTerminate, PRBool aReverse)
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

void nsScanner::ReplaceCharacter(nsScannerIterator& aPosition,
                                 PRUnichar aChar)
{
  if (mSlidingBuffer) {
    mSlidingBuffer->ReplaceCharacter(aPosition, aChar);
  }
}

void nsScanner::AppendToBuffer(nsScannerString::Buffer* aBuf)
{
  if (!mSlidingBuffer) {
    mSlidingBuffer = new nsScannerString(aBuf);
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
}

void nsScanner::AppendASCIItoBuffer(const char* aData, PRUint32 aLen)
{
  nsScannerString::Buffer* buf = nsScannerString::AllocBuffer(aLen);
  if (buf)
  {
    LossyConvertEncoding<char, PRUnichar> converter(buf->DataStart());
    converter.write(aData, aLen);
    converter.write_terminator();
    AppendToBuffer(buf);
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



