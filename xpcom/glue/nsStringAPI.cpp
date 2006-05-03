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

#ifdef MOZILLA_INTERNAL_API
#undef nsAString
#undef nsACString
#endif

#include "nsXPCOMStrings.h"

#ifdef MOZILLA_INTERNAL_API
#undef MOZILLA_INTERNAL_API
#endif

#include "nsStringAPI.h"

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

PRInt32
nsACString::DefaultComparator(const char_type *a, const char_type *b,
                              PRUint32 len)
{
  return memcmp(a, b, len);
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

// Utils

char*
ToNewUTF8String(const nsAString& aSource)
{
  nsCString temp;
  CopyUTF16toUTF8(aSource, temp);
  return NS_CStringCloneData(temp);
}
