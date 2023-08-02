/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDataHandler_h___
#define nsDataHandler_h___

#include "nsIProtocolHandler.h"
#include "nsWeakReference.h"

class nsDataHandler : public nsIProtocolHandler,
                      public nsSupportsWeakReference {
  virtual ~nsDataHandler() = default;

 public:
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods:
  NS_DECL_NSIPROTOCOLHANDLER

  // nsDataHandler methods:
  nsDataHandler() = default;

  static nsresult CreateNewURI(const nsACString& aSpec, const char* aCharset,
                               nsIURI* aBaseURI, nsIURI** result);

  // Define a Create method to be used with a factory:
  [[nodiscard]] static nsresult Create(const nsIID& aIID, void** aResult);

  // Parse a data: URI and return the individual parts
  // (the given spec will temporarily be modified but will be returned
  //  to the original before returning)
  // contentCharset and dataBuffer can be nullptr if they are not needed.
  [[nodiscard]] static nsresult ParseURI(const nsACString& spec,
                                         nsCString& contentType,
                                         nsCString* contentCharset,
                                         bool& isBase64, nsCString* dataBuffer);

  // Parse the path portion of a data: URI and return the individual parts.
  //
  // Note: The path is assumed *not* to have a ref portion.
  //
  // @arg aPath The path portion of the spec. Must not have ref portion.
  // @arg aContentType Out param, will hold the parsed content type.
  // @arg aContentCharset Optional, will hold the charset if specified.
  // @arg aIsBase64 Out param, indicates if the data is base64 encoded.
  // @arg aDataBuffer Optional, will reference the substring in |aPath| that
  //  contains the data portion of the path. No copy is made.
  [[nodiscard]] static nsresult ParsePathWithoutRef(
      const nsACString& aPath, nsCString& aContentType,
      nsCString* aContentCharset, bool& aIsBase64,
      nsDependentCSubstring* aDataBuffer, nsCString* aMimeType = nullptr);
};

#endif /* nsDataHandler_h___ */
