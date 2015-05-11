/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4METADATA_H_
#define MP4METADATA_H_

#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"
#include "mp4_demuxer/Index.h"
#include "mp4_demuxer/DecoderData.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "MediaInfo.h"
#include "MediaResource.h"

namespace mozilla { class MediaByteRange; }

namespace stagefright { class MetaData; }

namespace mp4_demuxer
{

struct StageFrightPrivate;

class MP4Metadata
{
public:
  explicit MP4Metadata(Stream* aSource);
  ~MP4Metadata();

  static bool HasCompleteMetadata(Stream* aSource);
  static mozilla::MediaByteRange MetadataRange(Stream* aSource);
  uint32_t GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  mozilla::UniquePtr<mozilla::TrackInfo> GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                                      size_t aTrackNumber) const;
  bool CanSeek() const;

  const CryptoFile& Crypto() const
  {
    return mCrypto;
  }

  bool ReadTrackIndex(nsTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID);

private:
  int32_t GetTrackNumber(mozilla::TrackID aTrackID);
  void UpdateCrypto(const stagefright::MetaData* aMetaData);
  nsAutoPtr<StageFrightPrivate> mPrivate;
  CryptoFile mCrypto;
  nsRefPtr<Stream> mSource;
};

} // namespace mp4_demuxer

#endif // MP4METADATA_H_
