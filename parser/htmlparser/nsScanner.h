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
#include "mozilla/Encoding.h"
#include "nsScannerString.h"
#include "mozilla/CheckedInt.h"

class nsReadEndCondition {
public:
  const char16_t *mChars;
  char16_t mFilter;
  explicit nsReadEndCondition(const char16_t* aTerminateChars);
private:
  nsReadEndCondition(const nsReadEndCondition& aOther); // No copying
  void operator=(const nsReadEndCondition& aOther); // No assigning
};

class nsScanner final {
      using Encoding = mozilla::Encoding;
      template <typename T> using NotNull = mozilla::NotNull<T>;
  public:

      /**
       *  Use this constructor for the XML fragment parsing case
       */
      explicit nsScanner(const nsAString& anHTMLString);

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
      nsresult GetChar(char16_t& ch);

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
      nsresult Append(const char* aBuffer, uint32_t aLen);

      /**
       *  Call this to copy bytes out of the scanner that have not yet been consumed
       *  by the tokenization process.
       *  
       *  @update  gess 5/12/98
       *  @param   aCopyBuffer is where the scanner buffer will be copied to
       *  @return  true if OK or false on OOM
       */
      bool CopyUnusedData(nsString& aCopyBuffer);

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
      nsresult SetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                  int32_t aSource);

      void BindSubstring(nsScannerSubstring& aSubstring, const nsScannerIterator& aStart, const nsScannerIterator& aEnd);
      void CurrentPosition(nsScannerIterator& aPosition);
      void EndReading(nsScannerIterator& aPosition);
      void SetPosition(nsScannerIterator& aPosition,
                       bool aTruncate = false);

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       */
      bool      IsIncremental(void) {return mIncremental;}
      void      SetIncremental(bool anIncrValue) {mIncremental=anIncrValue;}

  protected:

      bool AppendToBuffer(nsScannerString::Buffer* aBuffer);
      bool AppendToBuffer(const nsAString& aStr)
      {
        nsScannerString::Buffer* buf = nsScannerString::AllocBufferFromString(aStr);
        if (!buf)
          return false;
        AppendToBuffer(buf);
        return true;
      }

      nsScannerString*             mSlidingBuffer;
      nsScannerIterator            mCurrentPosition; // The position we will next read from in the scanner buffer
      nsScannerIterator            mMarkPosition;    // The position last marked (we may rewind to here)
      nsScannerIterator            mEndPosition;     // The current end of the scanner buffer
      nsString        mFilename;
      bool            mIncremental;
      int32_t         mCharsetSource;
      nsCString       mCharset;
      mozilla::UniquePtr<mozilla::Decoder> mUnicodeDecoder;

    private:
      nsScanner &operator =(const nsScanner &); // Not implemented.
};

#endif


