/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_Variant_h__
#define mozilla_storage_Variant_h__

#include <utility>

#include "nsIVariant.h"
#include "nsString.h"
#include "nsTArray.h"

/**
 * This class is used by the storage module whenever an nsIVariant needs to be
 * returned.  We provide traits for the basic sqlite types to make use easier.
 * The following types map to the indicated sqlite type:
 * PRInt64   -> INTEGER (use IntegerVariant)
 * double    -> FLOAT (use FloatVariant)
 * nsString  -> TEXT (use TextVariant)
 * nsCString -> TEXT (use UTF8TextVariant)
 * PRUint8[] -> BLOB (use BlobVariant)
 * nullptr    -> NULL (use NullVariant)
 */

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Base Class

class Variant_base : public nsIVariant
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVARIANT

protected:
  virtual ~Variant_base() { }
};

////////////////////////////////////////////////////////////////////////////////
//// Traits

/**
 * Generics
 */

template <typename DataType>
struct variant_traits
{
  static inline PRUint16 type() { return nsIDataType::VTYPE_EMPTY; }
};

template <typename DataType>
struct variant_storage_traits
{
  typedef DataType ConstructorType;
  typedef DataType StorageType;
  static inline StorageType storage_conversion(ConstructorType aData) { return aData; }
};

#define NO_CONVERSION return NS_ERROR_CANNOT_CONVERT_DATA;

template <typename DataType>
struct variant_integer_traits
{
  typedef typename variant_storage_traits<DataType>::StorageType StorageType;
  static inline nsresult asInt32(StorageType, PRInt32 *) { NO_CONVERSION }
  static inline nsresult asInt64(StorageType, PRInt64 *) { NO_CONVERSION }
};

template <typename DataType>
struct variant_float_traits
{
  typedef typename variant_storage_traits<DataType>::StorageType StorageType;
  static inline nsresult asDouble(StorageType, double *) { NO_CONVERSION }
};

template <typename DataType>
struct variant_text_traits
{
  typedef typename variant_storage_traits<DataType>::StorageType StorageType;
  static inline nsresult asUTF8String(StorageType, nsACString &) { NO_CONVERSION }
  static inline nsresult asString(StorageType, nsAString &) { NO_CONVERSION }
};

template <typename DataType>
struct variant_blob_traits
{
  typedef typename variant_storage_traits<DataType>::StorageType StorageType;
  static inline nsresult asArray(StorageType, PRUint16 *, PRUint32 *, void **)
  { NO_CONVERSION }
};

#undef NO_CONVERSION

/**
 * INTEGER types
 */

template < >
struct variant_traits<PRInt64>
{
  static inline PRUint16 type() { return nsIDataType::VTYPE_INT64; }
};
template < >
struct variant_integer_traits<PRInt64>
{
  static inline nsresult asInt32(PRInt64 aValue,
                                 PRInt32 *_result)
  {
    if (aValue > PR_INT32_MAX || aValue < PR_INT32_MIN)
      return NS_ERROR_CANNOT_CONVERT_DATA;

    *_result = static_cast<PRInt32>(aValue);
    return NS_OK;
  }
  static inline nsresult asInt64(PRInt64 aValue,
                                 PRInt64 *_result)
  {
    *_result = aValue;
    return NS_OK;
  }
};
// xpcvariant just calls get double for integers...
template < >
struct variant_float_traits<PRInt64>
{
  static inline nsresult asDouble(PRInt64 aValue,
                                  double *_result)
  {
    *_result = double(aValue);
    return NS_OK;
  }
};

/**
 * FLOAT types
 */

template < >
struct variant_traits<double>
{
  static inline PRUint16 type() { return nsIDataType::VTYPE_DOUBLE; }
};
template < >
struct variant_float_traits<double>
{
  static inline nsresult asDouble(double aValue,
                                  double *_result)
  {
    *_result = aValue;
    return NS_OK;
  }
};

/**
 * TEXT types
 */

template < >
struct variant_traits<nsString>
{
  static inline PRUint16 type() { return nsIDataType::VTYPE_ASTRING; }
};
template < >
struct variant_storage_traits<nsString>
{
  typedef const nsAString & ConstructorType;
  typedef nsString StorageType;
  static inline StorageType storage_conversion(ConstructorType aText)
  {
    return StorageType(aText);
  }
};
template < >
struct variant_text_traits<nsString>
{
  static inline nsresult asUTF8String(const nsString &aValue,
                                      nsACString &_result)
  {
    CopyUTF16toUTF8(aValue, _result);
    return NS_OK;
  }
  static inline nsresult asString(const nsString &aValue,
                                  nsAString &_result)
  {
    _result = aValue;
    return NS_OK;
  }
};

template < >
struct variant_traits<nsCString>
{
  static inline PRUint16 type() { return nsIDataType::VTYPE_UTF8STRING; }
};
template < >
struct variant_storage_traits<nsCString>
{
  typedef const nsACString & ConstructorType;
  typedef nsCString StorageType;
  static inline StorageType storage_conversion(ConstructorType aText)
  {
    return StorageType(aText);
  }
};
template < >
struct variant_text_traits<nsCString>
{
  static inline nsresult asUTF8String(const nsCString &aValue,
                                      nsACString &_result)
  {
    _result = aValue;
    return NS_OK;
  }
  static inline nsresult asString(const nsCString &aValue,
                                  nsAString &_result)
  {
    CopyUTF8toUTF16(aValue, _result);
    return NS_OK;
  }
};

/**
 * BLOB types
 */

template < >
struct variant_traits<PRUint8[]>
{
  static inline PRUint16 type() { return nsIDataType::VTYPE_ARRAY; }
};
template < >
struct variant_storage_traits<PRUint8[]>
{
  typedef std::pair<const void *, int> ConstructorType;
  typedef FallibleTArray<PRUint8> StorageType;
  static inline StorageType storage_conversion(ConstructorType aBlob)
  {
    StorageType data(aBlob.second);
    (void)data.AppendElements(static_cast<const PRUint8 *>(aBlob.first),
                              aBlob.second);
    return data;
  }
};
template < >
struct variant_blob_traits<PRUint8[]>
{
  static inline nsresult asArray(FallibleTArray<PRUint8> &aData,
                                 PRUint16 *_type,
                                 PRUint32 *_size,
                                 void **_result)
  {
    // For empty blobs, we return nullptr.
    if (aData.Length() == 0) {
      *_result = nullptr;
      *_type = nsIDataType::VTYPE_UINT8;
      *_size = 0;
      return NS_OK;
    }

    // Otherwise, we copy the array.
    *_result = nsMemory::Clone(aData.Elements(), aData.Length() * sizeof(PRUint8));
    NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);

    // Set type and size
    *_type = nsIDataType::VTYPE_UINT8;
    *_size = aData.Length();
    return NS_OK;
  }
};

/**
 * NULL type
 */

class NullVariant : public Variant_base
{
public:
  NS_IMETHOD GetDataType(PRUint16 *_type)
  {
    NS_ENSURE_ARG_POINTER(_type);
    *_type = nsIDataType::VTYPE_EMPTY;
    return NS_OK;
  }

  NS_IMETHOD GetAsAUTF8String(nsACString &_str)
  {
    // Return a void string.
    _str.Truncate(0);
    _str.SetIsVoid(true);
    return NS_OK;
  }

  NS_IMETHOD GetAsAString(nsAString &_str)
  {
    // Return a void string.
    _str.Truncate(0);
    _str.SetIsVoid(true);
    return NS_OK;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Template Implementation

template <typename DataType>
class Variant : public Variant_base
{
public:
  Variant(typename variant_storage_traits<DataType>::ConstructorType aData)
    : mData(variant_storage_traits<DataType>::storage_conversion(aData))
  {
  }

  NS_IMETHOD GetDataType(PRUint16 *_type)
  {
    *_type = variant_traits<DataType>::type();
    return NS_OK;
  }
  NS_IMETHOD GetAsInt32(PRInt32 *_integer)
  {
    return variant_integer_traits<DataType>::asInt32(mData, _integer);
  }

  NS_IMETHOD GetAsInt64(PRInt64 *_integer)
  {
    return variant_integer_traits<DataType>::asInt64(mData, _integer);
  }

  NS_IMETHOD GetAsDouble(double *_double)
  {
    return variant_float_traits<DataType>::asDouble(mData, _double);
  }

  NS_IMETHOD GetAsAUTF8String(nsACString &_str)
  {
    return variant_text_traits<DataType>::asUTF8String(mData, _str);
  }

  NS_IMETHOD GetAsAString(nsAString &_str)
  {
    return variant_text_traits<DataType>::asString(mData, _str);
  }

  NS_IMETHOD GetAsArray(PRUint16 *_type,
                        nsIID *,
                        PRUint32 *_size,
                        void **_data)
  {
    return variant_blob_traits<DataType>::asArray(mData, _type, _size, _data);
  }

private:
  typename variant_storage_traits<DataType>::StorageType mData;
};

////////////////////////////////////////////////////////////////////////////////
//// Handy typedefs!  Use these for the right mapping.

typedef Variant<PRInt64> IntegerVariant;
typedef Variant<double> FloatVariant;
typedef Variant<nsString> TextVariant;
typedef Variant<nsCString> UTF8TextVariant;
typedef Variant<PRUint8[]> BlobVariant;

} // namespace storage
} // namespace mozilla

#include "Variant_inl.h"

#endif // mozilla_storage_Variant_h__
