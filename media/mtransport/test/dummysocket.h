/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original authors: ekr@rtfm.com; ryan@tokbox.com

#ifndef MTRANSPORT_DUMMY_SOCKET_H_
#define MTRANSPORT_DUMMY_SOCKET_H_

#include "nr_socket_prsock.h"

extern "C" {
#include "transport_addr.h"
}

#include "databuffer.h"
#include "mozilla/UniquePtr.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

namespace mozilla {

static UniquePtr<DataBuffer> merge(UniquePtr<DataBuffer> a, UniquePtr<DataBuffer> b) {
  if (a && a->len() && b && b->len()) {
    UniquePtr<DataBuffer> merged(new DataBuffer());
    merged->Allocate(a->len() + b->len());

    memcpy(merged->data(), a->data(), a->len());
    memcpy(merged->data() + a->len(), b->data(), b->len());

    return merged;
  }

  if (a && a->len()) {
    return a;
  }

  if (b && b->len()) {
    return b;
  }

  return nullptr;
}

class DummySocket : public NrSocketBase {
 public:
  DummySocket()
      : writable_(UINT_MAX),
        write_buffer_(nullptr),
        readable_(UINT_MAX),
        read_buffer_(nullptr),
        cb_(nullptr),
        cb_arg_(nullptr),
        self_(nullptr) {}

  // the nr_socket APIs
  virtual int create(nr_transport_addr *addr) override {
    return 0;
  }

  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) override {
    MOZ_CRASH();
    return 0;
  }

  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) override {
    MOZ_CRASH();
    return 0;
  }

  virtual int getaddr(nr_transport_addr *addrp) override {
    MOZ_CRASH();
    return 0;
  }

  virtual void close() override {
  }

  virtual int connect(nr_transport_addr *addr) override {
    nr_transport_addr_copy(&connect_addr_, addr);
    return 0;
  }

  virtual int listen(int backlog) override {
    return 0;
  }

  virtual int accept(nr_transport_addr *addrp, nr_socket **sockp) override {
    return 0;
  }

  virtual int write(const void *msg, size_t len, size_t *written) override {
    size_t to_write = std::min(len, writable_);

    if (to_write) {
      UniquePtr<DataBuffer> msgbuf(new DataBuffer(static_cast<const uint8_t *>(msg), to_write));
      write_buffer_ = merge(std::move(write_buffer_), std::move(msgbuf));
    }

    *written = to_write;

    return 0;
  }

  virtual int read(void* buf, size_t maxlen, size_t *len) override {
    if (!read_buffer_.get()) {
      return R_WOULDBLOCK;
    }

    size_t to_read = std::min(read_buffer_->len(),
                              std::min(maxlen, readable_));

    memcpy(buf, read_buffer_->data(), to_read);
    *len = to_read;

    if (to_read < read_buffer_->len()) {
      read_buffer_.reset(new DataBuffer(read_buffer_->data() + to_read,
                                    read_buffer_->len() - to_read));
    } else {
      read_buffer_.reset();
    }

    return 0;
  }

  // Implementations of the async_event APIs.
  // These are no-ops because we handle scheduling manually
  // for test purposes.
  virtual int async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line) override {
    EXPECT_EQ(nullptr, cb_);
    cb_ = cb;
    cb_arg_ = cb_arg;

    return 0;
  }

  virtual int cancel(int how) override {
    cb_ = nullptr;
    cb_arg_ = nullptr;

    return 0;
  }

  // Read/Manipulate the current state.
  void CheckWriteBuffer(const uint8_t *data, size_t len) {
    if (!len) {
      EXPECT_EQ(nullptr, write_buffer_.get());
    } else {
      EXPECT_NE(nullptr, write_buffer_.get());
      ASSERT_EQ(len, write_buffer_->len());
      ASSERT_EQ(0, memcmp(data, write_buffer_->data(), len));
    }
  }

  void ClearWriteBuffer() {
    write_buffer_.reset();
  }

  void SetWritable(size_t val) {
    writable_ = val;
  }

  void FireWritableCb() {
    NR_async_cb cb = cb_;
    void *cb_arg = cb_arg_;

    cb_ = nullptr;
    cb_arg_ = nullptr;

    cb(this, NR_ASYNC_WAIT_WRITE, cb_arg);
  }

  void SetReadBuffer(const uint8_t *data, size_t len) {
    EXPECT_EQ(nullptr, write_buffer_.get());
    read_buffer_.reset(new DataBuffer(data, len));
  }

  void ClearReadBuffer() {
    read_buffer_.reset();
  }

  void SetReadable(size_t val) {
    readable_ = val;
  }

  nr_socket *get_nr_socket() {
    if (!self_) {
      int r = nr_socket_create_int(this, vtbl(), &self_);
      AddRef();
      if (r)
        return nullptr;
    }

    return self_;
  }

  nr_transport_addr *get_connect_addr() {
    return &connect_addr_;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DummySocket, override);

 private:
  ~DummySocket() {}

  DISALLOW_COPY_ASSIGN(DummySocket);

  size_t writable_;  // Amount we allow someone to write.
  UniquePtr<DataBuffer> write_buffer_;
  size_t readable_;   // Amount we allow someone to read.
  UniquePtr<DataBuffer> read_buffer_;

  NR_async_cb cb_;
  void *cb_arg_;
  nr_socket *self_;

  nr_transport_addr connect_addr_;
};

} //namespace mozilla

#endif
