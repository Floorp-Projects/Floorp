/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "mozilla/Scoped.h"

extern "C" {
#include "nr_api.h"
#include "nr_socket.h"
#include "nr_socket_buffered_stun.h"
#include "transport_addr.h"
#include "stun.h"
}

#include "databuffer.h"
#include "mtransport_test_utils.h"
#include "dummysocket.h"

#include "nr_socket_prsock.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MtransportTestUtils *test_utils;

static uint8_t kStunMessage[] = {
  0x00, 0x01, 0x00, 0x08, 0x21, 0x12, 0xa4, 0x42,
  0x9b, 0x90, 0xbe, 0x2c, 0xae, 0x1a, 0x0c, 0xa8,
  0xa0, 0xd6, 0x8b, 0x08, 0x80, 0x28, 0x00, 0x04,
  0xdb, 0x35, 0x5f, 0xaa
};
static size_t kStunMessageLen = sizeof(kStunMessage);

class BufferedStunSocketTest : public ::testing::Test {
 public:
  BufferedStunSocketTest()
      : dummy_(nullptr),
        test_socket_(nullptr) {}

  ~BufferedStunSocketTest() {
    nr_socket_destroy(&test_socket_);
  }

  void SetUp() {
    nsRefPtr<DummySocket> dummy(new DummySocket());

    int r = nr_socket_buffered_stun_create(
        dummy->get_nr_socket(),
        kStunMessageLen,
        TURN_TCP_FRAMING,
        &test_socket_);
    ASSERT_EQ(0, r);
    dummy_ = dummy.forget();  // Now owned by test_socket_.

    r = nr_str_port_to_transport_addr(
        (char *)"192.0.2.133", 3333, IPPROTO_TCP, &remote_addr_);
    ASSERT_EQ(0, r);

    r = nr_socket_connect(test_socket_,
                          &remote_addr_);
    ASSERT_EQ(0, r);
  }

  nr_socket *socket() { return test_socket_; }

 protected:
  nsRefPtr<DummySocket> dummy_;
  nr_socket *test_socket_;
  nr_transport_addr remote_addr_;
};


TEST_F(BufferedStunSocketTest, TestCreate) {
}

TEST_F(BufferedStunSocketTest, TestSendTo) {
  int r = nr_socket_sendto(test_socket_,
                           kStunMessage,
                           kStunMessageLen,
                           0, &remote_addr_);
  ASSERT_EQ(0, r);

  dummy_->CheckWriteBuffer(kStunMessage, kStunMessageLen);
}

TEST_F(BufferedStunSocketTest, TestSendToBuffered) {
  dummy_->SetWritable(0);

  int r = nr_socket_sendto(test_socket_,
                           kStunMessage,
                           kStunMessageLen,
                           0, &remote_addr_);
  ASSERT_EQ(0, r);

  dummy_->CheckWriteBuffer(nullptr, 0);

  dummy_->SetWritable(kStunMessageLen);
  dummy_->FireWritableCb();
  dummy_->CheckWriteBuffer(kStunMessage, kStunMessageLen);
}

TEST_F(BufferedStunSocketTest, TestSendFullThenDrain) {
  dummy_->SetWritable(0);

  for (;;) {
    int r = nr_socket_sendto(test_socket_,
                             kStunMessage,
                             kStunMessageLen,
                             0, &remote_addr_);
    if (r == R_WOULDBLOCK)
      break;

    ASSERT_EQ(0, r);
  }

  // Nothing was written.
  dummy_->CheckWriteBuffer(nullptr, 0);

  // Now flush.
  dummy_->SetWritable(kStunMessageLen);
  dummy_->FireWritableCb();
  dummy_->ClearWriteBuffer();

  // Verify we can write something.
  int r = nr_socket_sendto(test_socket_,
                             kStunMessage,
                             kStunMessageLen,
                             0, &remote_addr_);
  ASSERT_EQ(0, r);

  // And that it appears.
  dummy_->CheckWriteBuffer(kStunMessage, kStunMessageLen);
}

TEST_F(BufferedStunSocketTest, TestSendToPartialBuffered) {
  dummy_->SetWritable(10);

  int r = nr_socket_sendto(test_socket_,
                           kStunMessage,
                           kStunMessageLen,
                           0, &remote_addr_);
  ASSERT_EQ(0, r);

  dummy_->CheckWriteBuffer(kStunMessage, 10);
  dummy_->ClearWriteBuffer();

  dummy_->SetWritable(kStunMessageLen);
  dummy_->FireWritableCb();
  dummy_->CheckWriteBuffer(kStunMessage + 10, kStunMessageLen - 10);
}

TEST_F(BufferedStunSocketTest, TestSendToReject) {
  dummy_->SetWritable(0);

  int r = nr_socket_sendto(test_socket_,
                           kStunMessage,
                           kStunMessageLen,
                           0, &remote_addr_);
  ASSERT_EQ(0, r);

  dummy_->CheckWriteBuffer(nullptr, 0);

  r = nr_socket_sendto(test_socket_,
                       kStunMessage,
                       kStunMessageLen,
                       0, &remote_addr_);
  ASSERT_EQ(R_WOULDBLOCK, r);

  dummy_->CheckWriteBuffer(nullptr, 0);
}

TEST_F(BufferedStunSocketTest, TestSendToWrongAddr) {
  nr_transport_addr addr;

  int r = nr_str_port_to_transport_addr(
      (char *)"192.0.2.134", 3333, IPPROTO_TCP, &addr);
  ASSERT_EQ(0, r);

  r = nr_socket_sendto(test_socket_,
                       kStunMessage,
                       kStunMessageLen,
                       0, &addr);
  ASSERT_EQ(R_BAD_DATA, r);
}

TEST_F(BufferedStunSocketTest, TestReceiveRecvFrom) {
  dummy_->SetReadBuffer(kStunMessage, kStunMessageLen);

  unsigned char tmp[2048];
  size_t len;
  nr_transport_addr addr;

  int r = nr_socket_recvfrom(test_socket_,
                             tmp, sizeof(tmp), &len, 0,
                             &addr);
  ASSERT_EQ(0, r);
  ASSERT_EQ(kStunMessageLen, len);
  ASSERT_EQ(0, memcmp(kStunMessage, tmp, kStunMessageLen));
  ASSERT_EQ(0, nr_transport_addr_cmp(&addr, &remote_addr_,
                                     NR_TRANSPORT_ADDR_CMP_MODE_ALL));
}

TEST_F(BufferedStunSocketTest, TestReceiveRecvFromPartial) {
  dummy_->SetReadBuffer(kStunMessage, 15);

  unsigned char tmp[2048];
  size_t len;
  nr_transport_addr addr;

  int r = nr_socket_recvfrom(test_socket_,
                             tmp, sizeof(tmp), &len, 0,
                             &addr);
  ASSERT_EQ(R_WOULDBLOCK, r);


  dummy_->SetReadBuffer(kStunMessage + 15, kStunMessageLen - 15);

  r = nr_socket_recvfrom(test_socket_,
                         tmp, sizeof(tmp), &len, 0,
                         &addr);
  ASSERT_EQ(0, r);
  ASSERT_EQ(kStunMessageLen, len);
  ASSERT_EQ(0, memcmp(kStunMessage, tmp, kStunMessageLen));
  ASSERT_EQ(0, nr_transport_addr_cmp(&addr, &remote_addr_,
                                     NR_TRANSPORT_ADDR_CMP_MODE_ALL));

  r = nr_socket_recvfrom(test_socket_,
                         tmp, sizeof(tmp), &len, 0,
                         &addr);
  ASSERT_EQ(R_WOULDBLOCK, r);
}


TEST_F(BufferedStunSocketTest, TestReceiveRecvFromGarbage) {
  uint8_t garbage[50];
  memset(garbage, 0xff, sizeof(garbage));

  dummy_->SetReadBuffer(garbage, sizeof(garbage));

  unsigned char tmp[2048];
  size_t len;
  nr_transport_addr addr;
  int r = nr_socket_recvfrom(test_socket_,
                             tmp, sizeof(tmp), &len, 0,
                             &addr);
  ASSERT_EQ(R_BAD_DATA, r);

  r = nr_socket_recvfrom(test_socket_,
                         tmp, sizeof(tmp), &len, 0,
                         &addr);
  ASSERT_EQ(R_FAILED, r);
}

TEST_F(BufferedStunSocketTest, TestReceiveRecvFromTooShort) {
  dummy_->SetReadBuffer(kStunMessage, kStunMessageLen);

  unsigned char tmp[2048];
  size_t len;
  nr_transport_addr addr;

  int r = nr_socket_recvfrom(test_socket_,
                             tmp, kStunMessageLen - 1, &len, 0,
                             &addr);
  ASSERT_EQ(R_BAD_ARGS, r);
}

TEST_F(BufferedStunSocketTest, TestReceiveRecvFromReallyLong) {
  uint8_t garbage[4096];
  memset(garbage, 0xff, sizeof(garbage));
  memcpy(garbage, kStunMessage, kStunMessageLen);
  nr_stun_message_header *hdr = reinterpret_cast<nr_stun_message_header *>
      (garbage);
  hdr->length = htons(3000);

  dummy_->SetReadBuffer(garbage, sizeof(garbage));

  unsigned char tmp[4096];
  size_t len;
  nr_transport_addr addr;

  int r = nr_socket_recvfrom(test_socket_,
                             tmp, kStunMessageLen - 1, &len, 0,
                             &addr);
  ASSERT_EQ(R_BAD_DATA, r);
}



int main(int argc, char **argv)
{
  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();

  delete test_utils;
  return rv;
}
