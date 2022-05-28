/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEP_TRANSPORT_COLLECTION_H_
#define PC_JSEP_TRANSPORT_COLLECTION_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/sequence_checker.h"
#include "pc/jsep_transport.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

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
  BundleManager() {
    // Allow constructor to be called on a different thread.
    sequence_checker_.Detach();
  }
  const std::vector<std::unique_ptr<cricket::ContentGroup>>& bundle_groups()
      const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return bundle_groups_;
  }
  // Lookup a bundle group by a member mid name.
  const cricket::ContentGroup* LookupGroupByMid(const std::string& mid) const;
  cricket::ContentGroup* LookupGroupByMid(const std::string& mid);
  // Returns true if the MID is the first item of a group, or if
  // the MID is not a member of a group.
  bool IsFirstMidInGroup(const std::string& mid) const;
  // Update the groups description. This completely replaces the group
  // description with the one from the SessionDescription.
  void Update(const cricket::SessionDescription* description);
  // Delete a MID from the group that contains it.
  void DeleteMid(const cricket::ContentGroup* bundle_group,
                 const std::string& mid);
  // Delete a group.
  void DeleteGroup(const cricket::ContentGroup* bundle_group);

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  std::vector<std::unique_ptr<cricket::ContentGroup>> bundle_groups_
      RTC_GUARDED_BY(sequence_checker_);
  std::map<std::string, cricket::ContentGroup*>
      established_bundle_groups_by_mid_;
};

// This class keeps the mapping of MIDs to transports.
// It is pulled out here because a lot of the code that deals with
// bundles end up modifying this map, and the two need to be consistent;
// the managers may merge.
class JsepTransportCollection {
 public:
  JsepTransportCollection(std::function<bool(const std::string& mid,
                                             cricket::JsepTransport* transport)>
                              map_change_callback,
                          std::function<void()> state_change_callback)
      : map_change_callback_(map_change_callback),
        state_change_callback_(state_change_callback) {
    // Allow constructor to be called on a different thread.
    sequence_checker_.Detach();
  }

  void RegisterTransport(const std::string& mid,
                         std::unique_ptr<cricket::JsepTransport> transport);
  std::vector<cricket::JsepTransport*> Transports();
  void DestroyAllTransports();
  // Lookup a JsepTransport by the MID that was used to register it.
  cricket::JsepTransport* GetTransportByName(const std::string& mid);
  const cricket::JsepTransport* GetTransportByName(
      const std::string& mid) const;
  // Lookup a JsepTransport by any MID that refers to it.
  cricket::JsepTransport* GetTransportForMid(const std::string& mid);
  const cricket::JsepTransport* GetTransportForMid(
      const std::string& mid) const;
  // Set transport for a MID. This may destroy a transport if it is no
  // longer in use.
  bool SetTransportForMid(const std::string& mid,
                          cricket::JsepTransport* jsep_transport);
  // Remove a transport for a MID. This may destroy a transport if it is
  // no longer in use.
  void RemoveTransportForMid(const std::string& mid);
  // Roll back pending mid-to-transport mappings.
  void RollbackTransports();
  // Commit pending mid-transport mappings (rollback is no longer possible).
  void CommitTransports();
  // Returns true if any mid currently maps to this transport.
  bool TransportInUse(cricket::JsepTransport* jsep_transport) const;

 private:
  // Destroy a transport if it's no longer in use.
  void MaybeDestroyJsepTransport(cricket::JsepTransport* transport);

  bool IsConsistent();  // For testing only: Verify internal structure.

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  // This member owns the JSEP transports.
  std::map<std::string, std::unique_ptr<cricket::JsepTransport>>
      jsep_transports_by_name_ RTC_GUARDED_BY(sequence_checker_);

  // This keeps track of the mapping between media section
  // (BaseChannel/SctpTransport) and the JsepTransport underneath.
  std::map<std::string, cricket::JsepTransport*> mid_to_transport_
      RTC_GUARDED_BY(sequence_checker_);
  // Keep track of mids that have been mapped to transports. Used for rollback.
  std::vector<std::string> pending_mids_ RTC_GUARDED_BY(sequence_checker_);
  // Callback used to inform subscribers of altered transports.
  const std::function<bool(const std::string& mid,
                           cricket::JsepTransport* transport)>
      map_change_callback_;
  // Callback used to inform subscribers of possibly altered state.
  const std::function<void()> state_change_callback_;
};

}  // namespace webrtc

#endif  // PC_JSEP_TRANSPORT_COLLECTION_H_
