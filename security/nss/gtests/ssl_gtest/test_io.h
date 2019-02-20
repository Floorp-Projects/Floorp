/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef test_io_h_
#define test_io_h_

#include <string.h>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <string>

#include "databuffer.h"
#include "dummy_io.h"
#include "prio.h"
#include "nss_scoped_ptrs.h"
#include "sslt.h"

namespace nss_test {

class DataBuffer;
class DummyPrSocket;  // Fwd decl.

// Allow us to inspect a packet before it is written.
class PacketFilter {
 public:
  enum Action {
    KEEP,    // keep the original packet unmodified
    CHANGE,  // change the packet to a different value
    DROP     // drop the packet
  };
  PacketFilter(bool enabled = true) : enabled_(enabled) {}
  virtual ~PacketFilter() {}

  virtual Action Process(const DataBuffer& input, DataBuffer* output) {
    if (!enabled_) {
      return KEEP;
    }
    return Filter(input, output);
  }
  void Enable() { enabled_ = true; }
  void Disable() { enabled_ = false; }

  // The packet filter takes input and has the option of mutating it.
  //
  // A filter that modifies the data places the modified data in *output and
  // returns CHANGE.  A filter that does not modify data returns LEAVE, in which
  // case the value in *output is ignored.  A Filter can return DROP, in which
  // case the packet is dropped (and *output is ignored).
  virtual Action Filter(const DataBuffer& input, DataBuffer* output) = 0;

 private:
  bool enabled_;
};

class DummyPrSocket : public DummyIOLayerMethods {
 public:
  DummyPrSocket(const std::string& name, SSLProtocolVariant var)
      : name_(name),
        variant_(var),
        peer_(),
        input_(),
        filter_(nullptr),
        write_error_(0) {}
  virtual ~DummyPrSocket() {}

  static PRDescIdentity LayerId();

  // Create a file descriptor that will reference this object.  The fd must not
  // live longer than this adapter; call PR_Close() before.
  ScopedPRFileDesc CreateFD();

  std::weak_ptr<DummyPrSocket>& peer() { return peer_; }
  void SetPeer(const std::shared_ptr<DummyPrSocket>& p) { peer_ = p; }
  void SetPacketFilter(const std::shared_ptr<PacketFilter>& filter) {
    filter_ = filter;
  }
  // Drops peer, packet filter and any outstanding packets.
  void Reset();

  void PacketReceived(const DataBuffer& data);
  int32_t Read(PRFileDesc* f, void* data, int32_t len) override;
  int32_t Recv(PRFileDesc* f, void* buf, int32_t buflen, int32_t flags,
               PRIntervalTime to) override;
  int32_t Write(PRFileDesc* f, const void* buf, int32_t length) override;
  void SetWriteError(PRErrorCode code) { write_error_ = code; }

  SSLProtocolVariant variant() const { return variant_; }
  bool readable() const { return !input_.empty(); }

 private:
  class Packet : public DataBuffer {
   public:
    Packet(const DataBuffer& buf) : DataBuffer(buf), offset_(0) {}

    void Advance(size_t delta) {
      PR_ASSERT(offset_ + delta <= len());
      offset_ = std::min(len(), offset_ + delta);
    }

    size_t offset() const { return offset_; }
    size_t remaining() const { return len() - offset_; }

   private:
    size_t offset_;
  };

  const std::string name_;
  SSLProtocolVariant variant_;
  std::weak_ptr<DummyPrSocket> peer_;
  std::queue<Packet> input_;
  std::shared_ptr<PacketFilter> filter_;
  PRErrorCode write_error_;
};

// Marker interface.
class PollTarget {};

enum Event { READABLE_EVENT, TIMER_EVENT /* Must be last */ };

typedef void (*PollCallback)(PollTarget*, Event);

class Poller {
 public:
  static Poller* Instance();  // Get a singleton.
  static void Shutdown();     // Shut it down.

  class Timer {
   public:
    Timer(PRTime deadline, PollTarget* target, PollCallback callback)
        : deadline_(deadline), target_(target), callback_(callback) {}
    void Cancel() { callback_ = nullptr; }

    PRTime deadline_;
    PollTarget* target_;
    PollCallback callback_;
  };

  void Wait(Event event, std::shared_ptr<DummyPrSocket>& adapter,
            PollTarget* target, PollCallback cb);
  void Cancel(Event event, std::shared_ptr<DummyPrSocket>& adapter);
  void SetTimer(uint32_t timer_ms, PollTarget* target, PollCallback cb,
                std::shared_ptr<Timer>* handle);
  bool Poll();

 private:
  Poller() : waiters_(), timers_() {}
  ~Poller() {}

  class Waiter {
   public:
    Waiter(std::shared_ptr<DummyPrSocket> io) : io_(io) {
      memset(&targets_[0], 0, sizeof(targets_));
      memset(&callbacks_[0], 0, sizeof(callbacks_));
    }

    void WaitFor(Event event, PollCallback callback);

    std::shared_ptr<DummyPrSocket> io_;
    PollTarget* targets_[TIMER_EVENT];
    PollCallback callbacks_[TIMER_EVENT];
  };

  class TimerComparator {
   public:
    bool operator()(const std::shared_ptr<Timer> lhs,
                    const std::shared_ptr<Timer> rhs) {
      return lhs->deadline_ > rhs->deadline_;
    }
  };

  static Poller* instance;
  std::map<std::shared_ptr<DummyPrSocket>, std::unique_ptr<Waiter>> waiters_;
  std::priority_queue<std::shared_ptr<Timer>,
                      std::vector<std::shared_ptr<Timer>>, TimerComparator>
      timers_;
};

}  // namespace nss_test

#endif
