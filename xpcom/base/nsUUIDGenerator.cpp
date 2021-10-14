/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUUIDGenerator.h"

NS_IMPL_ISUPPORTS(nsUUIDGenerator, nsIUUIDGenerator)

nsUUIDGenerator::~nsUUIDGenerator() = default;

NS_IMETHODIMP
nsUUIDGenerator::GenerateUUID(nsID** aRet) {
  nsID* id = static_cast<nsID*>(moz_xmalloc(sizeof(nsID)));

  nsresult rv = GenerateUUIDInPlace(id);
  if (NS_FAILED(rv)) {
    free(id);
    return rv;
  }

  *aRet = id;
  return rv;
}

NS_IMETHODIMP
nsUUIDGenerator::GenerateUUIDInPlace(nsID* aId) {
  return nsID::GenerateUUIDInPlace(*aId);
}
