/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EndianUtils.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/ByteReader.h"
#include "mp4_demuxer/ByteWriter.h"
#include "MediaData.h"
#include "nsAutoPtr.h"

using namespace mozilla;

namespace mp4_demuxer
{

static const uint8_t kAnnexBDelimiter[] = { 0, 0, 0, 1 };

bool
AnnexB::ConvertSampleToAnnexB(mozilla::MediaRawData* aSample, bool aAddSPS)
{
  MOZ_ASSERT(aSample);

  if (!IsAVCC(aSample)) {
    return true;
  }
  MOZ_ASSERT(aSample->Data());

  if (!ConvertSampleTo4BytesAVCC(aSample)) {
    return false;
  }

  if (aSample->Size() < 4) {
    // Nothing to do, it's corrupted anyway.
    return true;
  }

  ByteReader reader(aSample->Data(), aSample->Size());

  mozilla::Vector<uint8_t> tmp;
  ByteWriter writer(tmp);

  while (reader.Remaining() >= 4) {
    uint32_t nalLen = reader.ReadU32();
    const uint8_t* p = reader.Read(nalLen);

    if (!writer.Write(kAnnexBDelimiter, ArrayLength(kAnnexBDelimiter))) {
      return false;
    }
    if (!p) {
      break;
    }
    if (!writer.Write(p, nalLen)) {
      return false;
    }
  }

  nsAutoPtr<MediaRawDataWriter> samplewriter(aSample->CreateWriter());

  if (!samplewriter->Replace(tmp.begin(), tmp.length())) {
    return false;
  }

  // Prepend the Annex B NAL with SPS and PPS tables to keyframes.
  if (aAddSPS && aSample->mKeyframe) {
    RefPtr<MediaByteBuffer> annexB =
      ConvertExtraDataToAnnexB(aSample->mExtraData);
    if (!samplewriter->Prepend(annexB->Elements(), annexB->Length())) {
      return false;
    }

    // Prepending the NAL with SPS/PPS will mess up the encryption subsample
    // offsets. So we need to account for the extra bytes by increasing
    // the length of the first clear data subsample. Otherwise decryption
    // will fail.
    if (aSample->mCrypto.mValid) {
      MOZ_ASSERT(samplewriter->mCrypto.mPlainSizes.Length() > 0);
      samplewriter->mCrypto.mPlainSizes[0] += annexB->Length();
    }
  }

  return true;
}

already_AddRefed<mozilla::MediaByteBuffer>
AnnexB::ConvertExtraDataToAnnexB(const mozilla::MediaByteBuffer* aExtraData)
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

  RefPtr<mozilla::MediaByteBuffer> annexB = new mozilla::MediaByteBuffer;

  ByteReader reader(*aExtraData);
  const uint8_t* ptr = reader.Read(5);
  if (ptr && ptr[0] == 1) {
    // Append SPS then PPS
    ConvertSPSOrPPS(reader, reader.ReadU8() & 31, annexB);
    ConvertSPSOrPPS(reader, reader.ReadU8(), annexB);

    // MP4Box adds extra bytes that we ignore. I don't know what they do.
  }

  return annexB.forget();
}

void
AnnexB::ConvertSPSOrPPS(ByteReader& aReader, uint8_t aCount,
                        mozilla::MediaByteBuffer* aAnnexB)
{
  for (int i = 0; i < aCount; i++) {
    uint16_t length = aReader.ReadU16();

    const uint8_t* ptr = aReader.Read(length);
    if (!ptr) {
      MOZ_ASSERT(false);
      return;
    }
    aAnnexB->AppendElements(kAnnexBDelimiter, ArrayLength(kAnnexBDelimiter));
    aAnnexB->AppendElements(ptr, length);
  }
}

static bool
FindStartCodeInternal(ByteReader& aBr) {
  size_t offset = aBr.Offset();

  for (uint32_t i = 0; i < aBr.Align() && aBr.Remaining() >= 3; i++) {
    if (aBr.PeekU24() == 0x000001) {
      return true;
    }
    aBr.Read(1);
  }

  while (aBr.Remaining() >= 6) {
    uint32_t x32 = aBr.PeekU32();
    if ((x32 - 0x01010101) & (~x32) & 0x80808080) {
      if ((x32 >> 8) == 0x000001) {
        return true;
      }
      if (x32 == 0x000001) {
        aBr.Read(1);
        return true;
      }
      if ((x32 & 0xff) == 0) {
        const uint8_t* p = aBr.Peek(1);
        if ((x32 & 0xff00) == 0 && p[4] == 1) {
          aBr.Read(2);
          return true;
        }
        if (p[4] == 0 && p[5] == 1) {
          aBr.Read(3);
          return true;
        }
      }
    }
    aBr.Read(4);
  }

  while (aBr.Remaining() >= 3) {
    if (aBr.PeekU24() == 0x000001) {
      return true;
    }
    aBr.Read(1);
  }

  // No start code were found; Go back to the beginning.
  aBr.Seek(offset);
  return false;
}

static bool
FindStartCode(ByteReader& aBr, size_t& aStartSize)
{
  if (!FindStartCodeInternal(aBr)) {
    aStartSize = 0;
    return false;
  }

  aStartSize = 3;
  if (aBr.Offset()) {
    // Check if it's 4-bytes start code
    aBr.Rewind(1);
    if (aBr.ReadU8() == 0) {
      aStartSize = 4;
    }
  }
  aBr.Read(3);
  return true;
}

static bool
ParseNALUnits(ByteWriter& aBw, ByteReader& aBr)
{
  size_t startSize;

  bool rv = FindStartCode(aBr, startSize);
  if (rv) {
    size_t startOffset = aBr.Offset();
    while (FindStartCode(aBr, startSize)) {
      size_t offset = aBr.Offset();
      size_t sizeNAL = offset - startOffset - startSize;
      aBr.Seek(startOffset);
      if (!aBw.WriteU32(sizeNAL)
          || !aBw.Write(aBr.Read(sizeNAL), sizeNAL)) {
        return false;
      }
      aBr.Read(startSize);
      startOffset = offset;
    }
  }
  size_t sizeNAL = aBr.Remaining();
  if (sizeNAL) {
    if (!aBw.WriteU32(sizeNAL)
        || !aBw.Write(aBr.Read(sizeNAL), sizeNAL)) {
      return false;
    }
  }
  return true;
}

bool
AnnexB::ConvertSampleToAVCC(mozilla::MediaRawData* aSample)
{
  if (IsAVCC(aSample)) {
    return ConvertSampleTo4BytesAVCC(aSample);
  }
  if (!IsAnnexB(aSample)) {
    // Not AnnexB, nothing to convert.
    return true;
  }

  mozilla::Vector<uint8_t> nalu;
  ByteWriter writer(nalu);
  ByteReader reader(aSample->Data(), aSample->Size());

  if (!ParseNALUnits(writer, reader)) {
    return false;
  }
  nsAutoPtr<MediaRawDataWriter> samplewriter(aSample->CreateWriter());
  if (!samplewriter->Replace(nalu.begin(), nalu.length())) {
    return false;
  }
  // Create the AVCC header.
  RefPtr<mozilla::MediaByteBuffer> extradata = new mozilla::MediaByteBuffer;
  static const uint8_t kFakeExtraData[] = {
    1 /* version */,
    0x64 /* profile (High) */,
    0 /* profile compat (0) */,
    40 /* level (40) */,
    0xfc | 3 /* nal size - 1 */,
    0xe0 /* num SPS (0) */,
    0 /* num PPS (0) */
  };
  if (!extradata->AppendElements(kFakeExtraData, ArrayLength(kFakeExtraData))) {
    return false;
  }
  aSample->mExtraData = extradata;
  return true;
}

already_AddRefed<mozilla::MediaByteBuffer>
AnnexB::ExtractExtraData(const mozilla::MediaRawData* aSample)
{
  MOZ_ASSERT(IsAVCC(aSample));

  RefPtr<mozilla::MediaByteBuffer> extradata = new mozilla::MediaByteBuffer;

  // SPS content
  mozilla::Vector<uint8_t> sps;
  ByteWriter spsw(sps);
  int numSps = 0;
  // PPS content
  mozilla::Vector<uint8_t> pps;
  ByteWriter ppsw(pps);
  int numPps = 0;

  int nalLenSize = ((*aSample->mExtraData)[4] & 3) + 1;
  ByteReader reader(aSample->Data(), aSample->Size());

  // Find SPS and PPS NALUs in AVCC data
  while (reader.Remaining() > nalLenSize) {
    uint32_t nalLen;
    switch (nalLenSize) {
      case 1: nalLen = reader.ReadU8();  break;
      case 2: nalLen = reader.ReadU16(); break;
      case 3: nalLen = reader.ReadU24(); break;
      case 4: nalLen = reader.ReadU32(); break;
    }
    uint8_t nalType = reader.PeekU8() & 0x1f;
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return extradata.forget();
    }

    if (nalType == 0x7) { /* SPS */
      numSps++;
      if (!spsw.WriteU16(nalLen)
          || !spsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    } else if (nalType == 0x8) { /* PPS */
      numPps++;
      if (!ppsw.WriteU16(nalLen)
          || !ppsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    }
  }

  if (numSps && sps.length() > 5) {
    extradata->AppendElement(1);        // version
    extradata->AppendElement(sps[3]);   // profile
    extradata->AppendElement(sps[4]);   // profile compat
    extradata->AppendElement(sps[5]);   // level
    extradata->AppendElement(0xfc | 3); // nal size - 1
    extradata->AppendElement(0xe0 | numSps);
    extradata->AppendElements(sps.begin(), sps.length());
    extradata->AppendElement(numPps);
    if (numPps) {
      extradata->AppendElements(pps.begin(), pps.length());
    }
  }

  return extradata.forget();
}

bool
AnnexB::HasSPS(const mozilla::MediaRawData* aSample)
{
  return HasSPS(aSample->mExtraData);
}

bool
AnnexB::HasSPS(const mozilla::MediaByteBuffer* aExtraData)
{
  if (!aExtraData) {
    return false;
  }

  ByteReader reader(aExtraData);
  const uint8_t* ptr = reader.Read(5);
  if (!ptr || !reader.CanRead8()) {
    return false;
  }
  uint8_t numSps = reader.ReadU8() & 0x1f;

  return numSps > 0;
}

bool
AnnexB::ConvertSampleTo4BytesAVCC(mozilla::MediaRawData* aSample)
{
  MOZ_ASSERT(IsAVCC(aSample));

  int nalLenSize = ((*aSample->mExtraData)[4] & 3) + 1;

  if (nalLenSize == 4) {
    return true;
  }
  mozilla::Vector<uint8_t> dest;
  ByteWriter writer(dest);
  ByteReader reader(aSample->Data(), aSample->Size());
  while (reader.Remaining() > nalLenSize) {
    uint32_t nalLen;
    switch (nalLenSize) {
      case 1: nalLen = reader.ReadU8();  break;
      case 2: nalLen = reader.ReadU16(); break;
      case 3: nalLen = reader.ReadU24(); break;
      case 4: nalLen = reader.ReadU32(); break;
    }
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return true;
    }
    if (!writer.WriteU32(nalLen)
        || !writer.Write(p, nalLen)) {
      return false;
    }
  }
  nsAutoPtr<MediaRawDataWriter> samplewriter(aSample->CreateWriter());
  return samplewriter->Replace(dest.begin(), dest.length());
}

bool
AnnexB::IsAVCC(const mozilla::MediaRawData* aSample)
{
  return aSample->Size() >= 3 && aSample->mExtraData &&
    aSample->mExtraData->Length() >= 7 && (*aSample->mExtraData)[0] == 1;
}

bool
AnnexB::IsAnnexB(const mozilla::MediaRawData* aSample)
{
  if (aSample->Size() < 4) {
    return false;
  }
  uint32_t header = mozilla::BigEndian::readUint32(aSample->Data());
  return header == 0x00000001 || (header >> 8) == 0x000001;
}

bool
AnnexB::CompareExtraData(const mozilla::MediaByteBuffer* aExtraData1,
                         const mozilla::MediaByteBuffer* aExtraData2)
{
  // Very crude comparison.
  return aExtraData1 == aExtraData2 || *aExtraData1 == *aExtraData2;
}

} // namespace mp4_demuxer
