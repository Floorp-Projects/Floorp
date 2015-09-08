/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSupportsPrimitives.h"
#include "nsMemory.h"
#include "mozilla/Assertions.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Snprintf.h"
#include <algorithm>

template<typename T>
static char*
DataToString(const char* aFormat, T aData)
{
  static const int size = 32;
  char buf[size];

  int len = snprintf_literal(buf, aFormat, aData);
  MOZ_ASSERT(len >= 0);

  return static_cast<char*>(nsMemory::Clone(buf, std::min(len + 1, size) *
                                                 sizeof(char)));
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsIDImpl, nsISupportsID, nsISupportsPrimitive)

nsSupportsIDImpl::nsSupportsIDImpl()
  : mData(nullptr)
{
}

NS_IMETHODIMP
nsSupportsIDImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_ID;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsIDImpl::GetData(nsID** aData)
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
nsSupportsIDImpl::SetData(const nsID* aData)
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
nsSupportsIDImpl::ToString(char** aResult)
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
 * nsSupportsCStringImpl
 *****************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsCStringImpl, nsISupportsCString,
                  nsISupportsPrimitive)

NS_IMETHODIMP
nsSupportsCStringImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_CSTRING;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCStringImpl::GetData(nsACString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCStringImpl::ToString(char** aResult)
{
  *aResult = ToNewCString(mData);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCStringImpl::SetData(const nsACString& aData)
{
  bool ok = mData.Assign(aData, mozilla::fallible);
  if (!ok) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/*****************************************************************************
 * nsSupportsStringImpl
 *****************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsStringImpl, nsISupportsString,
                  nsISupportsPrimitive)

NS_IMETHODIMP
nsSupportsStringImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_STRING;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsStringImpl::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsStringImpl::ToString(char16_t** aResult)
{
  *aResult = ToNewUnicode(mData);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSupportsStringImpl::SetData(const nsAString& aData)
{
  bool ok = mData.Assign(aData, mozilla::fallible);
  if (!ok) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRBoolImpl, nsISupportsPRBool,
                  nsISupportsPrimitive)

nsSupportsPRBoolImpl::nsSupportsPRBoolImpl()
  : mData(false)
{
}

NS_IMETHODIMP
nsSupportsPRBoolImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRBOOL;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRBoolImpl::GetData(bool* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRBoolImpl::SetData(bool aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRBoolImpl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  const char* str = mData ? "true" : "false";
  *aResult = static_cast<char*>(nsMemory::Clone(str, (strlen(str) + 1) *
                                                     sizeof(char)));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint8Impl, nsISupportsPRUint8,
                  nsISupportsPrimitive)

nsSupportsPRUint8Impl::nsSupportsPRUint8Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint8Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT8;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint8Impl::GetData(uint8_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint8Impl::SetData(uint8_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint8Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%u", static_cast<unsigned int>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint16Impl, nsISupportsPRUint16,
                  nsISupportsPrimitive)

nsSupportsPRUint16Impl::nsSupportsPRUint16Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint16Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT16;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint16Impl::GetData(uint16_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint16Impl::SetData(uint16_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint16Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%u", static_cast<unsigned int>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint32Impl, nsISupportsPRUint32,
                  nsISupportsPrimitive)

nsSupportsPRUint32Impl::nsSupportsPRUint32Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint32Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT32;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint32Impl::GetData(uint32_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint32Impl::SetData(uint32_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint32Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%u", mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRUint64Impl, nsISupportsPRUint64,
                  nsISupportsPrimitive)

nsSupportsPRUint64Impl::nsSupportsPRUint64Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRUint64Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRUINT64;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint64Impl::GetData(uint64_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint64Impl::SetData(uint64_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRUint64Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%llu", mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRTimeImpl, nsISupportsPRTime,
                  nsISupportsPrimitive)

nsSupportsPRTimeImpl::nsSupportsPRTimeImpl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRTimeImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRTIME;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRTimeImpl::GetData(PRTime* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRTimeImpl::SetData(PRTime aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRTimeImpl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%" PRIu64, mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsCharImpl, nsISupportsChar,
                  nsISupportsPrimitive)

nsSupportsCharImpl::nsSupportsCharImpl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsCharImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_CHAR;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCharImpl::GetData(char* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCharImpl::SetData(char aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsCharImpl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = static_cast<char*>(moz_xmalloc(2 * sizeof(char)));
  *aResult[0] = mData;
  *aResult[1] = '\0';

  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRInt16Impl, nsISupportsPRInt16,
                  nsISupportsPrimitive)

nsSupportsPRInt16Impl::nsSupportsPRInt16Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRInt16Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRINT16;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt16Impl::GetData(int16_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt16Impl::SetData(int16_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt16Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%d", static_cast<int>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRInt32Impl, nsISupportsPRInt32,
                  nsISupportsPrimitive)

nsSupportsPRInt32Impl::nsSupportsPRInt32Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRInt32Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRINT32;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt32Impl::GetData(int32_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt32Impl::SetData(int32_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt32Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%d", mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsPRInt64Impl, nsISupportsPRInt64,
                  nsISupportsPrimitive)

nsSupportsPRInt64Impl::nsSupportsPRInt64Impl()
  : mData(0)
{
}

NS_IMETHODIMP
nsSupportsPRInt64Impl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_PRINT64;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt64Impl::GetData(int64_t* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt64Impl::SetData(int64_t aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsPRInt64Impl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%" PRId64, mData);
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsFloatImpl, nsISupportsFloat,
                  nsISupportsPrimitive)

nsSupportsFloatImpl::nsSupportsFloatImpl()
  : mData(float(0.0))
{
}

NS_IMETHODIMP
nsSupportsFloatImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_FLOAT;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsFloatImpl::GetData(float* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsFloatImpl::SetData(float aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsFloatImpl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%f", static_cast<double>(mData));
  return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsSupportsDoubleImpl, nsISupportsDouble,
                  nsISupportsPrimitive)

nsSupportsDoubleImpl::nsSupportsDoubleImpl()
  : mData(double(0.0))
{
}

NS_IMETHODIMP
nsSupportsDoubleImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_DOUBLE;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDoubleImpl::GetData(double* aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDoubleImpl::SetData(double aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsDoubleImpl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  *aResult = DataToString("%f", mData);
  return  NS_OK;
}

/***************************************************************************/


NS_IMPL_ISUPPORTS(nsSupportsVoidImpl, nsISupportsVoid,
                  nsISupportsPrimitive)

nsSupportsVoidImpl::nsSupportsVoidImpl()
  : mData(nullptr)
{
}

NS_IMETHODIMP
nsSupportsVoidImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_VOID;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsVoidImpl::GetData(void** aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsVoidImpl::SetData(void* aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsVoidImpl::ToString(char** aResult)
{
  NS_ASSERTION(aResult, "Bad pointer");
  static const char str[] = "[raw data]";
  *aResult = static_cast<char*>(nsMemory::Clone(str, sizeof(str)));
  return NS_OK;
}

/***************************************************************************/


NS_IMPL_ISUPPORTS(nsSupportsInterfacePointerImpl,
                  nsISupportsInterfacePointer,
                  nsISupportsPrimitive)

nsSupportsInterfacePointerImpl::nsSupportsInterfacePointerImpl()
  : mIID(nullptr)
{
}

nsSupportsInterfacePointerImpl::~nsSupportsInterfacePointerImpl()
{
  if (mIID) {
    free(mIID);
  }
}

NS_IMETHODIMP
nsSupportsInterfacePointerImpl::GetType(uint16_t* aType)
{
  NS_ASSERTION(aType, "Bad pointer");
  *aType = TYPE_INTERFACE_POINTER;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointerImpl::GetData(nsISupports** aData)
{
  NS_ASSERTION(aData, "Bad pointer");
  *aData = mData;
  NS_IF_ADDREF(*aData);
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointerImpl::SetData(nsISupports* aData)
{
  mData = aData;
  return NS_OK;
}

NS_IMETHODIMP
nsSupportsInterfacePointerImpl::GetDataIID(nsID** aIID)
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
nsSupportsInterfacePointerImpl::SetDataIID(const nsID* aIID)
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
nsSupportsInterfacePointerImpl::ToString(char** aResult)
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
