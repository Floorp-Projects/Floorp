/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Empty test just to get code coverage metrics for this dir.
 */
#include "udp_transport.h"
#include "gtest/gtest.h"
// We include the implementation header file to get at the dependency-injecting
// constructor.
#include "udp_transport_impl.h"

TEST(UDPTransportTest, CreateTransport) {
  WebRtc_Word32 id = 0;
  WebRtc_UWord8 threads = 0;
  webrtc::UdpTransport* transport = webrtc::UdpTransport::Create(id, threads);
  webrtc::UdpTransport::Destroy(transport);
}

// This test verifies that the mock_socket is not called from the constructor.
TEST(UDPTransportTest, ConstructorDoesNotCreateSocket) {
  WebRtc_Word32 id = 0;
  WebRtc_UWord8 threads = 0;
  webrtc::UdpTransportImpl::SocketMaker* null_maker = NULL;
  webrtc::UdpTransport* transport = new webrtc::UdpTransportImpl(id, threads,
                                                                 null_maker);
  webrtc::UdpTransport::Destroy(transport);
}
