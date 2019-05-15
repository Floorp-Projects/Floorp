/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProperties.h"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsProperties, nsIProperties)

NS_IMETHODIMP
nsProperties::Get(const char* prop, const nsIID& uuid, void** result) {
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> value;
  if (!nsProperties_HashBase::Get(prop, getter_AddRefs(value))) {
    return NS_ERROR_FAILURE;
  }
  return (value) ? value->QueryInterface(uuid, result) : NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsProperties::Set(const char* prop, nsISupports* value) {
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }
  Put(prop, value);
  return NS_OK;
}

NS_IMETHODIMP
nsProperties::Undefine(const char* prop) {
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }

  return nsProperties_HashBase::Remove(prop) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsProperties::Has(const char* prop, bool* result) {
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }

  *result = nsProperties_HashBase::Contains(prop);
  return NS_OK;
}

NS_IMETHODIMP
nsProperties::GetKeys(nsTArray<nsCString>& aKeys) {
  uint32_t count = Count();
  aKeys.SetCapacity(count);

  for (auto iter = this->Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
