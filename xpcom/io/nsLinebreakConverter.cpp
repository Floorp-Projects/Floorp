/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsLinebreakConverter.h"

#include "nsMemory.h"
#include "nsCRT.h"


#if defined(XP_WIN) && defined(_MSC_VER) && (_MSC_VER <= 1100)
#define LOSER_CHAR_CAST(t)       (char *)(t)
#define LOSER_UNICHAR_CAST(t)    (PRUnichar *)(t)
#else
#define LOSER_CHAR_CAST(t)       (t)
#define LOSER_UNICHAR_CAST(t)    (t)
#endif

/*----------------------------------------------------------------------------
	GetLinebreakString 
	
	Could make this inline
----------------------------------------------------------------------------*/
static const char* GetLinebreakString(nsLinebreakConverter::ELinebreakType aBreakType)
{
  static char* sLinebreaks[] = {
    "",             // any
    NS_LINEBREAK,   // platform
    LFSTR,          // content
    CRLF,           // net
    CRSTR,          // Mac
    LFSTR,          // Unix
    CRLF,           // Windows
    nsnull  
  };
  
  return sLinebreaks[aBreakType];
}


/*----------------------------------------------------------------------------
	AppendLinebreak 
	
	Wee inline method to append a line break. Modifies ioDest.
----------------------------------------------------------------------------*/
template<class T>
static void AppendLinebreak(T*& ioDest, const char* lineBreakStr)
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
static PRInt32 CountLinebreaks(const T* aSrc, PRInt32 inLen, const char* breakStr)
{
  const T* src = aSrc;
  const T* srcEnd = aSrc + inLen;
  PRInt32 theCount = 0;

  while (src < srcEnd)
  {
    if (*src == *breakStr)
    {
      src ++;
      if (src < srcEnd && breakStr[1] && *src == breakStr[1])
        src ++;

      theCount ++;
    }
    else
    {
      src ++;
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
  
  T* resultString = nsnull;
   
  // handle the no conversion case
  if (nsCRT::strcmp(srcBreak, destBreak) == 0)
  {
    resultString = (T *)nsMemory::Alloc(sizeof(T) * ioLen);
    if (!resultString) return nsnull;
    nsCRT::memcpy(resultString, inSrc, sizeof(T) * ioLen); // includes the null, if any
    return resultString;
  }
    
  PRInt32 srcBreakLen = nsCRT::strlen(srcBreak);
  PRInt32 destBreakLen = nsCRT::strlen(destBreak);

  // handle the easy case, where the string length does not change, and the
  // breaks are only 1 char long, i.e. CR <-> LF
  if (srcBreakLen == destBreakLen && srcBreakLen == 1)
  {
    resultString = (T *)nsMemory::Alloc(sizeof(T) * ioLen);
    if (!resultString) return nsnull;
    
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
    if (!resultString) return nsnull;
    
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
      
        src ++;
        if (src < srcEnd && srcBreak[1] && *src == srcBreak[1])
          src ++;
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
    
    src ++;
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
  
  PRInt32 destBreakLen = nsCRT::strlen(destBreak);
  PRInt32 finalLen = 0;

  while (src < srcEnd)
  {
    if (*src == nsCRT::CR)
    {
      if (src < srcEnd && src[1] == nsCRT::LF)
      {
        // CRLF
        finalLen += destBreakLen;
        src ++;
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
      finalLen ++;
    }
    src ++;
  }
  
  T* resultString = (T *)nsMemory::Alloc(sizeof(T) * finalLen);
  if (!resultString) return nsnull;

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
        src ++;
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
    src ++;
  }

  ioLen = finalLen;
  return resultString;
}


#ifdef XP_MAC
#pragma mark -
#endif


/*----------------------------------------------------------------------------
	ConvertLineBreaks 
	
----------------------------------------------------------------------------*/
char* nsLinebreakConverter::ConvertLineBreaks(const char* aSrc,
            ELinebreakType aSrcBreaks, ELinebreakType aDestBreaks, PRInt32 aSrcLen, PRInt32* outLen)
{
  NS_ASSERTION(aDestBreaks != eLinebreakAny, "Invalid parameter");
  if (!aSrc) return nsnull;
  
  PRInt32 sourceLen = (aSrcLen == kIgnoreLen) ? nsCRT::strlen(aSrc) + 1 : aSrcLen;

  char* resultString;
  if (aSrcBreaks == eLinebreakAny)
    resultString = ConvertUnknownBreaks(LOSER_CHAR_CAST(aSrc), sourceLen, GetLinebreakString(aDestBreaks));
  else
    resultString = ConvertBreaks(LOSER_CHAR_CAST(aSrc), sourceLen, GetLinebreakString(aSrcBreaks), GetLinebreakString(aDestBreaks));
  
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
  
  NS_ASSERTION(aDestBreaks != eLinebreakAny, "Invalid parameter");

  PRInt32 sourceLen = (aSrcLen == kIgnoreLen) ? nsCRT::strlen(*ioBuffer) + 1 : aSrcLen;
  
  // can we convert in-place?
  const char* srcBreaks = GetLinebreakString(aSrcBreaks);
  const char* dstBreaks = GetLinebreakString(aDestBreaks);
  
  if ( (aSrcBreaks != eLinebreakAny) &&
       (nsCRT::strlen(srcBreaks) == 1) &&
       (nsCRT::strlen(dstBreaks) == 1) )
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
  NS_ASSERTION(aDestBreaks != eLinebreakAny, "Invalid parameter");
  if (!aSrc) return nsnull;
  
  PRInt32 bufLen = (aSrcLen == kIgnoreLen) ? nsCRT::strlen(aSrc) + 1 : aSrcLen;

  PRUnichar* resultString;
  if (aSrcBreaks == eLinebreakAny)
    resultString = ConvertUnknownBreaks(LOSER_UNICHAR_CAST(aSrc), bufLen, GetLinebreakString(aDestBreaks));
  else
    resultString = ConvertBreaks(LOSER_UNICHAR_CAST(aSrc), bufLen, GetLinebreakString(aSrcBreaks), GetLinebreakString(aDestBreaks));
  
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
  NS_ASSERTION(aDestBreaks != eLinebreakAny, "Invalid parameter");

  PRInt32 sourceLen = (aSrcLen == kIgnoreLen) ? nsCRT::strlen(*ioBuffer) + 1 : aSrcLen;

  // can we convert in-place?
  const char* srcBreaks = GetLinebreakString(aSrcBreaks);
  const char* dstBreaks = GetLinebreakString(aDestBreaks);
  
  if ( (aSrcBreaks != eLinebreakAny) &&
       (nsCRT::strlen(srcBreaks) == 1) &&
       (nsCRT::strlen(dstBreaks) == 1) )
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

  NS_ASSERTION(aDestBreaks != eLinebreakAny, "Invalid parameter");

  // nothing to do
  if (ioString.Length() == 0) return NS_OK;
  
  // we can't go messing with data we don't own
  if (!ioString.mOwnsBuffer) return NS_ERROR_UNEXPECTED;
  
  nsresult rv;
  
  if (ioString.mCharSize == eTwoByte)
  {
    PRUnichar  *stringBuf = ioString.mUStr;
    PRInt32    newLen, theLen = ioString.mLength + 1;
    
    rv = ConvertUnicharLineBreaksInSitu(&stringBuf, aSrcBreaks, aDestBreaks, theLen, &newLen);
    if (NS_FAILED(rv)) return rv;
    
    if (stringBuf != ioString.mUStr)  // we reallocated
    {
      // this is pretty evil. Is there a better way?
      nsMemory::Free(ioString.mUStr);
      ioString.mUStr = stringBuf;
      ioString.mLength = newLen - 1;
      ioString.mCapacity = newLen;
    }
  }
  else
  {
    char     *stringBuf = ioString.mStr;
    PRInt32  newLen, theLen = ioString.mLength + 1;
    
    rv = ConvertLineBreaksInSitu(&stringBuf, aSrcBreaks, aDestBreaks, theLen, &newLen);
    if (NS_FAILED(rv)) return rv;
    
    if (stringBuf != ioString.mStr)  // we reallocated
    {
      nsMemory::Free(ioString.mStr);
      ioString.mStr = stringBuf;
      ioString.mLength = newLen - 1;
      ioString.mCapacity = newLen;
    }
  }
  
  return NS_OK;
}



