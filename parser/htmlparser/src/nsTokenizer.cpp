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


#include <fstream.h>
#include "nsTokenizer.h"
#include "nsToken.h"
#include "nsScanner.h"
#include "nsIURL.h"


/**-------------------------------------------------------
 *  Default constructor
 *  
 *  @update gess 3/25/98
 *  @param  aFilename -- name of file to be tokenized
 *  @param  aDelegate -- ref to delegate to be used to tokenize
 *  @return 
 *------------------------------------------------------*/
CTokenizer::CTokenizer(nsIURL* aURL,ITokenizerDelegate* aDelegate,eParseMode aMode) :
  mTokenDeque() {
  mDelegate=aDelegate;
  mScanner=new CScanner(aURL,aMode);
  mParseMode=aMode;
}


/**-------------------------------------------------------
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
CTokenizer::~CTokenizer() {
  delete mScanner;
  delete mDelegate;
  mScanner=0;
}


/**
 * Retrieve a reference to the internal token deque.
 *
 * @update  gess 4/20/98
 * @return  deque reference
 */
nsDeque& CTokenizer::GetDeque(void) {
  return mTokenDeque;
}

/**-------------------------------------------------------
 *  Cause the tokenizer to consume the next token, and 
 *  return an error result.
 *  
 *  @update  gess 3/25/98
 *  @param   anError -- ref to error code
 *  @return  new token or null
 *------------------------------------------------------*/
CToken* CTokenizer::GetToken(PRInt32& anError) {
  CToken* nextToken=mDelegate->GetToken(*mScanner,anError);
  return nextToken;
}

/**-------------------------------------------------------
 *  Retrieve the number of elements in the deque
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int containing element count
 *-----------------------------------------------------*/
PRInt32 CTokenizer::GetSize(void) {
  return mTokenDeque.GetSize();
}


/**-------------------------------------------------------
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if it's ok to proceed
 *------------------------------------------------------*/
PRBool CTokenizer::WillTokenize(){
  PRBool result=PR_TRUE;
  result=mDelegate->WillTokenize();
  return result;
}

/**-------------------------------------------------------
 *  This is the primary control routine. It iteratively
 *  consumes tokens until an error occurs or you run out
 *  of data.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  error code 
 *------------------------------------------------------*/
PRInt32 CTokenizer::Tokenize(void) {
  CToken* nextToken;
  PRInt32     result;

  if(WillTokenize()) {
    do {
      nextToken=GetToken(result);
      if(nextToken) {
#ifdef VERBOSE_DEBUG
        nextToken->DebugDumpToken(cout);
#endif
        if(mDelegate->WillAddToken(*nextToken)) {
          mTokenDeque.Push(nextToken);
        }
      }
    } while(nextToken!=0);
    result=DidTokenize();
  }
  return result;
}


/**-------------------------------------------------------
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if all went well
 *------------------------------------------------------*/
PRBool CTokenizer::DidTokenize() {
  PRBool result=mDelegate->DidTokenize();

#ifdef VERBOSE_DEBUG
    DebugDumpTokens(cout);
#endif

  return result;
}

/**-------------------------------------------------------
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
void CTokenizer::DebugDumpTokens(ostream& out) {
  nsDequeIterator b=mTokenDeque.Begin();
  nsDequeIterator e=mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpToken(out);
  }
}


/**-------------------------------------------------------
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
void CTokenizer::DebugDumpSource(ostream& out) {
  nsDequeIterator b=mTokenDeque.Begin();
  nsDequeIterator e=mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpSource(out);
  }

}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
void CTokenizer::SelfTest(void) {
#ifdef _DEBUG
#endif
}


