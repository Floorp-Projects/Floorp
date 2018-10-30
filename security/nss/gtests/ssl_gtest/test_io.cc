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

extern bool g_ssl_gtest_verbose;

namespace nss_test {

#define LOG(a) std::cerr << name_ << ": " << a << std::endl
#define LOGV(a)                      \
  do {                               \
    if (g_ssl_gtest_verbose) LOG(a); \
  } while (false)

ScopedPRFileDesc DummyPrSocket::CreateFD() {
  static PRDescIdentity test_fd_identity =
      PR_GetUniqueIdentity("testtransportadapter");
  return DummyIOLayerMethods::CreateFD(test_fd_identity, this);
}

void DummyPrSocket::Reset() {
  auto p = peer_.lock();
  peer_.reset();
  if (p) {
    p->peer_.reset();
    p->Reset();
  }
  while (!input_.empty()) {
    input_.pop();
  }
  filter_ = nullptr;
  write_error_ = 0;
}

void DummyPrSocket::PacketReceived(const DataBuffer &packet) {
  input_.push(Packet(packet));
}

int32_t DummyPrSocket::Read(PRFileDesc *f, void *data, int32_t len) {
  PR_ASSERT(variant_ == ssl_variant_stream);
  if (variant_ != ssl_variant_stream) {
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
  }

  auto dst = peer_.lock();
  if (!dst) {
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  }

  if (input_.empty()) {
    LOGV("Read --> wouldblock " << len);
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  auto &front = input_.front();
  size_t to_read =
      std::min(static_cast<size_t>(len), front.len() - front.offset());
  memcpy(data, static_cast<const void *>(front.data() + front.offset()),
         to_read);
  front.Advance(to_read);

  if (!front.remaining()) {
    input_.pop();
  }

  return static_cast<int32_t>(to_read);
}

int32_t DummyPrSocket::Recv(PRFileDesc *f, void *buf, int32_t buflen,
                            int32_t flags, PRIntervalTime to) {
  PR_ASSERT(flags == 0);
  if (flags != 0) {
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
  }

  if (variant() != ssl_variant_datagram) {
    return Read(f, buf, buflen);
  }

  auto dst = peer_.lock();
  if (!dst) {
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  }

  if (input_.empty()) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  auto &front = input_.front();
  if (static_cast<size_t>(buflen) < front.len()) {
    PR_ASSERT(false);
    PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
    return -1;
  }

  size_t count = front.len();
  memcpy(buf, front.data(), count);

  input_.pop();
  return static_cast<int32_t>(count);
}

int32_t DummyPrSocket::Write(PRFileDesc *f, const void *buf, int32_t length) {
  if (write_error_) {
    PR_SetError(write_error_, 0);
    return -1;
  }

  auto dst = peer_.lock();
  if (!dst) {
    PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
    return -1;
  }

  DataBuffer packet(static_cast<const uint8_t *>(buf),
                    static_cast<size_t>(length));
  DataBuffer filtered;
  PacketFilter::Action action = PacketFilter::KEEP;
  if (filter_) {
    action = filter_->Process(packet, &filtered);
  }
  switch (action) {
    case PacketFilter::CHANGE:
      LOG("Original packet: " << packet);
      LOG("Filtered packet: " << filtered);
      dst->PacketReceived(filtered);
      break;
    case PacketFilter::DROP:
      LOG("Droppped packet: " << packet);
      break;
    case PacketFilter::KEEP:
      LOGV("Packet: " << packet);
      dst->PacketReceived(packet);
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

void Poller::Wait(Event event, std::shared_ptr<DummyPrSocket> &adapter,
                  PollTarget *target, PollCallback cb) {
  assert(event < TIMER_EVENT);
  if (event >= TIMER_EVENT) return;

  std::unique_ptr<Waiter> waiter;
  auto it = waiters_.find(adapter);
  if (it == waiters_.end()) {
    waiter.reset(new Waiter(adapter));
  } else {
    waiter = std::move(it->second);
  }

  waiter->targets_[event] = target;
  waiter->callbacks_[event] = cb;
  waiters_[adapter] = std::move(waiter);
}

void Poller::Cancel(Event event, std::shared_ptr<DummyPrSocket> &adapter) {
  auto it = waiters_.find(adapter);
  if (it == waiters_.end()) {
    return;
  }

  auto &waiter = it->second;
  waiter->targets_[event] = nullptr;
  waiter->callbacks_[event] = nullptr;

  // Clean up if there are no callbacks.
  for (size_t i = 0; i < TIMER_EVENT; ++i) {
    if (waiter->callbacks_[i]) return;
  }

  waiters_.erase(adapter);
}

void Poller::SetTimer(uint32_t timer_ms, PollTarget *target, PollCallback cb,
                      std::shared_ptr<Timer> *timer) {
  auto t = std::make_shared<Timer>(PR_Now() + timer_ms * 1000, target, cb);
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
    auto first_timer = timers_.top();
    if (now >= first_timer->deadline_) {
      // Timer expired.
      timeout = PR_INTERVAL_NO_WAIT;
    } else {
      timeout =
          PR_MillisecondsToInterval((first_timer->deadline_ - now) / 1000);
    }
  }

  for (auto it = waiters_.begin(); it != waiters_.end(); ++it) {
    auto &waiter = it->second;

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

    auto timer = timers_.top();
    timers_.pop();
    if (timer->callback_) {
      timer->callback_(timer->target_, TIMER_EVENT);
    }
  }

  return true;
}

}  // namespace nss_test
