/*
 * Copyright (C) 1998 Rick Gessner.  All Rights Reserved.
 */

#include "CScanner.h"

const int   kBufsize=1024;


/**
 *  Use this constructor if you want i/o to be stream based.
 *
 *  @update  gess 5/12/98
 *  @param   aStream --
 *  @param   assumeOwnership --
 *  @param   aFilename --
 *  @return  
 */
CScanner::CScanner(istream& aStream) : mBuffer(""), mFileStream(aStream) {    
  mIncremental=true;
  mOffset=0;
  mTotalRead=0;
}


/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CScanner::~CScanner() {
}

/** 
 * Grab data from underlying stream.
 *
 * @update  gess4/3/98
 * @return  error code
 */
int CScanner::fillBuffer(void) {
  int anError=kNoError;

  mBuffer="";
  if(!mFileStream) {
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    anError=(mIncremental) ? kInterrupted : kEOF;
  }
  else {
    int numread=0;
    char buf[kBufsize+1];
    buf[kBufsize]=0;

    if(mFileStream) {
      mFileStream.read(buf,kBufsize);
      numread=mFileStream.gcount();
      buf[numread]=0;
    }
    mOffset=0;
    if((0<numread) && (0==anError))
      mBuffer=buf;
    mTotalRead+=mBuffer.Length();
  }

  return anError;
}

/**
 *  determine if the scanner has reached EOF
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  0=!eof 1=eof kInterrupted=interrupted
 */
int CScanner::endOfFile() {
  int theError=kNoError;

  if(mOffset>=mBuffer.Length()) {
    theError=fillBuffer();  
  }
  
  if(kNoError==theError) {
    if (0==mBuffer.Length()) {
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
int CScanner::getChar(PRUnichar& aChar) {
  int result=kNoError;
  
  if(mOffset>=mBuffer.Length()) 
    result=endOfFile();

  if(kNoError == result) {
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
int CScanner::peek(PRUnichar& aChar) {
  int result=kNoError;
  
  if(mOffset>=mBuffer.Length()) 
    result=endOfFile();

  if(kNoError == result) {
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
int CScanner::push(PRUnichar aChar) {
  if(mOffset>0)
    mOffset--;
  else {
    mBuffer.Insert(aChar,0);
  }
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
int CScanner::readWhile(nsCString& aString,nsCString& aValidSet,bool addTerminal){
  PRUnichar theChar=0;
  int			result=kNoError;

  while(kNoError==result) {
    result=getChar(theChar);
    if(kNoError==result) {
      int pos=aValidSet.Find(theChar);
      if(kNotFound==pos) {
        if(addTerminal)
          aString+=theChar;
        else push(theChar);
        break;
      }
      else aString+=theChar;
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
int CScanner::readUntil(nsCString& aString,nsCString& aTerminalSet,bool addTerminal){
  PRUnichar	theChar=0;
  int			result=kNoError;

  while(kNoError == result) {
    result=getChar(theChar);
    if(kNoError==result) {
      int pos=aTerminalSet.Find(theChar);
      if(kNotFound!=pos) {
        if(addTerminal)
          aString+=theChar;
        else push(theChar);
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
int CScanner::readUntil(nsCString& aString, PRUnichar aTerminalChar, bool addTerminal) {
  PRUnichar	ch=0;
  int			result=kNoError;

  while(kNoError==result) {
    result=getChar(ch);
    if(ch==aTerminalChar) {
      if(addTerminal)
        aString+=ch;
      else push(ch);
      break;
    }
    else aString+=ch;
  }
  return result;
}


