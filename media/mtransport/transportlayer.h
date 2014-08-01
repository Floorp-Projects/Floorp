/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportlayer_h__
#define transportlayer_h__

#include "sigslot.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"

#include "m_cpp_utils.h"

namespace mozilla {

class TransportFlow;

typedef int TransportResult;

enum {
  TE_WOULDBLOCK = -1, TE_ERROR = -2, TE_INTERNAL = -3
};

#define TRANSPORT_LAYER_ID(name) \
  virtual const std::string id() { return name; } \
  static std::string ID() { return name; }

// Abstract base class for network transport layers.
class TransportLayer : public sigslot::has_slots<> {
 public:
  // The state of the transport flow
  // We can't use "ERROR" because Windows has a macro named "ERROR"
  enum State { TS_NONE, TS_INIT, TS_CONNECTING, TS_OPEN, TS_CLOSED, TS_ERROR };
  enum Mode { STREAM, DGRAM };

  // Is this a stream or datagram flow
  TransportLayer(Mode mode = STREAM) :
    mode_(mode),
    state_(TS_NONE),
    flow_id_(),
    downward_(nullptr) {}

  virtual ~TransportLayer() {}

  // Called to initialize
  nsresult Init();  // Called by Insert() to set up -- do not override
  virtual nsresult InitInternal() { return NS_OK; } // Called by Init

  // Called when inserted into a flow
  virtual void Inserted(TransportFlow *flow, TransportLayer *downward);

  // Downward interface
  TransportLayer *downward() { return downward_; }

  // Dispatch a call onto our thread (or run on the same thread if
  // thread is not set). This is always synchronous.
  nsresult RunOnThread(nsIRunnable *event);

  // Get the state
  State state() const { return state_; }
  // Must be implemented by derived classes
  virtual TransportResult SendPacket(const unsigned char *data, size_t len) = 0;

  // Get the thread.
  const nsCOMPtr<nsIEventTarget> GetThread() const {
    return target_;
  }

  // Event definitions that one can register for
  // State has changed
  sigslot::signal2<TransportLayer*, State> SignalStateChange;
  // Data received on the flow
  sigslot::signal3<TransportLayer*, const unsigned char *, size_t>
                         SignalPacketReceived;

  // Return the layer id for this layer
  virtual const std::string id() = 0;

  // The id of the flow
  const std::string& flow_id() {
    return flow_id_;
  }

 protected:
  virtual void WasInserted() {}
  virtual void SetState(State state, const char *file, unsigned line);
  // Check if we are on the right thread
  void CheckThread() {
    NS_ABORT_IF_FALSE(CheckThreadInt(), "Wrong thread");
  }

  Mode mode_;
  State state_;
  std::string flow_id_;
  TransportLayer *downward_; // The next layer in the stack
  nsCOMPtr<nsIEventTarget> target_;

 private:
  DISALLOW_COPY_ASSIGN(TransportLayer);

  bool CheckThreadInt() {
    bool on;

    if (!target_)  // OK if no thread set.
      return true;

    NS_ENSURE_SUCCESS(target_->IsOnCurrentThread(&on), false);
    NS_ENSURE_TRUE(on, false);

    return true;
  }
};

#define LAYER_INFO "Flow[" << flow_id() << "(none)" << "]; Layer[" << id() << "]: "
#define TL_SET_STATE(x) SetState((x), __FILE__, __LINE__)

}  // close namespace
#endif
