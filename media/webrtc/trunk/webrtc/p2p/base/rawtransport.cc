/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/p2p/base/rawtransport.h"
#include "webrtc/p2p/base/rawtransportchannel.h"
#include "webrtc/base/common.h"

#if defined(FEATURE_ENABLE_PSTN)
namespace cricket {

RawTransport::RawTransport(rtc::Thread* signaling_thread,
                           rtc::Thread* worker_thread,
                           const std::string& content_name,
                           PortAllocator* allocator)
    : Transport(signaling_thread, worker_thread,
                content_name, NS_GINGLE_RAW, allocator) {
}

RawTransport::~RawTransport() {
  DestroyAllChannels();
}

TransportChannelImpl* RawTransport::CreateTransportChannel(int component) {
  return new RawTransportChannel(content_name(), component, this,
                                 worker_thread(),
                                 port_allocator());
}

void RawTransport::DestroyTransportChannel(TransportChannelImpl* channel) {
  delete channel;
}

}  // namespace cricket
#endif  // defined(FEATURE_ENABLE_PSTN)
