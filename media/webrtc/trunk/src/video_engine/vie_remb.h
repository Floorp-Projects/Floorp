/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// 1. Register a RtpRtcp module to include in the REMB packet.
// 2. When UpdateBitrateEstimate is called for the first time for a SSRC, add it
//    to the map.
// 3. Send a new REMB every kRembSendIntervallMs or if a lower bitrate estimate
//    for a specified SSRC.


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_REMB_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_REMB_H_

#include <list>
#include <map>

#include "modules/interface/module.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;
class ProcessThread;
class RtpRtcp;

class VieRemb : public RtpRemoteBitrateObserver, public Module {
 public:
  VieRemb(ProcessThread* process_thread);
  ~VieRemb();

  // Called to add a receive channel to include in the REMB packet.
  void AddReceiveChannel(RtpRtcp* rtp_rtcp);

  // Removes the specified channel from REMB estimate.
  void RemoveReceiveChannel(RtpRtcp* rtp_rtcp);

  // Called to add a module that can generate and send REMB RTCP.
  void AddRembSender(RtpRtcp* rtp_rtcp);

  // Removes a REMB RTCP sender.
  void RemoveRembSender(RtpRtcp* rtp_rtcp);

  // Called to add a send channel encoding and sending data, affected by
  // received  REMB packets.
  void AddSendChannel(RtpRtcp* rtp_rtcp);

  // Removes the specified channel from receiving REMB packet estimates.
  void RemoveSendChannel(RtpRtcp* rtp_rtcp);

  // Returns true if the instance is in use, false otherwise.
  bool InUse() const;

  // Called every time there is a new bitrate estimate for the received stream
  // with given SSRC. This call will trigger a new RTCP REMB packet if the
  // bitrate estimate has decreased or if no RTCP REMB packet has been sent for
  // a certain time interval.
  // Implements RtpReceiveBitrateUpdate.
  virtual void OnReceiveBitrateChanged(unsigned int ssrc, unsigned int bitrate);

  // Called for every new receive REMB packet and distributes the estmate
  // between all sending modules.
  virtual void OnReceivedRemb(unsigned int bitrate);

  // Implements Module.
  virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);
  virtual WebRtc_Word32 TimeUntilNextProcess();
  virtual WebRtc_Word32 Process();

 private:
  typedef std::list<RtpRtcp*> RtpModules;
  typedef std::map<unsigned int, std::pair<int64_t, unsigned int> >
      SsrcTimeBitrate;

  ProcessThread* process_thread_;
  scoped_ptr<CriticalSectionWrapper> list_crit_;

  // The last time a REMB was sent.
  int64_t last_remb_time_;
  int last_send_bitrate_;

  // All RtpRtcp modules to include in the REMB packet.
  RtpModules receive_modules_;

  // All modules encoding and sending data.
  RtpModules send_modules_;

  // All modules that can send REMB RTCP.
  RtpModules rtcp_sender_;

  // The last bitrate update for each SSRC.
  SsrcTimeBitrate update_time_bitrates_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_REMB_H_
