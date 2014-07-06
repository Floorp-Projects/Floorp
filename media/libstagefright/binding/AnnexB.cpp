/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/DecoderData.h"

using namespace mozilla;

namespace mp4_demuxer
{

static const uint8_t kAnnexBDelimiter[] = { 0, 0, 0, 1 };

void
AnnexB::ConvertSample(MP4Sample* aSample,
                      const mozilla::Vector<uint8_t>& annexB)
{
  MOZ_ASSERT(aSample);
  MOZ_ASSERT(aSample->data);
  MOZ_ASSERT(aSample->size >= ArrayLength(kAnnexBDelimiter));

  uint8_t* d = aSample->data;
  while (d + 4 < aSample->data + aSample->size) {
    uint32_t nalLen = (uint32_t(d[0]) << 24) +
                      (uint32_t(d[1]) << 16) +
                      (uint32_t(d[2]) << 8) +
                       uint32_t(d[3]);
    // Overwrite the NAL length with the Annex B separator.
    memcpy(d, kAnnexBDelimiter, ArrayLength(kAnnexBDelimiter));
    d += 4 + nalLen;
  }

  // Prepend the Annex B header with SPS and PPS tables to keyframes.
  if (aSample->is_sync_point) {
    aSample->Prepend(annexB.begin(), annexB.length());
  }
}

Vector<uint8_t>
AnnexB::ConvertExtraDataToAnnexB(mozilla::Vector<uint8_t>& aExtraData)
{
  // AVCC 6 byte header looks like:
  //     +------+------+------+------+------+------+------+------+
  // [0] |   0  |   0  |   0  |   0  |   0  |   0  |   0  |   1  |
  //     +------+------+------+------+------+------+------+------+
  // [1] | profile                                               |
  //     +------+------+------+------+------+------+------+------+
  // [2] | compatiblity                                          |
  //     +------+------+------+------+------+------+------+------+
  // [3] | level                                                 |
  //     +------+------+------+------+------+------+------+------+
  // [4] | unused                                  | nalLenSiz-1 |
  //     +------+------+------+------+------+------+------+------+
  // [5] | unused             | numSps                           |
  //     +------+------+------+------+------+------+------+------+

  Vector<uint8_t> annexB;

  ByteReader reader(aExtraData);
  const uint8_t* ptr = reader.Read(5);
  if (ptr && ptr[0] == 1) {
    // Append SPS then PPS
    ConvertSPSOrPPS(reader, reader.ReadU8() & 31, &annexB);
    ConvertSPSOrPPS(reader, reader.ReadU8(), &annexB);

    // MP4Box adds extra bytes that we ignore. I don't know what they do.
  }
  reader.DiscardRemaining();

  return annexB;
}

void
AnnexB::ConvertSPSOrPPS(ByteReader& aReader, uint8_t aCount,
                        Vector<uint8_t>* aAnnexB)
{
  for (int i = 0; i < aCount; i++) {
    uint16_t length = aReader.ReadU16();

    const uint8_t* ptr = aReader.Read(length);
    if (!ptr) {
      MOZ_ASSERT(false);
      return;
    }
    aAnnexB->append(kAnnexBDelimiter, ArrayLength(kAnnexBDelimiter));
    aAnnexB->append(ptr, length);
  }
}

} // namespace mp4_demuxer
