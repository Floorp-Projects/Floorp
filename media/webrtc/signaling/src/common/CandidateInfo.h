/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CANDIDATE_INFO_H__
#define _CANDIDATE_INFO_H__

#include <string>
#include <cstdint>

namespace mozilla {

// This is used both by IPDL code, and by signaling code.
struct CandidateInfo {
  std::string mCandidate;
  std::string mUfrag;
  std::string mDefaultHostRtp;
  uint16_t mDefaultPortRtp = 0;
  std::string mDefaultHostRtcp;
  uint16_t mDefaultPortRtcp = 0;
};

}  // namespace mozilla

#endif  //_CANDIDATE_INFO_H__
