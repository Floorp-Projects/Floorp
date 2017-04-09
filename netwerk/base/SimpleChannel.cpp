/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseChannel.h"
#include "nsIInputStream.h"
#include "nsIRequest.h"
#include "SimpleChannel.h"

namespace mozilla {
namespace net {

// Like MOZ_TRY, but returns the unwrapped error value rather than a
// GenericErrorResult on failure.
#define TRY_VAR(target, expr) \
  do { \
    auto result = (expr); \
    if (result.isErr()) { \
      return result.unwrapErr(); \
    } \
    (target) = result.unwrap(); \
  } while (0)

class SimpleChannel final : public nsBaseChannel
{
public:
  explicit SimpleChannel(UniquePtr<SimpleChannelCallbacks>&& aCallbacks);

protected:
  virtual ~SimpleChannel() {}

  virtual nsresult OpenContentStream(bool async, nsIInputStream **streamOut,
                                     nsIChannel** channel) override;

  virtual nsresult BeginAsyncRead(nsIStreamListener* listener,
                                  nsIRequest** request) override;

private:
  UniquePtr<SimpleChannelCallbacks> mCallbacks;
};

SimpleChannel::SimpleChannel(UniquePtr<SimpleChannelCallbacks>&& aCallbacks)
  : mCallbacks(Move(aCallbacks))
{
  EnableSynthesizedProgressEvents(true);
}

nsresult
SimpleChannel::OpenContentStream(bool async, nsIInputStream **streamOut, nsIChannel** channel)
{
  NS_ENSURE_TRUE(mCallbacks, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIInputStream> stream;
  TRY_VAR(stream, mCallbacks->OpenContentStream(async, this));
  MOZ_ASSERT(stream);

  mCallbacks = nullptr;

  *channel = nullptr;
  stream.forget(streamOut);
  return NS_OK;
}

nsresult
SimpleChannel::BeginAsyncRead(nsIStreamListener* listener, nsIRequest** request)
{
  NS_ENSURE_TRUE(mCallbacks, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIRequest> req;
  TRY_VAR(req, mCallbacks->StartAsyncRead(listener, this));

  mCallbacks = nullptr;

  req.forget(request);
  return NS_OK;
}

#undef TRY_VAR

already_AddRefed<nsIChannel>
NS_NewSimpleChannelInternal(nsIURI* aURI, nsILoadInfo* aLoadInfo, UniquePtr<SimpleChannelCallbacks>&& aCallbacks)
{
  RefPtr<SimpleChannel> chan = new SimpleChannel(Move(aCallbacks));

  chan->SetURI(aURI);

  MOZ_ALWAYS_SUCCEEDS(chan->SetLoadInfo(aLoadInfo));

  return chan.forget();
}

} // namespace net
} // namespace mozilla
