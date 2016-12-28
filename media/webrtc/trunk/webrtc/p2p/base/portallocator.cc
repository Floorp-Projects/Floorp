/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/p2p/base/portallocator.h"

namespace cricket {

PortAllocatorSession::PortAllocatorSession(const std::string& content_name,
                                           int component,
                                           const std::string& ice_ufrag,
                                           const std::string& ice_pwd,
                                           uint32_t flags)
    : content_name_(content_name),
      component_(component),
      flags_(flags),
      generation_(0),
      ice_ufrag_(ice_ufrag),
      ice_pwd_(ice_pwd) {
  RTC_DCHECK(!ice_ufrag.empty());
  RTC_DCHECK(!ice_pwd.empty());
}

PortAllocatorSession* PortAllocator::CreateSession(
    const std::string& sid,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd) {
  return CreateSessionInternal(content_name, component, ice_ufrag, ice_pwd);
}

}  // namespace cricket
