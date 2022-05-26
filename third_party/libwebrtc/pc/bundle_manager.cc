/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/bundle_manager.h"

namespace webrtc {

void BundleManager::Update(const cricket::SessionDescription* description) {
  bundle_groups_.clear();
  for (const cricket::ContentGroup* new_bundle_group :
       description->GetGroupsByName(cricket::GROUP_TYPE_BUNDLE)) {
    bundle_groups_.push_back(
        std::make_unique<cricket::ContentGroup>(*new_bundle_group));
  }
}

void BundleManager::DeleteMid(const cricket::ContentGroup* bundle_group,
                              const std::string& mid) {
  // Remove the rejected content from the |bundle_group|.
  // The const pointer arg is used to identify the group, we verify
  // it before we use it to make a modification.
  auto bundle_group_it = std::find_if(
      bundle_groups_.begin(), bundle_groups_.end(),
      [bundle_group](std::unique_ptr<cricket::ContentGroup>& group) {
        return bundle_group == group.get();
      });
  RTC_DCHECK(bundle_group_it != bundle_groups_.end());
  (*bundle_group_it)->RemoveContentName(mid);
}

void BundleManager::DeleteGroup(const cricket::ContentGroup* bundle_group) {
  // Delete the BUNDLE group.
  auto bundle_group_it = std::find_if(
      bundle_groups_.begin(), bundle_groups_.end(),
      [bundle_group](std::unique_ptr<cricket::ContentGroup>& group) {
        return bundle_group == group.get();
      });
  RTC_DCHECK(bundle_group_it != bundle_groups_.end());
  bundle_groups_.erase(bundle_group_it);
}

}  // namespace webrtc
