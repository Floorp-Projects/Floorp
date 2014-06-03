/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/Adts.h"
#include "mp4_demuxer/DecoderData.h"
#include "media/stagefright/MediaBuffer.h"
#include "mozilla/Array.h"
#include "mozilla/ArrayUtils.h"

using namespace mozilla;

namespace mp4_demuxer
{

int8_t
Adts::GetFrequencyIndex(uint16_t aSamplesPerSecond)
{
  static const int freq_lookup[] = { 96000, 88200, 64000, 48000, 44100,
                                     32000, 24000, 22050, 16000, 12000,
                                     11025, 8000,  7350,  0 };

  int8_t i = 0;
  while (aSamplesPerSecond < freq_lookup[i]) {
    i++;
  }

  if (!freq_lookup[i]) {
    return -1;
  }

  return i;
}

bool
Adts::ConvertEsdsToAdts(uint16_t aChannelCount, int8_t aFrequencyIndex,
                        int8_t aProfile, MP4Sample* aSample)
{
  static const int kADTSHeaderSize = 7;

  size_t newSize = aSample->size + kADTSHeaderSize;

  // ADTS header uses 13 bits for packet size.
  if (newSize >= (1 << 13) || aChannelCount > 15 ||
      aFrequencyIndex < 0 || aProfile < 1 || aProfile > 4)
    return false;

  Array<uint8_t, kADTSHeaderSize> header;
  header[0] = 0xff;
  header[1] = 0xf1;
  header[2] =
    ((aProfile - 1) << 6) + (aFrequencyIndex << 2) + (aChannelCount >> 2);
  header[3] = ((aChannelCount & 0x3) << 6) + (newSize >> 11);
  header[4] = (newSize & 0x7ff) >> 3;
  header[5] = ((newSize & 7) << 5) + 0x1f;
  header[6] = 0xfc;

  aSample->Prepend(&header[0], ArrayLength(header));

  return true;
}
}
