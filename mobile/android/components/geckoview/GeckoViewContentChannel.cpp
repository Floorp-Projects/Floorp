/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewContentChannel.h"
#include "GeckoViewInputStream.h"
#include "mozilla/java/ContentInputStreamWrappers.h"

using namespace mozilla;

GeckoViewContentChannel::GeckoViewContentChannel(nsIURI* aURI) {
  SetURI(aURI);
  SetOriginalURI(aURI);
}

NS_IMETHODIMP
GeckoViewContentChannel::OpenContentStream(bool aAsync,
                                           nsIInputStream** aResult,
                                           nsIChannel** aChannel) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_MALFORMED_URI);

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isReadable = GeckoViewContentInputStream::isReadable(spec);
  if (!isReadable) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  rv = GeckoViewContentInputStream::getInstance(spec,
                                                getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_WARN_IF(!inputStream)) {
    return NS_ERROR_MALFORMED_URI;
  }

  inputStream.forget(aResult);

  return NS_OK;
}
