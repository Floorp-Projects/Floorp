/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsMemory.h"
#include "nsString.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageArgValueArray.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// ArgValueArray

ArgValueArray::ArgValueArray(int32_t aArgc,
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
ArgValueArray::GetNumEntries(uint32_t *_size)
{
  *_size = mArgc;
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetTypeOfIndex(uint32_t aIndex,
                              int32_t *_type)
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
ArgValueArray::GetInt32(uint32_t aIndex,
                        int32_t *_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  *_value = ::sqlite3_value_int(mArgv[aIndex]);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetInt64(uint32_t aIndex,
                        int64_t *_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  *_value = ::sqlite3_value_int64(mArgv[aIndex]);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetDouble(uint32_t aIndex,
                         double *_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  *_value = ::sqlite3_value_double(mArgv[aIndex]);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetUTF8String(uint32_t aIndex,
                             nsACString &_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  if (::sqlite3_value_type(mArgv[aIndex]) == SQLITE_NULL) {
    // NULL columns should have IsVoid set to distinguish them from an empty
    // string.
    _value.Truncate(0);
    _value.SetIsVoid(true);
  }
  else {
    _value.Assign(reinterpret_cast<const char *>(::sqlite3_value_text(mArgv[aIndex])),
                  ::sqlite3_value_bytes(mArgv[aIndex]));
  }
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetString(uint32_t aIndex,
                         nsAString &_value)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  if (::sqlite3_value_type(mArgv[aIndex]) == SQLITE_NULL) {
    // NULL columns should have IsVoid set to distinguish them from an empty
    // string.
    _value.Truncate(0);
    _value.SetIsVoid(true);
  } else {
    _value.Assign(static_cast<const PRUnichar *>(::sqlite3_value_text16(mArgv[aIndex])),
                  ::sqlite3_value_bytes16(mArgv[aIndex]) / 2);
  }
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetBlob(uint32_t aIndex,
                       uint32_t *_size,
                       uint8_t **_blob)
{
  ENSURE_INDEX_VALUE(aIndex, mArgc);

  int size = ::sqlite3_value_bytes(mArgv[aIndex]);
  void *blob = nsMemory::Clone(::sqlite3_value_blob(mArgv[aIndex]), size);
  NS_ENSURE_TRUE(blob, NS_ERROR_OUT_OF_MEMORY);

  *_blob = static_cast<uint8_t *>(blob);
  *_size = size;
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetIsNull(uint32_t aIndex,
                         bool *_isNull)
{
  // GetTypeOfIndex will check aIndex for us, so we don't have to.
  int32_t type;
  nsresult rv = GetTypeOfIndex(aIndex, &type);
  NS_ENSURE_SUCCESS(rv, rv);

  *_isNull = (type == VALUE_TYPE_NULL);
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetSharedUTF8String(uint32_t aIndex,
                                   uint32_t *_length,
                                   const char **_string)
{
  if (_length)
    *_length = ::sqlite3_value_bytes(mArgv[aIndex]);

  *_string = reinterpret_cast<const char *>(::sqlite3_value_text(mArgv[aIndex]));
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetSharedString(uint32_t aIndex,
                               uint32_t *_length,
                               const PRUnichar **_string)
{
  if (_length)
    *_length = ::sqlite3_value_bytes(mArgv[aIndex]);

  *_string = static_cast<const PRUnichar *>(::sqlite3_value_text16(mArgv[aIndex]));
  return NS_OK;
}

NS_IMETHODIMP
ArgValueArray::GetSharedBlob(uint32_t aIndex,
                             uint32_t *_size,
                             const uint8_t **_blob)
{
  *_size = ::sqlite3_value_bytes(mArgv[aIndex]);
  *_blob = static_cast<const uint8_t *>(::sqlite3_value_blob(mArgv[aIndex]));
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
