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


#include "nsScanner.h"
#include "nsIURL.h" 
#include "nsDebug.h"

const char* gURLRef;
const char* kBadHTMLText1="<HTML><BODY><H3>Oops...</H3>You just tried to read a non-existent document: <BR>";
const char* kBadHTMLText2="</BODY></HTML>";

/**
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aURL -- pointer to URL to be loaded
 *  @return  
 */
CScanner::CScanner(nsIURL* aURL,eParseMode aMode) : mBuffer("") {
  NS_ASSERTION(0!=aURL,"Error: Null URL!");
  mOffset=0;
  mStream=0;
  mTotalRead=0;
  mParseMode=aMode;
  if(aURL) {
    PRInt32 error;
    gURLRef=aURL->GetSpec();
    mStream=aURL->Open(&error);
  }
}

/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
CScanner::~CScanner() {
  mStream->Close();
  mStream->Release();
}


/*-------------------------------------------------------
 * 
 * @update  gess4/3/98
 * @param 
 * @return
 */
PRInt32 CScanner::FillBuffer(PRInt32& anError) {
  mBuffer.Cut(0,mBuffer.Length());
  if(!mStream) {
    //This is DEBUG code!!!!!!  XXX DEBUG XXX
    //If you're here, it means someone tried to load a
    //non-existent document. So as a favor, we emit a
    //little bit of HTML explaining the error.
    if(0==mTotalRead) {
      mBuffer.Append((const char*)kBadHTMLText1);
      mBuffer.Append((const char*)gURLRef);
      mBuffer.Append((const char*)kBadHTMLText2);
    }
    else return 0;
  }
  else {
    anError=0;
    PRInt32 numread=0;
    char buf[64];
    buf[sizeof(buf)-1]=0;
    numread=mStream->Read(&anError,buf,0,sizeof(buf)-1);
    if((0<numread) && (0==anError))
      mBuffer.Append((const char*)buf,numread);
  }
  mTotalRead+=mBuffer.Length();
  return mBuffer.Length();
}

/**
 *  determine if the scanner has reached EOF
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  PR_TRUE upon eof condition
 */
PRBool CScanner::Eof() {
  PRInt32 theError=0;
  if(mOffset>=mBuffer.Length()) {
    PRInt32 numread=FillBuffer(theError);
    mOffset=0;
  }
  PRBool result=PR_TRUE;
  if(0==theError) {
    result=PRBool(0==mBuffer.Length());
  }
  else {
    result=PR_TRUE;
  }
  return result;
}

/**
 *  retrieve next char from scanners internal input stream
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code reflecting read status
 */
PRInt32 CScanner::GetChar(PRUnichar& aChar) {
  if(!Eof()) {
    aChar=mBuffer[mOffset++];
    return kNoError;
  }
  return kEOF;
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
  if(!Eof()) {
    aChar=mBuffer[mOffset];        
    return kNoError;
  }
  return kEOF;
}


/**
 *  Push the given char back onto the scanner
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code
 */
PRInt32 CScanner::PutBack(PRUnichar aChar) {
  mOffset--;
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

  while(!Eof()) {
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



