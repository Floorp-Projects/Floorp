/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIParser.h"
#include "prtypes.h"
#include "nsIUnicodeDecoder.h"
#include "nsScannerString.h"
#include "nsIInputStream.h"

class nsParser;

class nsReadEndCondition {
public:
  const PRUnichar *mChars;
  PRUnichar mFilter;
  explicit nsReadEndCondition(const PRUnichar* aTerminateChars);
private:
  nsReadEndCondition(const nsReadEndCondition& aOther); // No copying
  void operator=(const nsReadEndCondition& aOther); // No assigning
};

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
      nsScanner(const nsAString& anHTMLString, const nsACString& aCharset, PRInt32 aSource);

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
      nsScanner(nsString& aFilename,PRBool aCreateStream, const nsACString& aCharset, PRInt32 aSource);

      /**
       *  Use this constructor if you want i/o to be stream based.
       *
       *  @update  ftang 3/02/99
       *  @param   aCharset charset
       *  @param   aCharsetSource - where the charset info came from 
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      nsScanner(const nsAString& aFilename, nsIInputStream* aStream, const nsACString& aCharset, PRInt32 aSource);


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
      nsresult Peek(PRUnichar& ch, PRUint32 aOffset=0);

      nsresult Peek(nsAString& aStr, PRInt32 aNumChars, PRInt32 aOffset = 0);

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
      nsresult SkipWhitespace(PRInt32& aNewlinesSkipped);

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
       *  Consume characters until you run into space, a '<', a '>', or a '/'.
       *  
       *  @param   aString - receives new data from stream
       *  @return  error code
       */
      nsresult ReadTagIdentifier(nsString& aString);

      /**
       *  Consume characters until you run into a char that's not valid in an
       *  entity name
       *  
       *  @param   aString - receives new data from stream
       *  @return  error code
       */
      nsresult ReadEntityIdentifier(nsString& aString);
      nsresult ReadNumber(nsString& aString,PRInt32 aBase);
      nsresult ReadWhitespace(nsString& aString, 
                              PRInt32& aNewlinesSkipped);
      nsresult ReadWhitespace(nsScannerIterator& aStart, 
                              nsScannerIterator& aEnd,
                              PRInt32& aNewlinesSkipped);

      /**
       *  Consume characters until you find the terminal char
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   aTerminal contains terminating char
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      nsresult ReadUntil(nsAString& aString,
                         PRUnichar aTerminal,
                         PRBool addTerminal);

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
      nsresult ReadUntil(nsAString& aString,
                         const nsReadEndCondition& aEndCondition, 
                         PRBool addTerminal);

      nsresult ReadUntil(nsScannerIterator& aStart,
                         nsScannerIterator& aEnd,
                         const nsReadEndCondition& aEndCondition, 
                         PRBool addTerminal);


      /**
       *  Consume characters while they're members of anInputSet
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   anInputSet contains valid chars
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      nsresult ReadWhile(nsString& aString,nsString& anInputSet,PRBool addTerminal);

      /**
       *  Records current offset position in input stream. This allows us
       *  to back up to this point if the need should arise, such as when
       *  tokenization gets interrupted.
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      void Mark(void);

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
      void RewindToMark(void);


      /**
       *  
       *  
       *  @update  harishd 01/12/99
       *  @param   
       *  @return  
       */
      PRBool UngetReadable(const nsAString& aBuffer);

      /**
       *  
       *  
       *  @update  gess 5/13/98
       *  @param   
       *  @return  
       */
      nsresult Append(const nsAString& aBuffer);

      /**
       *  
       *  
       *  @update  gess 5/21/98
       *  @param   
       *  @return  
       */
      nsresult Append(const char* aBuffer, PRUint32 aLen);

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
      nsresult SetDocumentCharset(const nsACString& aCharset, PRInt32 aSource);

      void BindSubstring(nsScannerSubstring& aSubstring, const nsScannerIterator& aStart, const nsScannerIterator& aEnd);
      void CurrentPosition(nsScannerIterator& aPosition);
      void EndReading(nsScannerIterator& aPosition);
      void SetPosition(nsScannerIterator& aPosition,
                       PRBool aTruncate = PR_FALSE,
                       PRBool aReverse = PR_FALSE);
      void ReplaceCharacter(nsScannerIterator& aPosition,
                            PRUnichar aChar);

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       */
      PRBool    IsIncremental(void) {return mIncremental;}
      void      SetIncremental(PRBool anIncrValue) {mIncremental=anIncrValue;}

      /**
       * Return the position of the first non-whitespace
       * character. This is only reliable before consumers start
       * reading from this scanner.
       */
      PRInt32 FirstNonWhitespacePosition()
      {
        return mFirstNonWhitespacePosition;
      }

      void SetParser(nsParser *aParser)
      {
        mParser = aParser;
      }

  protected:


    enum {eBufferSizeThreshold=0x1000};  //4K

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       */
      nsresult FillBuffer(void);

      void AppendToBuffer(nsScannerString::Buffer*);
      void AppendToBuffer(const nsAString& aStr) { AppendToBuffer(nsScannerString::AllocBufferFromString(aStr)); }
      void AppendASCIItoBuffer(const char* aData, PRUint32 aLen);

      nsCOMPtr<nsIInputStream>     mInputStream;
      nsScannerString*             mSlidingBuffer;
      nsScannerIterator            mCurrentPosition; // The position we will next read from in the scanner buffer
      nsScannerIterator            mMarkPosition;    // The position last marked (we may rewind to here)
      nsScannerIterator            mEndPosition;     // The current end of the scanner buffer
      nsString        mFilename;
      PRUint32        mCountRemaining; // The number of bytes still to be read
                                       // from the scanner buffer
      PRUint32        mTotalRead;
      PRPackedBool    mIncremental;
      PRInt32         mFirstNonWhitespacePosition;
      PRInt32         mCharsetSource;
      nsCString       mCharset;
      nsIUnicodeDecoder *mUnicodeDecoder;
      nsParser        *mParser;
};

#endif


