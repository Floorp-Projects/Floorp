/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Note: This file is included by Variant.h.
 */

#ifndef mozilla_storage_Variant_h__
#error "Do not include this file directly!"
#endif

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Variant_base

inline NS_IMPL_THREADSAFE_ADDREF(Variant_base)
inline NS_IMPL_THREADSAFE_RELEASE(Variant_base)
inline NS_IMPL_THREADSAFE_QUERY_INTERFACE1(
  Variant_base,
  nsIVariant
)

////////////////////////////////////////////////////////////////////////////////
//// nsIVariant

inline
NS_IMETHODIMP
Variant_base::GetDataType(PRUint16 *_type)
{
  NS_ENSURE_ARG_POINTER(_type);
  *_type = nsIDataType::VTYPE_VOID;
  return NS_OK;
}

inline
NS_IMETHODIMP
Variant_base::GetAsInt32(PRInt32 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsInt64(PRInt64 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsDouble(double *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsAUTF8String(nsACString &)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsAString(nsAString &)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsArray(PRUint16 *,
                         nsIID *,
                         PRUint32 *,
                         void **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsInt8(PRUint8 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsInt16(PRInt16 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsUint8(PRUint8 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsUint16(PRUint16 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsUint32(PRUint32 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsUint64(PRUint64 *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsFloat(float *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsBool(bool *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsChar(char *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsWChar(PRUnichar *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsID(nsID *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsDOMString(nsAString &)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsString(char **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsWString(PRUnichar **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsISupports(nsISupports **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsInterface(nsIID **,
                             void **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsACString(nsACString &)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsStringWithSize(PRUint32 *,
                                  char **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsWStringWithSize(PRUint32 *,
                                   PRUnichar **)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

inline
NS_IMETHODIMP
Variant_base::GetAsJSVal(JS::Value *)
{
  return NS_ERROR_CANNOT_CONVERT_DATA;
}

} // namespace storage
} // namespace mozilla
