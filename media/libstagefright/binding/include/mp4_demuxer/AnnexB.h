/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_ANNEX_B_H_
#define MP4_DEMUXER_ANNEX_B_H_

#include "mozilla/Vector.h"

namespace mp4_demuxer
{
class ByteReader;
class MP4Sample;

class AnnexB
{
public:
  // Convert a sample from NAL unit syntax to Annex B.
  // Assumes size of NAL length field is 4 bytes.
  static void ConvertSample(MP4Sample* aSample,
                            const mozilla::Vector<uint8_t>& annexB);

  // Parse an AVCC box and construct the Annex B sample header.
  static mozilla::Vector<uint8_t> ConvertExtraDataToAnnexB(
    mozilla::Vector<uint8_t>& aExtraData);

private:
  // AVCC box parser helper.
  static void ConvertSPSOrPPS(ByteReader& aReader, uint8_t aCount,
                              mozilla::Vector<uint8_t>* aAnnexB);
};

} // namespace mp4_demuxer

#endif // MP4_DEMUXER_ANNEX_B_H_
