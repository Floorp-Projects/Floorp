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
#include <fstream.h>

class nsIURL;
//class ifstream;

class CScanner {
  public:

      /**
       *  Use this constructor if you want an incremental (callback)
       *  based input stream.
       *
       *  @update  gess 5/12/98
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      CScanner(eParseMode aMode=eParseMode_navigator);
      
      /**
       *  Use this constructor if you want i/o to be based on a
       *  non-incremental netstream.
       *
       *  @update  gess 5/12/98
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      CScanner(nsIURL* aURL,eParseMode aMode=eParseMode_navigator);

      /**
       *  Use this constructor if you want i/o to be file based.
       *
       *  @update  gess 5/12/98
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      CScanner(const char* aFilename,eParseMode aMode=eParseMode_navigator);

      ~CScanner();

      /**
       *  retrieve next char from internal input stream
       *  
       *  @update  gess 3/25/98
       *  @param   ch is the char to accept new value
       *  @return  error code reflecting read status
       */
      PRInt32 GetChar(PRUnichar& ch);

      /**
       *  peek ahead to consume next char from scanner's internal
       *  input buffer
       *  
       *  @update  gess 3/25/98
       *  @param   ch is the char to accept new value
       *  @return  error code reflecting read status
       */
      PRInt32 Peek(PRUnichar& ch);

      /**
       *  Push the given char back onto the scanner
       *  
       *  @update  gess 3/25/98
       *  @param   character to be pushed
       *  @return  error code
       */
      PRInt32 PutBack(PRUnichar ch);

      /**
       *  Skip over chars as long as they're in aSkipSet
       *  
       *  @update  gess 3/25/98
       *  @param   set of chars to be skipped
       *  @return  error code
       */
      PRInt32 SkipOver(nsString& SkipChars);

      /**
       *  Skip over chars as long as they equal given char
       *  
       *  @update  gess 3/25/98
       *  @param   char to be skipped
       *  @return  error code
       */
      PRInt32 SkipOver(PRUnichar aSkipChar);

      /**
       *  Skip over chars as long as they're in aSequence
       *  
       *  @update  gess 3/25/98
       *  @param   contains sequence to be skipped
       *  @return  error code
       */
      PRInt32 SkipPast(nsString& aSequence);

      /**
       *  Skip whitespace on scanner input stream
       *  
       *  @update  gess 3/25/98
       *  @return  error status
       */
      PRInt32 SkipWhite(void);

      /**
       *  Determine if the scanner has reached EOF.
       *  This method can also cause the buffer to be filled
       *  if it happens to be empty
       *  
       *  @update  gess 3/25/98
       *  @return  PR_TRUE upon eof condition
       */
      PRInt32 Eof(void);

      /**
       *  Consume characters until you find the terminal char
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   aTerminal contains terminating char
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      PRInt32 ReadUntil(nsString& aString,PRUnichar aTerminal,PRBool addTerminal);

      /**
       *  Consume characters until you find one contained in given
       *  terminal set.
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   aTermSet contains set of terminating chars
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      PRInt32 ReadUntil(nsString& aString,nsString& aTermSet,PRBool addTerminal);

      /**
       *  Consume characters while they're members of anInputSet
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   anInputSet contains valid chars
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      PRInt32 ReadWhile(nsString& aString,nsString& anInputSet,PRBool addTerminal);

      /**
       *  Records current offset position in input stream. This allows us
       *  to back up to this point if the need should arise, such as when
       *  tokenization gets interrupted.
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      PRInt32 Mark(void);

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
      PRInt32 RewindToMark(void);


      /**
       *  
       *  
       *  @update  gess 5/13/98
       *  @param   
       *  @return  
       */
      PRBool Append(nsString& aBuffer);

      /**
       *  
       *  
       *  @update  gess 5/21/98
       *  @param   
       *  @return  
       */
      PRBool Append(const char* aBuffer, PRInt32 aLen);

      /**
       *  
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      PRInt32 IncrementalAppend(const char* aBuffer,PRInt32 aSize);

      /**
       *  
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      nsString& GetBuffer(void);

      static void SelfTest();

  protected:

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       * @param   anError is the resulting error code (hopefully 0)
       * @return  number of new bytes read
       */
      PRInt32 FillBuffer(void);


      fstream*        mFileStream;
      nsIInputStream* mNetStream;
      nsString        mBuffer;
      PRInt32         mOffset;
      PRInt32         mMarkPos;
      PRInt32         mTotalRead;
      eParseMode      mParseMode;
      PRBool          mIncremental;
};

#endif


