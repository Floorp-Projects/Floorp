/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "test_io.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>

#include "prerror.h"
#include "prlog.h"
#include "prthread.h"

#include "databuffer.h"

extern bool g_ssl_gtest_verbose;

namespace nss_test {

static PRDescIdentity test_fd_identity = PR_INVALID_IO_LAYER;

#define UNIMPLEMENTED()                                                        \
  std::cerr << "Call to unimplemented function " << __FUNCTION__ << std::endl; \
  PR_ASSERT(PR_FALSE);                                                         \
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0)

#define LOG(a) std::cerr << name_ << ": " << a << std::endl
#define LOGV(a)                      \
  do {                               \
    if (g_ssl_gtest_verbose) LOG(a); \
  } while (false)

class Packet : public DataBuffer {
 public:
  Packet(const DataBuffer &buf) : DataBuffer(buf), offset_(0) {}

  void Advance(size_t delta) {
    PR_ASSERT(offset_ + delta <= len());
    offset_ = std::min(len(), offset_ + delta);
  }

  size_t offset() const { return offset_; }
  size_t remaining() const { return len() - offset_; }

 private:
  size_t offset_;
};

// Implementation of NSPR methods
static PRStatus DummyClose(PRFileDesc *f) {
  DummyPrSocket *io = reinterpret_cast<DummyPrSocket *>(f->secret);
  f->secret = nullptr;
  f->dtor(f);
  delete io;
  return PR_SUCCESS;
}

static int32_t DummyRead(PRFileDesc *f, void *buf, int32_t length) {
  DummyPrSocket *io = reinterpret_cast<DummyPrSocket *>(f->secret);
  return io->Read(buf, length);
}

static int32_t DummyWrite(PRFileDesc *f, const void *buf, int32_t length) {
  DummyPrSocket *io = reinterpret_cast<DummyPrSocket *>(f->secret);
  return io->Write(buf, length);
}

static int32_t DummyAvailable(PRFileDesc *f) {
  UNIMPLEMENTED();
  return -1;
}

int64_t DummyAvailable64(PRFileDesc *f) {
  UNIMPLEMENTED();
  return -1;
}

static PRStatus DummySync(PRFileDesc *f) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static int32_t DummySeek(PRFileDesc *f, int32_t offset, PRSeekWhence how) {
  UNIMPLEMENTED();
  return -1;
}

static int64_t DummySeek64(PRFileDesc *f, int64_t offset, PRSeekWhence how) {
  UNIMPLEMENTED();
  return -1;
}

static PRStatus DummyFileInfo(PRFileDesc *f, PRFileInfo *info) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static PRStatus DummyFileInfo64(PRFileDesc *f, PRFileInfo64 *info) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static int32_t DummyWritev(PRFileDesc *f, const PRIOVec *iov, int32_t iov_size,
                           PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

static PRStatus DummyConnect(PRFileDesc *f, const PRNetAddr *addr,
                             PRIntervalTime to) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static PRFileDesc *DummyAccept(PRFileDesc *sd, PRNetAddr *addr,
                               PRIntervalTime to) {
  UNIMPLEMENTED();
  return nullptr;
}

static PRStatus DummyBind(PRFileDesc *f, const PRNetAddr *addr) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static PRStatus DummyListen(PRFileDesc *f, int32_t depth) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static PRStatus DummyShutdown(PRFileDesc *f, int32_t how) {
  DummyPrSocket *io = reinterpret_cast<DummyPrSocket *>(f->secret);
  io->Reset();
  return PR_SUCCESS;
}

// This function does not support peek.
static int32_t DummyRecv(PRFileDesc *f, void *buf, int32_t buflen,
                         int32_t flags, PRIntervalTime to) {
  PR_ASSERT(flags == 0);
  if (flags != 0) {
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
  }

  DummyPrSocket *io = reinterpret_cast<DummyPrSocket *>(f->secret);

  if (io->mode() == DGRAM) {
    return io->Recv(buf, buflen);
  } else {
    return io->Read(buf, buflen);
  }
}

// Note: this is always nonblocking and assumes a zero timeout.
static int32_t DummySend(PRFileDesc *f, const void *buf, int32_t amount,
                         int32_t flags, PRIntervalTime to) {
  int32_t written = DummyWrite(f, buf, amount);
  return written;
}

static int32_t DummyRecvfrom(PRFileDesc *f, void *buf, int32_t amount,
                             int32_t flags, PRNetAddr *addr,
                             PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

static int32_t DummySendto(PRFileDesc *f, const void *buf, int32_t amount,
                           int32_t flags, const PRNetAddr *addr,
                           PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

static int16_t DummyPoll(PRFileDesc *f, int16_t in_flags, int16_t *out_flags) {
  UNIMPLEMENTED();
  return -1;
}

static int32_t DummyAcceptRead(PRFileDesc *sd, PRFileDesc **nd,
                               PRNetAddr **raddr, void *buf, int32_t amount,
                               PRIntervalTime t) {
  UNIMPLEMENTED();
  return -1;
}

static int32_t DummyTransmitFile(PRFileDesc *sd, PRFileDesc *f,
                                 const void *headers, int32_t hlen,
                                 PRTransmitFileFlags flags, PRIntervalTime t) {
  UNIMPLEMENTED();
  return -1;
}

static PRStatus DummyGetpeername(PRFileDesc *f, PRNetAddr *addr) {
  // TODO: Modify to return unique names for each channel
  // somehow, as opposed to always the same static address. The current
  // implementation messes up the session cache, which is why it's off
  // elsewhere
  addr->inet.family = PR_AF_INET;
  addr->inet.port = 0;
  addr->inet.ip = 0;

  return PR_SUCCESS;
}

static PRStatus DummyGetsockname(PRFileDesc *f, PRNetAddr *addr) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static PRStatus DummyGetsockoption(PRFileDesc *f, PRSocketOptionData *opt) {
  switch (opt->option) {
    case PR_SockOpt_Nonblocking:
      opt->value.non_blocking = PR_TRUE;
      return PR_SUCCESS;
    default:
      UNIMPLEMENTED();
      break;
  }

  return PR_FAILURE;
}

// Imitate setting socket options. These are mostly noops.
static PRStatus DummySetsockoption(PRFileDesc *f,
                                   const PRSocketOptionData *opt) {
  switch (opt->option) {
    case PR_SockOpt_Nonblocking:
      return PR_SUCCESS;
    case PR_SockOpt_NoDelay:
      return PR_SUCCESS;
    default:
      UNIMPLEMENTED();
      break;
  }

  return PR_FAILURE;
}

static int32_t DummySendfile(PRFileDesc *out, PRSendFileData *in,
                             PRTransmitFileFlags flags, PRIntervalTime to) {
  UNIMPLEMENTED();
  return -1;
}

static PRStatus DummyConnectContinue(PRFileDesc *f, int16_t flags) {
  UNIMPLEMENTED();
  return PR_FAILURE;
}

static int32_t DummyReserved(PRFileDesc *f) {
  UNIMPLEMENTED();
  return -1;
}

DummyPrSocket::~DummyPrSocket() { Reset(); }

void DummyPrSocket::SetPacketFilter(PacketFilter *filter) {
  if (filter_) {
    delete filter_;
  }
  filter_ = filter;
}

void DummyPrSocket::Reset() {
  delete filter_;
  if (peer_) {
    peer_->SetPeer(nullptr);
    peer_ = nullptr;
  }
  while (!input_.empty()) {
    Packet *front = input_.front();
    input_.pop();
    delete front;
  }
}

static const struct PRIOMethods DummyMethods = {
    PR_DESC_LAYERED,    DummyClose,
    DummyRead,          DummyWrite,
    DummyAvailable,     DummyAvailable64,
    DummySync,          DummySeek,
    DummySeek64,        DummyFileInfo,
    DummyFileInfo64,    DummyWritev,
    DummyConnect,       DummyAccept,
    DummyBind,          DummyListen,
    DummyShutdown,      DummyRecv,
    DummySend,          DummyRecvfrom,
    DummySendto,        DummyPoll,
    DummyAcceptRead,    DummyTransmitFile,
    DummyGetsockname,   DummyGetpeername,
    DummyReserved,      DummyReserved,
    DummyGetsockoption, DummySetsockoption,
    DummySendfile,      DummyConnectContinue,
    DummyReserved,      DummyReserved,
    DummyReserved,      DummyReserved};

PRFileDesc *DummyPrSocket::CreateFD(const std::string &name, Mode mode) {
  if (test_fd_identity == PR_INVALID_IO_LAYER) {
    test_fd_identity = PR_GetUniqueIdentity("testtransportadapter");
  }

  PRFileDesc *fd = (PR_CreateIOLayerStub(test_fd_identity, &DummyMethods));
  fd->secret = reinterpret_cast<PRFilePrivate *>(new DummyPrSocket(name, mode));

  return fd;
}

DummyPrSocket *DummyPrSocket::GetAdapter(PRFileDesc *fd) {
  return reinterpret_cast<DummyPrSocket *>(fd->secret);
}

void DummyPrSocket::PacketReceived(const DataBuffer &packet) {
  input_.push(new Packet(packet));
}

int32_t DummyPrSocket::Read(void *data, int32_t len) {
  PR_ASSERT(mode_ == STREAM);

  if (mode_ != STREAM) {
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
  }

  if (input_.empty()) {
    LOGV("Read --> wouldblock " << len);
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  Packet *front = input_.front();
  size_t to_read =
      std::min(static_cast<size_t>(len), front->len() - front->offset());
  memcpy(data, static_cast<const void *>(front->data() + front->offset()),
         to_read);
  front->Advance(to_read);

  if (!front->remaining()) {
    input_.pop();
    delete front;
  }

  return static_cast<int32_t>(to_read);
}

int32_t DummyPrSocket::Recv(void *buf, int32_t buflen) {
  if (input_.empty()) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  Packet *front = input_.front();
  if (static_cast<size_t>(buflen) < front->len()) {
    PR_ASSERT(false);
    PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
    return -1;
  }

  size_t count = front->len();
  memcpy(buf, front->data(), count);

  input_.pop();
  delete front;

  return static_cast<int32_t>(count);
}

int32_t DummyPrSocket::Write(const void *buf, int32_t length) {
  if (!peer_) {
    PR_SetError(PR_IO_ERROR, 0);
    return -1;
  }

  DataBuffer packet(static_cast<const uint8_t *>(buf),
                    static_cast<size_t>(length));
  DataBuffer filtered;
  PacketFilter::Action action = PacketFilter::KEEP;
  if (filter_) {
    action = filter_->Filter(packet, &filtered);
  }
  switch (action) {
    case PacketFilter::CHANGE:
      LOG("Original packet: " << packet);
      LOG("Filtered packet: " << filtered);
      peer_->PacketReceived(filtered);
      break;
    case PacketFilter::DROP:
      LOG("Droppped packet: " << packet);
      break;
    case PacketFilter::KEEP:
      LOGV("Packet: " << packet);
      peer_->PacketReceived(packet);
      break;
  }
  // libssl can't handle it if this reports something other than the length
  // of what was passed in (or less, but we're not doing partial writes).
  return static_cast<int32_t>(packet.len());
}

Poller *Poller::instance;

Poller *Poller::Instance() {
  if (!instance) instance = new Poller();

  return instance;
}

void Poller::Shutdown() {
  delete instance;
  instance = nullptr;
}

Poller::~Poller() {
  while (!timers_.empty()) {
    Timer *timer = timers_.top();
    timers_.pop();
    delete timer;
  }
}

void Poller::Wait(Event event, DummyPrSocket *adapter, PollTarget *target,
                  PollCallback cb) {
  auto it = waiters_.find(adapter);
  Waiter *waiter;

  if (it == waiters_.end()) {
    waiter = new Waiter(adapter);
  } else {
    waiter = it->second;
  }

  assert(event < TIMER_EVENT);
  if (event >= TIMER_EVENT) return;

  waiter->targets_[event] = target;
  waiter->callbacks_[event] = cb;
  waiters_[adapter] = waiter;
}

void Poller::Cancel(Event event, DummyPrSocket *adapter) {
  auto it = waiters_.find(adapter);
  Waiter *waiter;

  if (it == waiters_.end()) {
    return;
  }

  waiter = it->second;

  waiter->targets_[event] = nullptr;
  waiter->callbacks_[event] = nullptr;

  // Clean up if there are no callbacks.
  for (size_t i = 0; i < TIMER_EVENT; ++i) {
    if (waiter->callbacks_[i]) return;
  }

  delete waiter;
  waiters_.erase(adapter);
}

void Poller::SetTimer(uint32_t timer_ms, PollTarget *target, PollCallback cb,
                      Timer **timer) {
  Timer *t = new Timer(PR_Now() + timer_ms * 1000, target, cb);
  timers_.push(t);
  if (timer) *timer = t;
}

bool Poller::Poll() {
  if (g_ssl_gtest_verbose) {
    std::cerr << "Poll() waiters = " << waiters_.size()
              << " timers = " << timers_.size() << std::endl;
  }
  PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;
  PRTime now = PR_Now();
  bool fired = false;

  // Figure out the timer for the select.
  if (!timers_.empty()) {
    Timer *first_timer = timers_.top();
    if (now >= first_timer->deadline_) {
      // Timer expired.
      timeout = PR_INTERVAL_NO_WAIT;
    } else {
      timeout =
          PR_MillisecondsToInterval((first_timer->deadline_ - now) / 1000);
    }
  }

  for (auto it = waiters_.begin(); it != waiters_.end(); ++it) {
    Waiter *waiter = it->second;

    if (waiter->callbacks_[READABLE_EVENT]) {
      if (waiter->io_->readable()) {
        PollCallback callback = waiter->callbacks_[READABLE_EVENT];
        PollTarget *target = waiter->targets_[READABLE_EVENT];
        waiter->callbacks_[READABLE_EVENT] = nullptr;
        waiter->targets_[READABLE_EVENT] = nullptr;
        callback(target, READABLE_EVENT);
        fired = true;
      }
    }
  }

  if (fired) timeout = PR_INTERVAL_NO_WAIT;

  // Can't wait forever and also have nothing readable now.
  if (timeout == PR_INTERVAL_NO_TIMEOUT) return false;

  // Sleep.
  if (timeout != PR_INTERVAL_NO_WAIT) {
    PR_Sleep(timeout);
  }

  // Now process anything that timed out.
  now = PR_Now();
  while (!timers_.empty()) {
    if (now < timers_.top()->deadline_) break;

    Timer *timer = timers_.top();
    timers_.pop();
    if (timer->callback_) {
      timer->callback_(timer->target_, TIMER_EVENT);
    }
    delete timer;
  }

  return true;
}

}  // namespace nss_test
