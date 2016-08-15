/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSupportsPrimitives.h"
#include "nsMemory.h"
#include "mozilla/Assertions.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"
#include <algorithm>

template<typename T>
static char*
DataToString(const char* aFormat, T aData)
{
  static const int size = 32;
  char buf[size];

  int len = SprintfLiteral(buf, aFormat, aData);
  MOZ_ASSERT(len >= 0);

  return static_cast<char*>(nsMemory::Clone(buf, std::min(len + 1, size) *
                                                 sizeof(char)));
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsID, nsISupportsID, nsISupportsPrimitive)

nsSupportsID::nsSupportsID()
  : mData(nullptr)
{
}

NS_IMETHODIMP
nsSupportsID::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_ID;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsID::GetData(nsID** aData)
{
  NS_ASSERTION(aData, "Bad pointer");

  if (mData) {
    *aData = static_cast<nsID*>(nsMemory::Clone(mData, sizeof(nsID)));
  } else {
    *aData = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsID::SetData(const nsID* aData)
{
  if (mData) {
    free(mData);
  }

  if (aData) {
    mData = static_cast<nsID*>(nsMemory::Clone(aData, sizeof(nsID)));
  } else {
    mData = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsID::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");

  if (mData) {
    *aResult = mData->ToString();
  } else {
    static const char nullStr[] = "null";
    *aResult = static_cast<char*>(nsMemory::Clone(nullStr, sizeof(nullStr)));
  }

  return NS_OK;
}

/*****************************************************************************
 * nsSupportsCString
 *****************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsCString, nsISupportsCString,
                  nsISupportsPrimitive)

NS_IMETHODIMP
nsSupportsCString::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_CSTRING;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCString::GetData(nsACString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCString::ToString(char** aResult)
{
  *aResult = ToNewCString(mData);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCString::SetData(const nsACString& aData)
{
  bool ok = mData.Assign(aData, mozilla::fallible);
  if (!ok) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/*****************************************************************************
 * nsSupportsString
 *****************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsString, nsISupportsString,
                  nsISupportsPrimitive)

NS_IMETHODIMP
nsSupportsString::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_STRING;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsString::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsString::ToString(char16_t** aResult)
{
  *aResult = ToNewUnicode(mData);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsString::SetData(const nsAString& aData)
{
  bool ok = mData.Assign(aData, mozilla::fallible);
  if (!ok) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRBool, nsISupportsPRBool,
                  nsISupportsPrimitive)

nsSupportsPRBool::nsSupportsPRBool()
  : mData(false)
{
}

NS_IMETHODIMP
nsSupportsPRBool::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRBOOL;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRBool::GetData(bool* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRBool::SetData(bool aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRBool::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  const char* str = mData ? "true" : "false";
  *aResult = static_cast<char*>(nsMemory::Clone(str, (strlen(str) + 1) *
                                                     sizeof(char)));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint8, nsISupportsPRUint8,
                  nsISupportsPrimitive)

nsSupportsPRUint8::nsSupportsPRUint8()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint8::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT8;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint8::GetData(uint8_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint8::SetData(uint8_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint8::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%u", static_cast<unsigned int>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint16, nsISupportsPRUint16,
                  nsISupportsPrimitive)

nsSupportsPRUint16::nsSupportsPRUint16()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint16::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT16;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint16::GetData(uint16_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint16::SetData(uint16_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint16::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%u", static_cast<unsigned int>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint32, nsISupportsPRUint32,
                  nsISupportsPrimitive)

nsSupportsPRUint32::nsSupportsPRUint32()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint32::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT32;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint32::GetData(uint32_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint32::SetData(uint32_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint32::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%u", mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint64, nsISupportsPRUint64,
                  nsISupportsPrimitive)

nsSupportsPRUint64::nsSupportsPRUint64()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint64::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT64;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint64::GetData(uint64_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint64::SetData(uint64_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint64::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%llu", mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRTime, nsISupportsPRTime,
                  nsISupportsPrimitive)

nsSupportsPRTime::nsSupportsPRTime()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRTime::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRTIME;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRTime::GetData(PRTime* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRTime::SetData(PRTime aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRTime::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%" PRIu64, mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsChar, nsISupportsChar,
                  nsISupportsPrimitive)

nsSupportsChar::nsSupportsChar()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsChar::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_CHAR;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsChar::GetData(char* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsChar::SetData(char aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsChar::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = static_cast<char*>(moz_xmalloc(2 * sizeof(char)));
  *aResult[0] = mData;
  *aResult[1] = '\0';

  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRInt16, nsISupportsPRInt16,
                  nsISupportsPrimitive)

nsSupportsPRInt16::nsSupportsPRInt16()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRInt16::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRINT16;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt16::GetData(int16_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt16::SetData(int16_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt16::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%d", static_cast<int>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRInt32, nsISupportsPRInt32,
                  nsISupportsPrimitive)

nsSupportsPRInt32::nsSupportsPRInt32()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRInt32::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRINT32;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt32::GetData(int32_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt32::SetData(int32_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt32::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%d", mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRInt64, nsISupportsPRInt64,
                  nsISupportsPrimitive)

nsSupportsPRInt64::nsSupportsPRInt64()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRInt64::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRINT64;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt64::GetData(int64_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt64::SetData(int64_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt64::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%" PRId64, mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsFloat, nsISupportsFloat,
                  nsISupportsPrimitive)

nsSupportsFloat::nsSupportsFloat()
  : mData(float(0.0))
{
}

NS_IMETHODIMP
nsSupportsFloat::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_FLOAT;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsFloat::GetData(float* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsFloat::SetData(float aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsFloat::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%f", static_cast<double>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsDouble, nsISupportsDouble,
                  nsISupportsPrimitive)

nsSupportsDouble::nsSupportsDouble()
  : mData(double(0.0))
{
}

NS_IMETHODIMP
nsSupportsDouble::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_DOUBLE;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDouble::GetData(double* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDouble::SetData(double aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDouble::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%f", mData);
  return  NS_OK;
}

/***************************************************************************/


NS_IMPL_ISUPPORTS(nsSupportsVoid, nsISupportsVoid,
                  nsISupportsPrimitive)

nsSupportsVoid::nsSupportsVoid()
  : mData(nullptr)
{
}

NS_IMETHODIMP
nsSupportsVoid::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_VOID;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsVoid::GetData(void** aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsVoid::SetData(void* aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsVoid::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  static const char str[] = "[raw data]";
  *aResult = static_cast<char*>(nsMemory::Clone(str, sizeof(str)));
  return NS_OK;
}

/***************************************************************************/


NS_IMPL_ISUPPORTS(nsSupportsInterfacePointer,
                  nsISupportsInterfacePointer,
                  nsISupportsPrimitive)

nsSupportsInterfacePointer::nsSupportsInterfacePointer()
  : mIID(nullptr)
{
}

nsSupportsInterfacePointer::~nsSupportsInterfacePointer()
{
  if (mIID) {
    free(mIID);
  }
}

NS_IMETHODIMP
nsSupportsInterfacePointer::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_INTERFACE_POINTER;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointer::GetData(nsISupports** aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  NS_IF_ADDREF(*aData);
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointer::SetData(nsISupports* aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointer::GetDataIID(nsID** aIID)
{
  NS_ASSERTION(aIID, "Bad pointer");

  if (mIID) {
    *aIID = static_cast<nsID*>(nsMemory::Clone(mIID, sizeof(nsID)));
  } else {
    *aIID = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointer::SetDataIID(const nsID* aIID)
{
  if (mIID) {
    free(mIID);
  }

  if (aIID) {
    mIID = static_cast<nsID*>(nsMemory::Clone(aIID, sizeof(nsID)));
  } else {
    mIID = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointer::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");

  static const char str[] = "[interface pointer]";
  // jband sez: think about asking nsIInterfaceInfoManager whether
  // the interface has a known human-readable name
  *aResult = static_cast<char*>(nsMemory::Clone(str, sizeof(str)));
  return  NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsDependentCString, nsISupportsCString,
                  nsISupportsPrimitive)

nsSupportsDependentCString::nsSupportsDependentCString(const char* aStr)
  : mData(aStr)
{ }

NS_IMETHODIMP
nsSupportsDependentCString::GetType(uint16_t* aType)
{
  if (NS_WARN_IF(!aType)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aType = TYPE_CSTRING;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDependentCString::GetData(nsACString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDependentCString::ToString(char** aResult)
{
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = ToNewCString(mData);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDependentCString::SetData(const nsACString& aData)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
