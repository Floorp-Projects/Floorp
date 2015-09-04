/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaData.h"
#include "mp4_demuxer/BufferStream.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/MoofParser.h"

using namespace mozilla;
using namespace mp4_demuxer;

TEST(stagefright_MP4Metadata, EmptyStream)
{
  nsRefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(0);
  nsRefPtr<BufferStream> stream = new BufferStream(buffer);

  EXPECT_FALSE(MP4Metadata::HasCompleteMetadata(stream));
  nsRefPtr<MediaByteBuffer> metadataBuffer = MP4Metadata::Metadata(stream);
  EXPECT_FALSE(metadataBuffer);

  MP4Metadata metadata(stream);
  EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kUndefinedTrack));
  EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kAudioTrack));
  EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kVideoTrack));
  EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kTextTrack));
  EXPECT_EQ(0u, metadata.GetNumberTracks(static_cast<TrackInfo::TrackType>(-1)));
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kUndefinedTrack, 0));
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kAudioTrack, 0));
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0));
  EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kTextTrack, 0));
  EXPECT_FALSE(metadata.GetTrackInfo(static_cast<TrackInfo::TrackType>(-1), 0));
  // We can seek anywhere in any MPEG4.
  EXPECT_TRUE(metadata.CanSeek());
  EXPECT_FALSE(metadata.Crypto().valid);
}

TEST(stagefright_MoofParser, EmptyStream)
{
  nsRefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(0);
  nsRefPtr<BufferStream> stream = new BufferStream(buffer);

  mozilla::Monitor monitor("MP4Metadata::gtest");
  mozilla::MonitorAutoLock mon(monitor);
  MoofParser parser(stream, 0, false, &monitor);
  EXPECT_EQ(0u, parser.mOffset);
  EXPECT_TRUE(parser.ReachedEnd());

  nsTArray<MediaByteRange> byteRanges;
  byteRanges.AppendElement(MediaByteRange(0, 0));
  EXPECT_FALSE(parser.RebuildFragmentedIndex(byteRanges));

  EXPECT_TRUE(parser.GetCompositionRange(byteRanges).IsNull());
  EXPECT_TRUE(parser.mInitRange.IsNull());
  EXPECT_EQ(0u, parser.mOffset);
  EXPECT_TRUE(parser.ReachedEnd());
  EXPECT_FALSE(parser.HasMetadata());
  nsRefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
  EXPECT_FALSE(metadataBuffer);
  EXPECT_TRUE(parser.FirstCompleteMediaSegment().IsNull());
  EXPECT_TRUE(parser.FirstCompleteMediaHeader().IsNull());
}
