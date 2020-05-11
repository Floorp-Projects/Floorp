/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FetchPreloader_h_
#define FetchPreloader_h_

#include "mozilla/PreloaderBase.h"
#include "nsCOMPtr.h"
#include "nsIAsyncOutputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIStreamListener.h"

class nsIChannel;
class nsILoadGroup;
class nsIInterfaceRequestor;

namespace mozilla {

class FetchPreloader : public PreloaderBase, public nsIStreamListener {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  FetchPreloader();
  nsresult OpenChannel(PreloadHashKey* aKey, nsIURI* aURI,
                       const CORSMode aCORSMode,
                       const dom::ReferrerPolicy& aReferrerPolicy,
                       dom::Document* aDocument);

  // PreloaderBase
  nsresult AsyncConsume(nsIStreamListener* aListener) override;
  static void PrioritizeAsPreload(nsIChannel* aChannel);
  void PrioritizeAsPreload() override;

 protected:
  explicit FetchPreloader(nsContentPolicyType aContentPolicyType);
  virtual ~FetchPreloader() = default;

  // Create and setup the channel with necessary security properties.  This is
  // overridable by subclasses to allow different initial conditions.
  virtual nsresult CreateChannel(nsIChannel** aChannel, nsIURI* aURI,
                                 const CORSMode aCORSMode,
                                 const dom::ReferrerPolicy& aReferrerPolicy,
                                 dom::Document* aDocument,
                                 nsILoadGroup* aLoadGroup,
                                 nsIInterfaceRequestor* aCallbacks);

 private:
  nsresult CheckContentPolicy(nsIURI* aURI, dom::Document* aDocument);

  class Cache final : public nsIStreamListener {
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    void AsyncConsume(nsIStreamListener* aListener);
    void Consume(nsCOMPtr<nsIStreamListener> aListener);

   private:
    virtual ~Cache() = default;

    struct StartRequest {};
    struct DataAvailable {
      nsCString mData;
    };
    struct StopRequest {
      nsresult mStatus;
    };

    typedef Variant<StartRequest, DataAvailable, StopRequest> Call;
    nsCOMPtr<nsIRequest> mRequest;
    nsCOMPtr<nsIStreamListener> mFinalListener;
    nsTArray<Call> mCalls;
  };

  // The listener passed to AsyncConsume in case we haven't started getting the
  // data from the channel yet.
  nsCOMPtr<nsIStreamListener> mConsumeListener;

  // Returned by AsyncConsume when a failure.  This remembers the result of
  // opening the channel and prevents duplicate consumation.
  nsresult mAsyncConsumeResult = NS_ERROR_NOT_AVAILABLE;

  // The CP type to check against.  Derived classes have to override call to CSP
  // constructor of this class.
  nsContentPolicyType mContentPolicyType;
};

}  // namespace mozilla

#endif
