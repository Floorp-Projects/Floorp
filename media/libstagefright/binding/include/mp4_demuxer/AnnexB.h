/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_DEMUXER_ANNEX_B_H_
#define MP4_DEMUXER_ANNEX_B_H_

template <class T> struct already_AddRefed;

namespace mozilla {
class MediaRawData;
class MediaByteBuffer;
}

namespace mp4_demuxer
{
class BufferReader;

class AnnexB
{
public:
  // All conversions assume size of NAL length field is 4 bytes.
  // Convert a sample from AVCC format to Annex B.
  static mozilla::Result<mozilla::Ok, nsresult>
    ConvertSampleToAnnexB(mozilla::MediaRawData* aSample, bool aAddSPS = true);
  // Convert a sample from Annex B to AVCC.
  // an AVCC extradata must not be set.
  static bool ConvertSampleToAVCC(mozilla::MediaRawData* aSample);
  static mozilla::Result<mozilla::Ok, nsresult> ConvertSampleTo4BytesAVCC(mozilla::MediaRawData* aSample);

  // Parse an AVCC extradata and construct the Annex B sample header.
  static already_AddRefed<mozilla::MediaByteBuffer> ConvertExtraDataToAnnexB(
    const mozilla::MediaByteBuffer* aExtraData);
  // Returns true if format is AVCC and sample has valid extradata.
  static bool IsAVCC(const mozilla::MediaRawData* aSample);
  // Returns true if format is AnnexB.
  static bool IsAnnexB(const mozilla::MediaRawData* aSample);

private:
  // AVCC box parser helper.
  static mozilla::Result<mozilla::Ok, nsresult>
    ConvertSPSOrPPS(BufferReader& aReader, uint8_t aCount, mozilla::MediaByteBuffer* aAnnexB);
};

} // namespace mp4_demuxer

#endif // MP4_DEMUXER_ANNEX_B_H_
