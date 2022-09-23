/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/ice_controller_interface.h"

#include <string>

#include "p2p/base/ice_switch_reason.h"

namespace cricket {

std::string IceRecheckEvent::ToString() const {
  std::string str = IceSwitchReasonToString(reason);
  if (recheck_delay_ms) {
    str += " (after delay: " + std::to_string(recheck_delay_ms) + ")";
  }
  return str;
}

// TODO(bugs.webrtc.org/14125) remove when Type is replaced with
// IceSwitchReason.
IceRecheckEvent::Type IceRecheckEvent::FromIceSwitchReason(
    IceSwitchReason reason) {
  switch (reason) {
    case IceSwitchReason::REMOTE_CANDIDATE_GENERATION_CHANGE:
      return IceRecheckEvent::REMOTE_CANDIDATE_GENERATION_CHANGE;
    case IceSwitchReason::NETWORK_PREFERENCE_CHANGE:
      return IceRecheckEvent::NETWORK_PREFERENCE_CHANGE;
    case IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE:
      return IceRecheckEvent::NEW_CONNECTION_FROM_LOCAL_CANDIDATE;
    case IceSwitchReason::NEW_CONNECTION_FROM_REMOTE_CANDIDATE:
      return IceRecheckEvent::NEW_CONNECTION_FROM_REMOTE_CANDIDATE;
    case IceSwitchReason::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS:
      return IceRecheckEvent::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS;
    case IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE:
      return IceRecheckEvent::NOMINATION_ON_CONTROLLED_SIDE;
    case IceSwitchReason::DATA_RECEIVED:
      return IceRecheckEvent::DATA_RECEIVED;
    case IceSwitchReason::CONNECT_STATE_CHANGE:
      return IceRecheckEvent::CONNECT_STATE_CHANGE;
    case IceSwitchReason::SELECTED_CONNECTION_DESTROYED:
      return IceRecheckEvent::SELECTED_CONNECTION_DESTROYED;
    case IceSwitchReason::ICE_CONTROLLER_RECHECK:
      return IceRecheckEvent::ICE_CONTROLLER_RECHECK;
  }
}

// TODO(bugs.webrtc.org/14125) remove when Type is replaced with
// IceSwitchReason.
IceSwitchReason IceRecheckEvent::FromType(IceRecheckEvent::Type type) {
  switch (type) {
    case IceRecheckEvent::REMOTE_CANDIDATE_GENERATION_CHANGE:
      return IceSwitchReason::REMOTE_CANDIDATE_GENERATION_CHANGE;
    case IceRecheckEvent::NETWORK_PREFERENCE_CHANGE:
      return IceSwitchReason::NETWORK_PREFERENCE_CHANGE;
    case IceRecheckEvent::NEW_CONNECTION_FROM_LOCAL_CANDIDATE:
      return IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE;
    case IceRecheckEvent::NEW_CONNECTION_FROM_REMOTE_CANDIDATE:
      return IceSwitchReason::NEW_CONNECTION_FROM_REMOTE_CANDIDATE;
    case IceRecheckEvent::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS:
      return IceSwitchReason::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS;
    case IceRecheckEvent::NOMINATION_ON_CONTROLLED_SIDE:
      return IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE;
    case IceRecheckEvent::DATA_RECEIVED:
      return IceSwitchReason::DATA_RECEIVED;
    case IceRecheckEvent::CONNECT_STATE_CHANGE:
      return IceSwitchReason::CONNECT_STATE_CHANGE;
    case IceRecheckEvent::SELECTED_CONNECTION_DESTROYED:
      return IceSwitchReason::SELECTED_CONNECTION_DESTROYED;
    case IceRecheckEvent::ICE_CONTROLLER_RECHECK:
      return IceSwitchReason::ICE_CONTROLLER_RECHECK;
  }
}

}  // namespace cricket
