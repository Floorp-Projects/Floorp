/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//#define __INCREMENTAL 1

#define NS_IMPL_IDS
#include "nsScanner.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsFileSpec.h"


const char* kBadHTMLText="<H3>Oops...</H3>You just tried to read a non-existent document: <BR>";
const char* kUnorderedStringError = "String argument must be ordered. Don't you read API's?";

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
nsScanner::nsScanner(nsString& anHTMLString, const nsString& aCharset, nsCharsetSource aSource) : 
  mBuffer(anHTMLString), mFilename("")
{
  mTotalRead=mBuffer.Length();
  mIncremental=PR_TRUE;
  mOwnsStream=PR_FALSE;
  mOffset=0;
  mMarkPos=-1;
  mInputStream=0;
  mUnicodeDecoder = 0;
  mCharset = "";
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
nsScanner::nsScanner(nsString& aFilename,PRBool aCreateStream, const nsString& aCharset, nsCharsetSource aSource) : 
    mBuffer(""), mFilename(aFilename)
{
  mIncremental=PR_TRUE;
  mOffset=0;
  mMarkPos=-1;
  mTotalRead=0;
  mOwnsStream=aCreateStream;
  mInputStream=0;
  if(aCreateStream) {
		mInputStream = new nsInputFileStream(nsFileSpec(aFilename));
  } //if
  mUnicodeDecoder = 0;
  mCharset = "";
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
nsScanner::nsScanner(nsString& aFilename,nsInputStream& aStream,const nsString& aCharset, nsCharsetSource aSource, PRBool assumeOwnership) :
    mBuffer(""), mFilename(aFilename) 
{    
  mIncremental=PR_TRUE;
  mOffset=0;
  mMarkPos=-1;
  mTotalRead=0;
  mOwnsStream=assumeOwnership;
  mInputStream=&aStream;
  mUnicodeDecoder = 0;
  mCharset = "";
  mCharsetSource = kCharsetUninitialized;
  SetDocumentCharset(aCharset, aSource);
}

nsresult nsScanner::SetDocumentCharset(const nsString& aCharset , nsCharsetSource aSource)
{

  nsresult res = NS_OK;

  if( aSource < mCharsetSource) // priority is lower the the current one , just
    return res;

  nsICharsetAlias* calias = nsnull;
  res = nsServiceManager::GetService(kCharsetAliasCID,
                                       kICharsetAliasIID,
                                       (nsISupports**)&calias);

  NS_ASSERTION( nsnull != calias, "cannot find charet alias");
  nsAutoString charsetName = aCharset;
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
    nsServiceManager::ReleaseService(kCharsetAliasCID, calias);

    if(NS_FAILED(res) && (kCharsetUninitialized == mCharsetSource) )
    {
       // failed - unknown alias , fallback to ISO-8859-1
      charsetName = "ISO-8859-1";
    }
    mCharset = charsetName;
    mCharsetSource = aSource;

    nsICharsetConverterManager * ccm = nsnull;
    res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                       kICharsetConverterManagerIID, 
                                       (nsISupports**)&ccm);
    if(NS_SUCCEEDED(res) && (nsnull != ccm))
    {
      nsIUnicodeDecoder * decoder = nsnull;
      res = ccm->GetUnicodeDecoder(&mCharset, &decoder);
      if(NS_SUCCEEDED(res) && (nsnull != decoder))
      {
         NS_IF_RELEASE(mUnicodeDecoder);

         mUnicodeDecoder = decoder;
      }    
      nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
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
PRUint32 nsScanner::RewindToMark(void){
  mOffset=mMarkPos;
  return mOffset;
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
PRUint32 nsScanner::Mark(void){
  if((mOffset>0) && (mOffset>eBufferSizeThreshold)) {
    mBuffer.Cut(0,mOffset);   //delete chars up to mark position
    mOffset=0;
  }
  mMarkPos=mOffset;
  return 0;
}
 

/** 
 * Append data to our underlying input buffer as
 * if it were read from an input stream.
 *
 * @update  gess4/3/98
 * @return  error code 
 */
PRBool nsScanner::Append(nsString& aBuffer) {
  mBuffer.Append(aBuffer);
  mTotalRead+=aBuffer.Length();
  return PR_TRUE;
}

/**
 *  
 *  
 *  @update  gess 5/21/98
 *  @param   
 *  @return  
 */
PRBool nsScanner::Append(const char* aBuffer, PRUint32 aLen){
 
  if(mUnicodeDecoder) {
   PRInt32 unicharLength = 0;
    PRInt32 srcLength = aLen;
    mUnicodeDecoder->Length(aBuffer, 0, aLen, &unicharLength);
    PRUnichar *unichars = new PRUnichar [ unicharLength ];
    nsresult res = mUnicodeDecoder->Convert(unichars, 0, &unicharLength,aBuffer, 0, &srcLength );
    mBuffer.Append(unichars, unicharLength);
    delete[] unichars;
    mTotalRead += unicharLength;
  }
  else {
    
    mBuffer.Append(aBuffer,aLen);
    mTotalRead+=aLen;
  }

  return PR_TRUE;
}

PRBool nsScanner::Append(const PRUnichar* aBuffer, PRUint32 aLen){
  mBuffer.Append(aBuffer,aLen);
  mTotalRead+=aLen;
  return PR_TRUE;
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
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    if(0==mTotalRead) {
      mBuffer.Append((const char*)kBadHTMLText);
      mBuffer.Append(mFilename);
    }
    else result=kEOF;
  }
  else {
    PRInt32 numread=0;
    char buf[kBufsize+1];
    buf[kBufsize]=0;

    if(mInputStream) {
    	numread = mInputStream->read(buf, kBufsize);
      if (0 == numread) {
        return kEOF;
      }
    }
    mOffset=mBuffer.Length();
    if((0<numread) && (0==result))
      mBuffer.Append((const char*)buf,numread);
    mTotalRead+=mBuffer.Length();
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

  if(mOffset>=(PRUint32)mBuffer.Length()) {
    theError=FillBuffer();  
  }
  
  if(NS_OK==theError) {
    if (0==(PRUint32)mBuffer.Length()) {
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
  if(mOffset>=(PRUint32)mBuffer.Length()) 
    result=Eof();

  if(NS_OK == result) {
    aChar=mBuffer.CharAt(mOffset++);
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
nsresult nsScanner::Peek(PRUnichar& aChar) {
  nsresult result=NS_OK;
  aChar=0;  
  if(mOffset>=(PRUint32)mBuffer.Length()) 
    result=Eof();

  if(NS_OK == result) {
    aChar=mBuffer.CharAt(mOffset); 
  }
  return result;
}


/**
 *  Push the given char back onto the scanner
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::PutBack(PRUnichar aChar) {
  if(mOffset>0)
    mOffset--;
  else mBuffer.Insert(aChar,0);
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
  static nsAutoString chars(" \n\r\t\b");
  return SkipOver(chars);
}

/**
 *  Skip over chars as long as they equal given char
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::SkipOver(PRUnichar aSkipChar){
  PRUnichar ch=0;
  nsresult   result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(ch);
    if(NS_OK == result) {
      if(ch!=aSkipChar) {
        PutBack(ch);
        break;
      }
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
  PRUnichar theChar=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(theChar);
    if(NS_OK == result) {
      PRInt32 pos=aSkipSet.Find(theChar);
      if(kNotFound==pos) {
        PutBack(theChar);
        break;
      }
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
  PRUnichar ch=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(ch);
    if(NS_OK == result) {
      PRInt32 pos=aValidSet.Find(ch);
      if(kNotFound!=pos) {
        PutBack(ch);
        break;
      }
    } 
    else break;
  } //while
  return result;
}



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
                             PRBool anOrderedSet,
                             PRBool addTerminal){

  NS_ASSERTION(((PR_FALSE==anOrderedSet) || aValidSet.IsOrdered()),kUnorderedStringError);

  PRUnichar theChar=0;
  nsresult   result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(theChar);
    if(NS_OK==result) {
      PRInt32 pos=(anOrderedSet) ? aValidSet.BinarySearch(theChar) : aValidSet.Find(theChar);
      if(kNotFound==pos) {
        if(addTerminal)
          aString+=theChar;
        else PutBack(theChar);
        break;
      }
      else aString+=theChar;
    }
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
nsresult nsScanner::ReadUntil(nsString& aString,
                             nsString& aTerminalSet,
                             PRBool anOrderedSet,
                             PRBool addTerminal){
  
  NS_ASSERTION(((PR_FALSE==anOrderedSet) || aTerminalSet.IsOrdered()),kUnorderedStringError);

  PRUnichar theChar=0;
  nsresult  result=NS_OK;

  while(NS_OK == result) {
    result=GetChar(theChar);
    if(NS_OK==result) {
      PRInt32 pos=(anOrderedSet) ? aTerminalSet.BinarySearch(theChar) : aTerminalSet.Find(theChar);
      if(kNotFound!=pos) {
        if(addTerminal)
          aString+=theChar;
        else PutBack(theChar);
        break;
      }
      else aString+=theChar;
    }
  }
  return result;
}


/**
 *  Consumes chars until you see the given terminalChar
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
nsresult nsScanner::ReadUntil(nsString& aString,
                             PRUnichar aTerminalChar,
                             PRBool addTerminal){
  PRUnichar theChar=0;
  nsresult  result=NS_OK;

  while(NS_OK==result) {
    result=GetChar(theChar);
    if(theChar==aTerminalChar) {
      if(addTerminal)
        aString+=theChar;
      else PutBack(theChar);
      break;
    }
    else aString+=theChar;
  }
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsString& nsScanner::GetBuffer(void) {
  return mBuffer;
}

/**
 *  Call this to copy bytes out of the scanner that have not yet been consumed
 *  by the tokenization process.
 *  
 *  @update  gess 5/12/98
 *  @param   aCopyBuffer is where the scanner buffer will be copied to
 *  @return  nada
 */
void nsScanner::CopyUnusedData(nsString& aCopyBuffer) {
  PRInt32 theLen=mBuffer.Length();
  if(0<theLen) {
    mBuffer.Right(aCopyBuffer,theLen-mMarkPos);
  }
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



