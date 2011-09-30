/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#include "nsError.h"
#include "nsMemory.h"
#include "nsString.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageArgValueArray.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// ArgValueArray

ArgValueArray::ArgValueArray(PRInt32 aArgc,
                             sqlite3_value **aArgv)
: mArgc(aArgc)
, mArgv(aArgv)
{
}

NS_IMPL_ISUPPORTS1(
  ArgValueArray,
  mozIStorageValueArray
)

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageValueArray

NS_IMETHODIMP
ArgValueArray::GetNumEntries(PRUint32 *_size)
{
  *_size = mArgc;
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetTypeOfIndex(PRUint32 aIndex,
                              PRInt32 *_type)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  int t = ::sqlite3_value_type(mArgv[aIndex]);
  switch (t) {
    case SQLITE_INTEGER:
      *_type = VALUE_TYPE_INTEGER;
      break;
    case SQLITE_FLOAT:
      *_type = VALUE_TYPE_FLOAT;
      break;
    case SQLITE_TEXT:
      *_type = VALUE_TYPE_TEXT;
      break;
    case SQLITE_BLOB:
      *_type = VALUE_TYPE_BLOB;
      break;
    case SQLITE_NULL:
      *_type = VALUE_TYPE_NULL;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetInt32(PRUint32 aIndex,
                        PRInt32 *_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  *_value = ::sqlite3_value_int(mArgv[aIndex]);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetInt64(PRUint32 aIndex,
                        PRInt64 *_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  *_value = ::sqlite3_value_int64(mArgv[aIndex]);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetDouble(PRUint32 aIndex,
                         double *_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  *_value = ::sqlite3_value_double(mArgv[aIndex]);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetUTF8String(PRUint32 aIndex,
                             nsACString &_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  if (::sqlite3_value_type(mArgv[aIndex]) == SQLITE_NULL) {
    // NULL columns should have IsVoid set to distinguish them from an empty
    // string.
    _value.Truncate(0);
    _value.SetIsVoid(PR_TRUE);
  }
  else {
    _value.Assign(reinterpret_cast<const char *>(::sqlite3_value_text(mArgv[aIndex])),
                  ::sqlite3_value_bytes(mArgv[aIndex]));
  }
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetString(PRUint32 aIndex,
                         nsAString &_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  if (::sqlite3_value_type(mArgv[aIndex]) == SQLITE_NULL) {
    // NULL columns should have IsVoid set to distinguish them from an empty
    // string.
    _value.Truncate(0);
    _value.SetIsVoid(PR_TRUE);
  } else {
    _value.Assign(static_cast<const PRUnichar *>(::sqlite3_value_text16(mArgv[aIndex])),
                  ::sqlite3_value_bytes16(mArgv[aIndex]) / 2);
  }
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetBlob(PRUint32 aIndex,
                       PRUint32 *_size,
                       PRUint8 **_blob)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  int size = ::sqlite3_value_bytes(mArgv[aIndex]);
  void *blob = nsMemory::Clone(::sqlite3_value_blob(mArgv[aIndex]), size);
  NS_ENSURE_TRUE(blob, NS_ERROR_OUT_OF_MEMORY);

  *_blob = static_cast<PRUint8 *>(blob);
  *_size = size;
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetIsNull(PRUint32 aIndex,
                         bool *_isNull)
{
  // GetTypeOfIndex will check aIndex for us, so we don't have to.
  PRInt32 type;
  nsresult rv = GetTypeOfIndex(aIndex, &type);
  NS_ENSURE_SUCCESS(rv, rv);

  *_isNull = (type == VALUE_TYPE_NULL);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetSharedUTF8String(PRUint32 aIndex,
                                   PRUint32 *_length,
                                   const char **_string)
{
  if (_length)
    *_length = ::sqlite3_value_bytes(mArgv[aIndex]);

  *_string = reinterpret_cast<const char *>(::sqlite3_value_text(mArgv[aIndex]));
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetSharedString(PRUint32 aIndex,
                               PRUint32 *_length,
                               const PRUnichar **_string)
{
  if (_length)
    *_length = ::sqlite3_value_bytes(mArgv[aIndex]);

  *_string = static_cast<const PRUnichar *>(::sqlite3_value_text16(mArgv[aIndex]));
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetSharedBlob(PRUint32 aIndex,
                             PRUint32 *_size,
                             const PRUint8 **_blob)
{
  *_size = ::sqlite3_value_bytes(mArgv[aIndex]);
  *_blob = static_cast<const PRUint8 *>(::sqlite3_value_blob(mArgv[aIndex]));
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
