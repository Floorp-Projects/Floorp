/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLinebreakConverter_h_
#define nsLinebreakConverter_h_

#include "nscore.h"
#include "nsString.h"

// utility class for converting between different line breaks.

class nsLinebreakConverter
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
    eLinebreakWindows,      // CRLF

    eLinebreakSpace         // space characters. Only valid as destination type
  
  } ELinebreakType;

  enum {
    kIgnoreLen = -1
  };
  
  /* ConvertLineBreaks
   * Convert line breaks in the supplied string, allocating and returning
   * a new buffer. Returns nullptr on failure.
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
                int32_t aSrcLen = kIgnoreLen, int32_t* aOutLen = nullptr);
  

  /* ConvertUnicharLineBreaks
   * Convert line breaks in the supplied string, allocating and returning
   * a new buffer. Returns nullptr on failure.
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
                int32_t aSrcLen = kIgnoreLen, int32_t* aOutLen = nullptr);
  

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
                        int32_t aSrcLen = kIgnoreLen, int32_t* aOutLen = nullptr);


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
                        int32_t aSrcLen = kIgnoreLen, int32_t* aOutLen = nullptr);
    
};

#endif // nsLinebreakConverter_h_
