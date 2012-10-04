/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_

#include <set>

#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class BitrateController;
struct OverUseDetectorOptions;
class ProcessThread;
class RemoteBitrateEstimator;
class RemoteBitrateObserver;
class ViEChannel;
class ViEEncoder;
class VieRemb;

// Channel group contains data common for several channels. All channels in the
// group are assumed to send/receive data to the same end-point.
class ChannelGroup {
 public:
  ChannelGroup(ProcessThread* process_thread,
               const OverUseDetectorOptions& options);
  ~ChannelGroup();

  void AddChannel(int channel_id);
  void RemoveChannel(int channel_id, unsigned int ssrc);
  bool HasChannel(int channel_id);
  bool Empty();

  bool SetChannelRembStatus(int channel_id,
                            bool sender,
                            bool receiver,
                            ViEChannel* channel,
                            ViEEncoder* encoder);

  BitrateController* GetBitrateController();
  RemoteBitrateEstimator* GetRemoteBitrateEstimator();

 private:
  typedef std::set<int> ChannelSet;

  scoped_ptr<VieRemb> remb_;
  scoped_ptr<BitrateController> bitrate_controller_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  ChannelSet channels_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_
