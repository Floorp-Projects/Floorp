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

#include "nsScanner.h"
#include "nsIURL.h" 
#include "nsDebug.h"

const char* gURLRef=0;
const char* kBadHTMLText="<H3>Oops...</H3>You just tried to read a non-existent document: <BR>";

#ifdef __INCREMENTAL
const int   kBufsize=1;
#else
const int   kBufsize=64;
#endif

/**
 *  Use this constructor if you want an incremental (callback)
 *  based input stream.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return  
 */
CScanner::CScanner(eParseMode aMode) : mBuffer("") {
  mOffset=0;
  mMarkPos=-1;
  mTotalRead=0;
  mParseMode=aMode;
  mNetStream=0;
  mFileStream=0;
  mIncremental=PR_TRUE;
}

/**
 *  Use this constructor if you want i/o to be file based.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return  
 */
CScanner::CScanner(const char* aFilename,eParseMode aMode) : mBuffer("") {
  NS_ASSERTION(0!=aFilename,"Error: Null filename!");
  mOffset=0;
  mMarkPos=-1;
  mTotalRead=0;
  mParseMode=aMode;
  mNetStream=0;
  mIncremental=PR_FALSE;
#if defined(XP_UNIX) && defined(IRIX)
  /* XXX: IRIX does not support ios::binary */
  mFileStream=new fstream(aFilename,ios::in);
#else
  mFileStream=new fstream(aFilename,ios::in|ios::binary);
#endif
}

/**
 *  Use this constructor if you want i/o to be based on a
 *  non-incremental netstream.
 *
 *  @update  gess 5/12/98
 *  @param   aMode represents the parser mode (nav, other)
 *  @return  
 */
CScanner::CScanner(nsIURL* aURL,eParseMode aMode) : mBuffer("") {
  NS_ASSERTION(0!=aURL,"Error: Null URL!");
  mOffset=0;
  mMarkPos=-1;
  mTotalRead=0;
  mParseMode=aMode;
  mFileStream=0;
  PRInt32 error=0;
  mIncremental=PR_FALSE;
  mNetStream=aURL->Open(&error);
  gURLRef=aURL->GetSpec();
}


/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CScanner::~CScanner() {
  if(mFileStream) {
    mFileStream->close();
    delete mFileStream;
  }
  else if(mNetStream) {
    mNetStream->Close();
    mNetStream->Release();
  }
  mFileStream=0;
  mNetStream=0;
  gURLRef=0;
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
PRInt32 CScanner::RewindToMark(void){
  mOffset=mMarkPos;
  return mOffset;
}

/**
 *  Records current offset position in input stream. This allows us
 *  to back up to this point if the need should arise, such as when
 *  tokenization gets interrupted.
 *
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
PRInt32 CScanner::Mark(void){
  mMarkPos=mOffset;
  return mMarkPos;
}


/**
 *  
 *  @update  gess 5/12/98
 */
void _PreCompressBuffer(nsString& aBuffer,PRInt32& anOffset,PRInt32& aMarkPos){
  //To determine how much of our internal buffer to truncate, 
  //we should check mMarkPos. That represents the point at which
  //we've guaranteed the client we can back up to, so make sure
  //you don't lose any of the data beyond that point.
  if((anOffset!=aMarkPos) && (0<=aMarkPos)) {
    if(aMarkPos>0) {
      aBuffer.Cut(0,aMarkPos);
      if(anOffset>aMarkPos)
        anOffset-=aMarkPos;
    }
  }
  else aBuffer.Truncate();
  aMarkPos=0;
}


/**
 *  This method should only be called by the parser when
 *  we're doing incremental i/o over the net.
 *  
 *  @update  gess 5/12/98
 *  @param   aBuffer contains next blob of i/o data
 *  @param   aSize contains size of buffer
 *  @return  0 if all went well, otherwise error code.
 */
PRInt32 CScanner::IncrementalAppend(const char* aBuffer,PRInt32 aSize){
  NS_ASSERTION(((!mFileStream) && (!mNetStream)),"Error: Should only be called during incremental net i/o!");

  PRInt32 result=0;
  if((!mFileStream) && (!mNetStream)) {

    _PreCompressBuffer(mBuffer,mOffset,mMarkPos);

    //now that the buffer is (possibly) shortened, let's append the new data.
    if(0<aSize) {
      mBuffer.Append(aBuffer,aSize);
      mTotalRead+=aSize;
    }
  }
  return result;
}

/** 
 * Grab data from underlying stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
PRBool CScanner::Append(nsString& aBuffer) {
  _PreCompressBuffer(mBuffer,mOffset,mMarkPos);
  mBuffer.Append(aBuffer);
  return PR_TRUE;
}

/** 
 * Grab data from underlying stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
PRInt32 CScanner::FillBuffer(void) {
  PRInt32 anError=0;

  _PreCompressBuffer(mBuffer,mOffset,mMarkPos);

  if((!mIncremental) && (!mNetStream) && (!mFileStream)) {
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    if(0==mTotalRead) {
      mBuffer.Append((const char*)kBadHTMLText);
      mBuffer.Append((const char*)gURLRef);
    }
    else return 0;
  }
  else if(!mIncremental) {
    PRInt32 numread=0;
    char buf[kBufsize+1];
    buf[kBufsize]=0;

    if(mFileStream) {
      mFileStream->read(buf,kBufsize);
      numread=mFileStream->gcount();
    }
    else if(mNetStream) {
      numread=mNetStream->Read(&anError,buf,0,kBufsize);
      if(1==anError)
        anError=kEOF;
    }
    mOffset=mBuffer.Length();
    if((0<numread) && (0==anError))
      mBuffer.Append((const char*)buf,numread);
    mTotalRead+=mBuffer.Length();
  }
  else anError=kInterrupted;

  return anError;
}

/**
 *  determine if the scanner has reached EOF
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  0=!eof 1=eof kInterrupted=interrupted
 */
PRInt32 CScanner::Eof() {
  PRInt32 theError=0;

  if(mOffset>=mBuffer.Length()) {
    if(!mIncremental)
      theError=FillBuffer();  
    else return kInterrupted;
  }
  
  if(0==theError) 
    return (0==mBuffer.Length());

  return theError;
}

/**
 *  retrieve next char from scanners internal input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code reflecting read status
 */
PRInt32 CScanner::GetChar(PRUnichar& aChar) {
  PRInt32 result=Eof();
  if(!result) {
    aChar=mBuffer[mOffset++];
    result=kNoError;
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
PRInt32 CScanner::Peek(PRUnichar& aChar){
  PRInt32 result=Eof();
  if(!result) {
    aChar=mBuffer[mOffset];        
    result=kNoError;
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
PRInt32 CScanner::PutBack(PRUnichar aChar) {
  if(mOffset>0)
    mOffset--;
  else mBuffer.Insert(aChar,0);
  return kNoError;
}


/**
 *  Skip whitespace on scanner input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error status
 */
PRInt32 CScanner::SkipWhite(void) {
  static nsAutoString chars(" \n\r\t");
  return SkipOver(chars);
}

/**
 *  Skip over chars as long as they equal given char
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
PRInt32 CScanner::SkipOver(PRUnichar aSkipChar){
  PRUnichar ch=0;
  PRInt32   result=kNoError;

  while(kNoError==result) {
    result=GetChar(ch);
    if(!result) {
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
 *  @param   
 *  @return  error code
 */
PRInt32 CScanner::SkipOver(nsString& aSkipSet){
  PRUnichar ch=0;
  PRInt32   result=kNoError;

  while(kNoError==result) {
    result=GetChar(ch);
    if(!result) {
      PRInt32 pos=aSkipSet.Find(ch);
      if(kNotFound==pos) {
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
 *  @param   
 *  @return  error code
 */
PRInt32 CScanner::SkipPast(nsString& aValidSet){
  NS_NOTYETIMPLEMENTED("Error: SkipPast not yet implemented.");
  return kNoError;
}

/**
 *  Consume chars as long as they are <i>in</i> the 
 *  given validSet of input chars.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
PRInt32 CScanner::ReadWhile(nsString& aString,nsString& aValidSet,PRBool addTerminal){
  PRUnichar ch=0;
  PRInt32   result=kNoError;
  PRInt32   wrPos=0;

  while(kNoError==result) {
    result=GetChar(ch);
    if(kNoError==result) {
      PRInt32 pos=aValidSet.Find(ch);
      if(kNotFound==pos) {
        if(addTerminal)
          aString+=ch;
        else PutBack(ch);
        break;
      }
      else aString+=ch;
    }
  }
  return result;
}


/**
 *  Consume characters until you find one contained in given
 *  input set.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
PRInt32 CScanner::ReadUntil(nsString& aString,nsString& aTerminalSet,PRBool addTerminal){
  PRUnichar ch=0;
  PRInt32   result=kNoError;

  while(!result) {
    result=GetChar(ch);
    if(kNoError==result) {
      PRInt32 pos=aTerminalSet.Find(ch);
      if(kNotFound!=pos) {
        if(addTerminal)
          aString+=ch;
        else PutBack(ch);
        break;
      }
      else aString+=ch;
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
PRInt32 CScanner::ReadUntil(nsString& aString,PRUnichar aTerminalChar,PRBool addTerminal){
  PRUnichar ch=0;
  PRInt32   result=kNoError;

  while(kNoError==result) {
    result=GetChar(ch);
    if(ch==aTerminalChar) {
      if(addTerminal)
        aString+=ch;
      else PutBack(ch);
      break;
    }
    else aString+=ch;
  }
  return result;
}


/**
 *  Conduct self test. Actually, selftesting for this class
 *  occurs in the parser selftest.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */

void CScanner::SelfTest(void) {
#ifdef _DEBUG
#endif
}



