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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

// Note: we are already in the namepace mozilla::storage

////////////////////////////////////////////////////////////////////////////////
//// Specialization Declarations

template <typename T>
int sqlite3_T_int(T aObj, int aValue);
template <typename T>
int sqlite3_T_int64(T aObj, sqlite_int64 aValue);
template <typename T>
int sqlite3_T_double(T aObj, double aValue);
template <typename T>
int sqlite3_T_text16(T aObj, nsString aValue);
template <typename T>
int sqlite3_T_null(T aObj);

////////////////////////////////////////////////////////////////////////////////
//// variantToSQLiteT Implementation

template <typename T>
int
variantToSQLiteT(T aObj,
                 nsIVariant *aValue)
{
  // Allow to return NULL not wrapped to nsIVariant for speed.
  if (!aValue)
    return sqlite3_T_null(aObj);

  PRUint16 type;
  (void)aValue->GetDataType(&type);
  switch (type) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    {
      PRInt32 value;
      nsresult rv = aValue->GetAsInt32(&value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_int(aObj, value);
    }
    case nsIDataType::VTYPE_UINT32: // Try to preserve full range
    case nsIDataType::VTYPE_INT64:
    // Data loss possible, but there is no unsigned types in SQLite
    case nsIDataType::VTYPE_UINT64:
    {
      PRInt64 value;
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
      PRBool value;
      nsresult rv = aValue->GetAsBool(&value);
      NS_ENSURE_SUCCESS(rv, SQLITE_MISMATCH);
      return sqlite3_T_int(aObj, value ? 1 : 0);
    }
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
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
      return sqlite3_T_null(aObj);
    // Maybe, it'll be possible to convert these
    // in future too.
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    default:
      return SQLITE_MISMATCH;
  }
  return SQLITE_OK;
}
