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
Variant_base::GetAsBool(PRBool *)
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

} // namespace storage
} // namespace mozilla
