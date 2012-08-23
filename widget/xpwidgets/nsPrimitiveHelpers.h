/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrimitiveHelpers_h___
#define nsPrimitiveHelpers_h___

#include "prtypes.h"
#include "nsError.h"
#include "nscore.h"

class nsISupports;


class nsPrimitiveHelpers
{
public:

    // Given some data and the flavor it corresponds to, creates the appropriate
    // nsISupports* wrapper for passing across IDL boundaries. The length parameter
    // should not include the null if the data is null terminated.
  static void CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, 
                                         uint32_t aDataLen, nsISupports** aPrimitive ) ;

    // Given a nsISupports* primitive and the flavor it represents, creates a new data
    // buffer with the data in it. This data will be null terminated, but the length
    // parameter does not reflect that.
  static void CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, 
                                         void** aDataBuff, uint32_t aDataLen ) ;

    // Given a unicode buffer (flavor text/unicode), this converts it to plain text using
    // the appropriate platform charset encoding. |inUnicodeLen| is the length of the input
    // string, not the # of bytes in the buffer. |outPlainTextData| is null terminated, 
    // but its length parameter, |outPlainTextLen|, does not reflect that.
  static nsresult ConvertUnicodeToPlatformPlainText ( PRUnichar* inUnicode, int32_t inUnicodeLen, 
                                                    char** outPlainTextData, int32_t* outPlainTextLen ) ;

    // Given a char buffer (flavor text/plaikn), this converts it to unicode using
    // the appropriate platform charset encoding. |outUnicode| is null terminated, 
    // but its length parameter, |outUnicodeLen|, does not reflect that. |outUnicodeLen| is
    // the length of the string in characters, not bytes.
  static nsresult ConvertPlatformPlainTextToUnicode ( const char* inText, int32_t inTextLen, 
                                                    PRUnichar** outUnicode, int32_t* outUnicodeLen ) ;

}; // class nsPrimitiveHelpers



class nsLinebreakHelpers
{
public:

    // Given some data, convert from the platform linebreaks into the LF expected by the
    // DOM. This will attempt to convert the data in place, but the buffer may still need to
    // be reallocated regardless (disposing the old buffer is taken care of internally, see
    // the note below).
    //
    // NOTE: this assumes that it can use nsMemory to dispose of the old buffer.
  static nsresult ConvertPlatformToDOMLinebreaks ( const char* inFlavor, void** ioData, int32_t* ioLengthInBytes ) ;

}; // class nsLinebreakHelpers


#endif // nsPrimitiveHelpers_h___
