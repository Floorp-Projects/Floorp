/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_MEDIA_FACTORY_H_
#define PC_MEDIA_FACTORY_H_

#include <memory>

#include "api/call/call_factory_interface.h"
#include "call/call.h"
#include "call/call_config.h"
#include "media/base/media_engine.h"

namespace webrtc {

// PeerConnectionFactoryDependencies is forward declared because of circular
// dependency between MediaFactory and PeerConnectionFactoryDependencies:
// PeerConnectionFactoryDependencies keeps an instance of MediaFactory and thus
// needs to know how to destroy it.
// MediaFactory mentions PeerConnectionFactoryDependencies in api, but does not
// need its full definition.
struct PeerConnectionFactoryDependencies;

// Interface repsponsible for constructing media specific classes for
// PeerConnectionFactory and PeerConnection.
// TODO(bugs.webrtc.org/15574): Delete CallFactoryInterface inheritance
// when call_factory is removed from PeerConnectionFactoryDependencies.
class MediaFactory : public CallFactoryInterface {
 public:
  virtual ~MediaFactory() = default;

  std::unique_ptr<Call> CreateCall(const CallConfig& config) override = 0;
  virtual std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryDependencies& dependencies) = 0;
};

}  // namespace webrtc

#endif  // PC_MEDIA_FACTORY_H_
