#include "gtest/gtest.h"

#include "mozilla/CORSMode.h"
#include "mozilla/dom/XMLDocument.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/FetchPreloader.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/PreloadHashKey.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsISupportsPriority.h"
#include "nsThreadUtils.h"
#include "nsStringStream.h"

namespace {

auto const ERROR_CANCEL = NS_ERROR_ABORT;
auto const ERROR_ONSTOP = NS_ERROR_NET_INTERRUPT;
auto const ERROR_THROW = NS_ERROR_FAILURE;

class FakeChannel : public nsIChannel {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUEST

  nsresult Start() { return mListener->OnStartRequest(this); }
  nsresult Data(const nsACString& aData) {
    if (NS_FAILED(mStatus)) {
      return mStatus;
    }
    nsCOMPtr<nsIInputStream> is;
    NS_NewCStringInputStream(getter_AddRefs(is), aData);
    return mListener->OnDataAvailable(this, is, 0, aData.Length());
  }
  nsresult Stop(nsresult status) {
    if (NS_SUCCEEDED(mStatus)) {
      mStatus = status;
    }
    mListener->OnStopRequest(this, mStatus);
    mListener = nullptr;
    return mStatus;
  }

 private:
  virtual ~FakeChannel() = default;
  bool mIsPending = false;
  bool mCanceled = false;
  nsresult mStatus = NS_OK;
  nsCOMPtr<nsIStreamListener> mListener;
};

NS_IMPL_ISUPPORTS(FakeChannel, nsIChannel, nsIRequest)

NS_IMETHODIMP FakeChannel::GetName(nsACString& result) { return NS_OK; }
NS_IMETHODIMP FakeChannel::IsPending(bool* result) {
  *result = mIsPending;
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetStatus(nsresult* status) {
  *status = mStatus;
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}
NS_IMETHODIMP FakeChannel::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}
NS_IMETHODIMP FakeChannel::CancelWithReason(nsresult aStatus,
                                            const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}
NS_IMETHODIMP FakeChannel::Cancel(nsresult status) {
  if (!mCanceled) {
    mCanceled = true;
    mStatus = status;
  }
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::Suspend() { return NS_OK; }
NS_IMETHODIMP FakeChannel::Resume() { return NS_OK; }
NS_IMETHODIMP FakeChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = 0;
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP FakeChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP FakeChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetOriginalURI(nsIURI** aURI) { return NS_OK; }
NS_IMETHODIMP FakeChannel::SetOriginalURI(nsIURI* aURI) { return NS_OK; }
NS_IMETHODIMP FakeChannel::GetURI(nsIURI** aURI) { return NS_OK; }
NS_IMETHODIMP FakeChannel::GetOwner(nsISupports** aOwner) { return NS_OK; }
NS_IMETHODIMP FakeChannel::SetOwner(nsISupports* aOwner) { return NS_OK; }
NS_IMETHODIMP FakeChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) { return NS_OK; }
NS_IMETHODIMP FakeChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetIsDocument(bool* aIsDocument) {
  *aIsDocument = false;
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aCallbacks) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetContentType(nsACString& aContentType) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetContentType(const nsACString& aContentType) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetContentCharset(nsACString& aContentCharset) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetContentCharset(
    const nsACString& aContentCharset) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetContentDisposition(
    uint32_t* aContentDisposition) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetContentDisposition(uint32_t aContentDisposition) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_AVAILABLE;
}
NS_IMETHODIMP FakeChannel::GetContentLength(int64_t* aContentLength) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::SetContentLength(int64_t aContentLength) {
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::GetCanceled(bool* aCanceled) {
  *aCanceled = mCanceled;
  return NS_OK;
}
NS_IMETHODIMP FakeChannel::Open(nsIInputStream** aStream) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
FakeChannel::AsyncOpen(nsIStreamListener* aListener) {
  mIsPending = true;
  mListener = aListener;
  return NS_OK;
}

class FakePreloader : public mozilla::FetchPreloader {
 public:
  explicit FakePreloader(FakeChannel* aChannel) : mDrivingChannel(aChannel) {}

 private:
  RefPtr<FakeChannel> mDrivingChannel;

  virtual nsresult CreateChannel(
      nsIChannel** aChannel, nsIURI* aURI, const mozilla::CORSMode aCORSMode,
      const mozilla::dom::ReferrerPolicy& aReferrerPolicy,
      mozilla::dom::Document* aDocument, nsILoadGroup* aLoadGroup,
      nsIInterfaceRequestor* aCallbacks, uint64_t aHttpChannelId,
      int32_t aSupportsPriorityValue) override {
    mDrivingChannel.forget(aChannel);
    return NS_OK;
  }
};

class FakeListener : public nsIStreamListener {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  enum { Never, OnStart, OnData, OnStop } mCancelIn = Never;

  nsresult mOnStartResult = NS_OK;
  nsresult mOnDataResult = NS_OK;
  nsresult mOnStopResult = NS_OK;

  bool mOnStart = false;
  nsCString mOnData;
  mozilla::Maybe<nsresult> mOnStop;

 private:
  virtual ~FakeListener() = default;
};

NS_IMPL_ISUPPORTS(FakeListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP FakeListener::OnStartRequest(nsIRequest* request) {
  EXPECT_FALSE(mOnStart);
  mOnStart = true;

  if (mCancelIn == OnStart) {
    request->Cancel(ERROR_CANCEL);
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
    request->Cancel(ERROR_CANCEL);
  }

  return mOnDataResult;
}
NS_IMETHODIMP FakeListener::OnStopRequest(nsIRequest* request,
                                          nsresult status) {
  EXPECT_FALSE(mOnStop);
  mOnStop.emplace(status);

  if (mCancelIn == OnStop) {
    request->Cancel(ERROR_CANCEL);
  }

  return mOnStopResult;
}

bool eventInProgress = true;

void Await() {
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "uriloader:TestFetchPreloader:Await"_ns, [&]() {
        bool yield = !eventInProgress;
        eventInProgress = true;  // Just for convenience
        return yield;
      }));
}

// WinBase.h defines this.
#undef Yield
void Yield() { eventInProgress = false; }

}  // namespace

// ****************************************************************************
// Test body
// ****************************************************************************

// Caching with all good results (data + NS_OK)
TEST(TestFetchPreloader, CacheNoneBeforeConsume)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == NS_OK);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CacheStartBeforeConsume)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());

  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);

    EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
    EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
    EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));

    EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == NS_OK);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CachePartOfDataBeforeConsume)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));

  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);

    EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));

    EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == NS_OK);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CacheAllDataBeforeConsume)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));

  // Request consumation of the preload...
  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));

    EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == NS_OK);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CacheAllBeforeConsume)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == NS_OK);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

// Get data before the channel fails
TEST(TestFetchPreloader, CacheAllBeforeConsumeWithChannelError)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_FAILED(channel->Stop(ERROR_ONSTOP));

  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_ONSTOP);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

// Cancel the channel between caching and consuming
TEST(TestFetchPreloader, CacheAllBeforeConsumeWithChannelCancel)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  channel->Cancel(ERROR_CANCEL);
  EXPECT_NS_FAILED(channel->Stop(ERROR_CANCEL));

  RefPtr<FakeListener> listener = new FakeListener();
  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    // XXX - This is hard to solve; the data is there but we won't deliver it.
    // This is a bit different case than e.g. a network error.  We want to
    // deliver some data in that case.  Cancellation probably happens because of
    // navigation or a demand to not consume the channel anyway.
    EXPECT_TRUE(listener->mOnData.IsEmpty());
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_CANCEL);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

// Let the listener throw while data is already cached
TEST(TestFetchPreloader, CacheAllBeforeConsumeThrowFromOnStartRequest)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mOnStartResult = ERROR_THROW;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.IsEmpty());
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_THROW);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CacheAllBeforeConsumeThrowFromOnDataAvailable)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mOnDataResult = ERROR_THROW;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("one"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_THROW);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CacheAllBeforeConsumeThrowFromOnStopRequest)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mOnStopResult = ERROR_THROW;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("onetwothree"));
    // Throwing from OnStopRequest is generally ignored.
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == NS_OK);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

// Cancel the channel in various callbacks
TEST(TestFetchPreloader, CacheAllBeforeConsumeCancelInOnStartRequest)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mCancelIn = FakeListener::OnStart;
  // check that throwing from OnStartRequest doesn't affect the cancellation
  // status.
  listener->mOnStartResult = ERROR_THROW;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.IsEmpty());
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_CANCEL);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CacheAllBeforeConsumeCancelInOnDataAvailable)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
  EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mCancelIn = FakeListener::OnData;
  // check that throwing from OnStartRequest doesn't affect the cancellation
  // status.
  listener->mOnDataResult = ERROR_THROW;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("one"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_CANCEL);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

// Corner cases
TEST(TestFetchPreloader, CachePartlyBeforeConsumeCancelInOnDataAvailable)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mCancelIn = FakeListener::OnData;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_NS_FAILED(channel->Data("three"_ns));
    EXPECT_NS_FAILED(channel->Stop(NS_OK));

    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("one"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_CANCEL);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CachePartlyBeforeConsumeCancelInOnStartRequestAndRace)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));

  // This has to simulate a possibiilty when stream listener notifications from
  // the channel are already pending in the queue while AsyncConsume is called.
  // At this moment the listener has not been notified yet.
  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
    EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));
  }));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mCancelIn = FakeListener::OnStart;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  // Check listener's been fed properly.  Expected is to NOT get any data and
  // propagate the cancellation code and not being called duplicated
  // OnStopRequest.
  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.IsEmpty());
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_CANCEL);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CachePartlyBeforeConsumeCancelInOnDataAvailableAndRace)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));

  // This has to simulate a possibiilty when stream listener notifications from
  // the channel are already pending in the queue while AsyncConsume is called.
  // At this moment the listener has not been notified yet.
  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
    EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));
  }));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mCancelIn = FakeListener::OnData;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  // Check listener's been fed properly.  Expected is to NOT get anything after
  // the first OnData and propagate the cancellation code and not being called
  // duplicated OnStopRequest.
  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.EqualsLiteral("one"));
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_CANCEL);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}

TEST(TestFetchPreloader, CachePartlyBeforeConsumeThrowFromOnStartRequestAndRace)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "https://example.com"_ns);
  auto key = mozilla::PreloadHashKey::CreateAsFetch(uri, mozilla::CORS_NONE);

  RefPtr<FakeChannel> channel = new FakeChannel();
  RefPtr<FakePreloader> preloader = new FakePreloader(channel);
  RefPtr<mozilla::dom::Document> doc;
  NS_NewXMLDocument(getter_AddRefs(doc), nullptr, nullptr);

  EXPECT_TRUE(NS_SUCCEEDED(preloader->OpenChannel(
      key, uri, mozilla::CORS_NONE, mozilla::dom::ReferrerPolicy::_empty, doc,
      0, nsISupportsPriority::PRIORITY_NORMAL)));

  EXPECT_NS_SUCCEEDED(channel->Start());
  EXPECT_NS_SUCCEEDED(channel->Data("one"_ns));
  EXPECT_NS_SUCCEEDED(channel->Data("two"_ns));

  // This has to simulate a possibiilty when stream listener notifications from
  // the channel are already pending in the queue while AsyncConsume is called.
  // At this moment the listener has not been notified yet.
  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_NS_SUCCEEDED(channel->Data("three"_ns));
    EXPECT_NS_SUCCEEDED(channel->Stop(NS_OK));
  }));

  RefPtr<FakeListener> listener = new FakeListener();
  listener->mOnStartResult = ERROR_THROW;

  EXPECT_NS_SUCCEEDED(preloader->AsyncConsume(listener));
  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));

  // Check listener's been fed properly.  Expected is to NOT get any data and
  // propagate the throwing code and not being called duplicated OnStopRequest.
  NS_DispatchToMainThread(NS_NewRunnableFunction("test", [&]() {
    EXPECT_TRUE(listener->mOnStart);
    EXPECT_TRUE(listener->mOnData.IsEmpty());
    EXPECT_TRUE(listener->mOnStop && *listener->mOnStop == ERROR_THROW);

    Yield();
  }));

  Await();

  EXPECT_FALSE(NS_SUCCEEDED(preloader->AsyncConsume(listener)));
}
