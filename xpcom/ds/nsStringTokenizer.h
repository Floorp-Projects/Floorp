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
 * @update  gess 4/1/98
 * 
 * This class knows how to read delimited data from a string.
 * Here are the 2 things you need to know to use this class effectively:
 *
 * ================================================
 * How To Setup The Tokenizer
 * ================================================
 *
 * The input charset can be either constrained or uncontrained. Constrained means
 * that you've chosen to allow only certain chars into your tokens. Unconstrained
 * means that any char (other than delimiters) are legal in your tokens.
 * If you want unconstrained input, use [*-*] your dataspec. To contrain your token
 * charset, you set ranges or single chars in the dataspec like this:
 *    "abc[0-9]"  -- which allow numbers and the letters a,b,c
 *
 *  Dataspecifier rules:
 *    abc   -- allows a set of characters
 *    [a-z] -- allows all chars in given range
 *    [*-*] -- allows all characters          
 *    ^abc  -- disallows a set of characters            //NOT_YET_IMPLEMENTED
 *    [a^z] -- disallows all characters in given range  //NOT_YET_IMPLEMENTED
 *    [a*b] -- specifies a delimiter pair for the entire token
 *    [a+b] -- specifies a delimiter pair for substrings in the token
 *
 * One other note: there is an optional argument called allowQuoting, which tells
 * the tokenizer whether to allow quoted strings within your fields. If you set this
 * to TRUE, then we allow nested quoted strings, which themselves can contain any data.
 * It's considered an error to set allowQuoting=TRUE and use a quote as a token or record delimiter.
 *
 * The other thing you need to set up for the tokenizer to work correctly are the delimiters.
 * They seperate fields and records, and be different. You can also have more than one kind
 * of delimiter for each. The distinguishment between tokens are records allows the caller
 * to deal with multi-line text files (where \n is the record seperator). Again, you don't have
 * to have a record seperator if it doesn't make sense in the context of your input dataset.
 *
 *
 * ================================================
 * How To Iterate Tokens
 * ================================================
 *
 * There are 2 ways to iterate tokens, either manually or automatically.
 * The manual method requires that you call a set of methods in the right order,
 * but gives you slightly more control. Here's the calling pattern:
 *
 * {
 *    nsString theBuffer("xxxxxxx");
 *    nsStringTokenizer tok(...);
 *    tok.SetBuffer(theBuffer);
 *    tok.FirstRecord(); 
 *    while(tok.HasNextToken()){
 *      while(tok.HasNextToken()){
 *        nsAutoString theToken;
 *        tok.GetNextToken(theToken);
 *        //do something with your token here...
 *      } //while
 *      tok.NextRecord();
 *    } //while
 *  }
 *
 * The automatic method handles all the iteration for you. You provide a callback functor
 * and you'll get called once for each token per record. To use that technique, you need
 * to define an object that provides the ITokenizeFunctor interface (1 method). Then
 * call the tokenizer method Iterate(...). Voila.
 *
 */


#ifndef nsStringTokenizer_
#define nsStringTokenizer_

#include "nsString.h"

class ITokenizeFunctor {
public:
  virtual operator ()(nsString& aToken,PRInt32 aRecordCount,PRInt32 aTokenCount)=0;
};

class nsStringTokenizer {
public:
          nsStringTokenizer(const char* aDataSpecifier="\"",const char* aFieldSep=",",const char* aRecordSep="\n");
          ~nsStringTokenizer();


    //Call these methods if you want to iterate the tokens yourself                
  void    SetBuffer(nsString& aBuffer);
  PRBool  FirstRecord(void);
  PRBool  NextRecord(void);
  PRBool  HasNextToken(void);
  PRInt32 GetNextToken(nsString& aToken);

    //Call this one (exclusively) if you want to be called back iteratively
  PRInt32 Iterate(nsString& aBuffer,ITokenizeFunctor& aFunctor);

protected:

  enum	eCharTypes    {eUnknown,eDataChar,eFieldSeparator,eDataDelimiter,eRecordSeparator};
  enum  eCharSpec     {eGivenChars,eAllChars,eExceptChars};

  PRInt32         SkipOver(nsString& aSkipSet);
  PRInt32         SkipOver(PRUnichar  aSkipChar);
  PRInt32         ReadUntil(nsString& aString,PRUnichar* aTermSet,PRBool aState);
  PRInt32         ReadUntil(nsString& aString,PRUnichar aChar,PRBool aState);
  PRBool          More(void);
  PRInt32         GetChar(PRUnichar& aChar);
  void            UnGetChar(PRUnichar aChar);
  PRBool          SkipToValidData(void);
  void            ExpandDataSpecifier(const char* aDataSpec) ;
  inline PRBool   IsValidDataChar(PRUnichar aChar);
  eCharTypes      DetermineCharType(PRUnichar aChar);

  PRInt32         mValidChars[4];
  PRInt32         mInvalidChars[4];
  nsString        mDataStartDelimiter;
  nsString        mDataEndDelimiter;
  nsString        mSubstrStartDelimiter;
  nsString        mSubstrEndDelimiter;
  nsString        mFieldSeparator;
  nsString        mRecordSeparator;
  PRInt32         mOffset;
  eCharSpec       mCharSpec;
  nsString*       mBuffer;
};

#endif
