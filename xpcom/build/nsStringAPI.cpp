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

#include "nsString.h"
#include "nsCharTraits.h"

#define nsAString_external nsAString_external_
#define nsACString_external nsACString_external_

#include "nsStringAPI.h"

/* ------------------------------------------------------------------------- */

NS_STRINGAPI(PRBool)
NS_StringContainerInit(nsStringContainer &aContainer)
{
  NS_ASSERTION(sizeof(nsStringContainer) >= sizeof(nsString),
      "nsStringContainer is not large enough");

  // use placement new to avoid heap allocating nsString object
  new (&aContainer) nsString();

  return PR_TRUE;
}

NS_STRINGAPI(void)
NS_StringContainerFinish(nsStringContainer &aContainer)
{
  // call the nsString dtor
  NS_REINTERPRET_CAST(nsString *, &aContainer)->~nsString();
}

/* ------------------------------------------------------------------------- */

NS_STRINGAPI(PRUint32)
NS_StringGetData(const nsAString &aStr, const PRUnichar **aData,
                 PRBool *aTerminated)
{
  if (aTerminated)
    *aTerminated = aStr.IsTerminated();

  nsAString::const_iterator begin;
  aStr.BeginReading(begin);
  *aData = begin.get();
  return begin.size_forward();
}

NS_STRINGAPI(void)
NS_StringSetData(nsAString &aStr, const PRUnichar *aData, PRUint32 aDataLength)
{
  aStr.Assign(aData, aDataLength);
}

NS_STRINGAPI(void)
NS_StringSetDataRange(nsAString &aStr,
                      PRUint32 aCutOffset, PRUint32 aCutLength,
                      const PRUnichar *aData, PRUint32 aDataLength)
{
  if (aCutOffset == PR_UINT32_MAX)
  {
    // append case
    if (aData)
      aStr.Append(aData, aDataLength);
    return;
  }

  if (aCutLength == PR_UINT32_MAX)
    aCutLength = aStr.Length() - aCutOffset;

  if (aData)
  {
    if (aDataLength == PR_UINT32_MAX)
      aStr.Replace(aCutOffset, aCutLength, nsDependentString(aData));
    else
      aStr.Replace(aCutOffset, aCutLength, Substring(aData, aData + aDataLength));
  }
  else
    aStr.Cut(aCutOffset, aCutLength);
}

NS_STRINGAPI(void)
NS_StringCopy(nsAString &aDest, const nsAString &aSrc)
{
  aDest.Assign(aSrc);
}

/* ------------------------------------------------------------------------- */

NS_STRINGAPI(PRBool)
NS_CStringContainerInit(nsCStringContainer &aContainer)
{
  NS_ASSERTION(sizeof(nsCStringContainer) >= sizeof(nsCString),
      "nsCStringContainer is not large enough");

  // use placement new to avoid heap allocating nsCString object
  new (&aContainer) nsCString();

  return PR_TRUE;
}

NS_STRINGAPI(void)
NS_CStringContainerFinish(nsCStringContainer &aContainer)
{
  // call the nsCString dtor
  NS_REINTERPRET_CAST(nsCString *, &aContainer)->~nsCString();
}

/* ------------------------------------------------------------------------- */

NS_STRINGAPI(PRUint32)
NS_CStringGetData(const nsACString &aStr, const char **aData,
                  PRBool *aTerminated)
{
  if (aTerminated)
    *aTerminated = aStr.IsTerminated();

  nsACString::const_iterator begin;
  aStr.BeginReading(begin);
  *aData = begin.get();
  return begin.size_forward();
}

NS_STRINGAPI(void)
NS_CStringSetData(nsACString &aStr, const char *aData, PRUint32 aDataLength)
{
  aStr.Assign(aData, aDataLength);
}

NS_STRINGAPI(void)
NS_CStringSetDataRange(nsACString &aStr,
                       PRUint32 aCutOffset, PRUint32 aCutLength,
                       const char *aData, PRUint32 aDataLength)
{
  if (aCutOffset == PR_UINT32_MAX)
  {
    // append case
    if (aData)
      aStr.Append(aData, aDataLength);
    return;
  }

  if (aCutLength == PR_UINT32_MAX)
    aCutLength = aStr.Length() - aCutOffset;

  if (aData)
  {
    if (aDataLength == PR_UINT32_MAX)
      aStr.Replace(aCutOffset, aCutLength, nsDependentCString(aData));
    else
      aStr.Replace(aCutOffset, aCutLength, Substring(aData, aData + aDataLength));
  }
  else
    aStr.Cut(aCutOffset, aCutLength);
}

NS_STRINGAPI(void)
NS_CStringCopy(nsACString &aDest, const nsACString &aSrc)
{
  aDest.Assign(aSrc);
}
