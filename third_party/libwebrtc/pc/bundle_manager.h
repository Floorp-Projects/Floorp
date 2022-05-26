/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_BUNDLE_MANAGER_H_
#define PC_BUNDLE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "pc/session_description.h"

namespace webrtc {
// This class manages information about RFC 8843 BUNDLE bundles
// in SDP descriptions.

// This is a work-in-progress. Planned steps:
// 1) Move all Bundle-related data structures from JsepTransport
//    into this class.
// 2) Move all Bundle-related functions into this class.
// 3) Move remaining Bundle-related logic into this class.
//    Make data members private.
// 4) Refine interface to have comprehensible semantics.
// 5) Add unit tests.
// 6) Change the logic to do what's right.
class BundleManager {
 public:
  const std::vector<std::unique_ptr<cricket::ContentGroup>>& bundle_groups()
      const {
    return bundle_groups_;
  }
  // Update the groups description. This completely replaces the group
  // description with the one from the SessionDescription.
  void Update(const cricket::SessionDescription* description);
  // Delete a MID from the group that contains it.
  void DeleteMid(const cricket::ContentGroup* bundle_group,
                 const std::string& mid);
  // Delete a group.
  void DeleteGroup(const cricket::ContentGroup* bundle_group);

 private:
  std::vector<std::unique_ptr<cricket::ContentGroup>> bundle_groups_;
};

}  // namespace webrtc

#endif  // PC_BUNDLE_MANAGER_H_
