/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsLinebreakConverter_h_
#define nsLinebreakConverter_h_


#include "nscore.h"
#include "nsString.h"

// utility class for converting between different line breaks.

class NS_COM nsLinebreakConverter
{
public:

  // Note: enum must match char* array in GetLinebreakString
  typedef enum {
    eLinebreakAny,          // any kind of linebreak (i.e. "don't care" source)
    
    eLinebreakPlatform,     // platform linebreak
    eLinebreakContent,      // Content model linebreak (LF)
    eLinebreakNet,          // Form submission linebreak (CRLF)
  
    eLinebreakMac,          // CR
    eLinebreakUnix,         // LF
    eLinebreakWindows       // CRLF
  
  } ELinebreakType;

  enum {
    kIgnoreLen = -1
  };
  
  /* ConvertLineBreaks
   * Convert line breaks in the supplied string, allocating and returning
   * a new buffer. Returns nsnull on failure.
   * @param aSrc: the source string. if aSrcLen == kIgnoreLen this string is assumed
   *              to be null terminated, otherwise it must be at least aSrcLen long.
   * @param aSrcBreaks: the line breaks in the source. If unknown, pass eLinebreakAny.
   *              If known, pass the known value, as this may be more efficient.
   * @param aDestBreaks: the line breaks you want in the output.
   * @param aSrcLen: length of the source. If -1, the source is assumed to be a null-
   *              terminated string.
   * @param aOutLen: used to return character length of returned buffer, if not null.
   */
  static char* ConvertLineBreaks(const char* aSrc,
                ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks,
                PRInt32 aSrcLen = kIgnoreLen, PRInt32* aOutLen = nsnull);
  

  /* ConvertUnicharLineBreaks
   * Convert line breaks in the supplied string, allocating and returning
   * a new buffer. Returns nsnull on failure.
   * @param aSrc: the source string. if aSrcLen == kIgnoreLen this string is assumed
   *              to be null terminated, otherwise it must be at least aSrcLen long.
   * @param aSrcBreaks: the line breaks in the source. If unknown, pass eLinebreakAny.
   *              If known, pass the known value, as this may be more efficient.
   * @param aDestBreaks: the line breaks you want in the output.
   * @param aSrcLen: length of the source, in characters. If -1, the source is assumed to be a null-
   *              terminated string.
   * @param aOutLen: used to return character length of returned buffer, if not null.
   */
  static PRUnichar* ConvertUnicharLineBreaks(const PRUnichar* aSrc,
                ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks,
                PRInt32 aSrcLen = kIgnoreLen, PRInt32* aOutLen = nsnull);
  

  /* ConvertStringLineBreaks
   * Convert line breaks in the supplied string, changing the string buffer (i.e. in-place conversion)
   * @param ioString: the string to be converted.
   * @param aSrcBreaks: the line breaks in the source. If unknown, pass eLinebreakAny.
   *              If known, pass the known value, as this may be more efficient.
   * @param aDestBreaks: the line breaks you want in the output.
   * @param aSrcLen: length of the source, in characters. If -1, the source is assumed to be a null-
   *              terminated string.
   */
  static nsresult ConvertStringLineBreaks(nsString& ioString, ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks);


  /* ConvertLineBreaksInSitu
   * Convert line breaks in place if possible. NOTE: THIS MAY REALLOCATE THE BUFFER,
   * BUT IT WON'T FREE THE OLD BUFFER (because it doesn't know how). So be prepared
   * to keep a copy of the old pointer, and free it if this passes back a new pointer.
   * ALSO NOTE: DON'T PASS A STATIC STRING POINTER TO THIS FUNCTION.
   * 
   * @param ioBuffer: the source buffer. if aSrcLen == kIgnoreLen this string is assumed
   *              to be null terminated, otherwise it must be at least aSrcLen long.
   * @param aSrcBreaks: the line breaks in the source. If unknown, pass eLinebreakAny.
   *              If known, pass the known value, as this may be more efficient.
   * @param aDestBreaks: the line breaks you want in the output.
   * @param aSrcLen: length of the source. If -1, the source is assumed to be a null-
   *              terminated string.
   * @param aOutLen: used to return character length of returned buffer, if not null.
   */
  static nsresult ConvertLineBreaksInSitu(char **ioBuffer, ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks,
                        PRInt32 aSrcLen = kIgnoreLen, PRInt32* aOutLen = nsnull);


  /* ConvertUnicharLineBreaksInSitu
   * Convert line breaks in place if possible. NOTE: THIS MAY REALLOCATE THE BUFFER,
   * BUT IT WON'T FREE THE OLD BUFFER (because it doesn't know how). So be prepared
   * to keep a copy of the old pointer, and free it if this passes back a new pointer.
   * 
   * @param ioBuffer: the source buffer. if aSrcLen == kIgnoreLen this string is assumed
   *              to be null terminated, otherwise it must be at least aSrcLen long.
   * @param aSrcBreaks: the line breaks in the source. If unknown, pass eLinebreakAny.
   *              If known, pass the known value, as this may be more efficient.
   * @param aDestBreaks: the line breaks you want in the output.
   * @param aSrcLen: length of the source in characters. If -1, the source is assumed to be a null-
   *              terminated string.
   * @param aOutLen: used to return character length of returned buffer, if not null.
   */
  static nsresult ConvertUnicharLineBreaksInSitu(PRUnichar **ioBuffer, ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks,
                        PRInt32 aSrcLen = kIgnoreLen, PRInt32* aOutLen = nsnull);
    
};




#endif // nsLinebreakConverter_h_
