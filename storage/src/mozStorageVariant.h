/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#ifndef __mozStorageVariant_h__
#define __mozStorageVariant_h__

#include "nsIVariant.h"
#include "nsTArray.h"
#include <utility>

/**
 * This class is used by the storage module whenever an nsIVariant needs to be
 * returned.  We provide traits for the basic sqlite types to make use easier.
 * The following types map to the indicated sqlite type:
 * PRInt64   -> INTEGER (use mozStorageInteger)
 * double    -> FLOAT (use mozStorageFloat)
 * nsString  -> TEXT (use mozStorageText)
 * PRUint8[] -> BLOB (use mozStorageBlob)
 * nsnull    -> NULL (use mozStorageNull)
 */

#define NO_CONVERSION return NS_ERROR_CANNOT_CONVERT_DATA;

////////////////////////////////////////////////////////////////////////////////
//// Base Class

class mozStorageVariant_base : public nsIVariant
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetDataType(PRUint16 *_type)
  {
    *_type = nsIDataType::VTYPE_EMPTY;
    return NS_OK;
  }

  NS_IMETHOD GetAsInt32(PRInt32 *_integer) { NO_CONVERSION }
  NS_IMETHOD GetAsInt64(PRInt64 *) { NO_CONVERSION }
  NS_IMETHOD GetAsDouble(double *) { NO_CONVERSION }
  NS_IMETHOD GetAsAUTF8String(nsACString &) { NO_CONVERSION }
  NS_IMETHOD GetAsAString(nsAString &) { NO_CONVERSION }
  NS_IMETHOD GetAsArray(PRUint16 *, nsIID *, PRUint32 *, void **) { NO_CONVERSION }
  NS_IMETHOD GetAsInt8(PRUint8 *) { NO_CONVERSION }
  NS_IMETHOD GetAsInt16(PRInt16 *) { NO_CONVERSION }
  NS_IMETHOD GetAsUint8(PRUint8 *) { NO_CONVERSION }
  NS_IMETHOD GetAsUint16(PRUint16 *) { NO_CONVERSION }
  NS_IMETHOD GetAsUint32(PRUint32 *) { NO_CONVERSION }
  NS_IMETHOD GetAsUint64(PRUint64 *) { NO_CONVERSION }
  NS_IMETHOD GetAsFloat(float *) { NO_CONVERSION }
  NS_IMETHOD GetAsBool(PRBool *) { NO_CONVERSION }
  NS_IMETHOD GetAsChar(char *) { NO_CONVERSION }
  NS_IMETHOD GetAsWChar(PRUnichar *) { NO_CONVERSION }
  NS_IMETHOD GetAsID(nsID *) { NO_CONVERSION }
  NS_IMETHOD GetAsDOMString(nsAString &) { NO_CONVERSION }
  NS_IMETHOD GetAsString(char **) { NO_CONVERSION }
  NS_IMETHOD GetAsWString(PRUnichar **) { NO_CONVERSION }
  NS_IMETHOD GetAsISupports(nsISupports **) { NO_CONVERSION }
  NS_IMETHOD GetAsInterface(nsIID **, void **) { NO_CONVERSION }
  NS_IMETHOD GetAsACString(nsACString &) { NO_CONVERSION }
  NS_IMETHOD GetAsStringWithSize(PRUint32 *, char **) { NO_CONVERSION }
  NS_IMETHOD GetAsWStringWithSize(PRUint32 *, PRUnichar **) { NO_CONVERSION }

protected:
  virtual ~mozStorageVariant_base() { }
};
NS_IMPL_THREADSAFE_ISUPPORTS1(
  mozStorageVariant_base,
  nsIVariant
)

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
  static inline nsresult asInt32(PRInt64 aValue, PRInt32 *_result)
  {
    if (aValue > PR_INT32_MAX || aValue < PR_INT32_MIN)
      return NS_ERROR_CANNOT_CONVERT_DATA;

    *_result = aValue;
    return NS_OK;
  }
  static inline nsresult asInt64(PRInt64 aValue, PRInt64 *_result)
  {
    *_result = aValue;
    return NS_OK;
  }
};
// xpcvariant just calls get double for integers...
template < >
struct variant_float_traits<PRInt64>
{
  static inline nsresult asDouble(PRInt64 aValue, double *_result)
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
  static inline nsresult asDouble(double aValue, double *_result)
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
  typedef nsTArray<PRUint8> StorageType;
  static inline StorageType storage_conversion(ConstructorType aBlob)
  {
    nsTArray<PRUint8> data(aBlob.second);
    (void)data.AppendElements(static_cast<const PRUint8 *>(aBlob.first),
                              aBlob.second);
    return data;
  }
};
template < >
struct variant_blob_traits<PRUint8[]>
{
  static inline nsresult asArray(nsTArray<PRUint8> &aData,
                                 PRUint16 *_type, PRUint32 *_size,
                                 void **_result)
  {
    // Copy the array
    *_result = nsMemory::Clone(aData.Elements(), aData.Length() * sizeof(PRUint8));
    NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);

    // Set type and size
    *_type = nsIDataType::VTYPE_UINT8;
    *_size = aData.Length();
    return NS_OK;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Template Implementation

template <typename DataType>
class mozStorageVariant : public mozStorageVariant_base
{
public:
  mozStorageVariant(typename variant_storage_traits<DataType>::ConstructorType aData) :
      mData(variant_storage_traits<DataType>::storage_conversion(aData))
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

  NS_IMETHOD GetAsArray(PRUint16 *_type, nsIID *, PRUint32 *_size, void **_data)
  {
    return variant_blob_traits<DataType>::asArray(mData, _type, _size, _data);
  }

private:
  mozStorageVariant() { }
  typename variant_storage_traits<DataType>::StorageType mData;
};


////////////////////////////////////////////////////////////////////////////////
//// Handy typedefs!  Use these for the right mapping.

typedef mozStorageVariant<PRInt64> mozStorageInteger;
typedef mozStorageVariant<double> mozStorageFloat;
typedef mozStorageVariant<nsString> mozStorageText;
typedef mozStorageVariant<PRUint8[]> mozStorageBlob;
typedef mozStorageVariant_base mozStorageNull;

#endif // __mozStorageVariant_h__
