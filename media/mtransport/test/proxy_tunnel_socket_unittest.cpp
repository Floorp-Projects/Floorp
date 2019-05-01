/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original authors: ekr@rtfm.com; ryan@tokbox.com

#include <vector>
#include <numeric>

#include "mozilla/net/NeckoChannelParams.h"
#include "nr_socket_proxy.h"
#include "nr_socket_proxy_config.h"
#include "WebrtcProxyChannelWrapper.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;

// update TestReadMultipleSizes if you change this
const std::string kHelloMessage = "HELLO IS IT ME YOU'RE LOOKING FOR?";

class NrSocketProxyTest : public MtransportTest {
 public:
  NrSocketProxyTest()
      : mSProxy(nullptr),
        nr_socket_(nullptr),
        mEmptyArray(0),
        mReadChunkSize(0),
        mReadChunkSizeIncrement(1),
        mReadAllowance(-1),
        mConnected(false) {}

  void SetUp() override {
    nsCString alpn = NS_LITERAL_CSTRING("webrtc");
    std::shared_ptr<NrSocketProxyConfig> config;
    config.reset(new NrSocketProxyConfig(0, alpn, net::LoadInfoArgs()));
    // config is never used but must be non-null
    mSProxy = new NrSocketProxy(config);
    int r = nr_socket_create_int((void*)mSProxy.get(), mSProxy->vtbl(),
                                 &nr_socket_);
    ASSERT_EQ(0, r);

    // fake calling AsyncOpen() due to IPC calls. must be non-null
    mSProxy->AssignChannel_DoNotUse(new WebrtcProxyChannelWrapper(nullptr));
  }

  void TearDown() override { mSProxy->close(); }

  static void readable_cb(NR_SOCKET s, int how, void* cb_arg) {
    NrSocketProxyTest* test = (NrSocketProxyTest*)cb_arg;
    size_t capacity = std::min(test->mReadChunkSize, test->mReadAllowance);
    nsTArray<uint8_t> array(capacity);
    size_t read;

    nr_socket_read(test->nr_socket_, (char*)array.Elements(), array.Capacity(),
                   &read, 0);

    ASSERT_TRUE(read <= array.Capacity());
    ASSERT_TRUE(test->mReadAllowance >= read);

    array.SetLength(read);
    test->mData.AppendElements(array);
    test->mReadAllowance -= read;

    // We may read more bytes each time we're called. This way we can ensure we
    // consume buffers partially and across multiple buffers.
    test->mReadChunkSize += test->mReadChunkSizeIncrement;

    if (test->mReadAllowance > 0) {
      NR_ASYNC_WAIT(s, how, &NrSocketProxyTest::readable_cb, cb_arg);
    }
  }

  static void writable_cb(NR_SOCKET s, int how, void* cb_arg) {
    NrSocketProxyTest* test = (NrSocketProxyTest*)cb_arg;
    test->mConnected = true;
  }

  const std::string DataString() {
    return std::string((char*)mData.Elements(), mData.Length());
  }

 protected:
  RefPtr<NrSocketProxy> mSProxy;
  nr_socket* nr_socket_;

  nsTArray<uint8_t> mData;
  nsTArray<uint8_t> mEmptyArray;

  uint32_t mReadChunkSize;
  uint32_t mReadChunkSizeIncrement;
  uint32_t mReadAllowance;

  bool mConnected;
};

TEST_F(NrSocketProxyTest, TestCreate) {}

TEST_F(NrSocketProxyTest, TestConnected) {
  ASSERT_TRUE(!mConnected);

  NR_ASYNC_WAIT(mSProxy, NR_ASYNC_WAIT_WRITE, &NrSocketProxyTest::writable_cb,
                this);

  // still not connected just registered for writes...
  ASSERT_TRUE(!mConnected);

  mSProxy->OnConnected();

  ASSERT_TRUE(mConnected);
}

TEST_F(NrSocketProxyTest, TestRead) {
  nsTArray<uint8_t> array;
  array.AppendElements(kHelloMessage.c_str(), kHelloMessage.length());

  NR_ASYNC_WAIT(mSProxy, NR_ASYNC_WAIT_READ, &NrSocketProxyTest::readable_cb,
                this);
  // this will read 0 bytes here
  mSProxy->OnRead(std::move(array));

  ASSERT_EQ(kHelloMessage.length(), mSProxy->CountUnreadBytes());

  // callback is still set but terminated due to 0 byte read
  // start callbacks again (first read is 0 then 1,2,3,...)
  mSProxy->OnRead(std::move(mEmptyArray));

  ASSERT_EQ(kHelloMessage.length(), mData.Length());
  ASSERT_EQ(kHelloMessage, DataString());
}

TEST_F(NrSocketProxyTest, TestReadConstantConsumeSize) {
  std::string data;

  // triangle number
  const int kCount = 32;

  //  ~17kb
  // triangle number formula n*(n+1)/2
  for (int i = 0; i < kCount * (kCount + 1) / 2; ++i) {
    data += kHelloMessage;
  }

  // decreasing buffer sizes
  for (int i = 0, start = 0; i < kCount; ++i) {
    int length = (kCount - i) * kHelloMessage.length();

    nsTArray<uint8_t> array;
    array.AppendElements(data.c_str() + start, length);
    start += length;

    mSProxy->OnRead(std::move(array));
  }

  ASSERT_EQ(data.length(), mSProxy->CountUnreadBytes());

  // read same amount each callback
  mReadChunkSize = 128;
  mReadChunkSizeIncrement = 0;
  NR_ASYNC_WAIT(mSProxy, NR_ASYNC_WAIT_READ, &NrSocketProxyTest::readable_cb,
                this);

  ASSERT_EQ(data.length(), mSProxy->CountUnreadBytes());

  // start callbacks
  mSProxy->OnRead(std::move(mEmptyArray));

  ASSERT_EQ(data.length(), mData.Length());
  ASSERT_EQ(data, DataString());
}

TEST_F(NrSocketProxyTest, TestReadNone) {
  char buf[4096];
  size_t read = 0;
  int r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);

  ASSERT_EQ(R_WOULDBLOCK, r);

  nsTArray<uint8_t> array;
  array.AppendElements(kHelloMessage.c_str(), kHelloMessage.length());
  mSProxy->OnRead(std::move(array));

  ASSERT_EQ(kHelloMessage.length(), mSProxy->CountUnreadBytes());

  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);

  ASSERT_EQ(0, r);
  ASSERT_EQ(kHelloMessage.length(), read);
  ASSERT_EQ(kHelloMessage, std::string(buf, read));
}

TEST_F(NrSocketProxyTest, TestReadMultipleSizes) {
  using namespace std;

  string data;
  // 515 * kHelloMessage.length() == 17510
  const size_t kCount = 515;
  // randomly generated numbers, sums to 17510, 20 numbers
  vector<int> varyingSizes = {404,  622, 1463, 1597, 1676, 389, 389,
                              1272, 781, 81,   1030, 1450, 256, 812,
                              1571, 29,  1045, 911,  643,  1089};

  // changing varyingSizes or the test message breaks this so check here
  ASSERT_EQ(kCount, 17510 / kHelloMessage.length());
  ASSERT_EQ(17510, accumulate(varyingSizes.begin(), varyingSizes.end(), 0));

  // ~17kb
  for (size_t i = 0; i < kCount; ++i) {
    data += kHelloMessage;
  }

  nsTArray<uint8_t> array;
  array.AppendElements(data.c_str(), data.length());

  for (int amountToRead : varyingSizes) {
    nsTArray<uint8_t> buffer;
    buffer.AppendElements(array.Elements(), amountToRead);
    array.RemoveElementsAt(0, amountToRead);
    mSProxy->OnRead(std::move(buffer));
  }

  ASSERT_EQ(data.length(), mSProxy->CountUnreadBytes());

  // don't need to read 0 on the first read, so start at 1 and keep going
  mReadChunkSize = 1;
  NR_ASYNC_WAIT(mSProxy, NR_ASYNC_WAIT_READ, &NrSocketProxyTest::readable_cb,
                this);
  // start callbacks
  mSProxy->OnRead(std::move(mEmptyArray));

  ASSERT_EQ(data.length(), mData.Length());
  ASSERT_EQ(data, DataString());
}

TEST_F(NrSocketProxyTest, TestReadConsumeReadDrain) {
  std::string data;
  // ~26kb total; should be even
  const int kCount = 512;

  // there's some division by 2 here so check that kCount is even
  ASSERT_EQ(0, kCount % 2);

  for (int i = 0; i < kCount; ++i) {
    data += kHelloMessage;
    nsTArray<uint8_t> array;
    array.AppendElements(kHelloMessage.c_str(), kHelloMessage.length());
    mSProxy->OnRead(std::move(array));
  }

  // read half at first
  mReadAllowance = kCount / 2 * kHelloMessage.length();
  // start by reading 1 byte
  mReadChunkSize = 1;
  NR_ASYNC_WAIT(mSProxy, NR_ASYNC_WAIT_READ, &NrSocketProxyTest::readable_cb,
                this);
  mSProxy->OnRead(std::move(mEmptyArray));

  ASSERT_EQ(data.length() / 2, mSProxy->CountUnreadBytes());
  ASSERT_EQ(data.length() / 2, mData.Length());

  // fill read buffer back up
  for (int i = 0; i < kCount / 2; ++i) {
    data += kHelloMessage;
    nsTArray<uint8_t> array;
    array.AppendElements(kHelloMessage.c_str(), kHelloMessage.length());
    mSProxy->OnRead(std::move(array));
  }

  // remove read limit
  mReadAllowance = -1;
  // used entire read allowance so we need to setup a new await
  NR_ASYNC_WAIT(mSProxy, NR_ASYNC_WAIT_READ, &NrSocketProxyTest::readable_cb,
                this);
  // start callbacks
  mSProxy->OnRead(std::move(mEmptyArray));

  ASSERT_EQ(data.length(), mData.Length());
  ASSERT_EQ(data, DataString());
}
