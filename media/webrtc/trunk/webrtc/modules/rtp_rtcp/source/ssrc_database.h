/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_

#include <set>

#include "webrtc/base/random.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/include/static_instance.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;

class SSRCDatabase {
 public:
  static SSRCDatabase* GetSSRCDatabase();
  static void ReturnSSRCDatabase();

  uint32_t CreateSSRC();
  void RegisterSSRC(uint32_t ssrc);
  void ReturnSSRC(uint32_t ssrc);

  SSRCDatabase();
  virtual ~SSRCDatabase();

 protected:
  static SSRCDatabase* CreateInstance() { return new SSRCDatabase(); }

 private:
  // Friend function to allow the SSRC destructor to be accessed from the
  // template class.
  friend SSRCDatabase* GetStaticInstance<SSRCDatabase>(
      CountOperation count_operation);

  rtc::scoped_ptr<CriticalSectionWrapper> crit_;
  Random random_ GUARDED_BY(crit_);
  std::set<uint32_t> ssrcs_ GUARDED_BY(crit_);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
