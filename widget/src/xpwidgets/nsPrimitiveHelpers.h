/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 1999 Netscape 
 * Communications Corp.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Mike Pinkerton
 */

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
                                         PRUint32 aDataLen, nsISupports** aPrimitive ) ;

    // Given a nsISupports* primitive and the flavor it represents, creates a new data
    // buffer with the data in it. This data will be null terminated, but the length
    // parameter does not reflect that.
  static void CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, 
                                         void** aDataBuff, PRUint32 aDataLen ) ;

    // Given a unicode buffer (flavor text/unicode), this converts it to plain text using
    // the appropriate platform charset encoding. |inUnicodeLen| is the length of the input
    // string, not the # of bytes in the buffer. |outPlainTextData| is null terminated, 
    // but its length parameter, |outPlainTextLen|, does not reflect that.
  static nsresult ConvertUnicodeToPlatformPlainText ( PRUnichar* inUnicode, PRInt32 inUnicodeLen, 
                                                    char** outPlainTextData, PRInt32* outPlainTextLen ) ;

    // Given a char buffer (flavor text/plaikn), this converts it to unicode using
    // the appropriate platform charset encoding. |outUnicode| is null terminated, 
    // but its length parameter, |outUnicodeLen|, does not reflect that. |outUnicodeLen| is
    // the length of the string in characters, not bytes.
  static nsresult ConvertPlatformPlainTextToUnicode ( const char* inText, PRInt32 inTextLen, 
                                                    PRUnichar** outUnicode, PRInt32* outUnicodeLen ) ;

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
  static nsresult ConvertPlatformToDOMLinebreaks ( const char* inFlavor, void** ioData, PRInt32* ioLengthInBytes ) ;

}; // class nsLinebreakHelpers


#endif // nsPrimitiveHelpers_h___
