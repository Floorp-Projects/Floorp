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

/**
 * MODULE NOTES:
 * LAST MODS:  gess 28Feb98
 * 
 * This file declares the basic tokenizer class. The 
 * central theme of this class is to control and 
 * coordinate a tokenization process. Note that this 
 * class is grammer-neutral: this class doesn't care
 * at all what the underlying stream consists of. 
 *
 * The main purpose of this class is to iterate over an
 * input stream with the help of a given scanner and a
 * given type-specific tokenizer-Delegate.
 *
 * The primary method here is the tokenize() method, which
 * simple loops calling getToken() until an EOF condition
 * (or some other error) occurs.
 *
 */


#ifndef  TOKENIZER
#define  TOKENIZER

#include "nsToken.h"
#include "nsITokenizerDelegate.h"
#include "nsDeque.h"
#include <iostream.h>

class CScanner;
class nsIURL;

class  CTokenizer {
  public:

    CTokenizer(ITokenizerDelegate* aDelegate,eParseMode aMode=eParseMode_navigator);
    CTokenizer(const char* aFilename,ITokenizerDelegate* aDelegate,eParseMode aMode=eParseMode_navigator);
    CTokenizer(nsIURL* aURL,ITokenizerDelegate* aDelegate,eParseMode aMode=eParseMode_navigator);

    ~CTokenizer();
    
    /**
     *  This method incrementally tokenizes as much content as
     *  it can get its hands on.
     *  
     *  @update  gess 3/25/98
     *  @return  TRUE if it's ok to proceed
     */
    PRInt32 Tokenize(int anIteration); //your friendly incremental version

    /**
     *  
     *  @update  gess 3/25/98
     *  @return  TRUE if it's ok to proceed
     */
    PRInt32 Tokenize(nsString& aSourceBuffer,PRBool appendTokens=PR_TRUE); 

    /**
     *  Cause the tokenizer to consume the next token, and 
     *  return an error result.
     *  
     *  @update  gess 3/25/98
     *  @param   anError -- ref to error code
     *  @return  new token or null
     */
    PRInt32 GetToken(CToken*& aToken);
    
    /**
     *  Retrieve the number of elements in the deque
     *  
     *  @update  gess 3/25/98
     *  @return  int containing element count
     */
    PRInt32 GetSize(void);
    
    /**
     * Retrieve a reference to the internal token deque.
     *
     * @update  gess 4/20/98
     * @return  deque reference
     */
    nsDeque& GetDeque(void);

    /**
     *
     * @update  gess 4/20/98
     * @return  deque reference
     */
    PRBool Append(nsString& aBuffer);


    /**
     *  
     *  
     *  @update  gess 5/13/98
     *  @param   
     *  @return  
     */
    PRBool SetBuffer(nsString& aBuffer);

    /**
     *  This debug routine is used to cause the tokenizer to
     *  iterate its token list, asking each token to dump its
     *  contents to the given output stream.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  
     */
    void DebugDumpSource(ostream& out);
    
    /**
     *  This debug routine is used to cause the tokenizer to
     *  iterate its token list, asking each token to dump its
     *  contents to the given output stream.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  
     */
    void DebugDumpTokens(ostream& out);
    
    static void SelfTest();

  protected:  

    /**
     *  This is the front-end of the code sandwich for the
     *  tokenization process. It gets called once just before
     *  tokenziation begins.
     *  
     *  @update  gess 3/25/98
     *  @param   aIncremental tells us if tokenization is incremental
     *  @return  TRUE if all went well
     */
    PRBool WillTokenize(PRBool aIncremental);
    

    /**
     *  This is the tail-end of the code sandwich for the
     *  tokenization process. It gets called once tokenziation
     *  has completed.
     *  
     *  @update  gess 3/25/98
     *  @param   aIncremental tells us if tokenization was incremental
     *  @return  TRUE if all went well
     */
    PRBool DidTokenize(PRBool aIncremental);

    ITokenizerDelegate* mDelegate;
    CScanner*           mScanner;
    nsDeque             mTokenDeque;
    eParseMode          mParseMode;
};

#endif


