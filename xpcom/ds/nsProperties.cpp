/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProperties.h"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(nsProperties)
NS_INTERFACE_MAP_BEGIN_AGGREGATED(nsProperties)
  NS_INTERFACE_MAP_ENTRY(nsIProperties)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsProperties::Get(const char* prop, const nsIID& uuid, void** result)
{
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
nsProperties::Set(const char* prop, nsISupports* value)
{
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }
  Put(prop, value);
  return NS_OK;
}

NS_IMETHODIMP
nsProperties::Undefine(const char* prop)
{
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> value;
  if (!nsProperties_HashBase::Get(prop, getter_AddRefs(value))) {
    return NS_ERROR_FAILURE;
  }

  Remove(prop);
  return NS_OK;
}

NS_IMETHODIMP
nsProperties::Has(const char* prop, bool* result)
{
  if (NS_WARN_IF(!prop)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> value;
  *result = nsProperties_HashBase::Get(prop, getter_AddRefs(value));
  return NS_OK;
}

NS_IMETHODIMP
nsProperties::GetKeys(uint32_t* aCount, char*** aKeys)
{
  if (NS_WARN_IF(!aCount) || NS_WARN_IF(!aKeys)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t count = Count();
  char** keys = (char**)moz_xmalloc(count * sizeof(char*));
  uint32_t j = 0;

  for (auto iter = this->Iter(); !iter.Done(); iter.Next()) {
    const char* key = iter.GetKey();
    keys[j] = strdup(key);

    if (!keys[j]) {
      // Free 'em all
      for (uint32_t i = 0; i < j; i++) {
        free(keys[i]);
      }
      free(keys);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    j++;
  }

  *aCount = count;
  *aKeys = keys;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
