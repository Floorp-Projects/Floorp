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
class ByteReader;

class AnnexB
{
public:
  // All conversions assume size of NAL length field is 4 bytes.
  // Convert a sample from AVCC format to Annex B.
  static bool ConvertSampleToAnnexB(mozilla::MediaRawData* aSample, bool aAddSPS = true);
  // Convert a sample from Annex B to AVCC.
  // an AVCC extradata must not be set.
  static bool ConvertSampleToAVCC(mozilla::MediaRawData* aSample);
  static bool ConvertSampleTo4BytesAVCC(mozilla::MediaRawData* aSample);

  // Parse an AVCC extradata and construct the Annex B sample header.
  static already_AddRefed<mozilla::MediaByteBuffer> ConvertExtraDataToAnnexB(
    const mozilla::MediaByteBuffer* aExtraData);
  // Extract SPS and PPS NALs from aSample, aSample must be in AVCC format.
  // If aSample already contains an extradata with an SPS, it will be returned
  // otherwise the SPS/PPS NALs are searched in-band.
  static already_AddRefed<mozilla::MediaByteBuffer> ExtractExtraData(
    const mozilla::MediaRawData* aSample);
  static bool HasSPS(const mozilla::MediaRawData* aSample);
  static bool HasSPS(const mozilla::MediaByteBuffer* aExtraData);
  // Returns true if format is AVCC and sample has valid extradata.
  static bool IsAVCC(const mozilla::MediaRawData* aSample);
  // Returns true if format is AnnexB.
  static bool IsAnnexB(const mozilla::MediaRawData* aSample);
  // Return true if both extradata are equal.
  static bool CompareExtraData(const mozilla::MediaByteBuffer* aExtraData1,
                               const mozilla::MediaByteBuffer* aExtraData2);

private:
  // AVCC box parser helper.
  static void ConvertSPSOrPPS(ByteReader& aReader, uint8_t aCount,
                              mozilla::MediaByteBuffer* aAnnexB);
};

} // namespace mp4_demuxer

#endif // MP4_DEMUXER_ANNEX_B_H_
