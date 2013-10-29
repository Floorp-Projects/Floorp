/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 * The scanner is a low-level service class that knows
 * how to consume characters out of an (internal) stream.
 * This class also offers a series of utility methods
 * that most tokenizers want, such as readUntil()
 * and SkipWhitespace().
 */


#ifndef SCANNER
#define SCANNER

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIParser.h"
#include "nsIUnicodeDecoder.h"
#include "nsScannerString.h"

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
       *  Use this constructor for the XML fragment parsing case
       */
      nsScanner(const nsAString& anHTMLString);

      /**
       *  Use this constructor if you want i/o to be based on 
       *  a file (therefore a stream) or just data you provide via Append().
       */
      nsScanner(nsString& aFilename, bool aCreateStream);

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
      nsresult Peek(PRUnichar& ch, uint32_t aOffset=0);

      nsresult Peek(nsAString& aStr, int32_t aNumChars, int32_t aOffset = 0);

      /**
       *  Skip over chars as long as they equal given char
       *  
       *  @update  gess 3/25/98
       *  @param   char to be skipped
       *  @return  error code
       */
      nsresult SkipOver(PRUnichar aSkipChar);

      /**
       *  Skip whitespace on scanner input stream
       *  
       *  @update  gess 3/25/98
       *  @return  error status
       */
      nsresult SkipWhitespace(int32_t& aNewlinesSkipped);

      /**
       *  Consume characters until you run into space, a '<', a '>', or a '/'.
       *  
       *  @param   aString - receives new data from stream
       *  @return  error code
       */
      nsresult ReadTagIdentifier(nsScannerSharedSubstring& aString);

      /**
       *  Consume characters until you run into a char that's not valid in an
       *  entity name
       *  
       *  @param   aString - receives new data from stream
       *  @return  error code
       */
      nsresult ReadEntityIdentifier(nsString& aString);
      nsresult ReadNumber(nsString& aString,int32_t aBase);
      nsresult ReadWhitespace(nsScannerSharedSubstring& aString, 
                              int32_t& aNewlinesSkipped,
                              bool& aHaveCR);
      nsresult ReadWhitespace(nsScannerIterator& aStart, 
                              nsScannerIterator& aEnd,
                              int32_t& aNewlinesSkipped);

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
                         bool addTerminal);

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
                         bool addTerminal);

      nsresult ReadUntil(nsScannerSharedSubstring& aString,
                         const nsReadEndCondition& aEndCondition,
                         bool addTerminal);

      nsresult ReadUntil(nsScannerIterator& aStart,
                         nsScannerIterator& aEnd,
                         const nsReadEndCondition& aEndCondition, 
                         bool addTerminal);

      /**
       *  Records current offset position in input stream. This allows us
       *  to back up to this point if the need should arise, such as when
       *  tokenization gets interrupted.
       *  
       *  @update  gess 5/12/98
       *  @param   
       *  @return  
       */
      int32_t Mark(void);

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
      bool UngetReadable(const nsAString& aBuffer);

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
      nsresult Append(const char* aBuffer, uint32_t aLen,
                      nsIRequest *aRequest);

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
      nsresult SetDocumentCharset(const nsACString& aCharset, int32_t aSource);

      void BindSubstring(nsScannerSubstring& aSubstring, const nsScannerIterator& aStart, const nsScannerIterator& aEnd);
      void CurrentPosition(nsScannerIterator& aPosition);
      void EndReading(nsScannerIterator& aPosition);
      void SetPosition(nsScannerIterator& aPosition,
                       bool aTruncate = false,
                       bool aReverse = false);
      void ReplaceCharacter(nsScannerIterator& aPosition,
                            PRUnichar aChar);

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       */
      bool      IsIncremental(void) {return mIncremental;}
      void      SetIncremental(bool anIncrValue) {mIncremental=anIncrValue;}

      /**
       * Return the position of the first non-whitespace
       * character. This is only reliable before consumers start
       * reading from this scanner.
       */
      int32_t FirstNonWhitespacePosition()
      {
        return mFirstNonWhitespacePosition;
      }

      /**
       * Override replacement character used by nsIUnicodeDecoder.
       * Default behavior is that it uses nsIUnicodeDecoder's mapping.
       *
       * @param aReplacementCharacter the replacement character
       *        XML (expat) parser uses 0xffff
       */
      void OverrideReplacementCharacter(PRUnichar aReplacementCharacter);

  protected:

      bool AppendToBuffer(nsScannerString::Buffer *, nsIRequest *aRequest, int32_t aErrorPos = -1);
      bool AppendToBuffer(const nsAString& aStr)
      {
        nsScannerString::Buffer* buf = nsScannerString::AllocBufferFromString(aStr);
        if (!buf)
          return false;
        AppendToBuffer(buf, nullptr);
        return true;
      }

      nsScannerString*             mSlidingBuffer;
      nsScannerIterator            mCurrentPosition; // The position we will next read from in the scanner buffer
      nsScannerIterator            mMarkPosition;    // The position last marked (we may rewind to here)
      nsScannerIterator            mEndPosition;     // The current end of the scanner buffer
      nsScannerIterator            mFirstInvalidPosition; // The position of the first invalid character that was detected
      nsString        mFilename;
      uint32_t        mCountRemaining; // The number of bytes still to be read
                                       // from the scanner buffer
      bool            mIncremental;
      bool            mHasInvalidCharacter;
      PRUnichar       mReplacementCharacter;
      int32_t         mFirstNonWhitespacePosition;
      int32_t         mCharsetSource;
      nsCString       mCharset;
      nsCOMPtr<nsIUnicodeDecoder> mUnicodeDecoder;

  private:
      nsScanner &operator =(const nsScanner &); // Not implemented.
};

#endif


