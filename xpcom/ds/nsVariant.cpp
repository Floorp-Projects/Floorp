/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVariant.h"
#include "prprf.h"
#include "prdtoa.h"
#include <math.h>
#include "nsCycleCollectionParticipant.h"
#include "xptinfo.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsCRTGlue.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Printf.h"

/***************************************************************************/
// Helpers for static convert functions...

static nsresult String2Double(const char* aString, double* aResult) {
  char* next;
  double value = PR_strtod(aString, &next);
  if (next == aString) {
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }
  *aResult = value;
  return NS_OK;
}

static nsresult AString2Double(const nsAString& aString, double* aResult) {
  char* pChars = ToNewCString(aString, mozilla::fallible);
  if (!pChars) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = String2Double(pChars, aResult);
  free(pChars);
  return rv;
}

static nsresult AUTF8String2Double(const nsAUTF8String& aString,
                                   double* aResult) {
  return String2Double(PromiseFlatUTF8String(aString).get(), aResult);
}

static nsresult ACString2Double(const nsACString& aString, double* aResult) {
  return String2Double(PromiseFlatCString(aString).get(), aResult);
}

// Fills aOutData with double, uint32_t, or int32_t.
// Returns NS_OK, an error code, or a non-NS_OK success code
nsresult nsDiscriminatedUnion::ToManageableNumber(
    nsDiscriminatedUnion* aOutData) const {
  nsresult rv;

  switch (mType) {
    // This group results in a int32_t...

#define CASE__NUMBER_INT32(type_, member_)      \
  case nsIDataType::type_:                      \
    aOutData->u.mInt32Value = u.member_;        \
    aOutData->mType = nsIDataType::VTYPE_INT32; \
    return NS_OK;

    CASE__NUMBER_INT32(VTYPE_INT8, mInt8Value)
    CASE__NUMBER_INT32(VTYPE_INT16, mInt16Value)
    CASE__NUMBER_INT32(VTYPE_INT32, mInt32Value)
    CASE__NUMBER_INT32(VTYPE_UINT8, mUint8Value)
    CASE__NUMBER_INT32(VTYPE_UINT16, mUint16Value)
    CASE__NUMBER_INT32(VTYPE_BOOL, mBoolValue)
    CASE__NUMBER_INT32(VTYPE_CHAR, mCharValue)
    CASE__NUMBER_INT32(VTYPE_WCHAR, mWCharValue)

#undef CASE__NUMBER_INT32

      // This group results in a uint32_t...

    case nsIDataType::VTYPE_UINT32:
      aOutData->u.mUint32Value = u.mUint32Value;
      aOutData->mType = nsIDataType::VTYPE_UINT32;
      return NS_OK;

      // This group results in a double...

    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT64:
      // XXX Need boundary checking here.
      // We may need to return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA
      aOutData->u.mDoubleValue = double(u.mInt64Value);
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_FLOAT:
      aOutData->u.mDoubleValue = u.mFloatValue;
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_DOUBLE:
      aOutData->u.mDoubleValue = u.mDoubleValue;
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      rv = String2Double(u.str.mStringValue, &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_ASTRING:
      rv = AString2Double(*u.mAStringValue, &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      rv = AUTF8String2Double(*u.mUTF8StringValue, &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      rv = ACString2Double(*u.mCStringValue, &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      rv = AString2Double(nsDependentString(u.wstr.mWStringValue),
                          &aOutData->u.mDoubleValue);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aOutData->mType = nsIDataType::VTYPE_DOUBLE;
      return NS_OK;

      // This group fails...

    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

/***************************************************************************/
// Array helpers...

void nsDiscriminatedUnion::FreeArray() {
  NS_ASSERTION(mType == nsIDataType::VTYPE_ARRAY, "bad FreeArray call");
  NS_ASSERTION(u.array.mArrayValue, "bad array");
  NS_ASSERTION(u.array.mArrayCount, "bad array count");

#define CASE__FREE_ARRAY_PTR(type_, ctype_)                 \
  case nsIDataType::type_: {                                \
    ctype_** p = (ctype_**)u.array.mArrayValue;             \
    for (uint32_t i = u.array.mArrayCount; i > 0; p++, i--) \
      if (*p) free((char*)*p);                              \
    break;                                                  \
  }

#define CASE__FREE_ARRAY_IFACE(type_, ctype_)               \
  case nsIDataType::type_: {                                \
    ctype_** p = (ctype_**)u.array.mArrayValue;             \
    for (uint32_t i = u.array.mArrayCount; i > 0; p++, i--) \
      if (*p) (*p)->Release();                              \
    break;                                                  \
  }

  switch (u.array.mArrayType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
      break;

      // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
      CASE__FREE_ARRAY_PTR(VTYPE_ID, nsID)
      CASE__FREE_ARRAY_PTR(VTYPE_CHAR_STR, char)
      CASE__FREE_ARRAY_PTR(VTYPE_WCHAR_STR, char16_t)
      CASE__FREE_ARRAY_IFACE(VTYPE_INTERFACE, nsISupports)
      CASE__FREE_ARRAY_IFACE(VTYPE_INTERFACE_IS, nsISupports)

    // The rest are illegal.
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    default:
      NS_ERROR("bad type in array!");
      break;
  }

  // Free the array memory.
  free((char*)u.array.mArrayValue);

#undef CASE__FREE_ARRAY_PTR
#undef CASE__FREE_ARRAY_IFACE
}

static nsresult CloneArray(uint16_t aInType, const nsIID* aInIID,
                           uint32_t aInCount, void* aInValue,
                           uint16_t* aOutType, nsIID* aOutIID,
                           uint32_t* aOutCount, void** aOutValue) {
  NS_ASSERTION(aInCount, "bad param");
  NS_ASSERTION(aInValue, "bad param");
  NS_ASSERTION(aOutType, "bad param");
  NS_ASSERTION(aOutCount, "bad param");
  NS_ASSERTION(aOutValue, "bad param");

  uint32_t i;

  // First we figure out the size of the elements for the new u.array.

  size_t elementSize;
  size_t allocSize;

  switch (aInType) {
    case nsIDataType::VTYPE_INT8:
      elementSize = sizeof(int8_t);
      break;
    case nsIDataType::VTYPE_INT16:
      elementSize = sizeof(int16_t);
      break;
    case nsIDataType::VTYPE_INT32:
      elementSize = sizeof(int32_t);
      break;
    case nsIDataType::VTYPE_INT64:
      elementSize = sizeof(int64_t);
      break;
    case nsIDataType::VTYPE_UINT8:
      elementSize = sizeof(uint8_t);
      break;
    case nsIDataType::VTYPE_UINT16:
      elementSize = sizeof(uint16_t);
      break;
    case nsIDataType::VTYPE_UINT32:
      elementSize = sizeof(uint32_t);
      break;
    case nsIDataType::VTYPE_UINT64:
      elementSize = sizeof(uint64_t);
      break;
    case nsIDataType::VTYPE_FLOAT:
      elementSize = sizeof(float);
      break;
    case nsIDataType::VTYPE_DOUBLE:
      elementSize = sizeof(double);
      break;
    case nsIDataType::VTYPE_BOOL:
      elementSize = sizeof(bool);
      break;
    case nsIDataType::VTYPE_CHAR:
      elementSize = sizeof(char);
      break;
    case nsIDataType::VTYPE_WCHAR:
      elementSize = sizeof(char16_t);
      break;

    // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      elementSize = sizeof(void*);
      break;

    // The rest are illegal.
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    default:
      NS_ERROR("bad type in array!");
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  // Alloc the u.array.

  allocSize = aInCount * elementSize;
  *aOutValue = moz_xmalloc(allocSize);

  // Clone the elements.

  switch (aInType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
      memcpy(*aOutValue, aInValue, allocSize);
      break;

    case nsIDataType::VTYPE_INTERFACE_IS:
      if (aOutIID) {
        *aOutIID = *aInIID;
      }
      [[fallthrough]];

    case nsIDataType::VTYPE_INTERFACE: {
      memcpy(*aOutValue, aInValue, allocSize);

      nsISupports** p = (nsISupports**)*aOutValue;
      for (i = aInCount; i > 0; ++p, --i)
        if (*p) {
          (*p)->AddRef();
        }
      break;
    }

    // XXX We ASSUME that "array of nsID" means "array of pointers to nsID".
    case nsIDataType::VTYPE_ID: {
      nsID** inp = (nsID**)aInValue;
      nsID** outp = (nsID**)*aOutValue;
      for (i = aInCount; i > 0; --i) {
        nsID* idp = *(inp++);
        if (idp) {
          *(outp++) = idp->Clone();
        } else {
          *(outp++) = nullptr;
        }
      }
      break;
    }

    case nsIDataType::VTYPE_CHAR_STR: {
      char** inp = (char**)aInValue;
      char** outp = (char**)*aOutValue;
      for (i = aInCount; i > 0; i--) {
        char* str = *(inp++);
        if (str) {
          *(outp++) = moz_xstrdup(str);
        } else {
          *(outp++) = nullptr;
        }
      }
      break;
    }

    case nsIDataType::VTYPE_WCHAR_STR: {
      char16_t** inp = (char16_t**)aInValue;
      char16_t** outp = (char16_t**)*aOutValue;
      for (i = aInCount; i > 0; i--) {
        char16_t* str = *(inp++);
        if (str) {
          *(outp++) = NS_xstrdup(str);
        } else {
          *(outp++) = nullptr;
        }
      }
      break;
    }

    // The rest are illegal.
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    default:
      NS_ERROR("bad type in array!");
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  *aOutType = aInType;
  *aOutCount = aInCount;
  return NS_OK;
}

/***************************************************************************/

#define TRIVIAL_DATA_CONVERTER(type_, member_, retval_) \
  if (mType == nsIDataType::type_) {                    \
    *retval_ = u.member_;                               \
    return NS_OK;                                       \
  }

#define NUMERIC_CONVERSION_METHOD_BEGIN(type_, Ctype_, name_)                 \
  nsresult nsDiscriminatedUnion::ConvertTo##name_(Ctype_* aResult) const {    \
    TRIVIAL_DATA_CONVERTER(type_, m##name_##Value, aResult)                   \
    nsDiscriminatedUnion tempData;                                            \
    nsresult rv = ToManageableNumber(&tempData);                              \
    /*                                                                     */ \
    /* NOTE: rv may indicate a success code that we want to preserve       */ \
    /* For the final return. So all the return cases below should return   */ \
    /* this rv when indicating success.                                    */ \
    /*                                                                     */ \
    if (NS_FAILED(rv)) return rv;                                             \
    switch (tempData.mType) {
#define CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(Ctype_) \
  case nsIDataType::VTYPE_INT32:                         \
    *aResult = (Ctype_)tempData.u.mInt32Value;           \
    return rv;

#define CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(Ctype_, min_, max_) \
  case nsIDataType::VTYPE_INT32: {                                 \
    int32_t value = tempData.u.mInt32Value;                        \
    if (value < min_ || value > max_)                              \
      return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                    \
    *aResult = (Ctype_)value;                                      \
    return rv;                                                     \
  }

#define CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(Ctype_) \
  case nsIDataType::VTYPE_UINT32:                         \
    *aResult = (Ctype_)tempData.u.mUint32Value;           \
    return rv;

#define CASE__NUMERIC_CONVERSION_UINT32_MAX(Ctype_, max_)       \
  case nsIDataType::VTYPE_UINT32: {                             \
    uint32_t value = tempData.u.mUint32Value;                   \
    if (value > max_) return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA; \
    *aResult = (Ctype_)value;                                   \
    return rv;                                                  \
  }

#define CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(Ctype_) \
  case nsIDataType::VTYPE_DOUBLE:                         \
    *aResult = (Ctype_)tempData.u.mDoubleValue;           \
    return rv;

#define CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX(Ctype_, min_, max_) \
  case nsIDataType::VTYPE_DOUBLE: {                                 \
    double value = tempData.u.mDoubleValue;                         \
    if (value < min_ || value > max_)                               \
      return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                     \
    *aResult = (Ctype_)value;                                       \
    return rv;                                                      \
  }

#define CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(Ctype_, min_, max_)       \
  case nsIDataType::VTYPE_DOUBLE: {                                           \
    double value = tempData.u.mDoubleValue;                                   \
    if (value < min_ || value > max_)                                         \
      return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;                               \
    *aResult = (Ctype_)value;                                                 \
    return (0.0 == fmod(value, 1.0)) ? rv                                     \
                                     : NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA; \
  }

#define CASES__NUMERIC_CONVERSION_NORMAL(Ctype_, min_, max_) \
  CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(Ctype_, min_, max_) \
  CASE__NUMERIC_CONVERSION_UINT32_MAX(Ctype_, max_)          \
  CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(Ctype_, min_, max_)

#define NUMERIC_CONVERSION_METHOD_END                      \
  default:                                                 \
    NS_ERROR("bad type returned from ToManageableNumber"); \
    return NS_ERROR_CANNOT_CONVERT_DATA;                   \
    }                                                      \
    }

#define NUMERIC_CONVERSION_METHOD_NORMAL(type_, Ctype_, name_, min_, max_) \
  NUMERIC_CONVERSION_METHOD_BEGIN(type_, Ctype_, name_)                    \
  CASES__NUMERIC_CONVERSION_NORMAL(Ctype_, min_, max_)                     \
  NUMERIC_CONVERSION_METHOD_END

/***************************************************************************/
// These expand into full public methods...

NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_INT8, uint8_t, Int8, (-127 - 1), 127)
NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_INT16, int16_t, Int16, (-32767 - 1),
                                 32767)

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_INT32, int32_t, Int32)
CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(int32_t)
CASE__NUMERIC_CONVERSION_UINT32_MAX(int32_t, 2147483647)
CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(int32_t, (-2147483647 - 1),
                                            2147483647)
NUMERIC_CONVERSION_METHOD_END

NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_UINT8, uint8_t, Uint8, 0, 255)
NUMERIC_CONVERSION_METHOD_NORMAL(VTYPE_UINT16, uint16_t, Uint16, 0, 65535)

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_UINT32, uint32_t, Uint32)
CASE__NUMERIC_CONVERSION_INT32_MIN_MAX(uint32_t, 0, 2147483647)
CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(uint32_t)
CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT(uint32_t, 0, 4294967295U)
NUMERIC_CONVERSION_METHOD_END

// XXX toFloat convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_FLOAT, float, Float)
CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(float)
CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(float)
CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(float)
NUMERIC_CONVERSION_METHOD_END

NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_DOUBLE, double, Double)
CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(double)
CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(double)
CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(double)
NUMERIC_CONVERSION_METHOD_END

// XXX toChar convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_CHAR, char, Char)
CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(char)
CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(char)
CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(char)
NUMERIC_CONVERSION_METHOD_END

// XXX toWChar convertions need to be fixed!
NUMERIC_CONVERSION_METHOD_BEGIN(VTYPE_WCHAR, char16_t, WChar)
CASE__NUMERIC_CONVERSION_INT32_JUST_CAST(char16_t)
CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST(char16_t)
CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST(char16_t)
NUMERIC_CONVERSION_METHOD_END

#undef NUMERIC_CONVERSION_METHOD_BEGIN
#undef CASE__NUMERIC_CONVERSION_INT32_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_INT32_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_UINT32_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_UINT32_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_DOUBLE_JUST_CAST
#undef CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX
#undef CASE__NUMERIC_CONVERSION_DOUBLE_MIN_MAX_INT
#undef CASES__NUMERIC_CONVERSION_NORMAL
#undef NUMERIC_CONVERSION_METHOD_END
#undef NUMERIC_CONVERSION_METHOD_NORMAL

/***************************************************************************/

// Just leverage a numeric converter for bool (but restrict the values).
// XXX Is this really what we want to do?

nsresult nsDiscriminatedUnion::ConvertToBool(bool* aResult) const {
  TRIVIAL_DATA_CONVERTER(VTYPE_BOOL, mBoolValue, aResult)

  double val;
  nsresult rv = ConvertToDouble(&val);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aResult = 0.0 != val;
  return rv;
}

/***************************************************************************/

nsresult nsDiscriminatedUnion::ConvertToInt64(int64_t* aResult) const {
  TRIVIAL_DATA_CONVERTER(VTYPE_INT64, mInt64Value, aResult)
  TRIVIAL_DATA_CONVERTER(VTYPE_UINT64, mUint64Value, aResult)

  nsDiscriminatedUnion tempData;
  nsresult rv = ToManageableNumber(&tempData);
  if (NS_FAILED(rv)) {
    return rv;
  }
  switch (tempData.mType) {
    case nsIDataType::VTYPE_INT32:
      *aResult = tempData.u.mInt32Value;
      return rv;
    case nsIDataType::VTYPE_UINT32:
      *aResult = tempData.u.mUint32Value;
      return rv;
    case nsIDataType::VTYPE_DOUBLE:
      // XXX should check for data loss here!
      *aResult = tempData.u.mDoubleValue;
      return rv;
    default:
      NS_ERROR("bad type returned from ToManageableNumber");
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

nsresult nsDiscriminatedUnion::ConvertToUint64(uint64_t* aResult) const {
  return ConvertToInt64((int64_t*)aResult);
}

/***************************************************************************/

bool nsDiscriminatedUnion::String2ID(nsID* aPid) const {
  nsAutoString tempString;
  nsAString* pString;

  switch (mType) {
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      return aPid->Parse(u.str.mStringValue);
    case nsIDataType::VTYPE_CSTRING:
      return aPid->Parse(PromiseFlatCString(*u.mCStringValue).get());
    case nsIDataType::VTYPE_UTF8STRING:
      return aPid->Parse(PromiseFlatUTF8String(*u.mUTF8StringValue).get());
    case nsIDataType::VTYPE_ASTRING:
      pString = u.mAStringValue;
      break;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      tempString.Assign(u.wstr.mWStringValue);
      pString = &tempString;
      break;
    default:
      NS_ERROR("bad type in call to String2ID");
      return false;
  }

  char* pChars = ToNewCString(*pString, mozilla::fallible);
  if (!pChars) {
    return false;
  }
  bool result = aPid->Parse(pChars);
  free(pChars);
  return result;
}

nsresult nsDiscriminatedUnion::ConvertToID(nsID* aResult) const {
  nsID id;

  switch (mType) {
    case nsIDataType::VTYPE_ID:
      *aResult = u.mIDValue;
      return NS_OK;
    case nsIDataType::VTYPE_INTERFACE:
      *aResult = NS_GET_IID(nsISupports);
      return NS_OK;
    case nsIDataType::VTYPE_INTERFACE_IS:
      *aResult = u.iface.mInterfaceID;
      return NS_OK;
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      if (!String2ID(&id)) {
        return NS_ERROR_CANNOT_CONVERT_DATA;
      }
      *aResult = id;
      return NS_OK;
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

/***************************************************************************/

nsresult nsDiscriminatedUnion::ToString(nsACString& aOutString) const {
  mozilla::SmprintfPointer pptr;

  switch (mType) {
    // all the stuff we don't handle...
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_WCHAR:
      NS_ERROR("ToString being called for a string type - screwy logic!");
      [[fallthrough]];

      // XXX We might want stringified versions of these... ???

    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      aOutString.SetIsVoid(true);
      return NS_OK;

    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;

      // nsID has its own text formatter.

    case nsIDataType::VTYPE_ID: {
      char* ptr = u.mIDValue.ToString();
      if (!ptr) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      aOutString.Assign(ptr);
      free(ptr);
      return NS_OK;
    }

    // Can't use Smprintf for floats, since it's locale-dependent
#define CASE__APPENDFLOAT_NUMBER(type_, member_) \
  case nsIDataType::type_: {                     \
    nsAutoCString str;                           \
    str.AppendFloat(u.member_);                  \
    aOutString.Assign(str);                      \
    return NS_OK;                                \
  }

      CASE__APPENDFLOAT_NUMBER(VTYPE_FLOAT, mFloatValue)
      CASE__APPENDFLOAT_NUMBER(VTYPE_DOUBLE, mDoubleValue)

#undef CASE__APPENDFLOAT_NUMBER

      // the rest can be Smprintf'd and use common code.

#define CASE__SMPRINTF_NUMBER(type_, format_, cast_, member_)          \
  case nsIDataType::type_:                                             \
    static_assert(sizeof(cast_) >= sizeof(u.member_),                  \
                  "size of type should be at least as big as member"); \
    pptr = mozilla::Smprintf(format_, (cast_)u.member_);               \
    break;

      CASE__SMPRINTF_NUMBER(VTYPE_INT8, "%d", int, mInt8Value)
      CASE__SMPRINTF_NUMBER(VTYPE_INT16, "%d", int, mInt16Value)
      CASE__SMPRINTF_NUMBER(VTYPE_INT32, "%d", int, mInt32Value)
      CASE__SMPRINTF_NUMBER(VTYPE_INT64, "%" PRId64, int64_t, mInt64Value)

      CASE__SMPRINTF_NUMBER(VTYPE_UINT8, "%u", unsigned, mUint8Value)
      CASE__SMPRINTF_NUMBER(VTYPE_UINT16, "%u", unsigned, mUint16Value)
      CASE__SMPRINTF_NUMBER(VTYPE_UINT32, "%u", unsigned, mUint32Value)
      CASE__SMPRINTF_NUMBER(VTYPE_UINT64, "%" PRIu64, int64_t, mUint64Value)

      // XXX Would we rather print "true" / "false" ?
      CASE__SMPRINTF_NUMBER(VTYPE_BOOL, "%d", int, mBoolValue)

      CASE__SMPRINTF_NUMBER(VTYPE_CHAR, "%c", char, mCharValue)

#undef CASE__SMPRINTF_NUMBER
  }

  if (!pptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOutString.Assign(pptr.get());
  return NS_OK;
}

nsresult nsDiscriminatedUnion::ConvertToAString(nsAString& aResult) const {
  switch (mType) {
    case nsIDataType::VTYPE_ASTRING:
      aResult.Assign(*u.mAStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      CopyASCIItoUTF16(*u.mCStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      CopyUTF8toUTF16(*u.mUTF8StringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
      CopyASCIItoUTF16(mozilla::MakeStringSpan(u.str.mStringValue), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
      aResult.Assign(u.wstr.mWStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      CopyASCIItoUTF16(
          nsDependentCString(u.str.mStringValue, u.str.mStringLength), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      aResult.Assign(u.wstr.mWStringValue, u.wstr.mWStringLength);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR:
      aResult.Assign(u.mWCharValue);
      return NS_OK;
    default: {
      nsAutoCString tempCString;
      nsresult rv = ToString(tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      CopyASCIItoUTF16(tempCString, aResult);
      return NS_OK;
    }
  }
}

nsresult nsDiscriminatedUnion::ConvertToACString(nsACString& aResult) const {
  switch (mType) {
    case nsIDataType::VTYPE_ASTRING:
      LossyCopyUTF16toASCII(*u.mAStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      aResult.Assign(*u.mCStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      // XXX This is an extra copy that should be avoided
      // once Jag lands support for UTF8String and associated
      // conversion methods.
      LossyCopyUTF16toASCII(NS_ConvertUTF8toUTF16(*u.mUTF8StringValue),
                            aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
      aResult.Assign(*u.str.mStringValue);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
      LossyCopyUTF16toASCII(nsDependentString(u.wstr.mWStringValue), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      aResult.Assign(u.str.mStringValue, u.str.mStringLength);
      return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      LossyCopyUTF16toASCII(
          nsDependentString(u.wstr.mWStringValue, u.wstr.mWStringLength),
          aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR: {
      const char16_t* str = &u.mWCharValue;
      LossyCopyUTF16toASCII(Substring(str, 1), aResult);
      return NS_OK;
    }
    default:
      return ToString(aResult);
  }
}

nsresult nsDiscriminatedUnion::ConvertToAUTF8String(
    nsAUTF8String& aResult) const {
  switch (mType) {
    case nsIDataType::VTYPE_ASTRING:
      CopyUTF16toUTF8(*u.mAStringValue, aResult);
      return NS_OK;
    case nsIDataType::VTYPE_CSTRING:
      // XXX Extra copy, can be removed if we're sure CSTRING can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(*u.mCStringValue), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_UTF8STRING:
      aResult.Assign(*u.mUTF8StringValue);
      return NS_OK;
    case nsIDataType::VTYPE_CHAR_STR:
      // XXX Extra copy, can be removed if we're sure CHAR_STR can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(u.str.mStringValue), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR_STR:
      CopyUTF16toUTF8(mozilla::MakeStringSpan(u.wstr.mWStringValue), aResult);
      return NS_OK;
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      // XXX Extra copy, can be removed if we're sure CHAR_STR can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(nsDependentCString(
                          u.str.mStringValue, u.str.mStringLength)),
                      aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      CopyUTF16toUTF8(
          nsDependentString(u.wstr.mWStringValue, u.wstr.mWStringLength),
          aResult);
      return NS_OK;
    case nsIDataType::VTYPE_WCHAR: {
      const char16_t* str = &u.mWCharValue;
      CopyUTF16toUTF8(Substring(str, 1), aResult);
      return NS_OK;
    }
    default: {
      nsAutoCString tempCString;
      nsresult rv = ToString(tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // XXX Extra copy, can be removed if we're sure tempCString can
      //     only contain ASCII.
      CopyUTF16toUTF8(NS_ConvertASCIItoUTF16(tempCString), aResult);
      return NS_OK;
    }
  }
}

nsresult nsDiscriminatedUnion::ConvertToString(char** aResult) const {
  uint32_t ignored;
  return ConvertToStringWithSize(&ignored, aResult);
}

nsresult nsDiscriminatedUnion::ConvertToWString(char16_t** aResult) const {
  uint32_t ignored;
  return ConvertToWStringWithSize(&ignored, aResult);
}

nsresult nsDiscriminatedUnion::ConvertToStringWithSize(uint32_t* aSize,
                                                       char** aStr) const {
  nsAutoString tempString;
  nsAutoCString tempCString;
  nsresult rv;

  switch (mType) {
    case nsIDataType::VTYPE_ASTRING:
      *aSize = u.mAStringValue->Length();
      *aStr = ToNewCString(*u.mAStringValue);
      break;
    case nsIDataType::VTYPE_CSTRING:
      *aSize = u.mCStringValue->Length();
      *aStr = ToNewCString(*u.mCStringValue);
      break;
    case nsIDataType::VTYPE_UTF8STRING: {
      // XXX This is doing 1 extra copy.  Need to fix this
      // when Jag lands UTF8String
      // we want:
      // *aSize = *mUTF8StringValue->Length();
      // *aStr = ToNewCString(*mUTF8StringValue);
      // But this will have to do for now.
      const NS_ConvertUTF8toUTF16 tempString16(*u.mUTF8StringValue);
      *aSize = tempString16.Length();
      *aStr = ToNewCString(tempString16);
      break;
    }
    case nsIDataType::VTYPE_CHAR_STR: {
      nsDependentCString cString(u.str.mStringValue);
      *aSize = cString.Length();
      *aStr = ToNewCString(cString);
      break;
    }
    case nsIDataType::VTYPE_WCHAR_STR: {
      nsDependentString string(u.wstr.mWStringValue);
      *aSize = string.Length();
      *aStr = ToNewCString(string);
      break;
    }
    case nsIDataType::VTYPE_STRING_SIZE_IS: {
      nsDependentCString cString(u.str.mStringValue, u.str.mStringLength);
      *aSize = cString.Length();
      *aStr = ToNewCString(cString);
      break;
    }
    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      nsDependentString string(u.wstr.mWStringValue, u.wstr.mWStringLength);
      *aSize = string.Length();
      *aStr = ToNewCString(string);
      break;
    }
    case nsIDataType::VTYPE_WCHAR:
      tempString.Assign(u.mWCharValue);
      *aSize = tempString.Length();
      *aStr = ToNewCString(tempString);
      break;
    default:
      rv = ToString(tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      *aSize = tempCString.Length();
      *aStr = ToNewCString(tempCString);
      break;
  }

  return *aStr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
nsresult nsDiscriminatedUnion::ConvertToWStringWithSize(uint32_t* aSize,
                                                        char16_t** aStr) const {
  nsAutoString tempString;
  nsAutoCString tempCString;
  nsresult rv;

  switch (mType) {
    case nsIDataType::VTYPE_ASTRING:
      *aSize = u.mAStringValue->Length();
      *aStr = ToNewUnicode(*u.mAStringValue);
      break;
    case nsIDataType::VTYPE_CSTRING:
      *aSize = u.mCStringValue->Length();
      *aStr = ToNewUnicode(*u.mCStringValue);
      break;
    case nsIDataType::VTYPE_UTF8STRING: {
      *aStr = UTF8ToNewUnicode(*u.mUTF8StringValue, aSize);
      break;
    }
    case nsIDataType::VTYPE_CHAR_STR: {
      nsDependentCString cString(u.str.mStringValue);
      *aSize = cString.Length();
      *aStr = ToNewUnicode(cString);
      break;
    }
    case nsIDataType::VTYPE_WCHAR_STR: {
      nsDependentString string(u.wstr.mWStringValue);
      *aSize = string.Length();
      *aStr = ToNewUnicode(string);
      break;
    }
    case nsIDataType::VTYPE_STRING_SIZE_IS: {
      nsDependentCString cString(u.str.mStringValue, u.str.mStringLength);
      *aSize = cString.Length();
      *aStr = ToNewUnicode(cString);
      break;
    }
    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      nsDependentString string(u.wstr.mWStringValue, u.wstr.mWStringLength);
      *aSize = string.Length();
      *aStr = ToNewUnicode(string);
      break;
    }
    case nsIDataType::VTYPE_WCHAR:
      tempString.Assign(u.mWCharValue);
      *aSize = tempString.Length();
      *aStr = ToNewUnicode(tempString);
      break;
    default:
      rv = ToString(tempCString);
      if (NS_FAILED(rv)) {
        return rv;
      }
      *aSize = tempCString.Length();
      *aStr = ToNewUnicode(tempCString);
      break;
  }

  return *aStr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsDiscriminatedUnion::ConvertToISupports(nsISupports** aResult) const {
  switch (mType) {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      if (u.iface.mInterfaceValue) {
        return u.iface.mInterfaceValue->QueryInterface(NS_GET_IID(nsISupports),
                                                       (void**)aResult);
      } else {
        *aResult = nullptr;
        return NS_OK;
      }
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }
}

nsresult nsDiscriminatedUnion::ConvertToInterface(nsIID** aIID,
                                                  void** aInterface) const {
  const nsIID* piid;

  switch (mType) {
    case nsIDataType::VTYPE_INTERFACE:
      piid = &NS_GET_IID(nsISupports);
      break;
    case nsIDataType::VTYPE_INTERFACE_IS:
      piid = &u.iface.mInterfaceID;
      break;
    default:
      return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  *aIID = piid->Clone();

  if (u.iface.mInterfaceValue) {
    return u.iface.mInterfaceValue->QueryInterface(*piid, aInterface);
  }

  *aInterface = nullptr;
  return NS_OK;
}

nsresult nsDiscriminatedUnion::ConvertToArray(uint16_t* aType, nsIID* aIID,
                                              uint32_t* aCount,
                                              void** aPtr) const {
  // XXX perhaps we'd like to add support for converting each of the various
  // types into an array containing one element of that type. We can leverage
  // CloneArray to do this if we want to support this.

  if (mType == nsIDataType::VTYPE_ARRAY) {
    return CloneArray(u.array.mArrayType, &u.array.mArrayInterfaceID,
                      u.array.mArrayCount, u.array.mArrayValue, aType, aIID,
                      aCount, aPtr);
  }
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

/***************************************************************************/
// static setter functions...

#define DATA_SETTER_PROLOGUE Cleanup()

#define DATA_SETTER_EPILOGUE(type_) mType = nsIDataType::type_;

#define DATA_SETTER(type_, member_, value_) \
  DATA_SETTER_PROLOGUE;                     \
  u.member_ = value_;                       \
  DATA_SETTER_EPILOGUE(type_)

#define DATA_SETTER_WITH_CAST(type_, member_, cast_, value_) \
  DATA_SETTER_PROLOGUE;                                      \
  u.member_ = cast_ value_;                                  \
  DATA_SETTER_EPILOGUE(type_)

/********************************************/

#define CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_) {
#define CASE__SET_FROM_VARIANT_VTYPE__GETTER(member_, name_) \
  rv = aValue->GetAs##name_(&(u.member_));

#define CASE__SET_FROM_VARIANT_VTYPE__GETTER_CAST(cast_, member_, name_) \
  rv = aValue->GetAs##name_(cast_ & (u.member_));

#define CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_) \
  if (NS_SUCCEEDED(rv)) {                            \
    mType = nsIDataType::type_;                      \
  }                                                  \
  break;                                             \
  }

#define CASE__SET_FROM_VARIANT_TYPE(type_, member_, name_) \
  case nsIDataType::type_:                                 \
    CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)           \
    CASE__SET_FROM_VARIANT_VTYPE__GETTER(member_, name_)   \
    CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)

#define CASE__SET_FROM_VARIANT_VTYPE_CAST(type_, cast_, member_, name_) \
  case nsIDataType::type_:                                              \
    CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(type_)                        \
    CASE__SET_FROM_VARIANT_VTYPE__GETTER_CAST(cast_, member_, name_)    \
    CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(type_)

nsresult nsDiscriminatedUnion::SetFromVariant(nsIVariant* aValue) {
  nsresult rv = NS_OK;

  Cleanup();

  uint16_t type = aValue->GetDataType();

  switch (type) {
    CASE__SET_FROM_VARIANT_VTYPE_CAST(VTYPE_INT8, (uint8_t*), mInt8Value, Int8)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_INT16, mInt16Value, Int16)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_INT32, mInt32Value, Int32)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT8, mUint8Value, Uint8)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT16, mUint16Value, Uint16)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_UINT32, mUint32Value, Uint32)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_FLOAT, mFloatValue, Float)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_DOUBLE, mDoubleValue, Double)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_BOOL, mBoolValue, Bool)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_CHAR, mCharValue, Char)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_WCHAR, mWCharValue, WChar)
    CASE__SET_FROM_VARIANT_TYPE(VTYPE_ID, mIDValue, ID)

    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_ASTRING);
      u.mAStringValue = new nsString();

      rv = aValue->GetAsAString(*u.mAStringValue);
      if (NS_FAILED(rv)) {
        delete u.mAStringValue;
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_ASTRING)

    case nsIDataType::VTYPE_CSTRING:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_CSTRING);
      u.mCStringValue = new nsCString();

      rv = aValue->GetAsACString(*u.mCStringValue);
      if (NS_FAILED(rv)) {
        delete u.mCStringValue;
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_CSTRING)

    case nsIDataType::VTYPE_UTF8STRING:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_UTF8STRING);
      u.mUTF8StringValue = new nsUTF8String();

      rv = aValue->GetAsAUTF8String(*u.mUTF8StringValue);
      if (NS_FAILED(rv)) {
        delete u.mUTF8StringValue;
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_UTF8STRING)

    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_STRING_SIZE_IS);
      rv = aValue->GetAsStringWithSize(&u.str.mStringLength,
                                       &u.str.mStringValue);
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_STRING_SIZE_IS)

    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_INTERFACE_IS);
      // XXX This iid handling is ugly!
      nsIID* iid;
      rv = aValue->GetAsInterface(&iid, (void**)&u.iface.mInterfaceValue);
      if (NS_SUCCEEDED(rv)) {
        u.iface.mInterfaceID = *iid;
        free((char*)iid);
      }
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_INTERFACE_IS)

    case nsIDataType::VTYPE_ARRAY:
      CASE__SET_FROM_VARIANT_VTYPE_PROLOGUE(VTYPE_ARRAY);
      rv = aValue->GetAsArray(&u.array.mArrayType, &u.array.mArrayInterfaceID,
                              &u.array.mArrayCount, &u.array.mArrayValue);
      CASE__SET_FROM_VARIANT_VTYPE_EPILOGUE(VTYPE_ARRAY)

    case nsIDataType::VTYPE_VOID:
      SetToVoid();
      rv = NS_OK;
      break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
      SetToEmptyArray();
      rv = NS_OK;
      break;
    case nsIDataType::VTYPE_EMPTY:
      SetToEmpty();
      rv = NS_OK;
      break;
    default:
      NS_ERROR("bad type in variant!");
      rv = NS_ERROR_FAILURE;
      break;
  }
  return rv;
}

void nsDiscriminatedUnion::SetFromInt8(uint8_t aValue) {
  DATA_SETTER_WITH_CAST(VTYPE_INT8, mInt8Value, (uint8_t), aValue);
}
void nsDiscriminatedUnion::SetFromInt16(int16_t aValue) {
  DATA_SETTER(VTYPE_INT16, mInt16Value, aValue);
}
void nsDiscriminatedUnion::SetFromInt32(int32_t aValue) {
  DATA_SETTER(VTYPE_INT32, mInt32Value, aValue);
}
void nsDiscriminatedUnion::SetFromInt64(int64_t aValue) {
  DATA_SETTER(VTYPE_INT64, mInt64Value, aValue);
}
void nsDiscriminatedUnion::SetFromUint8(uint8_t aValue) {
  DATA_SETTER(VTYPE_UINT8, mUint8Value, aValue);
}
void nsDiscriminatedUnion::SetFromUint16(uint16_t aValue) {
  DATA_SETTER(VTYPE_UINT16, mUint16Value, aValue);
}
void nsDiscriminatedUnion::SetFromUint32(uint32_t aValue) {
  DATA_SETTER(VTYPE_UINT32, mUint32Value, aValue);
}
void nsDiscriminatedUnion::SetFromUint64(uint64_t aValue) {
  DATA_SETTER(VTYPE_UINT64, mUint64Value, aValue);
}
void nsDiscriminatedUnion::SetFromFloat(float aValue) {
  DATA_SETTER(VTYPE_FLOAT, mFloatValue, aValue);
}
void nsDiscriminatedUnion::SetFromDouble(double aValue) {
  DATA_SETTER(VTYPE_DOUBLE, mDoubleValue, aValue);
}
void nsDiscriminatedUnion::SetFromBool(bool aValue) {
  DATA_SETTER(VTYPE_BOOL, mBoolValue, aValue);
}
void nsDiscriminatedUnion::SetFromChar(char aValue) {
  DATA_SETTER(VTYPE_CHAR, mCharValue, aValue);
}
void nsDiscriminatedUnion::SetFromWChar(char16_t aValue) {
  DATA_SETTER(VTYPE_WCHAR, mWCharValue, aValue);
}
void nsDiscriminatedUnion::SetFromID(const nsID& aValue) {
  DATA_SETTER(VTYPE_ID, mIDValue, aValue);
}
void nsDiscriminatedUnion::SetFromAString(const nsAString& aValue) {
  DATA_SETTER_PROLOGUE;
  u.mAStringValue = new nsString(aValue);
  DATA_SETTER_EPILOGUE(VTYPE_ASTRING);
}

void nsDiscriminatedUnion::SetFromACString(const nsACString& aValue) {
  DATA_SETTER_PROLOGUE;
  u.mCStringValue = new nsCString(aValue);
  DATA_SETTER_EPILOGUE(VTYPE_CSTRING);
}

void nsDiscriminatedUnion::SetFromAUTF8String(const nsAUTF8String& aValue) {
  DATA_SETTER_PROLOGUE;
  u.mUTF8StringValue = new nsUTF8String(aValue);
  DATA_SETTER_EPILOGUE(VTYPE_UTF8STRING);
}

nsresult nsDiscriminatedUnion::SetFromString(const char* aValue) {
  DATA_SETTER_PROLOGUE;
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  return SetFromStringWithSize(strlen(aValue), aValue);
}
nsresult nsDiscriminatedUnion::SetFromWString(const char16_t* aValue) {
  DATA_SETTER_PROLOGUE;
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  return SetFromWStringWithSize(NS_strlen(aValue), aValue);
}
void nsDiscriminatedUnion::SetFromISupports(nsISupports* aValue) {
  return SetFromInterface(NS_GET_IID(nsISupports), aValue);
}
void nsDiscriminatedUnion::SetFromInterface(const nsIID& aIID,
                                            nsISupports* aValue) {
  DATA_SETTER_PROLOGUE;
  NS_IF_ADDREF(aValue);
  u.iface.mInterfaceValue = aValue;
  u.iface.mInterfaceID = aIID;
  DATA_SETTER_EPILOGUE(VTYPE_INTERFACE_IS);
}
nsresult nsDiscriminatedUnion::SetFromArray(uint16_t aType, const nsIID* aIID,
                                            uint32_t aCount, void* aValue) {
  DATA_SETTER_PROLOGUE;
  if (!aValue || !aCount) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = CloneArray(aType, aIID, aCount, aValue, &u.array.mArrayType,
                           &u.array.mArrayInterfaceID, &u.array.mArrayCount,
                           &u.array.mArrayValue);
  if (NS_FAILED(rv)) {
    return rv;
  }
  DATA_SETTER_EPILOGUE(VTYPE_ARRAY);
  return NS_OK;
}
nsresult nsDiscriminatedUnion::SetFromStringWithSize(uint32_t aSize,
                                                     const char* aValue) {
  DATA_SETTER_PROLOGUE;
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  u.str.mStringValue = (char*)moz_xmemdup(aValue, (aSize + 1) * sizeof(char));
  u.str.mStringLength = aSize;
  DATA_SETTER_EPILOGUE(VTYPE_STRING_SIZE_IS);
  return NS_OK;
}
nsresult nsDiscriminatedUnion::SetFromWStringWithSize(uint32_t aSize,
                                                      const char16_t* aValue) {
  DATA_SETTER_PROLOGUE;
  if (!aValue) {
    return NS_ERROR_NULL_POINTER;
  }
  u.wstr.mWStringValue =
      (char16_t*)moz_xmemdup(aValue, (aSize + 1) * sizeof(char16_t));
  u.wstr.mWStringLength = aSize;
  DATA_SETTER_EPILOGUE(VTYPE_WSTRING_SIZE_IS);
  return NS_OK;
}
void nsDiscriminatedUnion::AllocateWStringWithSize(uint32_t aSize) {
  DATA_SETTER_PROLOGUE;
  u.wstr.mWStringValue = (char16_t*)moz_xmalloc((aSize + 1) * sizeof(char16_t));
  u.wstr.mWStringValue[aSize] = '\0';
  u.wstr.mWStringLength = aSize;
  DATA_SETTER_EPILOGUE(VTYPE_WSTRING_SIZE_IS);
}
void nsDiscriminatedUnion::SetToVoid() {
  DATA_SETTER_PROLOGUE;
  DATA_SETTER_EPILOGUE(VTYPE_VOID);
}
void nsDiscriminatedUnion::SetToEmpty() {
  DATA_SETTER_PROLOGUE;
  DATA_SETTER_EPILOGUE(VTYPE_EMPTY);
}
void nsDiscriminatedUnion::SetToEmptyArray() {
  DATA_SETTER_PROLOGUE;
  DATA_SETTER_EPILOGUE(VTYPE_EMPTY_ARRAY);
}

/***************************************************************************/

void nsDiscriminatedUnion::Cleanup() {
  switch (mType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    case nsIDataType::VTYPE_BOOL:
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ID:
      break;
    case nsIDataType::VTYPE_ASTRING:
      delete u.mAStringValue;
      break;
    case nsIDataType::VTYPE_CSTRING:
      delete u.mCStringValue;
      break;
    case nsIDataType::VTYPE_UTF8STRING:
      delete u.mUTF8StringValue;
      break;
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      free((char*)u.str.mStringValue);
      break;
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      free((char*)u.wstr.mWStringValue);
      break;
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      NS_IF_RELEASE(u.iface.mInterfaceValue);
      break;
    case nsIDataType::VTYPE_ARRAY:
      FreeArray();
      break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_EMPTY:
      break;
    default:
      NS_ERROR("bad type in variant!");
      break;
  }

  mType = nsIDataType::VTYPE_EMPTY;
}

void nsDiscriminatedUnion::Traverse(
    nsCycleCollectionTraversalCallback& aCb) const {
  switch (mType) {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mData");
      aCb.NoteXPCOMChild(u.iface.mInterfaceValue);
      break;
    case nsIDataType::VTYPE_ARRAY:
      switch (u.array.mArrayType) {
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS: {
          nsISupports** p = (nsISupports**)u.array.mArrayValue;
          for (uint32_t i = u.array.mArrayCount; i > 0; ++p, --i) {
            NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mData[i]");
            aCb.NoteXPCOMChild(*p);
          }
          break;
        }
        default:
          break;
      }
      break;
    default:
      break;
  }
}

/***************************************************************************/
/***************************************************************************/
// members...

nsVariantBase::nsVariantBase() : mWritable(true) {}

// For all the data getters we just forward to the static (and sharable)
// 'ConvertTo' functions.

uint16_t nsVariantBase::GetDataType() { return mData.GetType(); }

NS_IMETHODIMP
nsVariantBase::GetAsInt8(uint8_t* aResult) {
  return mData.ConvertToInt8(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsInt16(int16_t* aResult) {
  return mData.ConvertToInt16(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsInt32(int32_t* aResult) {
  return mData.ConvertToInt32(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsInt64(int64_t* aResult) {
  return mData.ConvertToInt64(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsUint8(uint8_t* aResult) {
  return mData.ConvertToUint8(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsUint16(uint16_t* aResult) {
  return mData.ConvertToUint16(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsUint32(uint32_t* aResult) {
  return mData.ConvertToUint32(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsUint64(uint64_t* aResult) {
  return mData.ConvertToUint64(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsFloat(float* aResult) {
  return mData.ConvertToFloat(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsDouble(double* aResult) {
  return mData.ConvertToDouble(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsBool(bool* aResult) { return mData.ConvertToBool(aResult); }

NS_IMETHODIMP
nsVariantBase::GetAsChar(char* aResult) { return mData.ConvertToChar(aResult); }

NS_IMETHODIMP
nsVariantBase::GetAsWChar(char16_t* aResult) {
  return mData.ConvertToWChar(aResult);
}

NS_IMETHODIMP_(nsresult)
nsVariantBase::GetAsID(nsID* aResult) { return mData.ConvertToID(aResult); }

NS_IMETHODIMP
nsVariantBase::GetAsAString(nsAString& aResult) {
  return mData.ConvertToAString(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsACString(nsACString& aResult) {
  return mData.ConvertToACString(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsAUTF8String(nsAUTF8String& aResult) {
  return mData.ConvertToAUTF8String(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsString(char** aResult) {
  return mData.ConvertToString(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsWString(char16_t** aResult) {
  return mData.ConvertToWString(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsISupports(nsISupports** aResult) {
  return mData.ConvertToISupports(aResult);
}

NS_IMETHODIMP
nsVariantBase::GetAsJSVal(JS::MutableHandleValue) {
  // Can only get the jsval from an XPCVariant.
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

NS_IMETHODIMP
nsVariantBase::GetAsInterface(nsIID** aIID, void** aInterface) {
  return mData.ConvertToInterface(aIID, aInterface);
}

NS_IMETHODIMP_(nsresult)
nsVariantBase::GetAsArray(uint16_t* aType, nsIID* aIID, uint32_t* aCount,
                          void** aPtr) {
  return mData.ConvertToArray(aType, aIID, aCount, aPtr);
}

NS_IMETHODIMP
nsVariantBase::GetAsStringWithSize(uint32_t* aSize, char** aStr) {
  return mData.ConvertToStringWithSize(aSize, aStr);
}

NS_IMETHODIMP
nsVariantBase::GetAsWStringWithSize(uint32_t* aSize, char16_t** aStr) {
  return mData.ConvertToWStringWithSize(aSize, aStr);
}

/***************************************************************************/

NS_IMETHODIMP
nsVariantBase::GetWritable(bool* aWritable) {
  *aWritable = mWritable;
  return NS_OK;
}
NS_IMETHODIMP
nsVariantBase::SetWritable(bool aWritable) {
  if (!mWritable && aWritable) {
    return NS_ERROR_FAILURE;
  }
  mWritable = aWritable;
  return NS_OK;
}

/***************************************************************************/

// For all the data setters we just forward to the static (and sharable)
// 'SetFrom' functions.

NS_IMETHODIMP
nsVariantBase::SetAsInt8(uint8_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromInt8(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsInt16(int16_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromInt16(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsInt32(int32_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromInt32(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsInt64(int64_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromInt64(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsUint8(uint8_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromUint8(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsUint16(uint16_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromUint16(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsUint32(uint32_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromUint32(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsUint64(uint64_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromUint64(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsFloat(float aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromFloat(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsDouble(double aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromDouble(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsBool(bool aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromBool(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsChar(char aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromChar(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsWChar(char16_t aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromWChar(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsID(const nsID& aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromID(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsAString(const nsAString& aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromAString(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsACString(const nsACString& aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromACString(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsAUTF8String(const nsAUTF8String& aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromAUTF8String(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsString(const char* aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return mData.SetFromString(aValue);
}

NS_IMETHODIMP
nsVariantBase::SetAsWString(const char16_t* aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return mData.SetFromWString(aValue);
}

NS_IMETHODIMP
nsVariantBase::SetAsISupports(nsISupports* aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromISupports(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsInterface(const nsIID& aIID, void* aInterface) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetFromInterface(aIID, (nsISupports*)aInterface);
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsArray(uint16_t aType, const nsIID* aIID, uint32_t aCount,
                          void* aPtr) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return mData.SetFromArray(aType, aIID, aCount, aPtr);
}

NS_IMETHODIMP
nsVariantBase::SetAsStringWithSize(uint32_t aSize, const char* aStr) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return mData.SetFromStringWithSize(aSize, aStr);
}

NS_IMETHODIMP
nsVariantBase::SetAsWStringWithSize(uint32_t aSize, const char16_t* aStr) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return mData.SetFromWStringWithSize(aSize, aStr);
}

NS_IMETHODIMP
nsVariantBase::SetAsVoid() {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetToVoid();
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsEmpty() {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetToEmpty();
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetAsEmptyArray() {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  mData.SetToEmptyArray();
  return NS_OK;
}

NS_IMETHODIMP
nsVariantBase::SetFromVariant(nsIVariant* aValue) {
  if (!mWritable) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }
  return mData.SetFromVariant(aValue);
}

/* nsVariant implementation */

NS_IMPL_ISUPPORTS(nsVariant, nsIVariant, nsIWritableVariant)

/* nsVariantCC implementation */
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsVariantCC)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIVariant)
  NS_INTERFACE_MAP_ENTRY(nsIWritableVariant)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsVariantCC)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsVariantCC)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsVariantCC)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsVariantCC)
  tmp->mData.Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsVariantCC)
  tmp->mData.Cleanup();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
