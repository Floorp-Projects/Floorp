/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"

#include "sqlite3.h"
#include "mozStoragePrivateHelpers.h"
#include "Variant.h"
#include "mozStorageRow.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Row

nsresult Row::initialize(sqlite3_stmt* aStatement) {
  // Get the number of results
  mNumCols = ::sqlite3_column_count(aStatement);

  // Start copying over values
  for (uint32_t i = 0; i < mNumCols; i++) {
    // Store the value
    nsIVariant* variant = nullptr;
    int type = ::sqlite3_column_type(aStatement, i);
    switch (type) {
      case SQLITE_INTEGER:
        variant = new IntegerVariant(::sqlite3_column_int64(aStatement, i));
        break;
      case SQLITE_FLOAT:
        variant = new FloatVariant(::sqlite3_column_double(aStatement, i));
        break;
      case SQLITE_TEXT: {
        const char16_t* value = static_cast<const char16_t*>(
            ::sqlite3_column_text16(aStatement, i));
        nsDependentString str(
            value, ::sqlite3_column_bytes16(aStatement, i) / sizeof(char16_t));
        variant = new TextVariant(str);
        break;
      }
      case SQLITE_NULL:
        variant = new NullVariant();
        break;
      case SQLITE_BLOB: {
        const void* data = ::sqlite3_column_blob(aStatement, i);
        int size = ::sqlite3_column_bytes(aStatement, i);
        variant = new BlobVariant(std::pair<const void*, int>(data, size));
        break;
      }
      default:
        return NS_ERROR_UNEXPECTED;
    }
    NS_ENSURE_TRUE(variant, NS_ERROR_OUT_OF_MEMORY);

    // Insert into our storage array
    NS_ENSURE_TRUE(mData.InsertObjectAt(variant, i), NS_ERROR_OUT_OF_MEMORY);

    // Associate the name (if any) with the index
    const char* name = ::sqlite3_column_name(aStatement, i);
    if (!name) break;
    nsAutoCString colName(name);
    mNameHashtable.Put(colName, i);
  }

  return NS_OK;
}

/**
 * Note:  This object is only ever accessed on one thread at a time.  It it not
 *        threadsafe, but it does need threadsafe AddRef and Release.
 */
NS_IMPL_ISUPPORTS(Row, mozIStorageRow, mozIStorageValueArray)

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageRow

NS_IMETHODIMP
Row::GetResultByIndex(uint32_t aIndex, nsIVariant** _result) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  NS_ADDREF(*_result = mData.ObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
Row::GetResultByName(const nsACString& aName, nsIVariant** _result) {
  uint32_t index;
  NS_ENSURE_TRUE(mNameHashtable.Get(aName, &index), NS_ERROR_NOT_AVAILABLE);
  return GetResultByIndex(index, _result);
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageValueArray

NS_IMETHODIMP
Row::GetNumEntries(uint32_t* _entries) {
  *_entries = mNumCols;
  return NS_OK;
}

NS_IMETHODIMP
Row::GetTypeOfIndex(uint32_t aIndex, int32_t* _type) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);

  uint16_t type = mData.ObjectAt(aIndex)->GetDataType();
  switch (type) {
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
      *_type = mozIStorageValueArray::VALUE_TYPE_INTEGER;
      break;
    case nsIDataType::VTYPE_DOUBLE:
      *_type = mozIStorageValueArray::VALUE_TYPE_FLOAT;
      break;
    case nsIDataType::VTYPE_ASTRING:
      *_type = mozIStorageValueArray::VALUE_TYPE_TEXT;
      break;
    case nsIDataType::VTYPE_ARRAY:
      *_type = mozIStorageValueArray::VALUE_TYPE_BLOB;
      break;
    default:
      *_type = mozIStorageValueArray::VALUE_TYPE_NULL;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
Row::GetInt32(uint32_t aIndex, int32_t* _value) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  return mData.ObjectAt(aIndex)->GetAsInt32(_value);
}

NS_IMETHODIMP
Row::GetInt64(uint32_t aIndex, int64_t* _value) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  return mData.ObjectAt(aIndex)->GetAsInt64(_value);
}

NS_IMETHODIMP
Row::GetDouble(uint32_t aIndex, double* _value) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  return mData.ObjectAt(aIndex)->GetAsDouble(_value);
}

NS_IMETHODIMP
Row::GetUTF8String(uint32_t aIndex, nsACString& _value) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  return mData.ObjectAt(aIndex)->GetAsAUTF8String(_value);
}

NS_IMETHODIMP
Row::GetString(uint32_t aIndex, nsAString& _value) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  return mData.ObjectAt(aIndex)->GetAsAString(_value);
}

NS_IMETHODIMP
Row::GetBlob(uint32_t aIndex, uint32_t* _size, uint8_t** _blob) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);

  uint16_t type;
  nsIID interfaceIID;
  return mData.ObjectAt(aIndex)->GetAsArray(&type, &interfaceIID, _size,
                                            reinterpret_cast<void**>(_blob));
}

NS_IMETHODIMP
Row::GetBlobAsString(uint32_t aIndex, nsAString& aValue) {
  return DoGetBlobAsString(this, aIndex, aValue);
}

NS_IMETHODIMP
Row::GetBlobAsUTF8String(uint32_t aIndex, nsACString& aValue) {
  return DoGetBlobAsString(this, aIndex, aValue);
}

NS_IMETHODIMP
Row::GetIsNull(uint32_t aIndex, bool* _isNull) {
  ENSURE_INDEX_VALUE(aIndex, mNumCols);
  NS_ENSURE_ARG_POINTER(_isNull);

  uint16_t type = mData.ObjectAt(aIndex)->GetDataType();
  *_isNull = type == nsIDataType::VTYPE_EMPTY;
  return NS_OK;
}

NS_IMETHODIMP
Row::GetSharedUTF8String(uint32_t, uint32_t*, char const**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Row::GetSharedString(uint32_t, uint32_t*, const char16_t**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Row::GetSharedBlob(uint32_t, uint32_t*, const uint8_t**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace storage
}  // namespace mozilla
