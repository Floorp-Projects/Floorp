/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptableBase64Encoder.h"
#include "mozilla/Base64.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsScriptableBase64Encoder, nsIScriptableBase64Encoder)

NS_IMETHODIMP
nsScriptableBase64Encoder::EncodeToCString(nsIInputStream* aStream,
                                           uint32_t aLength,
                                           nsACString& aResult)
{
  return Base64EncodeInputStream(aStream, aResult, aLength);
}

NS_IMETHODIMP
nsScriptableBase64Encoder::EncodeToString(nsIInputStream* aStream,
                                          uint32_t aLength,
                                          nsAString& aResult)
{
  return Base64EncodeInputStream(aStream, aResult, aLength);
}
