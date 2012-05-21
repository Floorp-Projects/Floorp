/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptableBase64Encoder.h"
#include "mozilla/Base64.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsScriptableBase64Encoder, nsIScriptableBase64Encoder)

/* ACString encodeToCString (in nsIInputStream stream, in unsigned long length); */
NS_IMETHODIMP
nsScriptableBase64Encoder::EncodeToCString(nsIInputStream *aStream,
                                           PRUint32 aLength,
                                           nsACString & _retval)
{
  Base64EncodeInputStream(aStream, _retval, aLength);
  return NS_OK;
}

/* AString encodeToString (in nsIInputStream stream, in unsigned long length); */
NS_IMETHODIMP
nsScriptableBase64Encoder::EncodeToString(nsIInputStream *aStream,
                                          PRUint32 aLength,
                                          nsAString & _retval)
{
  Base64EncodeInputStream(aStream, _retval, aLength);
  return NS_OK;
}
