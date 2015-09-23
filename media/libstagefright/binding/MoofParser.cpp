/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mp4_demuxer/MoofParser.h"
#include "mp4_demuxer/Box.h"
#include "mp4_demuxer/SinfParser.h"
#include <limits>
#include "Intervals.h"

#include "mozilla/Logging.h"

#if defined(MOZ_FMP4)
extern PRLogModuleInfo* GetDemuxerLog();

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOG(name, arg, ...) MOZ_LOG(GetDemuxerLog(), mozilla::LogLevel::Debug, (TOSTRING(name) "(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mp4_demuxer
{

using namespace stagefright;
using namespace mozilla;

bool
MoofParser::RebuildFragmentedIndex(
  const nsTArray<mozilla::MediaByteRange>& aByteRanges)
{
  BoxContext context(mSource, aByteRanges);
  return RebuildFragmentedIndex(context);
}

bool
MoofParser::RebuildFragmentedIndex(BoxContext& aContext)
{
  bool foundValidMoof = false;
  bool foundMdat = false;

  for (Box box(&aContext, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moov") && mInitRange.IsNull()) {
      mInitRange = MediaByteRange(0, box.Range().mEnd);
      ParseMoov(box);
    } else if (box.IsType("moof")) {
      Moof moof(box, mTrex, mMvhd, mMdhd, mEdts, mSinf, mIsAudio);

      if (!moof.IsValid() && !box.Next().IsAvailable()) {
        // Moof isn't valid abort search for now.
        break;
      }

      if (!mMoofs.IsEmpty()) {
        // Stitch time ranges together in the case of a (hopefully small) time
        // range gap between moofs.
        mMoofs.LastElement().FixRounding(moof);
      }

      mMoofs.AppendElement(moof);
      mMediaRanges.AppendElement(moof.mRange);
      foundValidMoof = true;
    } else if (box.IsType("mdat") && !Moofs().IsEmpty()) {
      // Check if we have all our data from last moof.
      Moof& moof = Moofs().LastElement();
      media::Interval<int64_t> datarange(moof.mMdatRange.mStart, moof.mMdatRange.mEnd, 0);
      media::Interval<int64_t> mdat(box.Range().mStart, box.Range().mEnd, 0);
      if (datarange.Intersects(mdat)) {
        mMediaRanges.LastElement() =
          mMediaRanges.LastElement().Extents(box.Range());
      }
    }
    mOffset = box.NextOffset();
  }
  return foundValidMoof;
}

MediaByteRange
MoofParser::FirstCompleteMediaHeader()
{
  if (Moofs().IsEmpty()) {
    return MediaByteRange();
  }
  return Moofs()[0].mRange;
}

MediaByteRange
MoofParser::FirstCompleteMediaSegment()
{
  for (uint32_t i = 0 ; i < mMediaRanges.Length(); i++) {
    if (mMediaRanges[i].Contains(Moofs()[i].mMdatRange)) {
      return mMediaRanges[i];
    }
  }
  return MediaByteRange();
}

class BlockingStream : public Stream {
public:
  explicit BlockingStream(Stream* aStream) : mStream(aStream)
  {
  }

  bool ReadAt(int64_t offset, void* data, size_t size, size_t* bytes_read)
    override
  {
    return mStream->ReadAt(offset, data, size, bytes_read);
  }

  bool CachedReadAt(int64_t offset, void* data, size_t size, size_t* bytes_read)
    override
  {
    return mStream->ReadAt(offset, data, size, bytes_read);
  }

  virtual bool Length(int64_t* size) override
  {
    return mStream->Length(size);
  }

private:
  nsRefPtr<Stream> mStream;
};

bool
MoofParser::BlockingReadNextMoof()
{
  int64_t length = std::numeric_limits<int64_t>::max();
  mSource->Length(&length);
  nsTArray<MediaByteRange> byteRanges;
  byteRanges.AppendElement(MediaByteRange(0, length));
  nsRefPtr<mp4_demuxer::BlockingStream> stream = new BlockingStream(mSource);

  BoxContext context(stream, byteRanges);
  for (Box box(&context, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("moof")) {
      byteRanges.Clear();
      byteRanges.AppendElement(MediaByteRange(mOffset, box.Range().mEnd));
      return RebuildFragmentedIndex(context);
    }
  }
  return false;
}

void
MoofParser::ScanForMetadata(mozilla::MediaByteRange& aFtyp,
                            mozilla::MediaByteRange& aMoov)
{
  int64_t length = std::numeric_limits<int64_t>::max();
  mSource->Length(&length);
  nsTArray<MediaByteRange> byteRanges;
  byteRanges.AppendElement(MediaByteRange(0, length));
  nsRefPtr<mp4_demuxer::BlockingStream> stream = new BlockingStream(mSource);

  BoxContext context(stream, byteRanges);
  for (Box box(&context, mOffset); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("ftyp")) {
      aFtyp = box.Range();
      continue;
    }
    if (box.IsType("moov")) {
      aMoov = box.Range();
      break;
    }
  }
  mInitRange = aFtyp.Extents(aMoov);
}

bool
MoofParser::HasMetadata()
{
  MediaByteRange ftyp;
  MediaByteRange moov;
  ScanForMetadata(ftyp, moov);
  return !!ftyp.Length() && !!moov.Length();
}

already_AddRefed<mozilla::MediaByteBuffer>
MoofParser::Metadata()
{
  MediaByteRange ftyp;
  MediaByteRange moov;
  ScanForMetadata(ftyp, moov);
  if (!ftyp.Length() || !moov.Length()) {
    return nullptr;
  }
  nsRefPtr<MediaByteBuffer> metadata = new MediaByteBuffer();
  if (!metadata->SetLength(ftyp.Length() + moov.Length(), fallible)) {
    // OOM
    return nullptr;
  }

  nsRefPtr<mp4_demuxer::BlockingStream> stream = new BlockingStream(mSource);
  size_t read;
  bool rv =
    stream->ReadAt(ftyp.mStart, metadata->Elements(), ftyp.Length(), &read);
  if (!rv || read != ftyp.Length()) {
    return nullptr;
  }
  rv =
    stream->ReadAt(moov.mStart, metadata->Elements() + ftyp.Length(), moov.Length(), &read);
  if (!rv || read != moov.Length()) {
    return nullptr;
  }
  return metadata.forget();
}

Interval<Microseconds>
MoofParser::GetCompositionRange(const nsTArray<MediaByteRange>& aByteRanges)
{
  Interval<Microseconds> compositionRange;
  BoxContext context(mSource, aByteRanges);
  for (size_t i = 0; i < mMoofs.Length(); i++) {
    Moof& moof = mMoofs[i];
    Box box(&context, moof.mRange.mStart);
    if (box.IsAvailable()) {
      compositionRange = compositionRange.Extents(moof.mTimeRange);
    }
  }
  return compositionRange;
}

bool
MoofParser::ReachedEnd()
{
  int64_t length;
  return mSource->Length(&length) && mOffset == length;
}

void
MoofParser::ParseMoov(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mvhd")) {
      mMvhd = Mvhd(box);
    } else if (box.IsType("trak")) {
      ParseTrak(box);
    } else if (box.IsType("mvex")) {
      ParseMvex(box);
    }
  }
}

void
MoofParser::ParseTrak(Box& aBox)
{
  Tkhd tkhd;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tkhd")) {
      tkhd = Tkhd(box);
    } else if (box.IsType("mdia")) {
      if (!mTrex.mTrackId || tkhd.mTrackId == mTrex.mTrackId) {
        ParseMdia(box, tkhd);
      }
    } else if (box.IsType("edts") &&
               (!mTrex.mTrackId || tkhd.mTrackId == mTrex.mTrackId)) {
      mEdts = Edts(box);
    }
  }
}

void
MoofParser::ParseMdia(Box& aBox, Tkhd& aTkhd)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("mdhd")) {
      mMdhd = Mdhd(box);
    } else if (box.IsType("minf")) {
      ParseMinf(box);
    }
  }
}

void
MoofParser::ParseMvex(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trex")) {
      Trex trex = Trex(box);
      if (!mTrex.mTrackId || trex.mTrackId == mTrex.mTrackId) {
        auto trackId = mTrex.mTrackId;
        mTrex = trex;
        // Keep the original trackId, as should it be 0 we want to continue
        // parsing all tracks.
        mTrex.mTrackId = trackId;
      }
    }
  }
}

void
MoofParser::ParseMinf(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("stbl")) {
      ParseStbl(box);
    }
  }
}

void
MoofParser::ParseStbl(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("stsd")) {
      ParseStsd(box);
    }
  }
}

void
MoofParser::ParseStsd(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("encv") || box.IsType("enca")) {
      ParseEncrypted(box);
    }
  }
}

void
MoofParser::ParseEncrypted(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    // Some MP4 files have been found to have multiple sinf boxes in the same
    // enc* box. This does not match spec anyway, so just choose the first
    // one that parses properly.
    if (box.IsType("sinf")) {
      mSinf = Sinf(box);

      if (mSinf.IsValid()) {
        break;
      }
    }
  }
}

Moof::Moof(Box& aBox, Trex& aTrex, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, Sinf& aSinf, bool aIsAudio)
  : mRange(aBox.Range())
  , mMaxRoundingError(35000)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("traf")) {
      ParseTraf(box, aTrex, aMvhd, aMdhd, aEdts, aSinf, aIsAudio);
    }
  }
  if (IsValid()) {
    ProcessCenc();
  }
}

bool
Moof::GetAuxInfo(AtomType aType, nsTArray<MediaByteRange>* aByteRanges)
{
  aByteRanges->Clear();

  Saiz* saiz = nullptr;
  for (int i = 0; ; i++) {
    if (i == mSaizs.Length()) {
      return false;
    }
    if (mSaizs[i].mAuxInfoType == aType) {
      saiz = &mSaizs[i];
      break;
    }
  }
  Saio* saio = nullptr;
  for (int i = 0; ; i++) {
    if (i == mSaios.Length()) {
      return false;
    }
    if (mSaios[i].mAuxInfoType == aType) {
      saio = &mSaios[i];
      break;
    }
  }

  if (saio->mOffsets.Length() == 1) {
    aByteRanges->SetCapacity(saiz->mSampleInfoSize.Length());
    uint64_t offset = mRange.mStart + saio->mOffsets[0];
    for (size_t i = 0; i < saiz->mSampleInfoSize.Length(); i++) {
      aByteRanges->AppendElement(
        MediaByteRange(offset, offset + saiz->mSampleInfoSize[i]));
      offset += saiz->mSampleInfoSize[i];
    }
    return true;
  }

  if (saio->mOffsets.Length() == saiz->mSampleInfoSize.Length()) {
    aByteRanges->SetCapacity(saiz->mSampleInfoSize.Length());
    for (size_t i = 0; i < saio->mOffsets.Length(); i++) {
      uint64_t offset = mRange.mStart + saio->mOffsets[i];
      aByteRanges->AppendElement(
        MediaByteRange(offset, offset + saiz->mSampleInfoSize[i]));
    }
    return true;
  }

  return false;
}

bool
Moof::ProcessCenc()
{
  nsTArray<MediaByteRange> cencRanges;
  if (!GetAuxInfo(AtomType("cenc"), &cencRanges) ||
      cencRanges.Length() != mIndex.Length()) {
    return false;
  }
  for (int i = 0; i < cencRanges.Length(); i++) {
    mIndex[i].mCencRange = cencRanges[i];
  }
  return true;
}

void
Moof::ParseTraf(Box& aBox, Trex& aTrex, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, Sinf& aSinf, bool aIsAudio)
{
  Tfhd tfhd(aTrex);
  Tfdt tfdt;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tfhd")) {
      tfhd = Tfhd(box, aTrex);
    } else if (!aTrex.mTrackId || tfhd.mTrackId == aTrex.mTrackId) {
      if (box.IsType("tfdt")) {
        tfdt = Tfdt(box);
      } else if (box.IsType("saiz")) {
        mSaizs.AppendElement(Saiz(box, aSinf.mDefaultEncryptionType));
      } else if (box.IsType("saio")) {
        mSaios.AppendElement(Saio(box, aSinf.mDefaultEncryptionType));
      }
    }
  }
  if (aTrex.mTrackId && tfhd.mTrackId != aTrex.mTrackId) {
    return;
  }
  if (!tfdt.IsValid()) {
    LOG(Moof, "Invalid tfdt dependency");
    return;
  }
  // Now search for TRUN boxes.
  uint64_t decodeTime = tfdt.mBaseMediaDecodeTime;
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("trun")) {
      if (ParseTrun(box, tfhd, aMvhd, aMdhd, aEdts, &decodeTime, aIsAudio)) {
        mValid = true;
      } else {
        mValid = false;
        break;
      }
    }
  }
}

void
Moof::FixRounding(const Moof& aMoof) {
  Microseconds gap = aMoof.mTimeRange.start - mTimeRange.end;
  if (gap > 0 && gap <= mMaxRoundingError) {
    mTimeRange.end = aMoof.mTimeRange.start;
  }
}

class CtsComparator
{
public:
  bool Equals(Sample* const aA, Sample* const aB) const
  {
    return aA->mCompositionRange.start == aB->mCompositionRange.start;
  }
  bool
  LessThan(Sample* const aA, Sample* const aB) const
  {
    return aA->mCompositionRange.start < aB->mCompositionRange.start;
  }
};

bool
Moof::ParseTrun(Box& aBox, Tfhd& aTfhd, Mvhd& aMvhd, Mdhd& aMdhd, Edts& aEdts, uint64_t* aDecodeTime, bool aIsAudio)
{
  if (!aTfhd.IsValid() || !aMvhd.IsValid() || !aMdhd.IsValid() ||
      !aEdts.IsValid()) {
    LOG(Moof, "Invalid dependencies: aTfhd(%d) aMvhd(%d) aMdhd(%d) aEdts(%d)",
        aTfhd.IsValid(), aMvhd.IsValid(), aMdhd.IsValid(), !aEdts.IsValid());
    return false;
  }

  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Moof, "Incomplete Box (missing flags)");
    return false;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;

  if (!reader->CanReadType<uint32_t>()) {
    LOG(Moof, "Incomplete Box (missing sampleCount)");
    return false;
  }
  uint32_t sampleCount = reader->ReadU32();
  if (sampleCount == 0) {
    return true;
  }

  size_t need =
    ((flags & 1) ? sizeof(uint32_t) : 0) +
    ((flags & 4) ? sizeof(uint32_t) : 0);
  uint16_t flag[] = { 0x100, 0x200, 0x400, 0x800, 0 };
  for (size_t i = 0; flag[i]; i++) {
    if (flags & flag[i]) {
      need += sizeof(uint32_t) * sampleCount;
    }
  }
  if (reader->Remaining() < need) {
    LOG(Moof, "Incomplete Box (have:%lld need:%lld)",
        reader->Remaining(), need);
    return false;
  }

  uint64_t offset = aTfhd.mBaseDataOffset + (flags & 1 ? reader->ReadU32() : 0);
  uint32_t firstSampleFlags =
    flags & 4 ? reader->ReadU32() : aTfhd.mDefaultSampleFlags;
  uint64_t decodeTime = *aDecodeTime;
  nsTArray<Interval<Microseconds>> timeRanges;

  if (!mIndex.SetCapacity(sampleCount, fallible)) {
    LOG(Moof, "Out of Memory");
    return false;
  }

  for (size_t i = 0; i < sampleCount; i++) {
    uint32_t sampleDuration =
      flags & 0x100 ? reader->ReadU32() : aTfhd.mDefaultSampleDuration;
    uint32_t sampleSize =
      flags & 0x200 ? reader->ReadU32() : aTfhd.mDefaultSampleSize;
    uint32_t sampleFlags =
      flags & 0x400 ? reader->ReadU32()
                    : i ? aTfhd.mDefaultSampleFlags : firstSampleFlags;
    int32_t ctsOffset = 0;
    if (flags & 0x800) {
      ctsOffset = reader->Read32();
    }

    Sample sample;
    sample.mByteRange = MediaByteRange(offset, offset + sampleSize);
    offset += sampleSize;

    sample.mDecodeTime =
      aMdhd.ToMicroseconds((int64_t)decodeTime - aEdts.mMediaStart) + aMvhd.ToMicroseconds(aEdts.mEmptyOffset);
    sample.mCompositionRange = Interval<Microseconds>(
      aMdhd.ToMicroseconds((int64_t)decodeTime + ctsOffset - aEdts.mMediaStart) + aMvhd.ToMicroseconds(aEdts.mEmptyOffset),
      aMdhd.ToMicroseconds((int64_t)decodeTime + ctsOffset + sampleDuration - aEdts.mMediaStart) + aMvhd.ToMicroseconds(aEdts.mEmptyOffset));
    decodeTime += sampleDuration;

    // Sometimes audio streams don't properly mark their samples as keyframes,
    // because every audio sample is a keyframe.
    sample.mSync = !(sampleFlags & 0x1010000) || aIsAudio;

    // FIXME: Make this infallible after bug 968520 is done.
    MOZ_ALWAYS_TRUE(mIndex.AppendElement(sample, fallible));

    mMdatRange = mMdatRange.Extents(sample.mByteRange);
  }
  mMaxRoundingError += aMdhd.ToMicroseconds(sampleCount);

  nsTArray<Sample*> ctsOrder;
  for (int i = 0; i < mIndex.Length(); i++) {
    ctsOrder.AppendElement(&mIndex[i]);
  }
  ctsOrder.Sort(CtsComparator());

  for (size_t i = 0; i < ctsOrder.Length(); i++) {
    if (i + 1 < ctsOrder.Length()) {
      ctsOrder[i]->mCompositionRange.end = ctsOrder[i + 1]->mCompositionRange.start;
    }
  }
  mTimeRange = Interval<Microseconds>(ctsOrder[0]->mCompositionRange.start,
      ctsOrder.LastElement()->mCompositionRange.end);
  *aDecodeTime = decodeTime;
  return true;
}

Tkhd::Tkhd(Box& aBox)
{
  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Tkhd, "Incomplete Box (missing flags)");
    return;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  size_t need =
    3*(version ? sizeof(int64_t) : sizeof(int32_t)) + 2*sizeof(int32_t);
  if (reader->Remaining() < need) {
    LOG(Tkhd, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  if (version == 0) {
    mCreationTime = reader->ReadU32();
    mModificationTime = reader->ReadU32();
    mTrackId = reader->ReadU32();
    uint32_t reserved = reader->ReadU32();
    NS_ASSERTION(!reserved, "reserved should be 0");
    mDuration = reader->ReadU32();
  } else if (version == 1) {
    mCreationTime = reader->ReadU64();
    mModificationTime = reader->ReadU64();
    mTrackId = reader->ReadU32();
    uint32_t reserved = reader->ReadU32();
    NS_ASSERTION(!reserved, "reserved should be 0");
    mDuration = reader->ReadU64();
  }
  // More stuff that we don't care about
  reader->DiscardRemaining();
  mValid = true;
}

Mvhd::Mvhd(Box& aBox)
{
  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Mdhd, "Incomplete Box (missing flags)");
    return;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  size_t need =
    3*(version ? sizeof(int64_t) : sizeof(int32_t)) + sizeof(uint32_t);
  if (reader->Remaining() < need) {
    LOG(Mvhd, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }

  if (version == 0) {
    mCreationTime = reader->ReadU32();
    mModificationTime = reader->ReadU32();
    mTimescale = reader->ReadU32();
    mDuration = reader->ReadU32();
  } else if (version == 1) {
    mCreationTime = reader->ReadU64();
    mModificationTime = reader->ReadU64();
    mTimescale = reader->ReadU32();
    mDuration = reader->ReadU64();
  } else {
    reader->DiscardRemaining();
    return;
  }
  // More stuff that we don't care about
  reader->DiscardRemaining();
  if (mTimescale) {
    mValid = true;
  }
}

Mdhd::Mdhd(Box& aBox)
  : Mvhd(aBox)
{
}

Trex::Trex(Box& aBox)
{
  BoxReader reader(aBox);
  if (reader->Remaining() < 6*sizeof(uint32_t)) {
    LOG(Trex, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)6*sizeof(uint32_t));
    return;
  }
  mFlags = reader->ReadU32();
  mTrackId = reader->ReadU32();
  mDefaultSampleDescriptionIndex = reader->ReadU32();
  mDefaultSampleDuration = reader->ReadU32();
  mDefaultSampleSize = reader->ReadU32();
  mDefaultSampleFlags = reader->ReadU32();
  mValid = true;
}

Tfhd::Tfhd(Box& aBox, Trex& aTrex)
  : Trex(aTrex)
{
  MOZ_ASSERT(aBox.IsType("tfhd"));
  MOZ_ASSERT(aBox.Parent()->IsType("traf"));
  MOZ_ASSERT(aBox.Parent()->Parent()->IsType("moof"));

  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Tfhd, "Incomplete Box (missing flags)");
    return;
  }
  mFlags = reader->ReadU32();
  size_t need = sizeof(uint32_t) /* trackid */;
  uint8_t flag[] = { 1, 2, 8, 0x10, 0x20, 0 };
  for (size_t i = 0; flag[i]; i++) {
    if (mFlags & flag[i]) {
      need += sizeof(uint32_t);
    }
  }
  if (reader->Remaining() < need) {
    LOG(Tfhd, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  mBaseDataOffset =
    mFlags & 1 ? reader->ReadU32() : aBox.Parent()->Parent()->Offset();
  mTrackId = reader->ReadU32();
  if (mFlags & 2) {
    mDefaultSampleDescriptionIndex = reader->ReadU32();
  }
  if (mFlags & 8) {
    mDefaultSampleDuration = reader->ReadU32();
  }
  if (mFlags & 0x10) {
    mDefaultSampleSize = reader->ReadU32();
  }
  if (mFlags & 0x20) {
    mDefaultSampleFlags = reader->ReadU32();
  }
  mValid = true;
}

Tfdt::Tfdt(Box& aBox)
{
  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Tfdt, "Incomplete Box (missing flags)");
    return;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  size_t need = version ? sizeof(uint64_t) : sizeof(uint32_t) ;
  if (reader->Remaining() < need) {
    LOG(Tfdt, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  if (version == 0) {
    mBaseMediaDecodeTime = reader->ReadU32();
  } else if (version == 1) {
    mBaseMediaDecodeTime = reader->ReadU64();
  }
  reader->DiscardRemaining();
  mValid = true;
}

Edts::Edts(Box& aBox)
  : mMediaStart(0)
  , mEmptyOffset(0)
{
  Box child = aBox.FirstChild();
  if (!child.IsType("elst")) {
    return;
  }

  BoxReader reader(child);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Edts, "Incomplete Box (missing flags)");
    return;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  size_t need =
    sizeof(uint32_t) + 2*(version ? sizeof(int64_t) : sizeof(uint32_t));
  if (reader->Remaining() < need) {
    LOG(Edts, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  bool emptyEntry = false;
  uint32_t entryCount = reader->ReadU32();
  for (uint32_t i = 0; i < entryCount; i++) {
    uint64_t segment_duration;
    int64_t media_time;
    if (version == 1) {
      segment_duration = reader->ReadU64();
      media_time = reader->Read64();
    } else {
      segment_duration = reader->ReadU32();
      media_time = reader->Read32();
    }
    if (media_time == -1 && i) {
      LOG(Edts, "Multiple empty edit, not handled");
    } else if (media_time == -1) {
      mEmptyOffset = segment_duration;
      emptyEntry = true;
    } else if (i > 1 || (i > 0 && !emptyEntry)) {
      LOG(Edts, "More than one edit entry, not handled. A/V sync will be wrong");
      break;
    } else {
      mMediaStart = media_time;
    }
    reader->ReadU32(); // media_rate_integer and media_rate_fraction
  }
}

Saiz::Saiz(Box& aBox, AtomType aDefaultType)
  : mAuxInfoType(aDefaultType)
  , mAuxInfoTypeParameter(0)
{
  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Saiz, "Incomplete Box (missing flags)");
    return;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  size_t need =
    ((flags & 1) ? 2*sizeof(uint32_t) : 0) + sizeof(uint8_t) + sizeof(uint32_t);
  if (reader->Remaining() < need) {
    LOG(Saiz, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  if (flags & 1) {
    mAuxInfoType = reader->ReadU32();
    mAuxInfoTypeParameter = reader->ReadU32();
  }
  uint8_t defaultSampleInfoSize = reader->ReadU8();
  uint32_t count = reader->ReadU32();
  if (defaultSampleInfoSize) {
    for (int i = 0; i < count; i++) {
      mSampleInfoSize.AppendElement(defaultSampleInfoSize);
    }
  } else {
    if (!reader->ReadArray(mSampleInfoSize, count)) {
      LOG(Saiz, "Incomplete Box (missing count:%u)", count);
      return;
    }
  }
  mValid = true;
}

Saio::Saio(Box& aBox, AtomType aDefaultType)
  : mAuxInfoType(aDefaultType)
  , mAuxInfoTypeParameter(0)
{
  BoxReader reader(aBox);
  if (!reader->CanReadType<uint32_t>()) {
    LOG(Saio, "Incomplete Box (missing flags)");
    return;
  }
  uint32_t flags = reader->ReadU32();
  uint8_t version = flags >> 24;
  size_t need = ((flags & 1) ? (2*sizeof(uint32_t)) : 0) + sizeof(uint32_t);
  if (reader->Remaining() < need) {
    LOG(Saio, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  if (flags & 1) {
    mAuxInfoType = reader->ReadU32();
    mAuxInfoTypeParameter = reader->ReadU32();
  }
  size_t count = reader->ReadU32();
  need = (version ? sizeof(uint64_t) : sizeof(uint32_t)) * count;
  if (reader->Remaining() < count) {
    LOG(Saio, "Incomplete Box (have:%lld need:%lld)",
        (uint64_t)reader->Remaining(), (uint64_t)need);
    return;
  }
  mOffsets.SetCapacity(count);
  if (version == 0) {
    for (size_t i = 0; i < count; i++) {
      mOffsets.AppendElement(reader->ReadU32());
    }
  } else {
    for (size_t i = 0; i < count; i++) {
      mOffsets.AppendElement(reader->ReadU64());
    }
  }
  mValid = true;
}

#undef LOG
}
