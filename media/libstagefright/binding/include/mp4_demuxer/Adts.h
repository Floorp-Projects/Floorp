/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ADTS_H_
#define ADTS_H_

#include <stdint.h>

namespace mp4_demuxer
{

class MP4Sample;

class Adts
{
public:
  static int8_t GetFrequencyIndex(uint16_t aSamplesPerSecond);
  static bool ConvertEsdsToAdts(uint16_t aChannelCount, int8_t aFrequencyIndex,
                                int8_t aProfile, MP4Sample* aSample);
};
}

#endif
