/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
static const char* GetLinebreakString(nsLinebreakConverter::ELinebreakType aBreakType)
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
void AppendLinebreak(T*& ioDest, const char* lineBreakStr)
{
  *ioDest++ = *lineBreakStr;

  if (lineBreakStr[1])
    *ioDest++ = lineBreakStr[1];
}

/*----------------------------------------------------------------------------
	CountChars 
	
	Counts occurrences of breakStr in aSrc
----------------------------------------------------------------------------*/
template<class T>
PRInt32 CountLinebreaks(const T* aSrc, PRInt32 inLen, const char* breakStr)
{
  const T* src = aSrc;
  const T* srcEnd = aSrc + inLen;
  PRInt32 theCount = 0;

  while (src < srcEnd)
  {
    if (*src == *breakStr)
    {
      src++;

      if (breakStr[1])
      {
        if (src < srcEnd && *src == breakStr[1])
        {
          src++;
          theCount++;
        }
      }
      else
      {
        theCount++;
      }
    }
    else
    {
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
static T* ConvertBreaks(const T* inSrc, PRInt32& ioLen, const char* srcBreak, const char* destBreak)
{
  NS_ASSERTION(inSrc && srcBreak && destBreak, "Got a null string");
  
  T* resultString = nullptr;
   
  // handle the no conversion case
  if (nsCRT::strcmp(srcBreak, destBreak) == 0)
  {
    resultString = (T *)nsMemory::Alloc(sizeof(T) * ioLen);
    if (!resultString) return nullptr;
    memcpy(resultString, inSrc, sizeof(T) * ioLen); // includes the null, if any
    return resultString;
  }
    
  PRInt32 srcBreakLen = strlen(srcBreak);
  PRInt32 destBreakLen = strlen(destBreak);

  // handle the easy case, where the string length does not change, and the
  // breaks are only 1 char long, i.e. CR <-> LF
  if (srcBreakLen == destBreakLen && srcBreakLen == 1)
  {
    resultString = (T *)nsMemory::Alloc(sizeof(T) * ioLen);
    if (!resultString) return nullptr;
    
    const T* src = inSrc;
    const T* srcEnd = inSrc + ioLen;		// includes null, if any
    T*       dst = resultString;
    
    char srcBreakChar = *srcBreak;        // we know it's one char long already
    char dstBreakChar = *destBreak;
    
    while (src < srcEnd)
    {
      if (*src == srcBreakChar)
      {
        *dst++ = dstBreakChar;
        src++;
      }
      else
      {
        *dst++ = *src++;
      }
    }

    // ioLen does not change
  }
  else
  {
    // src and dest termination is different length. Do it a slower way.
    
    // count linebreaks in src. Assumes that chars in 2-char linebreaks are unique.
    PRInt32 numLinebreaks = CountLinebreaks(inSrc, ioLen, srcBreak);
    
    PRInt32 newBufLen = ioLen - (numLinebreaks * srcBreakLen) + (numLinebreaks * destBreakLen);
    resultString = (T *)nsMemory::Alloc(sizeof(T) * newBufLen);
    if (!resultString) return nullptr;
    
    const T* src = inSrc;
    const T* srcEnd = inSrc + ioLen;		// includes null, if any
    T*       dst = resultString;
    
    while (src < srcEnd)
    {
      if (*src == *srcBreak)
      {
        *dst++ = *destBreak;
        if (destBreak[1])
          *dst++ = destBreak[1];
      
        src++;
        if (src < srcEnd && srcBreak[1] && *src == srcBreak[1])
          src++;
      }
      else
      {
        *dst++ = *src++;
      }
    }
    
    ioLen = newBufLen;
  }
  
  return resultString;
}


/*----------------------------------------------------------------------------
  ConvertBreaksInSitu 
	
  Convert breaks in situ. Can only do this if the linebreak length
  does not change.
----------------------------------------------------------------------------*/
template<class T>
static void ConvertBreaksInSitu(T* inSrc, PRInt32 inLen, char srcBreak, char destBreak)
{
  T* src = inSrc;
  T* srcEnd = inSrc + inLen;

  while (src < srcEnd)
  {
    if (*src == srcBreak)
      *src = destBreak;
    
    src++;
  }
}


/*----------------------------------------------------------------------------
  ConvertUnknownBreaks 
	
  Convert unknown line breaks to the specified break.
	
  This will convert CRLF pairs to one break, and single CR or LF to a break.
----------------------------------------------------------------------------*/
template<class T>
static T* ConvertUnknownBreaks(const T* inSrc, PRInt32& ioLen, const char* destBreak)
{
  const T* src = inSrc;
  const T* srcEnd = inSrc + ioLen;		// includes null, if any
  
  PRInt32 destBreakLen = strlen(destBreak);
  PRInt32 finalLen = 0;

  while (src < srcEnd)
  {
    if (*src == nsCRT::CR)
    {
      if (src < srcEnd && src[1] == nsCRT::LF)
      {
        // CRLF
        finalLen += destBreakLen;
        src++;
      }
      else
      {
        // Lone CR
        finalLen += destBreakLen;
      }
    }
    else if (*src == nsCRT::LF)
    {
      // Lone LF
      finalLen += destBreakLen;
    }
    else
    {
      finalLen++;
    }
    src++;
  }
  
  T* resultString = (T *)nsMemory::Alloc(sizeof(T) * finalLen);
  if (!resultString) return nullptr;

  src = inSrc;
  srcEnd = inSrc + ioLen;		// includes null, if any

  T* dst = resultString;
  
  while (src < srcEnd)
  {
    if (*src == nsCRT::CR)
    {
      if (src < srcEnd && src[1] == nsCRT::LF)
      {
        // CRLF
        AppendLinebreak(dst, destBreak);
        src++;
      }
      else
      {
        // Lone CR
        AppendLinebreak(dst, destBreak);
      }
    }
    else if (*src == nsCRT::LF)
    {
      // Lone LF
      AppendLinebreak(dst, destBreak);
    }
    else
    {
      *dst++ = *src;
    }
    src++;
  }

  ioLen = finalLen;
  return resultString;
}


/*----------------------------------------------------------------------------
	ConvertLineBreaks 
	
----------------------------------------------------------------------------*/
char* nsLinebreakConverter::ConvertLineBreaks(const char* aSrc,
            ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks, PRInt32 aSrcLen, PRInt32* outLen)
{
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");
  if (!aSrc) return nullptr;
  
  PRInt32 sourceLen = (aSrcLen == kIgnoreLen) ? strlen(aSrc) + 1 : aSrcLen;

  char* resultString;
  if (aSrcBreaks == eLinebreakAny)
    resultString = ConvertUnknownBreaks(aSrc, sourceLen, GetLinebreakString(aDestBreaks));
  else
    resultString = ConvertBreaks(aSrc, sourceLen, GetLinebreakString(aSrcBreaks), GetLinebreakString(aDestBreaks));
  
  if (outLen)
    *outLen = sourceLen;
  return resultString;
}


/*----------------------------------------------------------------------------
	ConvertLineBreaksInSitu 
	
----------------------------------------------------------------------------*/
nsresult nsLinebreakConverter::ConvertLineBreaksInSitu(char **ioBuffer, ELinebreakType aSrcBreaks,
            ELinebreakType aDestBreaks, PRInt32 aSrcLen, PRInt32* outLen)
{
  NS_ASSERTION(ioBuffer && *ioBuffer, "Null pointer passed");
  if (!ioBuffer || !*ioBuffer) return NS_ERROR_NULL_POINTER;
  
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");

  PRInt32 sourceLen = (aSrcLen == kIgnoreLen) ? strlen(*ioBuffer) + 1 : aSrcLen;
  
  // can we convert in-place?
  const char* srcBreaks = GetLinebreakString(aSrcBreaks);
  const char* dstBreaks = GetLinebreakString(aDestBreaks);
  
  if ( (aSrcBreaks != eLinebreakAny) &&
       (strlen(srcBreaks) == 1) &&
       (strlen(dstBreaks) == 1) )
  {
    ConvertBreaksInSitu(*ioBuffer, sourceLen, *srcBreaks, *dstBreaks);
    if (outLen)
      *outLen = sourceLen;
  }
  else
  {
    char* destBuffer;
    
    if (aSrcBreaks == eLinebreakAny)
      destBuffer = ConvertUnknownBreaks(*ioBuffer, sourceLen, dstBreaks);
    else
      destBuffer = ConvertBreaks(*ioBuffer, sourceLen, srcBreaks, dstBreaks);

    if (!destBuffer) return NS_ERROR_OUT_OF_MEMORY;
    *ioBuffer = destBuffer;
    if (outLen)
      *outLen = sourceLen;
  }
  
  return NS_OK;
}


/*----------------------------------------------------------------------------
	ConvertUnicharLineBreaks 
	
----------------------------------------------------------------------------*/
PRUnichar* nsLinebreakConverter::ConvertUnicharLineBreaks(const PRUnichar* aSrc,
            ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks, PRInt32 aSrcLen, PRInt32* outLen)
{
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");
  if (!aSrc) return nullptr;
  
  PRInt32 bufLen = (aSrcLen == kIgnoreLen) ? NS_strlen(aSrc) + 1 : aSrcLen;

  PRUnichar* resultString;
  if (aSrcBreaks == eLinebreakAny)
    resultString = ConvertUnknownBreaks(aSrc, bufLen, GetLinebreakString(aDestBreaks));
  else
    resultString = ConvertBreaks(aSrc, bufLen, GetLinebreakString(aSrcBreaks), GetLinebreakString(aDestBreaks));
  
  if (outLen)
    *outLen = bufLen;
  return resultString;
}


/*----------------------------------------------------------------------------
	ConvertStringLineBreaks 
	
----------------------------------------------------------------------------*/
nsresult nsLinebreakConverter::ConvertUnicharLineBreaksInSitu(PRUnichar **ioBuffer,
            ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks, PRInt32 aSrcLen, PRInt32* outLen)
{
  NS_ASSERTION(ioBuffer && *ioBuffer, "Null pointer passed");
  if (!ioBuffer || !*ioBuffer) return NS_ERROR_NULL_POINTER;
  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");

  PRInt32 sourceLen = (aSrcLen == kIgnoreLen) ? NS_strlen(*ioBuffer) + 1 : aSrcLen;

  // can we convert in-place?
  const char* srcBreaks = GetLinebreakString(aSrcBreaks);
  const char* dstBreaks = GetLinebreakString(aDestBreaks);
  
  if ( (aSrcBreaks != eLinebreakAny) &&
       (strlen(srcBreaks) == 1) &&
       (strlen(dstBreaks) == 1) )
  {
    ConvertBreaksInSitu(*ioBuffer, sourceLen, *srcBreaks, *dstBreaks);
    if (outLen)
      *outLen = sourceLen;
  }
  else
  {
    PRUnichar* destBuffer;
    
    if (aSrcBreaks == eLinebreakAny)
      destBuffer = ConvertUnknownBreaks(*ioBuffer, sourceLen, dstBreaks);
    else
      destBuffer = ConvertBreaks(*ioBuffer, sourceLen, srcBreaks, dstBreaks);

    if (!destBuffer) return NS_ERROR_OUT_OF_MEMORY;
    *ioBuffer = destBuffer;
    if (outLen)
      *outLen = sourceLen;
  }
  
  return NS_OK;
}

/*----------------------------------------------------------------------------
	ConvertStringLineBreaks 
	
----------------------------------------------------------------------------*/
nsresult nsLinebreakConverter::ConvertStringLineBreaks(nsString& ioString,
          ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks)
{

  NS_ASSERTION(aDestBreaks != eLinebreakAny &&
               aSrcBreaks != eLinebreakSpace, "Invalid parameter");

  // nothing to do
  if (ioString.IsEmpty()) return NS_OK;
  
  nsresult rv;
  
  // remember the old buffer in case
  // we blow it away later
  nsString::char_iterator stringBuf;
  ioString.BeginWriting(stringBuf);
  
  PRInt32    newLen;
    
  rv = ConvertUnicharLineBreaksInSitu(&stringBuf,
                                      aSrcBreaks, aDestBreaks,
                                      ioString.Length() + 1, &newLen);
  if (NS_FAILED(rv)) return rv;

  if (stringBuf != ioString.get())
    ioString.Adopt(stringBuf);
  
  return NS_OK;
}



