/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   rhp@netscape.com
 *   Jungshik Shin <jshin@mailaps.org>
 *   John G Myers   <jgmyers@netscape.com>
 *   Takayuki Tei   <taka@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <string.h>
#include "prtypes.h"
#include "prmem.h"
#include "prprf.h"
#include "plstr.h"
#include "plbase64.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsIUTF8ConverterService.h"
#include "nsUConvCID.h"
#include "nsIServiceManager.h"
#include "nsMIMEHeaderParamImpl.h"
#include "nsReadableUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsNetError.h"

// static functions declared below are moved from mailnews/mime/src/comi18n.cpp
  
static char *DecodeQ(const char *, PRUint32);
static bool Is7bitNonAsciiString(const char *, PRUint32);
static void CopyRawHeader(const char *, PRUint32, const char *, nsACString &);
static nsresult DecodeRFC2047Str(const char *, const char *, bool, nsACString&);

// XXX The chance of UTF-7 being used in the message header is really
// low, but in theory it's possible. 
#define IS_7BIT_NON_ASCII_CHARSET(cset)            \
    (!nsCRT::strncasecmp((cset), "ISO-2022", 8) || \
     !nsCRT::strncasecmp((cset), "HZ-GB", 5)    || \
     !nsCRT::strncasecmp((cset), "UTF-7", 5))   

NS_IMPL_ISUPPORTS1(nsMIMEHeaderParamImpl, nsIMIMEHeaderParam)

NS_IMETHODIMP 
nsMIMEHeaderParamImpl::GetParameter(const nsACString& aHeaderVal, 
                                    const char *aParamName,
                                    const nsACString& aFallbackCharset, 
                                    bool aTryLocaleCharset, 
                                    char **aLang, nsAString& aResult)
{
  return DoGetParameter(aHeaderVal, aParamName, RFC_2231_DECODING,
                        aFallbackCharset, aTryLocaleCharset, aLang, aResult);
}

NS_IMETHODIMP 
nsMIMEHeaderParamImpl::GetParameter5987(const nsACString& aHeaderVal, 
                                        const char *aParamName,
                                        const nsACString& aFallbackCharset, 
                                        bool aTryLocaleCharset, 
                                        char **aLang, nsAString& aResult)
{
  return DoGetParameter(aHeaderVal, aParamName, RFC_5987_DECODING,
                        aFallbackCharset, aTryLocaleCharset, aLang, aResult);
}

// XXX : aTryLocaleCharset is not yet effective.
nsresult 
nsMIMEHeaderParamImpl::DoGetParameter(const nsACString& aHeaderVal, 
                                      const char *aParamName,
                                      ParamDecoding aDecoding,
                                      const nsACString& aFallbackCharset, 
                                      bool aTryLocaleCharset, 
                                      char **aLang, nsAString& aResult)
{
    aResult.Truncate();
    nsresult rv;

    // get parameter (decode RFC 2231/5987 when applicable, as specified by
    // aDecoding (5987 being a subset of 2231) and return charset.)
    nsXPIDLCString med;
    nsXPIDLCString charset;
    rv = DoParameterInternal(PromiseFlatCString(aHeaderVal).get(), aParamName, 
                             aDecoding, getter_Copies(charset), aLang, 
                             getter_Copies(med));
    if (NS_FAILED(rv))
        return rv; 

    // convert to UTF-8 after charset conversion and RFC 2047 decoding 
    // if necessary.
    
    nsCAutoString str1;
    rv = DecodeParameter(med, charset.get(), nsnull, false, str1);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aFallbackCharset.IsEmpty())
    {
        nsCAutoString str2;
        nsCOMPtr<nsIUTF8ConverterService> 
          cvtUTF8(do_GetService(NS_UTF8CONVERTERSERVICE_CONTRACTID));
        if (cvtUTF8 &&
            NS_SUCCEEDED(cvtUTF8->ConvertStringToUTF8(str1, 
                PromiseFlatCString(aFallbackCharset).get(), false, str2))) {
          CopyUTF8toUTF16(str2, aResult);
          return NS_OK;
        }
    }

    if (IsUTF8(str1)) {
      CopyUTF8toUTF16(str1, aResult);
      return NS_OK;
    }

    if (aTryLocaleCharset && !NS_IsNativeUTF8()) 
      return NS_CopyNativeToUnicode(str1, aResult);

    CopyASCIItoUTF16(str1, aResult);
    return NS_OK;
}

// remove backslash-encoded sequences from quoted-strings
// modifies string in place, potentially shortening it
void RemoveQuotedStringEscapes(char *src)
{
  char *dst = src;

  for (char *c = src; *c; ++c)
  {
    if (c[0] == '\\' && c[1])
    {
      // skip backslash if not at end
      ++c;
    }
    *dst++ = *c;
  }
  *dst = 0;
}

// Support for continuations (RFC 2231, Section 3)

// only a sane number supported
#define MAX_CONTINUATIONS 999

// part of a continuation

class Continuation {
  public:
    Continuation(const char *aValue, PRUint32 aLength,
                 bool aNeedsPercentDecoding) {
      value = aValue;
      length = aLength;
      needsPercentDecoding = aNeedsPercentDecoding;
    }
    Continuation() {
      // empty constructor needed for nsTArray
      value = 0L;
      length = 0;
      needsPercentDecoding = false;
    }
    ~Continuation() {}

    const char *value;
    PRUint32 length;
    bool needsPercentDecoding;
};

// combine segments into a single string, returning the allocated string
// (or NULL) while emptying the list 
char *combineContinuations(nsTArray<Continuation>& aArray)
{
  // Sanity check
  if (aArray.Length() == 0)
    return NULL;

  // Get an upper bound for the length
  PRUint32 length = 0;
  for (PRUint32 i = 0; i < aArray.Length(); i++) {
    length += aArray[i].length;
  }

  // Allocate
  char *result = (char *) nsMemory::Alloc(length + 1);

  // Concatenate
  if (result) {
    *result = '\0';

    for (PRUint32 i = 0; i < aArray.Length(); i++) {
      Continuation cont = aArray[i];
      if (! cont.value) break;

      char *c = result + strlen(result);
      strncat(result, cont.value, cont.length);
      if (cont.needsPercentDecoding) {
        nsUnescape(c);
      }
    }

    // return null if empty value
    if (*result == '\0') {
      nsMemory::Free(result);
      result = NULL;
    }
  } else {
    // Handle OOM
    NS_WARNING("Out of memory\n");
  }

  return result;
}

// add a continuation, return false on error if segment already has been seen
bool addContinuation(nsTArray<Continuation>& aArray, PRUint32 aIndex,
                     const char *aValue, PRUint32 aLength,
                     bool aNeedsPercentDecoding)
{
  if (aIndex < aArray.Length() && aArray[aIndex].value) {
    NS_WARNING("duplicate RC2231 continuation segment #\n");
    return false;
  }

  if (aIndex > MAX_CONTINUATIONS) {
    NS_WARNING("RC2231 continuation segment # exceeds limit\n");
    return false;
  }

  Continuation cont (aValue, aLength, aNeedsPercentDecoding);

  if (aArray.Length() <= aIndex) {
    aArray.SetLength(aIndex + 1);
  }
  aArray[aIndex] = cont;

  return true;
}

// parse a segment number; return -1 on error
PRInt32 parseSegmentNumber(const char *aValue, PRInt32 aLen)
{
  if (aLen < 1) {
    NS_WARNING("segment number missing\n");
    return -1;
  }

  if (aLen > 1 && aValue[0] == '0') {
    NS_WARNING("leading '0' not allowed in segment number\n");
    return -1;
  }

  PRInt32 segmentNumber = 0;

  for (PRInt32 i = 0; i < aLen; i++) {
    if (! (aValue[i] >= '0' && aValue[i] <= '9')) {
      NS_WARNING("invalid characters in segment number\n");
      return -1;
    }

    segmentNumber *= 10;
    segmentNumber += aValue[i] - '0';
    if (segmentNumber > MAX_CONTINUATIONS) {
      NS_WARNING("Segment number exceeds sane size\n");
      return -1;
    }
  }

  return segmentNumber;
}

// validate a given octet sequence for compliance with the specified
// encoding
bool IsValidOctetSequenceForCharset(nsACString& aCharset, const char *aOctets)
{
  nsCOMPtr<nsIUTF8ConverterService> cvtUTF8(do_GetService
    (NS_UTF8CONVERTERSERVICE_CONTRACTID));
  if (!cvtUTF8) {
    NS_WARNING("Can't get UTF8ConverterService\n");
    return false;
  }

  nsCAutoString tmpRaw;
  tmpRaw.Assign(aOctets);
  nsCAutoString tmpDecoded;

  nsresult rv = cvtUTF8->ConvertStringToUTF8(tmpRaw,
                                             PromiseFlatCString(aCharset).get(),
                                             true, tmpDecoded);

  if (rv != NS_OK) {
    // we can't decode; charset may be unsupported, or the octet sequence
    // is broken (illegal or incomplete octet sequence contained)
    NS_WARNING("RFC2231/5987 parameter value does not decode according to specified charset\n");
    return false;
  }

  return true;
}

// moved almost verbatim from mimehdrs.cpp
// char *
// MimeHeaders_get_parameter (const char *header_value, const char *parm_name,
//                            char **charset, char **language)
//
// The format of these header lines  is
// <token> [ ';' <token> '=' <token-or-quoted-string> ]*
NS_IMETHODIMP 
nsMIMEHeaderParamImpl::GetParameterInternal(const char *aHeaderValue, 
                                            const char *aParamName,
                                            char **aCharset,
                                            char **aLang,
                                            char **aResult)
{
  return DoParameterInternal(aHeaderValue, aParamName, RFC_2231_DECODING,
                             aCharset, aLang, aResult);
}


nsresult 
nsMIMEHeaderParamImpl::DoParameterInternal(const char *aHeaderValue, 
                                           const char *aParamName,
                                           ParamDecoding aDecoding,
                                           char **aCharset,
                                           char **aLang,
                                           char **aResult)
{

  if (!aHeaderValue ||  !*aHeaderValue || !aResult)
    return NS_ERROR_INVALID_ARG;

  *aResult = nsnull;

  if (aCharset) *aCharset = nsnull;
  if (aLang) *aLang = nsnull;

  nsCAutoString charset;

  bool acceptContinuations = (aDecoding != RFC_5987_DECODING);

  const char *str = aHeaderValue;

  // skip leading white space.
  for (; *str &&  nsCRT::IsAsciiSpace(*str); ++str)
    ;
  const char *start = str;
  
  // aParamName is empty. return the first (possibly) _unnamed_ 'parameter'
  // For instance, return 'inline' in the following case:
  // Content-Disposition: inline; filename=.....
  if (!aParamName || !*aParamName) 
    {
      for (; *str && *str != ';' && !nsCRT::IsAsciiSpace(*str); ++str)
        ;
      if (str == start)
        return NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY;

      *aResult = (char *) nsMemory::Clone(start, (str - start) + 1);
      NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
      (*aResult)[str - start] = '\0';  // null-terminate
      return NS_OK;
    }

  /* Skip forward to first ';' */
  for (; *str && *str != ';' && *str != ','; ++str)
    ;
  if (*str)
    str++;
  /* Skip over following whitespace */
  for (; *str && nsCRT::IsAsciiSpace(*str); ++str)
    ;

  // Some broken http servers just specify parameters
  // like 'filename' without specifying disposition
  // method. Rewind to the first non-white-space
  // character.
  
  if (!*str)
    str = start;

  // RFC2231 - The legitimate parm format can be:
  // A. title=ThisIsTitle 
  // B. title*=us-ascii'en-us'This%20is%20wierd.
  // C. title*0*=us-ascii'en'This%20is%20wierd.%20We
  //    title*1*=have%20to%20support%20this.
  //    title*2="Else..."
  // D. title*0="Hey, what you think you are doing?"
  //    title*1="There is no charset and lang info."
  // RFC5987: only A and B
  
  // collect results for the different algorithms (plain filename,
  // RFC5987/2231-encoded filename, + continuations) separately and decide
  // which to use at the end
  char *caseAResult = NULL;
  char *caseBResult = NULL;
  char *caseCDResult = NULL;

  // collect continuation segments
  nsTArray<Continuation> segments;


  // our copies of the charset parameter, kept separately as they might
  // differ for the two formats
  nsDependentCSubstring charsetB, charsetCD;

  nsDependentCSubstring lang;

  PRInt32 paramLen = strlen(aParamName);

  while (*str) {
    // find name/value

    const char *nameStart = str;
    const char *nameEnd = NULL;
    const char *valueStart = str;
    const char *valueEnd = NULL;
    bool isQuotedString = false;

    NS_ASSERTION(!nsCRT::IsAsciiSpace(*str), "should be after whitespace.");

    // Skip forward to the end of this token. 
    for (; *str && !nsCRT::IsAsciiSpace(*str) && *str != '=' && *str != ';'; str++)
      ;
    nameEnd = str;

    PRInt32 nameLen = nameEnd - nameStart;

    // Skip over whitespace, '=', and whitespace
    while (nsCRT::IsAsciiSpace(*str)) ++str;
    if (!*str) {
      break;
    }
    if (*str++ != '=') {
      // don't accept parameters without "="
      goto increment_str;
    }
    while (nsCRT::IsAsciiSpace(*str)) ++str;

    if (*str != '"') {
      // The value is a token, not a quoted string.
      valueStart = str;
      for (valueEnd = str;
           *valueEnd && !nsCRT::IsAsciiSpace (*valueEnd) && *valueEnd != ';';
           valueEnd++)
        ;
      str = valueEnd;
    } else {
      isQuotedString = true;
      
      ++str;
      valueStart = str;
      for (valueEnd = str; *valueEnd; ++valueEnd) {
        if (*valueEnd == '\\')
          ++valueEnd;
        else if (*valueEnd == '"')
          break;
      }
      str = valueEnd;
      // *valueEnd != null means that *valueEnd is quote character.
      if (*valueEnd)
        str++;
    }

    // See if this is the simplest case (case A above),
    // a 'single' line value with no charset and lang.
    // If so, copy it and return.
    if (nameLen == paramLen &&
        !nsCRT::strncasecmp(nameStart, aParamName, paramLen)) {

      if (caseAResult) {
        // we already have one caseA result, ignore subsequent ones
        goto increment_str;
      }

      // if the parameter spans across multiple lines we have to strip out the
      //     line continuation -- jht 4/29/98 
      nsCAutoString tempStr(valueStart, valueEnd - valueStart);
      tempStr.StripChars("\r\n");
      char *res = ToNewCString(tempStr);
      NS_ENSURE_TRUE(res, NS_ERROR_OUT_OF_MEMORY);
      
      if (isQuotedString)
        RemoveQuotedStringEscapes(res);

      caseAResult = res;
      // keep going, we may find a RFC 2231/5987 encoded alternative
    }
    // case B, C, and D
    else if (nameLen > paramLen &&
             !nsCRT::strncasecmp(nameStart, aParamName, paramLen) &&
             *(nameStart + paramLen) == '*') {

      // 1st char past '*'       
      const char *cp = nameStart + paramLen + 1; 

      // if param name ends in "*" we need do to RFC5987 "ext-value" decoding
      bool needExtDecoding = *(nameEnd - 1) == '*';      

      bool caseB = nameLen == paramLen + 1;
      bool caseCStart = (*cp == '0') && needExtDecoding;

      // parse the segment number
      PRInt32 segmentNumber = -1;
      if (!caseB) {
        PRInt32 segLen = (nameEnd - cp) - (needExtDecoding ? 1 : 0);
        segmentNumber = parseSegmentNumber(cp, segLen);

        if (segmentNumber == -1) {
          acceptContinuations = false;
          goto increment_str;
        }
      }

      // CaseB and start of CaseC: requires charset and optional language
      // in quotes (quotes required even if lang is blank)
      if (caseB || (caseCStart && acceptContinuations)) {
        // look for single quotation mark(')
        const char *sQuote1 = PL_strchr(valueStart, 0x27);
        const char *sQuote2 = sQuote1 ? PL_strchr(sQuote1 + 1, 0x27) : nsnull;

        // Two single quotation marks must be present even in
        // absence of charset and lang. 
        if (!sQuote1 || !sQuote2) {
          NS_WARNING("Mandatory two single quotes are missing in header parameter\n");
        }

        const char *charsetStart = NULL;
        PRInt32 charsetLength = 0;
        const char *langStart = NULL;
        PRInt32 langLength = 0;
        const char *rawValStart = NULL;
        PRInt32 rawValLength = 0;

        if (sQuote2 && sQuote1) {
          // both delimiters present: charSet'lang'rawVal
          rawValStart = sQuote2 + 1;
          rawValLength = valueEnd - rawValStart;

          langStart = sQuote1 + 1;
          langLength = sQuote2 - langStart;

          charsetStart = valueStart;
          charsetLength = sQuote1 - charsetStart;
        }
        else if (sQuote1) {
          // one delimiter; assume charset'rawVal
          rawValStart = sQuote1 + 1;
          rawValLength = valueEnd - rawValStart;

          charsetStart = valueStart;
          charsetLength = sQuote1 - valueStart;
        }
        else {
          // no delimiter: just rawVal
          rawValStart = valueStart;
          rawValLength = valueEnd - valueStart;
        }

        if (langLength != 0) {
          lang.Assign(langStart, langLength);
        }

        // keep the charset for later
        if (caseB) {
          charsetB.Assign(charsetStart, charsetLength);
        } else {
          // if caseCorD
          charsetCD.Assign(charsetStart, charsetLength);
        }

        // non-empty value part
        if (rawValLength > 0) {
          if (!caseBResult && caseB) {
            // allocate buffer for the raw value
            char *tmpResult = (char *) nsMemory::Clone(rawValStart, rawValLength + 1);
            if (!tmpResult) {
              goto increment_str;
            }
            *(tmpResult + rawValLength) = 0;

            nsUnescape(tmpResult);
            caseBResult = tmpResult;
          } else {
            // caseC
            bool added = addContinuation(segments, 0, rawValStart,
                                         rawValLength, needExtDecoding);

            if (!added) {
              // continuation not added, stop processing them
              acceptContinuations = false;
            }
          }
        }
      }  // end of if-block :  title*0*=  or  title*= 
      // caseD: a line of multiline param with no need for unescaping : title*[0-9]=
      // or 2nd or later lines of a caseC param : title*[1-9]*= 
      else if (acceptContinuations && segmentNumber != -1) {
        PRUint32 valueLength = valueEnd - valueStart;

        bool added = addContinuation(segments, segmentNumber, valueStart,
                                     valueLength, needExtDecoding);

        if (!added) {
          // continuation not added, stop processing them
          acceptContinuations = false;
        }
      } // end of if-block :  title*[0-9]= or title*[1-9]*=
    }

    // str now points after the end of the value.
    //   skip over whitespace, ';', whitespace.
increment_str:      
    while (nsCRT::IsAsciiSpace(*str)) ++str;
    if (*str == ';') ++str;
    while (nsCRT::IsAsciiSpace(*str)) ++str;
  }

  caseCDResult = combineContinuations(segments);

  if (caseBResult && !charsetB.IsEmpty()) {
    // check that the 2231/5987 result decodes properly given the
    // specified character set
    if (!IsValidOctetSequenceForCharset(charsetB, caseBResult))
      caseBResult = NULL;
  }

  if (caseCDResult && !charsetCD.IsEmpty()) {
    // check that the 2231/5987 result decodes properly given the
    // specified character set
    if (!IsValidOctetSequenceForCharset(charsetCD, caseCDResult))
      caseCDResult = NULL;
  }

  if (caseBResult) {
    // prefer simple 5987 format over 2231 with continuations
    *aResult = caseBResult;
    caseBResult = NULL;
    charset.Assign(charsetB);
  }
  else if (caseCDResult) {
    // prefer 2231/5987 with or without continuations over plain format
    *aResult = caseCDResult;
    caseCDResult = NULL;
    charset.Assign(charsetCD);
  }
  else if (caseAResult) {
    *aResult = caseAResult;
    caseAResult = NULL;
  }

  // free unused stuff
  nsMemory::Free(caseAResult);
  nsMemory::Free(caseBResult);
  nsMemory::Free(caseCDResult);

  // if we have a result
  if (*aResult) {
    // then return charset and lang as well
    if (aLang && !lang.IsEmpty()) {
      PRUint32 len = lang.Length();
      *aLang = (char *) nsMemory::Clone(lang.BeginReading(), len + 1);
      if (*aLang) {
        *(*aLang + len) = 0;
      }
   }
    if (aCharset && !charset.IsEmpty()) {
      PRUint32 len = charset.Length();
      *aCharset = (char *) nsMemory::Clone(charset.BeginReading(), len + 1);
      if (*aCharset) {
        *(*aCharset + len) = 0;
      }
    }
  }

  return *aResult ? NS_OK : NS_ERROR_INVALID_ARG;
}


NS_IMETHODIMP
nsMIMEHeaderParamImpl::DecodeRFC2047Header(const char* aHeaderVal, 
                                           const char* aDefaultCharset, 
                                           bool aOverrideCharset, 
                                           bool aEatContinuations,
                                           nsACString& aResult)
{
  aResult.Truncate();
  if (!aHeaderVal)
    return NS_ERROR_INVALID_ARG;
  if (!*aHeaderVal)
    return NS_OK;


  // If aHeaderVal is RFC 2047 encoded or is not a UTF-8 string  but
  // aDefaultCharset is specified, decodes RFC 2047 encoding and converts
  // to UTF-8. Otherwise, just strips away CRLF. 
  if (PL_strstr(aHeaderVal, "=?") || 
      (aDefaultCharset && (!IsUTF8(nsDependentCString(aHeaderVal)) || 
      Is7bitNonAsciiString(aHeaderVal, PL_strlen(aHeaderVal))))) {
    DecodeRFC2047Str(aHeaderVal, aDefaultCharset, aOverrideCharset, aResult);
  } else if (aEatContinuations && 
             (PL_strchr(aHeaderVal, '\n') || PL_strchr(aHeaderVal, '\r'))) {
    aResult = aHeaderVal;
  } else {
    aEatContinuations = false;
    aResult = aHeaderVal;
  }

  if (aEatContinuations) {
    nsCAutoString temp(aResult);
    temp.ReplaceSubstring("\n\t", " ");
    temp.ReplaceSubstring("\r\t", " ");
    temp.StripChars("\r\n");
    aResult = temp;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsMIMEHeaderParamImpl::DecodeParameter(const nsACString& aParamValue,
                                       const char* aCharset,
                                       const char* aDefaultCharset,
                                       bool aOverrideCharset, 
                                       nsACString& aResult)
{
  aResult.Truncate();
  // If aCharset is given, aParamValue was obtained from RFC2231/5987 
  // encoding and we're pretty sure that it's in aCharset.
  if (aCharset && *aCharset)
  {
    nsCOMPtr<nsIUTF8ConverterService> cvtUTF8(do_GetService(NS_UTF8CONVERTERSERVICE_CONTRACTID));
    if (cvtUTF8)
      return cvtUTF8->ConvertStringToUTF8(aParamValue, aCharset,
          true, aResult);
  }

  const nsAFlatCString& param = PromiseFlatCString(aParamValue);
  nsCAutoString unQuoted;
  nsACString::const_iterator s, e;
  param.BeginReading(s);
  param.EndReading(e);

  // strip '\' when used to quote CR, LF, '"' and '\'
  for ( ; s != e; ++s) {
    if ((*s == '\\')) {
      if (++s == e) {
        --s; // '\' is at the end. move back and append '\'.
      }
      else if (*s != nsCRT::CR && *s != nsCRT::LF && *s != '"' && *s != '\\') {
        --s; // '\' is not foll. by CR,LF,'"','\'. move back and append '\'
      }
      // else : skip '\' and append the quoted character.
    }
    unQuoted.Append(*s);
  }

  aResult = unQuoted;
  
  nsCAutoString decoded;

  // Try RFC 2047 encoding, instead.
  nsresult rv = DecodeRFC2047Header(unQuoted.get(), aDefaultCharset, 
                                    aOverrideCharset, true, decoded);
  
  if (NS_SUCCEEDED(rv) && !decoded.IsEmpty())
    aResult = decoded;
  
  return rv;
}

#define ISHEXCHAR(c) \
        ((0x30 <= PRUint8(c) && PRUint8(c) <= 0x39)  ||  \
         (0x41 <= PRUint8(c) && PRUint8(c) <= 0x46)  ||  \
         (0x61 <= PRUint8(c) && PRUint8(c) <= 0x66))

// Decode Q encoding (RFC 2047).
// static
char *DecodeQ(const char *in, PRUint32 length)
{
  char *out, *dest = 0;

  out = dest = (char *)PR_Calloc(length + 1, sizeof(char));
  if (dest == nsnull)
    return nsnull;
  while (length > 0) {
    PRUintn c = 0;
    switch (*in) {
    case '=':
      // check if |in| in the form of '=hh'  where h is [0-9a-fA-F].
      if (length < 3 || !ISHEXCHAR(in[1]) || !ISHEXCHAR(in[2]))
        goto badsyntax;
      PR_sscanf(in + 1, "%2X", &c);
      *out++ = (char) c;
      in += 3;
      length -= 3;
      break;

    case '_':
      *out++ = ' ';
      in++;
      length--;
      break;

    default:
      if (*in & 0x80) goto badsyntax;
      *out++ = *in++;
      length--;
    }
  }
  *out++ = '\0';

  for (out = dest; *out ; ++out) {
    if (*out == '\t')
      *out = ' ';
  }

  return dest;

 badsyntax:
  PR_Free(dest);
  return nsnull;
}

// check if input is HZ (a 7bit encoding for simplified Chinese : RFC 1842)) 
// or has  ESC which may be an  indication that  it's in one of many ISO 
// 2022 7bit  encodings (e.g. ISO-2022-JP(-2)/CN : see RFC 1468, 1922, 1554).
// static
bool Is7bitNonAsciiString(const char *input, PRUint32 len)
{
  PRInt32 c;

  enum { hz_initial, // No HZ seen yet
         hz_escaped, // Inside an HZ ~{ escape sequence 
         hz_seen, // Have seen at least one complete HZ sequence 
         hz_notpresent // Have seen something that is not legal HZ
  } hz_state;

  hz_state = hz_initial;
  while (len) {
    c = PRUint8(*input++);
    len--;
    if (c & 0x80) return false;
    if (c == 0x1B) return true;
    if (c == '~') {
      switch (hz_state) {
      case hz_initial:
      case hz_seen:
        if (*input == '{') {
          hz_state = hz_escaped;
        } else if (*input == '~') {
          // ~~ is the HZ encoding of ~.  Skip over second ~ as well
          hz_state = hz_seen;
          input++;
          len--;
        } else {
          hz_state = hz_notpresent;
        }
        break;

      case hz_escaped:
        if (*input == '}') hz_state = hz_seen;
        break;
      default:
        break;
      }
    }
  }
  return hz_state == hz_seen;
}

#define REPLACEMENT_CHAR "\357\277\275" // EF BF BD (UTF-8 encoding of U+FFFD)

// copy 'raw' sequences of octets in aInput to aOutput.
// If aDefaultCharset is specified, the input is assumed to be in the
// charset and converted to UTF-8. Otherwise, a blind copy is made.
// If aDefaultCharset is specified, but the conversion to UTF-8
// is not successful, each octet is replaced by Unicode replacement
// chars. *aOutput is advanced by the number of output octets.
// static
void CopyRawHeader(const char *aInput, PRUint32 aLen, 
                   const char *aDefaultCharset, nsACString &aOutput)
{
  PRInt32 c;

  // If aDefaultCharset is not specified, make a blind copy.
  if (!aDefaultCharset || !*aDefaultCharset) {
    aOutput.Append(aInput, aLen);
    return;
  }

  // Copy as long as it's US-ASCII.  An ESC may indicate ISO 2022
  // A ~ may indicate it is HZ
  while (aLen && (c = PRUint8(*aInput++)) != 0x1B && c != '~' && !(c & 0x80)) {
    aOutput.Append(char(c));
    aLen--;
  }
  if (!aLen) {
    return;
  }
  aInput--;

  // skip ASCIIness/UTF8ness test if aInput is supected to be a 7bit non-ascii
  // string and aDefaultCharset is a 7bit non-ascii charset.
  bool skipCheck = (c == 0x1B || c == '~') && 
                     IS_7BIT_NON_ASCII_CHARSET(aDefaultCharset);

  // If not UTF-8, treat as default charset
  nsCOMPtr<nsIUTF8ConverterService> 
    cvtUTF8(do_GetService(NS_UTF8CONVERTERSERVICE_CONTRACTID));
  nsCAutoString utf8Text;
  if (cvtUTF8 &&
      NS_SUCCEEDED(
      cvtUTF8->ConvertStringToUTF8(Substring(aInput, aInput + aLen), 
      aDefaultCharset, skipCheck, utf8Text))) {
    aOutput.Append(utf8Text);
  } else { // replace each octet with Unicode replacement char in UTF-8.
    for (PRUint32 i = 0; i < aLen; i++) {
      c = PRUint8(*aInput++);
      if (c & 0x80)
        aOutput.Append(REPLACEMENT_CHAR);
      else
        aOutput.Append(char(c));
    }
  }
}

static const char especials[] = "()<>@,;:\\\"/[]?.=";

// |decode_mime_part2_str| taken from comi18n.c
// Decode RFC2047-encoded words in the input and convert the result to UTF-8.
// If aOverrideCharset is true, charset in RFC2047-encoded words is 
// ignored and aDefaultCharset is assumed, instead. aDefaultCharset
// is also used to convert raw octets (without RFC 2047 encoding) to UTF-8.
//static
nsresult DecodeRFC2047Str(const char *aHeader, const char *aDefaultCharset, 
                          bool aOverrideCharset, nsACString &aResult)
{
  const char *p, *q = nsnull, *r;
  char *decodedText;
  const char *begin; // tracking pointer for where we are in the input buffer
  PRInt32 isLastEncodedWord = 0;
  const char *charsetStart, *charsetEnd;
  char charset[80];

  // initialize charset name to an empty string
  charset[0] = '\0';

  begin = aHeader;

  // To avoid buffer realloc, if possible, set capacity in advance. No 
  // matter what,  more than 3x expansion can never happen for all charsets
  // supported by Mozilla. SCSU/BCSU with the sliding window set to a
  // non-BMP block may be exceptions, but Mozilla does not support them. 
  // Neither any known mail/news program use them. Even if there's, we're
  // safe because we don't use a raw *char any more.
  aResult.SetCapacity(3 * strlen(aHeader));

  while ((p = PL_strstr(begin, "=?")) != 0) {
    if (isLastEncodedWord) {
      // See if it's all whitespace.
      for (q = begin; q < p; ++q) {
        if (!PL_strchr(" \t\r\n", *q)) break;
      }
    }

    if (!isLastEncodedWord || q < p) {
      // copy the part before the encoded-word
      CopyRawHeader(begin, p - begin, aDefaultCharset, aResult);
      begin = p;
    }

    p += 2;

    // Get charset info
    charsetStart = p;
    charsetEnd = 0;
    for (q = p; *q != '?'; q++) {
      if (*q <= ' ' || PL_strchr(especials, *q)) {
        goto badsyntax;
      }

      // RFC 2231 section 5
      if (!charsetEnd && *q == '*') {
        charsetEnd = q; 
      }
    }
    if (!charsetEnd) {
      charsetEnd = q;
    }

    // Check for too-long charset name
    if (PRUint32(charsetEnd - charsetStart) >= sizeof(charset)) 
      goto badsyntax;
    
    memcpy(charset, charsetStart, charsetEnd - charsetStart);
    charset[charsetEnd - charsetStart] = 0;

    q++;
    if (*q != 'Q' && *q != 'q' && *q != 'B' && *q != 'b')
      goto badsyntax;

    if (q[1] != '?')
      goto badsyntax;

    r = q;
    for (r = q + 2; *r != '?'; r++) {
      if (*r < ' ') goto badsyntax;
    }
    if (r[1] != '=')
        goto badsyntax;
    else if (r == q + 2) {
        // it's empty, skip
        begin = r + 2;
        isLastEncodedWord = 1;
        continue;
    }

    if(*q == 'Q' || *q == 'q')
      decodedText = DecodeQ(q + 2, r - (q + 2));
    else {
      // bug 227290. ignore an extraneous '=' at the end.
      // (# of characters in B-encoded part has to be a multiple of 4)
      PRInt32 n = r - (q + 2);
      n -= (n % 4 == 1 && !PL_strncmp(r - 3, "===", 3)) ? 1 : 0;
      decodedText = PL_Base64Decode(q + 2, n, nsnull);
    }

    if (decodedText == nsnull)
      goto badsyntax;

    // Override charset if requested.  Never override labeled UTF-8.
    // Use default charset instead of UNKNOWN-8BIT
    if ((aOverrideCharset && 0 != nsCRT::strcasecmp(charset, "UTF-8")) ||
        (aDefaultCharset && 0 == nsCRT::strcasecmp(charset, "UNKNOWN-8BIT"))) {
      PL_strncpy(charset, aDefaultCharset, sizeof(charset) - 1);
      charset[sizeof(charset) - 1] = '\0';
    }

    {
      nsCOMPtr<nsIUTF8ConverterService> 
        cvtUTF8(do_GetService(NS_UTF8CONVERTERSERVICE_CONTRACTID));
      nsCAutoString utf8Text;
      // skip ASCIIness/UTF8ness test if aCharset is 7bit non-ascii charset.
      if (cvtUTF8 &&
          NS_SUCCEEDED(
            cvtUTF8->ConvertStringToUTF8(nsDependentCString(decodedText),
            charset, IS_7BIT_NON_ASCII_CHARSET(charset), utf8Text))) {
        aResult.Append(utf8Text);
      } else {
        aResult.Append(REPLACEMENT_CHAR);
      }
    }
    PR_Free(decodedText);
    begin = r + 2;
    isLastEncodedWord = 1;
    continue;

  badsyntax:
    // copy the part before the encoded-word
    aResult.Append(begin, p - begin);
    begin = p;
    isLastEncodedWord = 0;
  }

  // put the tail back
  CopyRawHeader(begin, strlen(begin), aDefaultCharset, aResult);

  nsCAutoString tempStr(aResult);
  tempStr.ReplaceChar('\t', ' ');
  aResult = tempStr;

  return NS_OK;
}
