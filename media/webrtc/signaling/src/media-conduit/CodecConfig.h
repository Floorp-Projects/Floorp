
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
                            int channels, int rate): mType(type),
                                                     mName(name),
                                                     mFreq(freq),
                                                     mPacSize(pacSize),
                                                     mChannels(channels),
                                                     mRate(rate)

  {
  }
};

/*
 * Minimalisitc video codec configuration
 * More to be added later depending on the use-case
 */

struct VideoCodecConfig
{
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::VideoCodec data-types.
   */
  int mType;
  std::string mName;
  uint32_t mRtcpFbTypes;

  VideoCodecConfig(int type,
                   std::string name,
                   int rtcpFbTypes): mType(type),
                                     mName(name),
                                     mRtcpFbTypes(rtcpFbTypes)
  {
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
