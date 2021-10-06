#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/PreloadHashKey.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsThreadUtils.h"
#include "nsStringStream.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsIInterfaceRequestor.h"

class FakeListener : public nsIStreamListener, public nsIInterfaceRequestor {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

  enum { Never, OnStart, OnData, OnStop } mCancelIn = Never;

  nsresult mOnStartResult = NS_OK;
  nsresult mOnDataResult = NS_OK;
  nsresult mOnStopResult = NS_OK;

  bool mOnStart = false;
  nsCString mOnData;
  Maybe<nsresult> mOnStop;

 private:
  virtual ~FakeListener() = default;
};

NS_IMPL_ISUPPORTS(FakeListener, nsIStreamListener, nsIRequestObserver,
                  nsIInterfaceRequestor)

NS_IMETHODIMP
FakeListener::GetInterface(const nsIID& aIID, void** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  return NS_NOINTERFACE;
}

NS_IMETHODIMP FakeListener::OnStartRequest(nsIRequest* request) {
  EXPECT_FALSE(mOnStart);
  mOnStart = true;

  if (mCancelIn == OnStart) {
    request->Cancel(NS_ERROR_ABORT);
  }

  return mOnStartResult;
}

NS_IMETHODIMP FakeListener::OnDataAvailable(nsIRequest* request,
                                            nsIInputStream* input,
                                            uint64_t offset, uint32_t count) {
  nsAutoCString data;
  data.SetLength(count);

  uint32_t read;
  input->Read(data.BeginWriting(), count, &read);
  mOnData += data;

  if (mCancelIn == OnData) {
    request->Cancel(NS_ERROR_ABORT);
  }

  return mOnDataResult;
}

NS_IMETHODIMP FakeListener::OnStopRequest(nsIRequest* request,
                                          nsresult status) {
  EXPECT_FALSE(mOnStop);
  mOnStop.emplace(status);

  if (mCancelIn == OnStop) {
    request->Cancel(NS_ERROR_ABORT);
  }

  return mOnStopResult;
}

// Test that nsHttpChannel::AsyncOpen properly picks up changes to
// loadInfo.mPrivateBrowsingId that occur after the channel was created.
TEST(TestHttpChannel, PBAsyncOpen)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "http://localhost/"_ns);

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannel(
      getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_OTHER);
  ASSERT_EQ(rv, NS_OK);

  RefPtr<FakeListener> listener = new FakeListener();
  rv = channel->SetNotificationCallbacks(listener);
  ASSERT_EQ(rv, NS_OK);

  nsCOMPtr<nsIPrivateBrowsingChannel> pbchannel = do_QueryInterface(channel);
  ASSERT_TRUE(pbchannel);

  bool isPrivate = false;
  rv = pbchannel->GetIsChannelPrivate(&isPrivate);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(isPrivate, false);

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  OriginAttributes attrs;
  attrs.mPrivateBrowsingId = 1;
  rv = loadInfo->SetOriginAttributes(attrs);
  ASSERT_EQ(rv, NS_OK);

  rv = pbchannel->GetIsChannelPrivate(&isPrivate);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(isPrivate, false);

  rv = channel->AsyncOpen(listener);
  ASSERT_EQ(rv, NS_OK);

  rv = pbchannel->GetIsChannelPrivate(&isPrivate);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(isPrivate, true);

  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      [&]() -> bool { return listener->mOnStop.isSome(); }));
}
