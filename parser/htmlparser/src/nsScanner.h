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
 * readWhile() and SkipWhitespace().
 */


#ifndef SCANNER
#define SCANNER

#include "nsString.h"
#include "nsIParser.h"
#include "prtypes.h"
#include "nsIUnicodeDecoder.h"
#include "nsFileStream.h"


typedef enum {
   kCharsetUninitialized = 0,
   kCharsetFromUserDefault ,
   kCharsetFromDocTypeDefault,
   kCharsetFromParentFrame,
   kCharsetFromAutoDetection,
   kCharsetFromMetaTag,
   kCharsetFromHTTPHeader
} nsCharsetSource;

class nsScanner {
  public:

      /**
       *  Use this constructor if you want i/o to be based on 
       *  a single string you hand in during construction.
       *  This short cut was added for Javascript.
       *
       *  @update  ftang 3/02/99
       *  @param   aCharset charset
       *  @param   aCharsetSource - where the charset info came from 
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      nsScanner(nsString& anHTMLString, const nsString& aCharset, nsCharsetSource aSource);

      /**
       *  Use this constructor if you want i/o to be based on 
       *  a file (therefore a stream) or just data you provide via Append().
       *
       *  @update  ftang 3/02/99
       *  @param   aCharset charset
       *  @param   aCharsetSource - where the charset info came from 
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      nsScanner(nsString& aFilename,PRBool aCreateStream, const nsString& aCharset, nsCharsetSource aSource);

      /**
       *  Use this constructor if you want i/o to be stream based.
       *
       *  @update  ftang 3/02/99
       *  @param   aCharset charset
       *  @param   aCharsetSource - where the charset info came from 
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      nsScanner(nsString& aFilename, nsInputStream& aStream, const nsString& aCharset, nsCharsetSource aSource,PRBool assumeOwnership=PR_TRUE);


      ~nsScanner();

      /**
       *  retrieve next char from internal input stream
       *  
       *  @update  gess 3/25/98
       *  @param   ch is the char to accept new value
       *  @return  error code reflecting read status
       */
      nsresult GetChar(PRUnichar& ch);

      /**
       *  peek ahead to consume next char from scanner's internal
       *  input buffer
       *  
       *  @update  gess 3/25/98
       *  @param   ch is the char to accept new value
       *  @return  error code reflecting read status
       */
      nsresult Peek(PRUnichar& ch);

      /**
       *  Push the given char back onto the scanner
       *  
       *  @update  gess 3/25/98
       *  @param   character to be pushed
       *  @return  error code
       */
      nsresult PutBack(PRUnichar ch);

      /**
       *  Skip over chars as long as they're in aSkipSet
       *  
       *  @update  gess 3/25/98
       *  @param   set of chars to be skipped
       *  @return  error code
       */
      nsresult SkipOver(nsString& SkipChars);

      /**
       *  Skip over chars as long as they equal given char
       *  
       *  @update  gess 3/25/98
       *  @param   char to be skipped
       *  @return  error code
       */
      nsresult SkipOver(PRUnichar aSkipChar);

      /**
       *  Skip over chars until they're in aValidSet
       *  
       *  @update  gess 3/25/98
       *  @param   aValid set contains chars you're looking for
       *  @return  error code
       */
      nsresult SkipTo(nsString& aValidSet);

      /**
       *  Skip over chars as long as they're in aSequence
       *  
       *  @update  gess 3/25/98
       *  @param   contains sequence to be skipped
       *  @return  error code
       */
      nsresult SkipPast(nsString& aSequence);

      /**
       *  Skip whitespace on scanner input stream
       *  
       *  @update  gess 3/25/98
       *  @return  error status
       */
      nsresult SkipWhitespace(void);

      /**
       *  Determine if the scanner has reached EOF.
       *  This method can also cause the buffer to be filled
       *  if it happens to be empty
       *  
       *  @update  gess 3/25/98
       *  @return  PR_TRUE upon eof condition
       */
      nsresult Eof(void);

      /**
       *  Consume characters until you find the terminal char
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   aTerminal contains terminating char
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      nsresult ReadUntil(nsString& aString,PRUnichar aTerminal,PRBool addTerminal);

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
      nsresult ReadUntil(nsString& aString,nsString& aTermSet,PRBool anOrderedSet,PRBool addTerminal);

      /**
       *  Consume characters while they're members of anInputSet
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   anInputSet contains valid chars
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      nsresult ReadWhile(nsString& aString,nsString& anInputSet,PRBool anOrderedSet,PRBool addTerminal);

      /**
       *  Records current offset position in input stream. This allows us
       *  to back up to this point if the need should arise, such as when
       *  tokenization gets interrupted.
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      PRUint32 Mark(void);

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
      PRUint32 RewindToMark(void);


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
      PRBool Append(const char* aBuffer, PRUint32 aLen);

      PRBool Append(const PRUnichar* aBuffer, PRUint32 aLen);

      /**
       *  
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      nsString& GetBuffer(void);

      /**
       *  Call this to copy bytes out of the scanner that have not yet been consumed
       *  by the tokenization process.
       *  
       *  @update  gess 5/12/98
       *  @param   aCopyBuffer is where the scanner buffer will be copied to
       *  @return  nada
       */
      void CopyUnusedData(nsString& aCopyBuffer);

      /**
       *  Retrieve the name of the file that the scanner is reading from.
       *  In some cases, it's just a given name, because the scanner isn't
       *  really reading from a file.
       *  
       *  @update  gess 5/12/98
       *  @return  
       */
      nsString& GetFilename(void);

      static void SelfTest();

      /**
       *  Use this setter to change the scanner's unicode decoder
       *
       *  @update  ftang 3/02/99
       *  @param   aCharset a normalized (alias resolved) charset name
       *  @param   aCharsetSource- where the charset info came from
       *  @return  
       */
      nsresult SetDocumentCharset(const nsString& aCharset, nsCharsetSource aSource);

  protected:

    enum {eBufferSizeThreshold=512};

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       */
      nsresult FillBuffer(void);

      nsInputStream*  mInputStream;
      nsString        mBuffer;
      nsString        mFilename;
      PRUint32        mOffset;
      PRUint32        mMarkPos;
      PRUint32        mTotalRead;
      PRBool          mOwnsStream;
      PRBool          mIncremental;
      nsCharsetSource mCharsetSource;
      nsString        mCharset;
      nsIUnicodeDecoder *mUnicodeDecoder;
};

#endif


