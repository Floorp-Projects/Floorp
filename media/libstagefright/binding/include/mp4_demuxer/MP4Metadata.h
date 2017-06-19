/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4METADATA_H_
#define MP4METADATA_H_

#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mp4_demuxer/DecoderData.h"
#include "mp4_demuxer/Index.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "Stream.h"
#include "mp4parse.h"

namespace mp4_demuxer {

class MP4MetadataStagefright;
class MP4MetadataRust;

class IndiceWrapper {
public:
  virtual size_t Length() const = 0;

  // TODO: Index::Indice is from stagefright, we should use another struct once
  //       stagefrigth is removed.
  virtual bool GetIndice(size_t aIndex, Index::Indice& aIndice) const = 0;

  virtual ~IndiceWrapper() {}
};

class MP4Metadata
{
public:
  explicit MP4Metadata(Stream* aSource);
  ~MP4Metadata();

  // Simple template class containing a MediaResult and another type.
  template <typename T>
  class ResultAndType
  {
  public:
    template <typename M2, typename T2>
    ResultAndType(M2&& aM, T2&& aT)
      : mResult(Forward<M2>(aM)), mT(Forward<T2>(aT))
    {
    }
    ResultAndType(const ResultAndType&) = default;
    ResultAndType& operator=(const ResultAndType&) = default;
    ResultAndType(ResultAndType&&) = default;
    ResultAndType& operator=(ResultAndType&&) = default;

    mozilla::MediaResult& Result() { return mResult; }
    T& Ref() { return mT; }

  private:
    mozilla::MediaResult mResult;
    typename mozilla::Decay<T>::Type mT;
  };

  using ResultAndByteBuffer = ResultAndType<RefPtr<mozilla::MediaByteBuffer>>;
  static ResultAndByteBuffer Metadata(Stream* aSource);

  static constexpr uint32_t NumberTracksError() { return UINT32_MAX; }
  using ResultAndTrackCount = ResultAndType<uint32_t>;
  ResultAndTrackCount GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;

  using ResultAndTrackInfo =
    ResultAndType<mozilla::UniquePtr<mozilla::TrackInfo>>;
  ResultAndTrackInfo GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                  size_t aTrackNumber) const;

  bool CanSeek() const;

  using ResultAndCryptoFile = ResultAndType<const CryptoFile*>;
  ResultAndCryptoFile Crypto() const;

  using ResultAndIndice = ResultAndType<mozilla::UniquePtr<IndiceWrapper>>;
  ResultAndIndice GetTrackIndice(mozilla::TrackID aTrackID);

private:
  UniquePtr<MP4MetadataStagefright> mStagefright;
  UniquePtr<MP4MetadataRust> mRust;
  mutable bool mDisableRust;
  mutable bool mReportedAudioTrackTelemetry;
  mutable bool mReportedVideoTrackTelemetry;
  bool ShouldPreferRust() const;
};

} // namespace mp4_demuxer

#endif // MP4METADATA_H_
