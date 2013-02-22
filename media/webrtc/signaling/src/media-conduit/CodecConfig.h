
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CODEC_CONFIG_H_
#define CODEC_CONFIG_H_

#include <string>


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

  /* When we have resolution negotiation information (RFC 6236)
   * it will be stored here.
   */

  VideoCodecConfig(int type, std::string name): mType(type),
                                                mName(name)
  {
  }

};
}
#endif
