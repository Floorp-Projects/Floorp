
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CODEC_CONFIG_H_
#define CODEC_CONFIG_H_

#include <string>
#include "ccsdp_rtcp_fb.h"

namespace mozilla {

/**
 * Minimalistic Audio Codec Config Params
 */
struct AudioCodecConfig
{
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::CodecInst data-types.
   */
  int mType;
  std::string mName;
  int mFreq;
  int mPacSize;
  int mChannels;
  int mRate;

  /* Default constructor is not provided since as a consumer, we
   * can't decide the default configuration for the codec
   */
  explicit AudioCodecConfig(int type, std::string name,
                            int freq,int pacSize,
                            int channels, int rate)
                                                   : mType(type),
                                                     mName(name),
                                                     mFreq(freq),
                                                     mPacSize(pacSize),
                                                     mChannels(channels),
                                                     mRate(rate)

  {
  }
};

/*
 * Minimalistic video codec configuration
 * More to be added later depending on the use-case
 */

#define    MAX_SPROP_LEN    128

// used for holding SDP negotiation results
struct VideoCodecConfigH264
{
    char       sprop_parameter_sets[MAX_SPROP_LEN];
    int        packetization_mode;
    int        profile_level_id;
    int        max_mbps;
    int        max_fs;
    int        max_cpb;
    int        max_dpb;
    int        max_br;
    int        tias_bw;
};


// class so the std::strings can get freed more easily/reliably
class VideoCodecConfig
{
public:
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::VideoCodec data-types.
   */
  int mType; // payload type
  std::string mName;
  uint32_t mRtcpFbTypes;
  unsigned int mMaxFrameSize;
  unsigned int mMaxFrameRate;
  unsigned int mMaxMBPS;    // in macroblocks-per-second
  unsigned int mMaxBitrate;
  // max_cpb & max_dpb would be streaming/mode-2 only
  std::string mSpropParameterSets;
  uint8_t mProfile;
  uint8_t mConstraints;
  uint8_t mLevel;
  uint8_t mPacketizationMode;
  // TODO: add external negotiated SPS/PPS

  VideoCodecConfig(int type,
                   std::string name,
                   int rtcpFbTypes,
                   unsigned int max_fs = 0,
                   unsigned int max_fr = 0,
                   const struct VideoCodecConfigH264 *h264 = nullptr) :
    mType(type),
    mName(name),
    mRtcpFbTypes(rtcpFbTypes),
    mMaxFrameSize(max_fs), // may be overridden
    mMaxFrameRate(max_fr),
    mMaxMBPS(0),
    mMaxBitrate(0),
    mProfile(0x42),
    mConstraints(0xE0),
    mLevel(0x0C),
    mPacketizationMode(1)
  {
    if (h264) {
      if (max_fs == 0 || (h264->max_fs != 0 && (unsigned int) h264->max_fs < max_fs)) {
        mMaxFrameSize = h264->max_fs;
      }
      mMaxMBPS = h264->max_mbps;
      mMaxBitrate = h264->max_br;
      mProfile = (h264->profile_level_id & 0x00FF0000) >> 16;
      mConstraints = (h264->profile_level_id & 0x0000FF00) >> 8;
      mLevel = (h264->profile_level_id & 0x000000FF);
      mPacketizationMode = h264->packetization_mode;
      mSpropParameterSets = h264->sprop_parameter_sets;
    }
  }

  bool RtcpFbIsSet(sdp_rtcp_fb_nack_type_e type) const
  {
    return mRtcpFbTypes & sdp_rtcp_fb_nack_to_bitmap(type);
  }

  bool RtcpFbIsSet(sdp_rtcp_fb_ack_type_e type) const
  {
    return mRtcpFbTypes & sdp_rtcp_fb_ack_to_bitmap(type);
  }

  bool RtcpFbIsSet(sdp_rtcp_fb_ccm_type_e type) const
  {
    return mRtcpFbTypes & sdp_rtcp_fb_ccm_to_bitmap(type);
  }
};
}
#endif
