/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLinebreakConverter.h"

#include "nsMemory.h"
#include "nsCRT.h"


/*----------------------------------------------------------------------------
	GetLinebreakString

	Could make this inline
----------------------------------------------------------------------------*/
static const char*
GetLinebreakString(nsLinebreakConverter::ELinebreakType aBreakType)
{
  static const char* const sLinebreaks[] = {
    "",             // any
    NS_LINEBREAK,   // platform
    LFSTR,          // content
    CRLF,           // net
    CRSTR,          // Mac
    LFSTR,          // Unix
    CRLF,           // Windows
    " ",            // space
    nullptr
  };

  return sLinebreaks[aBreakType];
}


/*----------------------------------------------------------------------------
	AppendLinebreak

	Wee inline method to append a line break. Modifies ioDest.
----------------------------------------------------------------------------*/
template<class T>
void
AppendLinebreak(T*& aIoDest, const char* aLineBreakStr)
{
  *aIoDest++ = *aLineBreakStr;

  if (aLineBreakStr[1]) {
    *aIoDest++ = aLineBreakStr[1];
  }
}

/*----------------------------------------------------------------------------
	CountChars

	Counts occurrences of breakStr in aSrc
----------------------------------------------------------------------------*/
template<class T>
int32_t
CountLinebreaks(const T* aSrc, int32_t aInLen, const char* aBreakStr)
{
  const T* src = aSrc;
  const T* srcEnd = aSrc + aInLen;
  int32_t theCount = 0;

  while (src < srcEnd) {
    if (*src == *aBreakStr) {
      src++;

      if (aBreakStr[1]) {
        if (src < srcEnd && *src == aBreakStr[1]) {
          src++;
          theCount++;
        }
      } else {
        theCount++;
      }
    } else {
      src++;
    }
  }

  return theCount;
}


/*----------------------------------------------------------------------------
	ConvertBreaks

	ioLen *includes* a terminating null, if any
----------------------------------------------------------------------------*/
template<class T>
static T*
ConvertBreaks(const T* aInSrc, int32_t& aIoLen, const char* aSrcBreak,
              const char* aDestBreak)
{
  NS_ASSERTION(aInSrc && aSrcBreak && aDestBreak, "Got a null string");

  T* resultString = nullptr;

  // handle the no conversion case
  if (nsCRT::strcmp(aSrcBreak, aDestBreak) == 0) {
    resultString = (T*)nsMemory::Alloc(sizeof(T) * aIoLen);
    if (!resultString) {
      return nullptr;
    }
    memcpy(resultString, aInSrc, sizeof(T) * aIoLen); // includes the null, if any
    return resultString;
  }

  int32_t srcBreakLen = strlen(aSrcBreak);
  int32_t destBreakLen = strlen(aDestBreak);

  // handle the easy case, where the string length does not change, and the
  // breaks are only 1 char long, i.e. CR <-> LF
  if (srcBreakLen == destBreakLen && srcBreakLen == 1) {
    resultString = (T*)nsMemory::Alloc(sizeof(T) * aIoLen);
    if (!resultString) {
      return nullptr;
    }

    const T* src = aInSrc;
    const T* srcEnd = aInSrc + aIoLen;  // includes null, if any
    T* dst = resultString;

    char srcBreakChar = *aSrcBreak;  // we know it's one char long already
    char dstBreakChar = *aDestBreak;

    while (src < srcEnd) {
      if (*src == srcBreakChar) {
        *dst++ = dstBreakChar;
        src++;
      } else {
        *dst++ = *src++;
      }
    }

    // aIoLen does not change
  } else {
    // src and dest termination is different length. Do it a slower way.

    // count linebreaks in src. Assumes that chars in 2-char linebreaks are unique.
    int32_t numLinebreaks = CountLinebreaks(aInSrc, aIoLen, aSrcBreak);

    int32_t newBufLen =
      aIoLen - (numLinebreaks * srcBreakLen) + (numLinebreaks * destBreakLen);
    resultString = (T*)nsMemory::Alloc(sizeof(T) * newBufLen);
    if (!resultString) {
      return nullptr;
    }

    const T* src = aInSrc;
    const T* srcEnd = aInSrc + aIoLen;  // includes null, if any
    T* dst = resultString;

    while (src < srcEnd) {
      if (*src == *aSrcBreak) {
        *dst++ = *aDestBreak;
        if (aDestBreak[1]) {
          *dst++ = aDestBreak[1];
        }

        src++;
        if (src < srcEnd && aSrcBreak[1] && *src == aSrcBreak[1]) {
          src++;
        }
      } else {
        *dst++ = *src++;
      }
    }

    aIoLen = newBufLen;
  }

  return resultString;
}


/*----------------------------------------------------------------------------
  ConvertBreaksInSitu

  Convert breaks in situ. Can only do this if the linebreak length
  does not change.
----------------------------------------------------------------------------*/
template<class T>
static void
ConvertBreaksInSitu(T* aInSrc, int32_t aInLen, char aSrcBreak, char aDestBreak)
{
  T* src = aInSrc;
  T* srcEnd = aInSrc + aInLen;

  while (src < srcEnd) {
    if (*src == aSrcBreak) {
      *src = aDestBreak;
    }

    src++;
  }
}


/*----------------------------------------------------------------------------
  ConvertUnknownBreaks

  Convert unknown line breaks to the specified break.

  This will convert CRLF pairs to one break, and single CR or LF to a break.
----------------------------------------------------------------------------*/
template<class T>
static T*
ConvertUnknownBreaks(const T* aInSrc, int32_t& aIoLen, const char* aDestBreak)
{
  const T* src = aInSrc;
  const T* srcEnd = aInSrc + aIoLen;  // includes null, if any

  int32_t destBreakLen = strlen(aDestBreak);
  int32_t finalLen = 0;

  while (src < srcEnd) {
    if (*src == nsCRT::CR) {
      if (src < srcEnd && src[1] == nsCRT::LF) {
        // CRLF
        finalLen += destBreakLen;
        src++;
      } else {
        // Lone CR
        finalLen += destBreakLen;
      }
    } else if (*src == nsCRT::LF) {
      // Lone LF
      finalLen += destBreakLen;
    } else {
      finalLen++;
    }
    src++;
  }

  T* resultString = (T*)nsMemory::Alloc(sizeof(T) * finalLen);
  if (!resultString) {
    return nullptr;
  }

  src = aInSrc;
  srcEnd = aInSrc + aIoLen;  // includes null, if any

  T* dst = resultString;

  while (src < srcEnd) {
    if (*src == nsCRT::CR) {
      if (src < srcEnd && src[1] == nsCRT::LF) {
        // CRLF
        AppendLinebreak(dst, aDestBreak);
        src++;
      } else {
        // Lone CR
        AppendLinebreak(dst, aDestBreak);
      }
    } else if (*src == nsCRT::LF) {
      // Lone LF
      AppendLinebreak(dst, aDestBreak);
    } else {
      *dst++ = *src;
    }
    src++;
  }

  aIoLen = finalLen;
  return resultString;
}


/*----------------------------------------------------------------------------
	ConvertLineBreaks

----------------------------------------------------------------------------*/
char*
nsLinebreakConverter::ConvertLineBreaks(const char* aSrc,
                                        ELinebreakType aSrcBreaks,
                                        ELinebreakType aDestBreaks,
                                        int32_t aSrcLen, int32_t* aOutLen)
{
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");
  if (!aSrc) {
    return nullptr;
  }

  int32_t sourceLen = (aSrcLen == kIgnoreLen) ? strlen(aSrc) + 1 : aSrcLen;

  char* resultString;
  if (aSrcBreaks == eLinebreakAny) {
    resultString = ConvertUnknownBreaks(aSrc, sourceLen, GetLinebreakString(aDestBreaks));
  } else
    resultString = ConvertBreaks(aSrc, sourceLen,
                                 GetLinebreakString(aSrcBreaks),
                                 GetLinebreakString(aDestBreaks));

  if (aOutLen) {
    *aOutLen = sourceLen;
  }
  return resultString;
}


/*----------------------------------------------------------------------------
	ConvertLineBreaksInSitu

----------------------------------------------------------------------------*/
nsresult
nsLinebreakConverter::ConvertLineBreaksInSitu(char** aIoBuffer,
                                              ELinebreakType aSrcBreaks,
                                              ELinebreakType aDestBreaks,
                                              int32_t aSrcLen, int32_t* aOutLen)
{
  NS_ASSERTION(aIoBuffer && *aIoBuffer, "Null pointer passed");
  if (!aIoBuffer || !*aIoBuffer) {
    return NS_ERROR_NULL_POINTER;
  }

  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");

  int32_t sourceLen = (aSrcLen == kIgnoreLen) ? strlen(*aIoBuffer) + 1 : aSrcLen;

  // can we convert in-place?
  const char* srcBreaks = GetLinebreakString(aSrcBreaks);
  const char* dstBreaks = GetLinebreakString(aDestBreaks);

  if (aSrcBreaks != eLinebreakAny &&
      strlen(srcBreaks) == 1 &&
      strlen(dstBreaks) == 1) {
    ConvertBreaksInSitu(*aIoBuffer, sourceLen, *srcBreaks, *dstBreaks);
    if (aOutLen) {
      *aOutLen = sourceLen;
    }
  } else {
    char* destBuffer;

    if (aSrcBreaks == eLinebreakAny) {
      destBuffer = ConvertUnknownBreaks(*aIoBuffer, sourceLen, dstBreaks);
    } else {
      destBuffer = ConvertBreaks(*aIoBuffer, sourceLen, srcBreaks, dstBreaks);
    }

    if (!destBuffer) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *aIoBuffer = destBuffer;
    if (aOutLen) {
      *aOutLen = sourceLen;
    }
  }

  return NS_OK;
}


/*----------------------------------------------------------------------------
	ConvertUnicharLineBreaks

----------------------------------------------------------------------------*/
char16_t*
nsLinebreakConverter::ConvertUnicharLineBreaks(const char16_t* aSrc,
                                               ELinebreakType aSrcBreaks,
                                               ELinebreakType aDestBreaks,
                                               int32_t aSrcLen,
                                               int32_t* aOutLen)
{
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");
  if (!aSrc) {
    return nullptr;
  }

  int32_t bufLen = (aSrcLen == kIgnoreLen) ? NS_strlen(aSrc) + 1 : aSrcLen;

  char16_t* resultString;
  if (aSrcBreaks == eLinebreakAny) {
    resultString = ConvertUnknownBreaks(aSrc, bufLen, GetLinebreakString(aDestBreaks));
  } else
    resultString = ConvertBreaks(aSrc, bufLen, GetLinebreakString(aSrcBreaks),
                                 GetLinebreakString(aDestBreaks));

  if (aOutLen) {
    *aOutLen = bufLen;
  }
  return resultString;
}


/*----------------------------------------------------------------------------
	ConvertStringLineBreaks

----------------------------------------------------------------------------*/
nsresult
nsLinebreakConverter::ConvertUnicharLineBreaksInSitu(
    char16_t** aIoBuffer, ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks,
    int32_t aSrcLen, int32_t* aOutLen)
{
  NS_ASSERTION(aIoBuffer && *aIoBuffer, "Null pointer passed");
  if (!aIoBuffer || !*aIoBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");

  int32_t sourceLen =
    (aSrcLen == kIgnoreLen) ? NS_strlen(*aIoBuffer) + 1 : aSrcLen;

  // can we convert in-place?
  const char* srcBreaks = GetLinebreakString(aSrcBreaks);
  const char* dstBreaks = GetLinebreakString(aDestBreaks);

  if ((aSrcBreaks != eLinebreakAny) &&
      (strlen(srcBreaks) == 1) &&
      (strlen(dstBreaks) == 1)) {
    ConvertBreaksInSitu(*aIoBuffer, sourceLen, *srcBreaks, *dstBreaks);
    if (aOutLen) {
      *aOutLen = sourceLen;
    }
  } else {
    char16_t* destBuffer;

    if (aSrcBreaks == eLinebreakAny) {
      destBuffer = ConvertUnknownBreaks(*aIoBuffer, sourceLen, dstBreaks);
    } else {
      destBuffer = ConvertBreaks(*aIoBuffer, sourceLen, srcBreaks, dstBreaks);
    }

    if (!destBuffer) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *aIoBuffer = destBuffer;
    if (aOutLen) {
      *aOutLen = sourceLen;
    }
  }

  return NS_OK;
}

/*----------------------------------------------------------------------------
	ConvertStringLineBreaks

----------------------------------------------------------------------------*/
nsresult
nsLinebreakConverter::ConvertStringLineBreaks(nsString& aIoString,
                                              ELinebreakType aSrcBreaks,
                                              ELinebreakType aDestBreaks)
{

  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");

  // nothing to do
  if (aIoString.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;

  // remember the old buffer in case
  // we blow it away later
  nsString::char_iterator stringBuf;
  aIoString.BeginWriting(stringBuf);

  int32_t    newLen;

  rv = ConvertUnicharLineBreaksInSitu(&stringBuf,
                                      aSrcBreaks, aDestBreaks,
                                      aIoString.Length() + 1, &newLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (stringBuf != aIoString.get()) {
    aIoString.Adopt(stringBuf);
  }

  return NS_OK;
}



