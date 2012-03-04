/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is C++ hashtable templates.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "prbit.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

PRUint32
HashString( const nsAString& aStr )
{
  PRUint32 code = 0;

#ifdef MOZILLA_INTERNAL_API
  nsAString::const_iterator begin, end;
  aStr.BeginReading(begin);
  aStr.EndReading(end);
#else
  const PRUnichar *begin, *end;
  PRUint32 len = NS_StringGetData(aStr, &begin);
  end = begin + len;
#endif

  while (begin != end) {
    code = AddToHash(code, *begin);
    ++begin;
  }

  return code;
}

PRUint32
HashString( const nsACString& aStr )
{
  PRUint32 code = 0;

#ifdef MOZILLA_INTERNAL_API
  nsACString::const_iterator begin, end;
  aStr.BeginReading(begin);
  aStr.EndReading(end);
#else
  const char *begin, *end;
  PRUint32 len = NS_CStringGetData(aStr, &begin);
  end = begin + len;
#endif

  while (begin != end) {
    code = AddToHash(code, *begin);
    ++begin;
  }

  return code;
}

PRUint32
HashString(const char *str)
{
  PRUint32 code = 0;
  const char *origStr = str;

  while (*str) {
    code = AddToHash(code, *str);
    ++str;
  }

  return code;
}

PRUint32
HashString(const PRUnichar *str)
{
  PRUint32 code = 0;
  const PRUnichar *origStr = str;

  while (*str) {
    code = AddToHash(code, *str);
    ++str;
  }

  return code;
}

PLDHashOperator
PL_DHashStubEnumRemove(PLDHashTable    *table,
                                       PLDHashEntryHdr *entry,
                                       PRUint32         ordinal,
                                       void            *userarg)
{
  return PL_DHASH_REMOVE;
}

PRUint32 nsIDHashKey::HashKey(const nsID* id)
{
  PRUint32 h = id->m0;
  PRUint32 i;

  h = PR_ROTATE_LEFT32(h, 4) ^ id->m1;
  h = PR_ROTATE_LEFT32(h, 4) ^ id->m2;

  for (i = 0; i < 8; i++)
    h = PR_ROTATE_LEFT32(h, 4) ^ id->m3[i];

  return h;
}
