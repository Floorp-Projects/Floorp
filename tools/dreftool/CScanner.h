/*
 * Copyright (C) 1998 Rick Gessner.  All Rights Reserved.
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


#ifndef CSCANNER_
#define CSCANNER_


#include "SharedTypes.h"
#include "nsString.h"
#include <fstream.h>


class  CScanner {
  public:


      /**
       *  Use this constructor if you want i/o to be stream based.
       *
       *  @update  gess 5/12/98
       *  @param   aMode represents the parser mode (nav, other)
       *  @return  
       */
      CScanner(istream& aStream);


      ~CScanner();

      /**
       *  retrieve next char from internal input stream
       *  
       *  @update  gess 3/25/98
       *  @param   ch is the char to accept new value
       *  @return  error code reflecting read status
       */
      int getChar(PRUnichar& ch);

      /**
       *  peek ahead to consume next char from scanner's internal
       *  input buffer
       *  
       *  @update  gess 3/25/98
       *  @param   ch is the char to accept new value
       *  @return  error code reflecting read status
       */
      int peek(PRUnichar& ch);

      /**
       *  Push the given char back onto the scanner
       *  
       *  @update  gess 3/25/98
       *  @param   character to be pushed
       *  @return  error code
       */
      int push(PRUnichar ch);

      /**
       *  Determine if the scanner has reached EOF.
       *  This method can also cause the buffer to be filled
       *  if it happens to be empty
       *  
       *  @update  gess 3/25/98
       *  @return  true upon eof condition
       */
      int endOfFile(void);

      /**
       *  Consume characters until you find the terminal char
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   aTerminal contains terminating char
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      int readUntil(nsCString& aString,PRUnichar aTerminal,bool addTerminal);

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
      int readUntil(nsCString& aString,nsCString& aTermSet,bool addTerminal);

      /**
       *  Consume characters while they're members of anInputSet
       *  
       *  @update  gess 3/25/98
       *  @param   aString receives new data from stream
       *  @param   anInputSet contains valid chars
       *  @param   addTerminal tells us whether to append terminal to aString
       *  @return  error code
       */
      int readWhile(nsCString& aString,nsCString& anInputSet,bool addTerminal);


  protected:

      /**
       * Internal method used to cause the internal buffer to
       * be filled with data. 
       *
       * @update  gess4/3/98
       */
      int fillBuffer(void);


      istream&  mFileStream;
      nsCString	mBuffer;
      int       mOffset;
      int       mTotalRead;
      bool      mIncremental;
};

#endif


