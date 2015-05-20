/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: we are already in the namepace mozilla::storage

// Note 2: whoever #includes this file must provide implementations of
// sqlite3_T_* prior.

////////////////////////////////////////////////////////////////////////////////
//// variantToSQLiteT Implementation

template <typename T>
int
variantToSQLiteT(T aObj,
                 nsIVariant *aValue)
{
  // Allow to return nullptr not wrapped to nsIVariant for speed.
  if (!aValue)
    return sqlite3_T_null(aObj);

  uint16_t type;
  (void)aValue->GetDataType(&type);
  switch (type) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    {
      int32_t value;
      nsresult rv = aValue->GetAsInt32(&value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_int(aObj, value);
    }
    case nsIDataType::VTYPE_UINT32: // Try to preserve full range
    case nsIDataType::VTYPE_INT64:
    // Data loss possible, but there is no unsigned types in SQLite
    case nsIDataType::VTYPE_UINT64:
    {
      int64_t value;
      nsresult rv = aValue->GetAsInt64(&value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_int64(aObj, value);
    }
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    {
      double value;
      nsresult rv = aValue->GetAsDouble(&value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_double(aObj, value);
    }
    case nsIDataType::VTYPE_BOOL:
    {
      bool value;
      nsresult rv = aValue->GetAsBool(&value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_int(aObj, value ? 1 : 0);
    }
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    {
      nsAutoCString value;
      // GetAsAUTF8String should never perform conversion when coming from
      // 8-bit string types, and thus can accept strings with arbitrary encoding
      // (including UTF8 and ASCII).
      nsresult rv = aValue->GetAsAUTF8String(value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_text(aObj, value);
    }
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_ASTRING:
    {
      nsAutoString value;
      // GetAsAString does proper conversion to UCS2 from all string-like types.
      // It can be used universally without problems (unless someone implements
      // their own variant, but that's their problem).
      nsresult rv = aValue->GetAsAString(value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_text16(aObj, value);
    }
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
      return sqlite3_T_null(aObj);
    case nsIDataType::VTYPE_ARRAY:
    {
      uint16_t type;
      nsIID iid;
      uint32_t count;
      void *data;
      nsresult rv = aValue->GetAsArray(&type, &iid, &count, &data);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);

      // Check to make sure it's a supported type.
      NS_ASSERTION(type == nsIDataType::VTYPE_UINT8,
                   "Invalid type passed!  You may leak!");
      if (type != nsIDataType::VTYPE_UINT8) {
        // Technically this could leak with certain data types, but somebody was
        // being stupid passing us this anyway.
        free(data);
        return SQLITE_MISMATCH;
      }

      // Finally do our thing.  The function should free the array accordingly!
      int rc = sqlite3_T_blob(aObj, data, count);
      return rc;
    }
    // Maybe, it'll be possible to convert these
    // in future too.
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    default:
      return SQLITE_MISMATCH;
  }
  return SQLITE_OK;
}
