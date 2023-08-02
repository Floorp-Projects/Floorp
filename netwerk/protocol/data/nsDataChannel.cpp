/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// data implementation

#include "nsDataChannel.h"

#include "mozilla/Base64.h"
#include "nsDataHandler.h"
#include "nsIInputStream.h"
#include "nsEscape.h"
#include "nsStringStream.h"

using namespace mozilla;

/**
 * Helper for performing a fallible unescape.
 *
 * @param aStr The string to unescape.
 * @param aBuffer Buffer to unescape into if necessary.
 * @param rv Out: nsresult indicating success or failure of unescaping.
 * @return Reference to the string containing the unescaped data.
 */
const nsACString& Unescape(const nsACString& aStr, nsACString& aBuffer,
                           nsresult* rv) {
  MOZ_ASSERT(rv);

  bool appended = false;
  *rv = NS_UnescapeURL(aStr.Data(), aStr.Length(), /* aFlags = */ 0, aBuffer,
                       appended, mozilla::fallible);
  if (NS_FAILED(*rv) || !appended) {
    return aStr;
  }

  return aBuffer;
}

nsresult nsDataChannel::OpenContentStream(bool async, nsIInputStream** result,
                                          nsIChannel** channel) {
  NS_ENSURE_TRUE(URI(), NS_ERROR_NOT_INITIALIZED);

  nsresult rv;

  // In order to avoid potentially building up a new path including the
  // ref portion of the URI, which we don't care about, we clone a version
  // of the URI that does not have a ref and in most cases should share
  // string buffers with the original URI.
  nsCOMPtr<nsIURI> uri;
  rv = NS_GetURIWithoutRef(URI(), getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  nsAutoCString path;
  rv = uri->GetPathQueryRef(path);
  if (NS_FAILED(rv)) return rv;

  nsCString contentType, contentCharset;
  nsDependentCSubstring dataRange;
  bool lBase64;
  rv = nsDataHandler::ParsePathWithoutRef(path, contentType, &contentCharset,
                                          lBase64, &dataRange, &mMimeType);
  if (NS_FAILED(rv)) return rv;

  // This will avoid a copy if nothing needs to be unescaped.
  nsAutoCString unescapedBuffer;
  const nsACString& data = Unescape(dataRange, unescapedBuffer, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (lBase64 && &data == &unescapedBuffer) {
    // Don't allow spaces in base64-encoded content. This is only
    // relevant for escaped spaces; other spaces are stripped in
    // NewURI. We know there were no escaped spaces if the data buffer
    // wasn't used in |Unescape|.
    unescapedBuffer.StripWhitespace();
  }

  nsCOMPtr<nsIInputStream> bufInStream;
  uint32_t contentLen;
  if (lBase64) {
    nsAutoCString decodedData;
    rv = Base64Decode(data, decodedData);
    if (NS_FAILED(rv)) {
      // Returning this error code instead of what Base64Decode returns
      // (NS_ERROR_ILLEGAL_VALUE) will prevent rendering of redirect response
      // content by HTTP channels.  It's also more logical error to return.
      // Here we know the URL is actually corrupted.
      return NS_ERROR_MALFORMED_URI;
    }

    contentLen = decodedData.Length();
    rv = NS_NewCStringInputStream(getter_AddRefs(bufInStream), decodedData);
  } else {
    contentLen = data.Length();
    rv = NS_NewCStringInputStream(getter_AddRefs(bufInStream), data);
  }

  if (NS_FAILED(rv)) return rv;

  SetContentType(contentType);
  SetContentCharset(contentCharset);
  mContentLength = contentLen;

  bufInStream.forget(result);

  return NS_OK;
}
