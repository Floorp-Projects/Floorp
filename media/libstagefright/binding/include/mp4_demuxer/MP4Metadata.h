/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4METADATA_H_
#define MP4METADATA_H_

#include "mozilla/UniquePtr.h"
#include "mp4_demuxer/DecoderData.h"
#include "mp4_demuxer/Index.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "Stream.h"

namespace mp4_demuxer
{

class MP4MetadataStagefright;
class MP4MetadataRust;

class MP4Metadata
{
public:
  explicit MP4Metadata(Stream* aSource);
  ~MP4Metadata();

  static bool HasCompleteMetadata(Stream* aSource);
  static already_AddRefed<mozilla::MediaByteBuffer> Metadata(Stream* aSource);
  uint32_t GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  mozilla::UniquePtr<mozilla::TrackInfo> GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                                      size_t aTrackNumber) const;
  bool CanSeek() const;

  const CryptoFile& Crypto() const;

  bool ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID);

private:
  UniquePtr<MP4MetadataStagefright> mStagefright;
#ifdef MOZ_RUST_MP4PARSE
  UniquePtr<MP4MetadataRust> mRust;
  mutable bool mPreferRust;
  mutable bool mReportedAudioTrackTelemetry;
  mutable bool mReportedVideoTrackTelemetry;
  bool ShouldPreferRust() const;
#endif
};

} // namespace mp4_demuxer

#endif // MP4METADATA_H_
