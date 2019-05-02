/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <mutex>

#include "mozilla/net/WebrtcProxyChannel.h"
#include "mozilla/net/WebrtcProxyChannelCallback.h"

#include "nsISocketTransport.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest/Helpers.h"
#include "gtest_utils.h"

static const uint32_t kDefaultTestTimeout = 2000;
static const char kReadData[] = "Hello, World!";
static const size_t kReadDataLength = sizeof(kReadData) - 1;
static const std::string kReadDataString =
    std::string(kReadData, kReadDataLength);
static int kDataLargeOuterLoopCount = 128;
static int kDataLargeInnerLoopCount = 1024;

namespace mozilla {

using namespace net;
using namespace testing;

class WebrtcProxyChannelTestCallback;

class FakeSocketTransportProvider : public nsISocketTransport {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // nsISocketTransport
  NS_IMETHOD GetHost(nsACString& aHost) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetPort(int32_t* aPort) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetScriptableOriginAttributes(
      JSContext* cx, JS::MutableHandleValue aOriginAttributes) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetScriptableOriginAttributes(
      JSContext* cx, JS::HandleValue aOriginAttributes) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  virtual nsresult GetOriginAttributes(
      mozilla::OriginAttributes* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  virtual nsresult SetOriginAttributes(
      const mozilla::OriginAttributes& aOriginAttrs) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetPeerAddr(mozilla::net::NetAddr* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSelfAddr(mozilla::net::NetAddr* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD Bind(mozilla::net::NetAddr* aLocalAddr) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetScriptablePeerAddr(nsINetAddr** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetScriptableSelfAddr(nsINetAddr** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSecurityInfo(nsISupports** aSecurityInfo) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSecurityCallbacks(
      nsIInterfaceRequestor** aSecurityCallbacks) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetSecurityCallbacks(
      nsIInterfaceRequestor* aSecurityCallbacks) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD IsAlive(bool* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetTimeout(uint32_t aType, uint32_t* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetTimeout(uint32_t aType, uint32_t aValue) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetLinger(bool aPolarity, int16_t aTimeout) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetReuseAddrPort(bool reuseAddrPort) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetConnectionFlags(uint32_t* aConnectionFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetConnectionFlags(uint32_t aConnectionFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetTlsFlags(uint32_t* aTlsFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetTlsFlags(uint32_t aTlsFlags) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetQoSBits(uint8_t* aQoSBits) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetQoSBits(uint8_t aQoSBits) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetRecvBufferSize(uint32_t* aRecvBufferSize) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetSendBufferSize(uint32_t* aSendBufferSize) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetKeepaliveEnabled(bool* aKeepaliveEnabled) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetKeepaliveEnabled(bool aKeepaliveEnabled) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetKeepaliveVals(int32_t keepaliveIdleTime,
                              int32_t keepaliveRetryInterval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetFastOpenCallback(
      mozilla::net::TCPFastOpen* aFastOpen) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetFirstRetryError(nsresult* aFirstRetryError) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetResetIPFamilyPreference(
      bool* aResetIPFamilyPreference) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD GetEsniUsed(bool* aEsniUsed) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD ResolvedByTRR(bool* _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }

  // nsITransport
  NS_IMETHOD OpenInputStream(uint32_t aFlags, uint32_t aSegmentSize,
                             uint32_t aSegmentCount,
                             nsIInputStream** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD OpenOutputStream(uint32_t aFlags, uint32_t aSegmentSize,
                              uint32_t aSegmentCount,
                              nsIOutputStream** _retval) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }
  NS_IMETHOD SetEventSink(nsITransportEventSink* aSink,
                          nsIEventTarget* aEventTarget) override {
    MOZ_ASSERT(false);
    return NS_OK;
  }

  // fake except for these methods which are OK to call
  // nsISocketTransport
  NS_IMETHOD SetRecvBufferSize(uint32_t aRecvBufferSize) override {
    return NS_OK;
  }
  NS_IMETHOD SetSendBufferSize(uint32_t aSendBufferSize) override {
    return NS_OK;
  }
  // nsITransport
  NS_IMETHOD Close(nsresult aReason) override { return NS_OK; }

 protected:
  virtual ~FakeSocketTransportProvider() = default;
};

NS_IMPL_ISUPPORTS(FakeSocketTransportProvider, nsISocketTransport, nsITransport)

// Implements some common elements to WebrtcProxyChannelTestOutputStream and
// WebrtcProxyChannelTestInputStream.
class WebrtcProxyChannelTestStream {
 public:
  WebrtcProxyChannelTestStream();

  void Fail() { mMustFail = true; }

  size_t DataLength();
  template <typename T>
  void AppendElements(const T* aBuffer, size_t aLength);

 protected:
  virtual ~WebrtcProxyChannelTestStream() = default;

  nsTArray<uint8_t> mData;
  std::mutex mDataMutex;

  bool mMustFail;
};

WebrtcProxyChannelTestStream::WebrtcProxyChannelTestStream()
    : mMustFail(false) {}

template <typename T>
void WebrtcProxyChannelTestStream::AppendElements(const T* aBuffer,
                                                  size_t aLength) {
  std::lock_guard<std::mutex> guard(mDataMutex);
  mData.AppendElements(aBuffer, aLength);
}

size_t WebrtcProxyChannelTestStream::DataLength() {
  std::lock_guard<std::mutex> guard(mDataMutex);
  return mData.Length();
}

class WebrtcProxyChannelTestInputStream : public nsIAsyncInputStream,
                                          public WebrtcProxyChannelTestStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAM

  WebrtcProxyChannelTestInputStream()
      : mMaxReadSize(1024 * 1024), mAllowCallbacks(false) {}

  void DoCallback();
  void CallCallback(const nsCOMPtr<nsIInputStreamCallback>& aCallback);
  void AllowCallbacks() { mAllowCallbacks = true; }

  size_t mMaxReadSize;

 protected:
  virtual ~WebrtcProxyChannelTestInputStream() = default;

 private:
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;

  bool mAllowCallbacks;
};

NS_IMPL_ISUPPORTS(WebrtcProxyChannelTestInputStream, nsIAsyncInputStream,
                  nsIInputStream)

nsresult WebrtcProxyChannelTestInputStream::AsyncWait(
    nsIInputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(!aEventTarget, "no event target should be set");

  mCallback = aCallback;
  mCallbackTarget = NS_GetCurrentThread();

  if (mAllowCallbacks && DataLength() > 0) {
    DoCallback();
  }

  return NS_OK;
}

nsresult WebrtcProxyChannelTestInputStream::CloseWithStatus(nsresult aStatus) {
  return Close();
}

nsresult WebrtcProxyChannelTestInputStream::Close() { return NS_OK; }

nsresult WebrtcProxyChannelTestInputStream::Available(uint64_t* aAvailable) {
  *aAvailable = DataLength();
  return NS_OK;
}

nsresult WebrtcProxyChannelTestInputStream::Read(char* aBuffer, uint32_t aCount,
                                                 uint32_t* aRead) {
  std::lock_guard<std::mutex> guard(mDataMutex);
  if (mMustFail) {
    return NS_ERROR_FAILURE;
  }
  *aRead = std::min({(size_t)aCount, mData.Length(), mMaxReadSize});
  memcpy(aBuffer, mData.Elements(), *aRead);
  mData.RemoveElementsAt(0, *aRead);
  return *aRead > 0 ? NS_OK : NS_BASE_STREAM_WOULD_BLOCK;
}

nsresult WebrtcProxyChannelTestInputStream::ReadSegments(
    nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
    uint32_t* _retval) {
  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult WebrtcProxyChannelTestInputStream::IsNonBlocking(
    bool* aIsNonBlocking) {
  *aIsNonBlocking = true;
  return NS_OK;
}

void WebrtcProxyChannelTestInputStream::CallCallback(
    const nsCOMPtr<nsIInputStreamCallback>& aCallback) {
  aCallback->OnInputStreamReady(this);
}

void WebrtcProxyChannelTestInputStream::DoCallback() {
  if (mCallback) {
    mCallbackTarget->Dispatch(
        NewRunnableMethod<const nsCOMPtr<nsIInputStreamCallback>&>(
            "WebrtcProxyChannelTestInputStream::DoCallback", this,
            &WebrtcProxyChannelTestInputStream::CallCallback,
            std::move(mCallback)));

    mCallbackTarget = nullptr;
  }
}

class WebrtcProxyChannelTestOutputStream : public nsIAsyncOutputStream,
                                           public WebrtcProxyChannelTestStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSIOUTPUTSTREAM

  WebrtcProxyChannelTestOutputStream() : mMaxWriteSize(1024 * 1024) {}

  void DoCallback();
  void CallCallback(const nsCOMPtr<nsIOutputStreamCallback>& aCallback);

  const std::string DataString();

  uint32_t mMaxWriteSize;

 protected:
  virtual ~WebrtcProxyChannelTestOutputStream() = default;

 private:
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
};

NS_IMPL_ISUPPORTS(WebrtcProxyChannelTestOutputStream, nsIAsyncOutputStream,
                  nsIOutputStream)

nsresult WebrtcProxyChannelTestOutputStream::AsyncWait(
    nsIOutputStreamCallback* aCallback, uint32_t aFlags,
    uint32_t aRequestedCount, nsIEventTarget* aEventTarget) {
  MOZ_ASSERT(!aEventTarget, "no event target should be set");

  mCallback = aCallback;
  mCallbackTarget = NS_GetCurrentThread();

  return NS_OK;
}

nsresult WebrtcProxyChannelTestOutputStream::CloseWithStatus(nsresult aStatus) {
  return Close();
}

nsresult WebrtcProxyChannelTestOutputStream::Close() { return NS_OK; }

nsresult WebrtcProxyChannelTestOutputStream::Flush() { return NS_OK; }

nsresult WebrtcProxyChannelTestOutputStream::Write(const char* aBuffer,
                                                   uint32_t aCount,
                                                   uint32_t* aWrote) {
  if (mMustFail) {
    return NS_ERROR_FAILURE;
  }
  *aWrote = std::min(aCount, mMaxWriteSize);
  AppendElements(aBuffer, *aWrote);
  return NS_OK;
}

nsresult WebrtcProxyChannelTestOutputStream::WriteSegments(
    nsReadSegmentFun aReader, void* aClosure, uint32_t aCount,
    uint32_t* _retval) {
  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult WebrtcProxyChannelTestOutputStream::WriteFrom(
    nsIInputStream* aFromStream, uint32_t aCount, uint32_t* _retval) {
  MOZ_ASSERT(false);
  return NS_OK;
}

nsresult WebrtcProxyChannelTestOutputStream::IsNonBlocking(
    bool* aIsNonBlocking) {
  *aIsNonBlocking = true;
  return NS_OK;
}

void WebrtcProxyChannelTestOutputStream::CallCallback(
    const nsCOMPtr<nsIOutputStreamCallback>& aCallback) {
  aCallback->OnOutputStreamReady(this);
}

void WebrtcProxyChannelTestOutputStream::DoCallback() {
  if (mCallback) {
    mCallbackTarget->Dispatch(
        NewRunnableMethod<const nsCOMPtr<nsIOutputStreamCallback>&>(
            "WebrtcProxyChannelTestOutputStream::CallCallback", this,
            &WebrtcProxyChannelTestOutputStream::CallCallback,
            std::move(mCallback)));

    mCallbackTarget = nullptr;
  }
}

const std::string WebrtcProxyChannelTestOutputStream::DataString() {
  std::lock_guard<std::mutex> guard(mDataMutex);
  return std::string((char*)mData.Elements(), mData.Length());
}

// Fake as in not the real WebrtcProxyChannel but real enough
class FakeWebrtcProxyChannel : public WebrtcProxyChannel {
 public:
  explicit FakeWebrtcProxyChannel(WebrtcProxyChannelCallback* aCallback)
      : WebrtcProxyChannel(aCallback) {}

 protected:
  virtual ~FakeWebrtcProxyChannel() = default;

  void InvokeOnClose(nsresult aReason) override;
  void InvokeOnConnected() override;
  void InvokeOnRead(nsTArray<uint8_t>&& aReadData) override;
};

void FakeWebrtcProxyChannel::InvokeOnClose(nsresult aReason) {
  mProxyCallbacks->OnClose(aReason);
}

void FakeWebrtcProxyChannel::InvokeOnConnected() {
  mProxyCallbacks->OnConnected();
}

void FakeWebrtcProxyChannel::InvokeOnRead(nsTArray<uint8_t>&& aReadData) {
  mProxyCallbacks->OnRead(std::move(aReadData));
}

class WebrtcProxyChannelTest : public MtransportTest {
 public:
  WebrtcProxyChannelTest()
      : MtransportTest(),
        mSocketThread(nullptr),
        mSocketTransport(nullptr),
        mInputStream(nullptr),
        mOutputStream(nullptr),
        mChannel(nullptr),
        mCallback(nullptr),
        mOnCloseCalled(false),
        mOnConnectedCalled(false) {}

  // WebrtcProxyChannelCallback forwards from mCallback
  void OnClose(nsresult aReason);
  void OnConnected();
  void OnRead(nsTArray<uint8_t>&& aReadData);

  void SetUp() override;
  void TearDown() override;

  void DoTransportAvailable();

  const std::string ReadDataAsString();
  const std::string GetDataLarge();

  nsCOMPtr<nsIEventTarget> mSocketThread;

  nsCOMPtr<nsISocketTransport> mSocketTransport;
  RefPtr<WebrtcProxyChannelTestInputStream> mInputStream;
  RefPtr<WebrtcProxyChannelTestOutputStream> mOutputStream;
  RefPtr<FakeWebrtcProxyChannel> mChannel;
  RefPtr<WebrtcProxyChannelTestCallback> mCallback;

  bool mOnCloseCalled;
  bool mOnConnectedCalled;

  size_t ReadDataLength();
  template <typename T>
  void AppendReadData(const T* aBuffer, size_t aLength);

 private:
  nsTArray<uint8_t> mReadData;
  std::mutex mReadDataMutex;
};

class WebrtcProxyChannelTestCallback : public WebrtcProxyChannelCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcProxyChannelTestCallback,
                                        override)

  explicit WebrtcProxyChannelTestCallback(WebrtcProxyChannelTest* aTest)
      : mTest(aTest) {}

  // WebrtcProxyChannelCallback
  void OnClose(nsresult aReason) override;
  void OnConnected() override;
  void OnRead(nsTArray<uint8_t>&& aReadData) override;

 protected:
  virtual ~WebrtcProxyChannelTestCallback() = default;

 private:
  WebrtcProxyChannelTest* mTest;
};

void WebrtcProxyChannelTest::SetUp() {
  nsresult rv;
  // WebrtcProxyChannel's threading model is the same as mtransport
  // all socket operations are done on the socket thread
  // callbacks are invoked on the main thread
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  mSocketTransport = new FakeSocketTransportProvider();
  mInputStream = new WebrtcProxyChannelTestInputStream();
  mOutputStream = new WebrtcProxyChannelTestOutputStream();
  mCallback = new WebrtcProxyChannelTestCallback(this);
  mChannel = new FakeWebrtcProxyChannel(mCallback.get());
}

void WebrtcProxyChannelTest::TearDown() {}

// WebrtcProxyChannelCallback
void WebrtcProxyChannelTest::OnRead(nsTArray<uint8_t>&& aReadData) {
  AppendReadData(aReadData.Elements(), aReadData.Length());
}

void WebrtcProxyChannelTest::OnConnected() { mOnConnectedCalled = true; }

void WebrtcProxyChannelTest::OnClose(nsresult aReason) {
  mOnCloseCalled = true;
}

void WebrtcProxyChannelTest::DoTransportAvailable() {
  if (!mSocketThread->IsOnCurrentThread()) {
    mSocketThread->Dispatch(
        NS_NewRunnableFunction("DoTransportAvailable", [this]() -> void {
          nsresult rv;
          rv = mChannel->OnTransportAvailable(mSocketTransport, mInputStream,
                                              mOutputStream);
          ASSERT_EQ(NS_OK, rv);
        }));
  } else {
    // should always be called on the main thread
    MOZ_ASSERT(0);
  }
}

const std::string WebrtcProxyChannelTest::ReadDataAsString() {
  std::lock_guard<std::mutex> guard(mReadDataMutex);
  return std::string((char*)mReadData.Elements(), mReadData.Length());
}

const std::string WebrtcProxyChannelTest::GetDataLarge() {
  std::string data;
  for (int i = 0; i < kDataLargeOuterLoopCount * kDataLargeInnerLoopCount;
       ++i) {
    data += kReadData;
  }
  return data;
}

template <typename T>
void WebrtcProxyChannelTest::AppendReadData(const T* aBuffer, size_t aLength) {
  std::lock_guard<std::mutex> guard(mReadDataMutex);
  mReadData.AppendElements(aBuffer, aLength);
}

size_t WebrtcProxyChannelTest::ReadDataLength() {
  std::lock_guard<std::mutex> guard(mReadDataMutex);
  return mReadData.Length();
}

void WebrtcProxyChannelTestCallback::OnClose(nsresult aReason) {
  mTest->OnClose(aReason);
}

void WebrtcProxyChannelTestCallback::OnConnected() { mTest->OnConnected(); }

void WebrtcProxyChannelTestCallback::OnRead(nsTArray<uint8_t>&& aReadData) {
  mTest->OnRead(std::move(aReadData));
}

}  // namespace mozilla

typedef mozilla::WebrtcProxyChannelTest WebrtcProxyChannelTest;

TEST_F(WebrtcProxyChannelTest, SetUp) {}

TEST_F(WebrtcProxyChannelTest, TransportAvailable) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);
}

TEST_F(WebrtcProxyChannelTest, Read) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  mInputStream->AppendElements(kReadData, kReadDataLength);
  mInputStream->DoCallback();

  ASSERT_TRUE_WAIT(ReadDataAsString() == kReadDataString, kDefaultTestTimeout);
}

TEST_F(WebrtcProxyChannelTest, Write) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  nsTArray<uint8_t> data;
  data.AppendElements(kReadData, kReadDataLength);
  mChannel->Write(std::move(data));

  ASSERT_TRUE_WAIT(mChannel->CountUnwrittenBytes() == kReadDataLength,
                   kDefaultTestTimeout);

  mOutputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOutputStream->DataString() == kReadDataString,
                   kDefaultTestTimeout);
}

TEST_F(WebrtcProxyChannelTest, ReadFail) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  mInputStream->AppendElements(kReadData, kReadDataLength);
  mInputStream->Fail();
  mInputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOnCloseCalled, kDefaultTestTimeout);
  ASSERT_EQ(0U, ReadDataLength());
}

TEST_F(WebrtcProxyChannelTest, WriteFail) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  nsTArray<uint8_t> array;
  array.AppendElements(kReadData, kReadDataLength);
  mChannel->Write(std::move(array));

  ASSERT_TRUE_WAIT(mChannel->CountUnwrittenBytes() == kReadDataLength,
                   kDefaultTestTimeout);

  mOutputStream->Fail();
  mOutputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOnCloseCalled, kDefaultTestTimeout);
  ASSERT_EQ(0U, mOutputStream->DataLength());
}

TEST_F(WebrtcProxyChannelTest, ReadLarge) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  const std::string data = GetDataLarge();

  mInputStream->AppendElements(data.c_str(), data.length());
  // make sure reading loops more than once
  mInputStream->mMaxReadSize = 3072;
  mInputStream->AllowCallbacks();
  mInputStream->DoCallback();

  ASSERT_TRUE_WAIT(ReadDataAsString() == data, kDefaultTestTimeout);
}

TEST_F(WebrtcProxyChannelTest, WriteLarge) {
  DoTransportAvailable();
  ASSERT_TRUE_WAIT(mOnConnectedCalled, kDefaultTestTimeout);

  const std::string data = GetDataLarge();

  for (int i = 0; i < kDataLargeOuterLoopCount; ++i) {
    nsTArray<uint8_t> array;
    int chunkSize = kReadDataString.length() * kDataLargeInnerLoopCount;
    int offset = i * chunkSize;
    array.AppendElements(data.c_str() + offset, chunkSize);
    mChannel->Write(std::move(array));
  }

  ASSERT_TRUE_WAIT(mChannel->CountUnwrittenBytes() == data.length(),
                   kDefaultTestTimeout);

  // make sure writing loops more than once per write request
  mOutputStream->mMaxWriteSize = 1024;
  mOutputStream->DoCallback();

  ASSERT_TRUE_WAIT(mOutputStream->DataString() == data, kDefaultTestTimeout);
}
