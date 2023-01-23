/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_BinaryHttpRequest_h
#define mozilla_net_BinaryHttpRequest_h

#include "nsIBinaryHttp.h"
#include "nsString.h"

namespace mozilla::net {

class BinaryHttpRequest final : public nsIBinaryHttpRequest {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIBINARYHTTPREQUEST

  BinaryHttpRequest(const nsACString& aMethod, const nsACString& aScheme,
                    const nsACString& aAuthority, const nsACString& aPath,
                    nsTArray<nsCString>&& aHeaderNames,
                    nsTArray<nsCString>&& aHeaderValues,
                    nsTArray<uint8_t>&& aContent)
      : mMethod(aMethod),
        mScheme(aScheme),
        mAuthority(aAuthority),
        mPath(aPath),
        mHeaderNames(std::move(aHeaderNames)),
        mHeaderValues(std::move(aHeaderValues)),
        mContent(std::move(aContent)) {}

 private:
  ~BinaryHttpRequest() = default;

  const nsAutoCString mMethod;
  const nsAutoCString mScheme;
  const nsAutoCString mAuthority;
  const nsAutoCString mPath;
  const nsTArray<nsCString> mHeaderNames;
  const nsTArray<nsCString> mHeaderValues;
  const nsTArray<uint8_t> mContent;
};

}  // namespace mozilla::net

#endif  // mozilla_net_BinaryHttpRequest_h
