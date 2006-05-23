/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nscore.h"
#include "nsCRTGlue.h"

#ifdef MOZILLA_INTERNAL_API
#undef nsAString
#undef nsACString
#endif

#include "nsXPCOMStrings.h"
#include "nsDebug.h"

#ifdef MOZILLA_INTERNAL_API
#undef MOZILLA_INTERNAL_API
#endif

#include "nsStringAPI.h"

#include <stdio.h>

#ifdef XP_WIN
#define snprintf _snprintf
#endif

// nsAString

PRUint32
nsAString::BeginReading(const char_type **begin, const char_type **end) const
{
  PRUint32 len = NS_StringGetData(*this, begin);
  if (end)
    *end = *begin + len;

  return len;
}

const nsAString::char_type*
nsAString::BeginReading() const
{
  const char_type *data;
  NS_StringGetData(*this, &data);
  return data;
}

const nsAString::char_type*
nsAString::EndReading() const
{
  const char_type *data;
  PRUint32 len = NS_StringGetData(*this, &data);
  return data + len;
}

PRUint32
nsAString::BeginWriting(char_type **begin, char_type **end, PRUint32 newSize)
{
  PRUint32 len = NS_StringGetMutableData(*this, newSize, begin);
  if (end)
    *end = *begin + len;

  return len;
}

nsAString::char_type*
nsAString::BeginWriting()
{
  char_type *data;
  NS_StringGetMutableData(*this, PR_UINT32_MAX, &data);
  return data;
}

nsAString::char_type*
nsAString::EndWriting()
{
  char_type *data;
  PRUint32 len = NS_StringGetMutableData(*this, PR_UINT32_MAX, &data);
  return data + len;
}

PRBool
nsAString::SetLength(PRUint32 aLen)
{
  char_type *data;
  NS_StringGetMutableData(*this, aLen, &data);
  return data != nsnull;
}

void
nsAString::StripChars(const char *aSet)
{
  nsString copy(*this);

  const char_type *source, *sourceEnd;
  copy.BeginReading(&source, &sourceEnd);

  char_type *dest;
  BeginWriting(&dest);
  if (!dest)
    return;

  char_type *curDest = dest;

  for (; source < sourceEnd; ++source) {
    const char *test;
    for (test = aSet; *test; ++test) {
      if (*source == char_type(*test))
        break;
    }

    if (!*test) {
      // not stripped, copy this char
      *curDest = *source;
      ++curDest;
    }
  }

  SetLength(curDest - dest);
}

void
nsAString::Trim(const char *aSet, PRBool aLeading, PRBool aTrailing)
{
  NS_ASSERTION(aLeading || aTrailing, "Ineffective Trim");

  const PRUnichar *start, *end;
  PRUint32 cutLen;

  if (aLeading) {
    BeginReading(&start, &end);
    for (cutLen = 0; start < end; ++start, ++cutLen) {
      const char *test;
      for (test = aSet; *test; ++test) {
        if (*test == *start)
          break;
      }
      if (!*test)
        break;
    }
    if (cutLen) {
      NS_StringCutData(*this, 0, cutLen);
    }
  }
  if (aTrailing) {
    PRUint32 len = BeginReading(&start, &end);
    --end;
    for (cutLen = 0; end >= start; --end, ++cutLen) {
      const char *test;
      for (test = aSet; *test; ++test) {
        if (*test == *end)
          break;
      }
      if (!*test)
        break;
    }
    if (cutLen) {
      NS_StringCutData(*this, len - cutLen, cutLen);
    }
  }
}

PRInt32
nsAString::DefaultComparator(const char_type *a, const char_type *b,
                             PRUint32 len)
{
  for (const char_type *end = a + len; a < end; ++a, ++b) {
    if (*a == *b)
      continue;

    return a < b ? -1 : 1;
  }

  return 0;
}

PRBool
nsAString::Equals(const char_type *other, ComparatorFunc c) const
{
  const char_type *cself;
  PRUint32 selflen = NS_StringGetData(*this, &cself);
  PRUint32 otherlen = NS_strlen(other);

  if (selflen != otherlen)
    return PR_FALSE;

  return c(cself, other, selflen) == 0;
}

PRBool
nsAString::Equals(const self_type &other, ComparatorFunc c) const
{
  const char_type *cself;
  const char_type *cother;
  PRUint32 selflen = NS_StringGetData(*this, &cself);
  PRUint32 otherlen = NS_StringGetData(other, &cother);

  if (selflen != otherlen)
    return PR_FALSE;

  return c(cself, cother, selflen) == 0;
}

PRBool
nsAString::LowerCaseEqualsLiteral(const char *aASCIIString) const
{
  const PRUnichar *begin, *end;
  BeginReading(&begin, &end);

  for (; begin < end; ++begin, ++aASCIIString) {
    if (!*aASCIIString || !NS_IsAscii(*begin) ||
        NS_ToLower((char) *begin) != *aASCIIString) {
      return PR_FALSE;
    }
  }

  if (*aASCIIString)
    return PR_FALSE;

  return PR_TRUE;
}

PRInt32
nsAString::Find(const self_type& aStr, ComparatorFunc c) const
{
  const char_type *begin, *end;
  PRUint32 selflen = BeginReading(&begin, &end);

  const char_type *other;
  PRUint32 otherlen = aStr.BeginReading(&other);

  if (otherlen > selflen)
    return -1;

  // We want to stop searching otherlen characters before the end of the string
  end -= otherlen;

  for (const char_type *cur = begin; cur <= end; ++cur) {
    if (!c(cur, other, otherlen))
      return cur - begin;
  }
  return -1;
}

static PRBool ns_strnmatch(const PRUnichar *aStr, const char* aSubstring,
                           PRUint32 aLen)
{
  for (; aLen; ++aStr, ++aSubstring, --aLen) {
    if (!NS_IsAscii(*aStr))
      return PR_FALSE;

    if ((char) *aStr != *aSubstring)
      return PR_FALSE;
  }

  return PR_TRUE;
}

static PRBool ns_strnimatch(const PRUnichar *aStr, const char* aSubstring,
                            PRUint32 aLen)
{
  for (; aLen; ++aStr, ++aSubstring, --aLen) {
    if (!NS_IsAscii(*aStr))
      return PR_FALSE;

    if (NS_ToLower((char) *aStr) != *aSubstring)
      return PR_FALSE;
  }

  return PR_TRUE;
}

PRInt32
nsAString::Find(const char *aStr, PRBool aIgnoreCase) const
{
  PRBool (*match)(const PRUnichar*, const char*, PRUint32) =
    aIgnoreCase ? ns_strnimatch : ns_strnmatch;

  const char_type *begin, *end;
  PRUint32 selflen = BeginReading(&begin, &end);

  PRUint32 otherlen = strlen(aStr);

  if (otherlen > selflen)
    return -1;

  // We want to stop searching otherlen characters before the end of the string
  end -= otherlen;

  for (const char_type *cur = begin; cur <= end; ++cur) {
    if (match(cur, aStr, otherlen)) {
      return cur - begin;
    }
  }
  return -1;
}

PRInt32
nsAString::RFindChar(char_type aChar) const
{
  const PRUnichar *start, *end;
  BeginReading(&start, &end);

  do {
    --end;

    if (*end == aChar)
      return end - start;

  } while (end >= start);

  return -1;
}

void
nsAString::AppendInt(int aInt, PRInt32 aRadix)
{
  const char *fmt;
  switch (aRadix) {
  case 8:
    fmt = "%o";
    break;

  case 10:
    fmt = "%d";
    break;

  case 16:
    fmt = "%x";
    break;

  default:
    NS_ERROR("Unrecognized radix");
    fmt = "";
  };

  char buf[20];
  int len = snprintf(buf, sizeof(buf), fmt, aInt);
  buf[sizeof(buf) - 1] = '\0';

  Append(NS_ConvertASCIItoUTF16(buf, len));
}

// nsACString

PRUint32
nsACString::BeginReading(const char_type **begin, const char_type **end) const
{
  PRUint32 len = NS_CStringGetData(*this, begin);
  if (end)
    *end = *begin + len;

  return len;
}

const nsACString::char_type*
nsACString::BeginReading() const
{
  const char_type *data;
  NS_CStringGetData(*this, &data);
  return data;
}

const nsACString::char_type*
nsACString::EndReading() const
{
  const char_type *data;
  PRUint32 len = NS_CStringGetData(*this, &data);
  return data + len;
}

PRUint32
nsACString::BeginWriting(char_type **begin, char_type **end, PRUint32 newSize)
{
  PRUint32 len = NS_CStringGetMutableData(*this, newSize, begin);
  if (end)
    *end = *begin + len;

  return len;
}

nsACString::char_type*
nsACString::BeginWriting()
{
  char_type *data;
  NS_CStringGetMutableData(*this, PR_UINT32_MAX, &data);
  return data;
}

nsACString::char_type*
nsACString::EndWriting()
{
  char_type *data;
  PRUint32 len = NS_CStringGetMutableData(*this, PR_UINT32_MAX, &data);
  return data + len;
}

PRBool
nsACString::SetLength(PRUint32 aLen)
{
  char_type *data;
  NS_CStringGetMutableData(*this, aLen, &data);
  return data != nsnull;
}

void
nsACString::StripChars(const char *aSet)
{
  nsCString copy(*this);

  const char_type *source, *sourceEnd;
  copy.BeginReading(&source, &sourceEnd);

  char_type *dest;
  BeginWriting(&dest);
  if (!dest)
    return;

  char_type *curDest = dest;

  for (; source < sourceEnd; ++source) {
    const char *test;
    for (test = aSet; *test; ++test) {
      if (*source == char_type(*test))
        break;
    }

    if (!*test) {
      // not stripped, copy this char
      *curDest = *source;
      ++curDest;
    }
  }

  SetLength(curDest - dest);
}

void
nsACString::Trim(const char *aSet, PRBool aLeading, PRBool aTrailing)
{
  NS_ASSERTION(aLeading || aTrailing, "Ineffective Trim");

  const char *start, *end;
  PRUint32 cutLen;

  if (aLeading) {
    BeginReading(&start, &end);
    for (cutLen = 0; start < end; ++start, ++cutLen) {
      const char *test;
      for (test = aSet; *test; ++test) {
        if (*test == *start)
          break;
      }
      if (!*test)
        break;
    }
    if (cutLen) {
      NS_CStringCutData(*this, 0, cutLen);
    }
  }
  if (aTrailing) {
    PRUint32 len = BeginReading(&start, &end);
    --end;
    for (cutLen = 0; end >= start; --end, ++cutLen) {
      const char *test;
      for (test = aSet; *test; ++test) {
        if (*test == *end)
          break;
      }
      if (!*test)
        break;
    }
    if (cutLen) {
      NS_CStringCutData(*this, len - cutLen, cutLen);
    }
  }
}

PRInt32
nsACString::DefaultComparator(const char_type *a, const char_type *b,
                              PRUint32 len)
{
  return memcmp(a, b, len);
}

PRBool
nsACString::Equals(const char_type *other, ComparatorFunc c) const
{
  const char_type *cself;
  PRUint32 selflen = NS_CStringGetData(*this, &cself);
  PRUint32 otherlen = strlen(other);

  if (selflen != otherlen)
    return PR_FALSE;

  return c(cself, other, selflen) == 0;
}

PRBool
nsACString::Equals(const self_type &other, ComparatorFunc c) const
{
  const char_type *cself;
  const char_type *cother;
  PRUint32 selflen = NS_CStringGetData(*this, &cself);
  PRUint32 otherlen = NS_CStringGetData(other, &cother);

  if (selflen != otherlen)
    return PR_FALSE;

  return c(cself, cother, selflen) == 0;
}

PRInt32
nsACString::Find(const self_type& aStr, ComparatorFunc c) const
{
  const char_type *begin;
  PRUint32 len = aStr.BeginReading(&begin);
  return Find(begin, len, c);
}

PRInt32
nsACString::Find(const char_type *aStr, ComparatorFunc c) const
{
  return Find(aStr, strlen(aStr), c);
}

PRInt32
nsACString::Find(const char_type *aStr, PRUint32 aLen, ComparatorFunc c) const
{
  const char_type *begin, *end;
  PRUint32 selflen = BeginReading(&begin, &end);

  if (aLen == 0) {
    NS_WARNING("Searching for zero-length string.");
    return -1;
  }

   if (aLen > selflen)
    return -1;

  // We want to stop searching otherlen characters before the end of the string
  end -= aLen;

  for (const char_type *cur = begin; cur <= end; ++cur) {
    if (!c(begin, aStr, aLen))
      return cur - begin;
  }
  return -1;
}

PRInt32
nsACString::RFindChar(char_type aChar) const
{
  const char *start, *end;
  BeginReading(&start, &end);

  for (; end >= start; --end) {
    if (*end == aChar)
      return end - start;
  }

  return -1;
}

void
nsACString::AppendInt(int aInt, PRInt32 aRadix)
{
  const char *fmt;
  switch (aRadix) {
  case 8:
    fmt = "%o";
    break;

  case 10:
    fmt = "%d";
    break;

  case 16:
    fmt = "%x";
    break;

  default:
    NS_ERROR("Unrecognized radix");
    fmt = "";
  };

  char buf[20];
  int len = snprintf(buf, sizeof(buf), fmt, aInt);
  buf[sizeof(buf) - 1] = '\0';

  Append(buf, len);
}

// Substrings

nsDependentSubstring::nsDependentSubstring(const abstract_string_type& aStr,
                                           PRUint32 aStartPos)
{
  const PRUnichar* data;
  PRUint32 len = NS_StringGetData(aStr, &data);

  if (aStartPos > len)
    aStartPos = len;

  NS_StringContainerInit2(*this, data + aStartPos, len - aStartPos,
                          NS_STRING_CONTAINER_INIT_DEPEND |
                          NS_STRING_CONTAINER_INIT_SUBSTRING);
}

nsDependentSubstring::nsDependentSubstring(const abstract_string_type& aStr,
                                           PRUint32 aStartPos,
                                           PRUint32 aLength)
{
  const PRUnichar* data;
  PRUint32 len = NS_StringGetData(aStr, &data);

  if (aStartPos > len)
    aStartPos = len;

  if (aStartPos + aLength > len)
    aLength = len - aStartPos;

  NS_StringContainerInit2(*this, data + aStartPos, aLength,
                          NS_STRING_CONTAINER_INIT_DEPEND |
                            NS_STRING_CONTAINER_INIT_SUBSTRING);
}

nsDependentCSubstring::nsDependentCSubstring(const abstract_string_type& aStr,
                                             PRUint32 aStartPos)
{
  const char* data;
  PRUint32 len = NS_CStringGetData(aStr, &data);

  if (aStartPos > len)
    aStartPos = len;

  NS_CStringContainerInit2(*this, data + aStartPos, len - aStartPos,
                           NS_CSTRING_CONTAINER_INIT_DEPEND |
                           NS_CSTRING_CONTAINER_INIT_SUBSTRING);
}

nsDependentCSubstring::nsDependentCSubstring(const abstract_string_type& aStr,
                                             PRUint32 aStartPos,
                                             PRUint32 aLength)
{
  const char* data;
  PRUint32 len = NS_CStringGetData(aStr, &data);

  if (aStartPos > len)
    aStartPos = len;

  if (aStartPos + aLength > len)
    aLength = len - aStartPos;

  NS_CStringContainerInit2(*this, data + aStartPos, aLength,
                           NS_CSTRING_CONTAINER_INIT_DEPEND |
                           NS_CSTRING_CONTAINER_INIT_SUBSTRING);
}

// Utils

char*
ToNewUTF8String(const nsAString& aSource)
{
  nsCString temp;
  CopyUTF16toUTF8(aSource, temp);
  return NS_CStringCloneData(temp);
}

void
CompressWhitespace(nsAString& aString)
{
  aString.Trim(" \n\t\r");

  PRUnichar *start;
  PRUint32 len = NS_StringGetMutableData(aString, PR_UINT32_MAX, &start);
  PRUnichar *end = start + len;

  for (PRUnichar *cur = start; cur < end; ++cur) {
    if (!NS_IsAsciiWhitespace(*cur))
      continue;

    *cur = ' ';

    PRUnichar *wend;
    for (wend = cur + 1; wend < end && NS_IsAsciiWhitespace(*wend); ++wend) {
      // nothing to do but loop
    }

    if (wend == cur + 1)
      continue;

    PRUint32 wlen = wend - cur - 1;

    // fix "end"
    end -= wlen;

    // move everything forwards a bit
    for (PRUnichar *m = cur + 1; m < end; ++m) {
      *m = *(m + wlen);
    }
  }

  // re-terminate
  *end = '\0';

  // Set the new length.
  aString.SetLength(end - start);
}

PRUint32
ToLowerCase(nsACString& aStr)
{
  char *begin, *end;
  PRUint32 len = aStr.BeginWriting(&begin, &end);

  for (; begin < end; ++begin) {
    *begin = NS_ToLower(*begin);
  }

  return len;
}

PRUint32
ToUpperCase(nsACString& aStr)
{
  char *begin, *end;
  PRUint32 len = aStr.BeginWriting(&begin, &end);

  for (; begin < end; ++begin) {
    *begin = NS_ToUpper(*begin);
  }

  return len;
}

PRUint32
ToLowerCase(const nsACString& aSrc, nsACString& aDest)
{
  const char *begin,  *end;
  PRUint32 len = aSrc.BeginReading(&begin, &end);

  char *dest;
  NS_CStringGetMutableData(aDest, len, &dest);

  for (; begin < end; ++begin, ++dest) {
    *dest = NS_ToLower(*begin);
  }

  return len;
}

PRUint32
ToUpperCase(const nsACString& aSrc, nsACString& aDest)
{
  const char *begin,  *end;
  PRUint32 len = aSrc.BeginReading(&begin, &end);

  char *dest;
  NS_CStringGetMutableData(aDest, len, &dest);

  for (; begin < end; ++begin, ++dest) {
    *dest = NS_ToUpper(*begin);
  }

  return len;
}

PRInt32
CaseInsensitiveCompare(const char *a, const char *b,
                       PRUint32 len)
{
  for (const char *aend = a + len; a < aend; ++a, ++b) {
    char la = NS_ToLower(*a);
    char lb = NS_ToLower(*b);

    if (la == lb)
      continue;

    return la < lb ? -1 : 1;
  }

  return 0;
}
