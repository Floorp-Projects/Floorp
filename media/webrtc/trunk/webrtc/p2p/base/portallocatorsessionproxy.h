/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_PORTALLOCATORSESSIONPROXY_H_
#define WEBRTC_P2P_BASE_PORTALLOCATORSESSIONPROXY_H_

#include <string>

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/portallocator.h"

namespace cricket {
class PortAllocator;
class PortAllocatorSessionProxy;
class PortProxy;

// This class maintains the list of cricket::Port* objects. Ports will be
// deleted upon receiving SignalDestroyed signal. This class is used when
// PORTALLOCATOR_ENABLE_BUNDLE flag is set.

class PortAllocatorSessionMuxer : public rtc::MessageHandler,
                                  public sigslot::has_slots<> {
 public:
  explicit PortAllocatorSessionMuxer(PortAllocatorSession* session);
  virtual ~PortAllocatorSessionMuxer();

  void RegisterSessionProxy(PortAllocatorSessionProxy* session_proxy);

  void OnPortReady(PortAllocatorSession* session, PortInterface* port);
  void OnPortDestroyed(PortInterface* port);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);

  const std::vector<PortInterface*>& ports() { return ports_; }

  sigslot::signal1<PortAllocatorSessionMuxer*> SignalDestroyed;

 private:
  virtual void OnMessage(rtc::Message *pmsg);
  void OnSessionProxyDestroyed(PortAllocatorSession* proxy);
  void SendAllocationDone_w(PortAllocatorSessionProxy* proxy);
  void SendAllocatedPorts_w(PortAllocatorSessionProxy* proxy);

  // Port will be deleted when SignalDestroyed received, otherwise delete
  // happens when PortAllocatorSession dtor is called.
  rtc::Thread* worker_thread_;
  std::vector<PortInterface*> ports_;
  rtc::scoped_ptr<PortAllocatorSession> session_;
  std::vector<PortAllocatorSessionProxy*> session_proxies_;
  bool candidate_done_signal_received_;
};

class PortAllocatorSessionProxy : public PortAllocatorSession {
 public:
  PortAllocatorSessionProxy(const std::string& content_name,
                            int component,
                            uint32 flags)
        // Use empty string as the ufrag and pwd because the proxy always uses
        // the ufrag and pwd from the underlying implementation.
      : PortAllocatorSession(content_name, component, "", "", flags),
        impl_(NULL) {
  }

  virtual ~PortAllocatorSessionProxy();

  PortAllocatorSession* impl() { return impl_; }
  void set_impl(PortAllocatorSession* session);

  // Forwards call to the actual PortAllocatorSession.
  virtual void StartGettingPorts();
  virtual void StopGettingPorts();
  virtual bool IsGettingPorts();

  virtual void set_generation(uint32 generation) {
    ASSERT(impl_ != NULL);
    impl_->set_generation(generation);
  }

  virtual uint32 generation() {
    ASSERT(impl_ != NULL);
    return impl_->generation();
  }

 private:
  void OnPortReady(PortAllocatorSession* session, PortInterface* port);
  void OnCandidatesReady(PortAllocatorSession* session,
                         const std::vector<Candidate>& candidates);
  void OnPortDestroyed(PortInterface* port);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);

  // This is the actual PortAllocatorSession, owned by PortAllocator.
  PortAllocatorSession* impl_;
  std::map<PortInterface*, PortProxy*> proxy_ports_;

  friend class PortAllocatorSessionMuxer;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_PORTALLOCATORSESSIONPROXY_H_
