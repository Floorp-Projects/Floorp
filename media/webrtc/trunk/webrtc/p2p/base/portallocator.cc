/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/portallocator.h"

#include "webrtc/p2p/base/portallocatorsessionproxy.h"

namespace cricket {

PortAllocatorSession::PortAllocatorSession(const std::string& content_name,
                                           int component,
                                           const std::string& ice_ufrag,
                                           const std::string& ice_pwd,
                                           uint32 flags)
    : content_name_(content_name),
      component_(component),
      flags_(flags),
      generation_(0),
      // If PORTALLOCATOR_ENABLE_SHARED_UFRAG flag is not enabled, ignore the
      // incoming ufrag and pwd, which will cause each Port to generate one
      // by itself.
      username_(flags_ & PORTALLOCATOR_ENABLE_SHARED_UFRAG ? ice_ufrag : ""),
      password_(flags_ & PORTALLOCATOR_ENABLE_SHARED_UFRAG ? ice_pwd : "") {
  // If bundle is enabled, shared ufrag must be enabled too.
  ASSERT((!(flags_ & PORTALLOCATOR_ENABLE_BUNDLE)) ||
         (flags_ & PORTALLOCATOR_ENABLE_SHARED_UFRAG));
}

PortAllocator::~PortAllocator() {
  for (SessionMuxerMap::iterator iter = muxers_.begin();
       iter != muxers_.end(); ++iter) {
    delete iter->second;
  }
}

PortAllocatorSession* PortAllocator::CreateSession(
    const std::string& sid,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd) {
  if (flags_ & PORTALLOCATOR_ENABLE_BUNDLE) {
    // If we just use |sid| as key in identifying PortAllocatorSessionMuxer,
    // ICE restart will not result in different candidates, as |sid| will
    // be same. To yield different candiates we are using combination of
    // |ice_ufrag| and |ice_pwd|.
    // Ideally |ice_ufrag| and |ice_pwd| should change together, but
    // there can be instances where only ice_pwd will be changed.
    std::string key_str = ice_ufrag + ":" + ice_pwd;
    PortAllocatorSessionMuxer* muxer = GetSessionMuxer(key_str);
    if (!muxer) {
      PortAllocatorSession* session_impl = CreateSessionInternal(
          content_name, component, ice_ufrag, ice_pwd);
      // Create PortAllocatorSessionMuxer object for |session_impl|.
      muxer = new PortAllocatorSessionMuxer(session_impl);
      muxer->SignalDestroyed.connect(
          this, &PortAllocator::OnSessionMuxerDestroyed);
      // Add PortAllocatorSession to the map.
      muxers_[key_str] = muxer;
    }
    PortAllocatorSessionProxy* proxy =
        new PortAllocatorSessionProxy(content_name, component, flags_);
    muxer->RegisterSessionProxy(proxy);
    return proxy;
  }
  return CreateSessionInternal(content_name, component, ice_ufrag, ice_pwd);
}

PortAllocatorSessionMuxer* PortAllocator::GetSessionMuxer(
    const std::string& key) const {
  SessionMuxerMap::const_iterator iter = muxers_.find(key);
  if (iter != muxers_.end())
    return iter->second;
  return NULL;
}

void PortAllocator::OnSessionMuxerDestroyed(
    PortAllocatorSessionMuxer* session) {
  SessionMuxerMap::iterator iter;
  for (iter = muxers_.begin(); iter != muxers_.end(); ++iter) {
    if (iter->second == session)
      break;
  }
  if (iter != muxers_.end())
    muxers_.erase(iter);
}

}  // namespace cricket
