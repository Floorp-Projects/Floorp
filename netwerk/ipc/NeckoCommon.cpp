/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NeckoCommon.h"

#include "nsIInputStream.h"
#include "nsIMultiPartChannel.h"
#include "nsIParentChannel.h"
#include "nsStringStream.h"

namespace mozilla::net {

nsresult ForwardStreamListenerFunctions(nsTArray<StreamListenerFunction> aCalls,
                                        nsIStreamListener* aParent) {
  nsresult rv = NS_OK;
  for (auto& variant : aCalls) {
    variant.match(
        [&](OnStartRequestParams& aParams) {
          rv = aParent->OnStartRequest(aParams.request);
          if (NS_FAILED(rv)) {
            aParams.request->Cancel(rv);
          }
        },
        [&](OnDataAvailableParams& aParams) {
          // Don't deliver OnDataAvailable if we've
          // already failed.
          if (NS_FAILED(rv)) {
            return;
          }
          nsCOMPtr<nsIInputStream> stringStream;
          rv = NS_NewCStringInputStream(getter_AddRefs(stringStream),
                                        std::move(aParams.data));
          if (NS_SUCCEEDED(rv)) {
            rv = aParent->OnDataAvailable(aParams.request, stringStream,
                                          aParams.offset, aParams.count);
          }
          if (NS_FAILED(rv)) {
            aParams.request->Cancel(rv);
          }
        },
        [&](OnStopRequestParams& aParams) {
          if (NS_SUCCEEDED(rv)) {
            aParent->OnStopRequest(aParams.request, aParams.status);
          } else {
            aParent->OnStopRequest(aParams.request, rv);
          }
          rv = NS_OK;
        },
        [&](OnAfterLastPartParams& aParams) {
          nsCOMPtr<nsIMultiPartChannelListener> multiListener =
              do_QueryInterface(aParent);
          if (multiListener) {
            if (NS_SUCCEEDED(rv)) {
              multiListener->OnAfterLastPart(aParams.status);
            } else {
              multiListener->OnAfterLastPart(rv);
            }
          }
        });
  }
  return rv;
}

}  // namespace mozilla::net
