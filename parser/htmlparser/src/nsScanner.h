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
 * The scanner is a low-level service class that knows
 * how to consume characters out of an (internal) stream.
 * This class also offers a series of utility methods
 * that most tokenizers want, such as readUntil(), 
 * readWhile() and SkipWhite().
 */

#ifndef SCANNER
#define SCANNER


#include "nsString.h"
#include "nsParserTypes.h"
#include "prtypes.h"
#include "nsIInputStream.h"


class nsIURL;
class ifstream;

class CScanner {
  public:
   					      CScanner(nsIURL* aURL,eParseMode aMode=eParseMode_navigator);
            		  ~CScanner();

      PRInt32     GetChar(PRUnichar& ch);
			PRInt32   	Peek(PRUnichar& ch);
      PRInt32   	PutBack(PRUnichar ch);
			PRInt32    	SkipOver(nsString& SkipChars);
			PRInt32   	SkipPast(nsString& aSequence);
			PRInt32   	SkipPast(PRUnichar aChar);
			PRInt32    	SkipWhite(void);
      PRBool      Eof(void);

			PRInt32     ReadUntil(nsString& aString,PRUnichar aTerminal,PRBool addTerminal);
			PRInt32     ReadUntil(nsString& aString,nsString& terminals,PRBool addTerminal);
			PRInt32     ReadWhile(nsString& aString,nsString& validChars,PRBool addTerminal);

      static void SelfTest();

  protected:

      PRInt32     FillBuffer(PRInt32& anError);

			nsIInputStream* mStream;
      nsString        mBuffer;
      PRInt32         mOffset;
      PRInt32         mTotalRead;
      eParseMode      mParseMode;
};

#endif
