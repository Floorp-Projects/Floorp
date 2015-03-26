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
#include <queue>
#include <string>
#include <ostream>

#include "prio.h"

namespace nss_test {

class DataBuffer;
class Packet;
class DummyPrSocket;  // Fwd decl.

// Allow us to inspect a packet before it is written.
class PacketFilter {
 public:
  virtual ~PacketFilter() {}

  // The packet filter takes input and has the option of mutating it.
  //
  // A filter that modifies the data places the modified data in *output and
  // returns true.  A filter that does not modify data returns false, in which
  // case the value in *output is ignored.
  virtual bool Filter(const DataBuffer& input, DataBuffer* output) = 0;
};

enum Mode { STREAM, DGRAM };

inline std::ostream& operator<<(std::ostream& os, Mode m) {
  return os << ((m == STREAM) ? "TLS" : "DTLS");
}

class DummyPrSocket {
 public:
  ~DummyPrSocket();

  static PRFileDesc* CreateFD(const std::string& name,
                              Mode mode);  // Returns an FD.
  static DummyPrSocket* GetAdapter(PRFileDesc* fd);

  void SetPeer(DummyPrSocket* peer) { peer_ = peer; }

  void SetPacketFilter(PacketFilter* filter) { filter_ = filter; }

  void PacketReceived(const DataBuffer& data);
  int32_t Read(void* data, int32_t len);
  int32_t Recv(void* buf, int32_t buflen);
  int32_t Write(const void* buf, int32_t length);

  Mode mode() const { return mode_; }
  bool readable() const { return !input_.empty(); }
  bool writable() { return true; }

 private:
  DummyPrSocket(const std::string& name, Mode mode)
      : name_(name),
        mode_(mode),
        peer_(nullptr),
        input_(),
        filter_(nullptr) {}

  const std::string name_;
  Mode mode_;
  DummyPrSocket* peer_;
  std::queue<Packet*> input_;
  PacketFilter* filter_;
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

  void Wait(Event event, DummyPrSocket* adapter, PollTarget* target,
            PollCallback cb);
  void SetTimer(uint32_t timer_ms, PollTarget* target, PollCallback cb,
                Timer** handle);
  bool Poll();

 private:
  Poller() : waiters_(), timers_() {}

  class Waiter {
   public:
    Waiter(DummyPrSocket* io) : io_(io) {
      memset(&callbacks_[0], 0, sizeof(callbacks_));
    }

    void WaitFor(Event event, PollCallback callback);

    DummyPrSocket* io_;
    PollTarget* targets_[TIMER_EVENT];
    PollCallback callbacks_[TIMER_EVENT];
  };

  class TimerComparator {
   public:
    bool operator()(const Timer* lhs, const Timer* rhs) {
      return lhs->deadline_ > rhs->deadline_;
    }
  };

  static Poller* instance;
  std::map<DummyPrSocket*, Waiter*> waiters_;
  std::priority_queue<Timer*, std::vector<Timer*>, TimerComparator> timers_;
};

}  // end of namespace

#endif
