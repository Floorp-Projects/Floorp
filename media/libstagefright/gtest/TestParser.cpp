/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaData.h"
#include "mozilla/ArrayUtils.h"
#include "mp4_demuxer/BufferStream.h"
#include "mp4_demuxer/MP4Metadata.h"
#include "mp4_demuxer/MoofParser.h"

using namespace mozilla;
using namespace mp4_demuxer;

class TestStream : public Stream
{
public:
  TestStream(const uint8_t* aBuffer, size_t aSize)
    : mHighestSuccessfulEndOffset(0)
    , mBuffer(aBuffer)
    , mSize(aSize)
  {
  }
  bool ReadAt(int64_t aOffset, void* aData, size_t aLength,
              size_t* aBytesRead) override
  {
    if (aOffset < 0 || aOffset > static_cast<int64_t>(mSize)) {
      return false;
    }
    // After the test, 0 <= aOffset <= mSize <= SIZE_MAX, so it's safe to cast to size_t.
    size_t offset = static_cast<size_t>(aOffset);
    // Don't read past the end (but it's not an error to try).
    if (aLength > mSize - offset) {
      aLength = mSize - offset;
    }
    // Now, 0 <= offset <= offset + aLength <= mSize <= SIZE_MAX.
    *aBytesRead = aLength;
    memcpy(aData, mBuffer + offset, aLength);
    if (mHighestSuccessfulEndOffset < offset + aLength)
    {
      mHighestSuccessfulEndOffset = offset + aLength;
    }
    return true;
  }
  bool CachedReadAt(int64_t aOffset, void* aData, size_t aLength,
                    size_t* aBytesRead) override
  {
    return ReadAt(aOffset, aData, aLength, aBytesRead);
  }
  bool Length(int64_t* aLength) override
  {
    *aLength = mSize;
    return true;
  }
  void DiscardBefore(int64_t aOffset) override
  {
  }

  // Offset past the last character ever read. 0 when nothing read yet.
  size_t mHighestSuccessfulEndOffset;
protected:
  virtual ~TestStream()
  {
  }

  const uint8_t* mBuffer;
  size_t mSize;
};

TEST(stagefright_MP4Metadata, EmptyStream)
{
  nsRefPtr<Stream> stream = new TestStream(nullptr, 0);

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
  nsRefPtr<Stream> stream = new TestStream(nullptr, 0);

  Monitor monitor("MP4Metadata::gtest");
  MonitorAutoLock mon(monitor);
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

nsTArray<uint8_t>
ReadTestFile(const char* aFilename)
{
  if (!aFilename) {
    return {};
  }
  FILE* f = fopen(aFilename, "rb");
  if (!f) {
    return {};
   }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return {};
  }
  long position = ftell(f);
  // I know EOF==-1, so this test is made obsolete by '<0', but I don't want
  // the code to rely on that.
  if (position == 0 || position == EOF || position < 0) {
    fclose(f);
    return {};
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return {};
  }

  size_t len = static_cast<size_t>(position);
  nsTArray<uint8_t> buffer(len);
  buffer.SetLength(len);
  size_t read = fread(buffer.Elements(), 1, len, f);
  fclose(f);
  if (read != len) {
    return {};
  }

  return buffer;
}

static const char* testFiles[] = {
  "test_case_1187067.mp4"
};

TEST(stagefright_MPEG4Metadata, test_case_mp4)
{
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFiles[test]);
    ASSERT_FALSE(buffer.IsEmpty());
    nsRefPtr<Stream> stream = new TestStream(buffer.Elements(), buffer.Length());

    EXPECT_TRUE(MP4Metadata::HasCompleteMetadata(stream));
    nsRefPtr<MediaByteBuffer> metadataBuffer = MP4Metadata::Metadata(stream);
    EXPECT_TRUE(metadataBuffer);

    MP4Metadata metadata(stream);
    EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kUndefinedTrack));
    EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kAudioTrack));
    EXPECT_EQ(1u, metadata.GetNumberTracks(TrackInfo::kVideoTrack));
    EXPECT_EQ(0u, metadata.GetNumberTracks(TrackInfo::kTextTrack));
    EXPECT_EQ(0u, metadata.GetNumberTracks(static_cast<TrackInfo::TrackType>(-1)));
    EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kUndefinedTrack, 0));
    EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kAudioTrack, 0));
    UniquePtr<TrackInfo> track = metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0);
    EXPECT_TRUE(!!track);
    EXPECT_FALSE(metadata.GetTrackInfo(TrackInfo::kTextTrack, 0));
    EXPECT_FALSE(metadata.GetTrackInfo(static_cast<TrackInfo::TrackType>(-1), 0));
    // We can see anywhere in any MPEG4.
    EXPECT_TRUE(metadata.CanSeek());
    EXPECT_FALSE(metadata.Crypto().valid);
  }
}

TEST(stagefright_MPEG4Metadata, test_case_mp4_skimming)
{
  static const size_t step = 1u;
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFiles[test]);
    ASSERT_FALSE(buffer.IsEmpty());
    ASSERT_LE(step, buffer.Length());
    // Just exercizing the parser starting at different points through the file,
    // making sure it doesn't crash.
    // No checks because results would differ for each position.
    for (size_t offset = 0; offset < buffer.Length() - step; offset += step) {
      size_t size = buffer.Length() - offset;
      while (size > 0) {
        nsRefPtr<TestStream> stream =
          new TestStream(buffer.Elements() + offset, size);

        MP4Metadata::HasCompleteMetadata(stream);
        nsRefPtr<MediaByteBuffer> metadataBuffer = MP4Metadata::Metadata(stream);
        MP4Metadata metadata(stream);

        if (stream->mHighestSuccessfulEndOffset <= 0) {
          // No successful reads -> Cutting down the size won't change anything.
          break;
        }
        if (stream->mHighestSuccessfulEndOffset < size) {
          // Read up to a point before the end -> Resize down to that point.
          size = stream->mHighestSuccessfulEndOffset;
        } else {
          // Read up to the end (or after?!) -> Just cut 1 byte.
          size -= 1;
        }
      }
    }
  }
}

TEST(stagefright_MoofParser, test_case_mp4)
{
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFiles[test]);
    ASSERT_FALSE(buffer.IsEmpty());
    nsRefPtr<Stream> stream = new TestStream(buffer.Elements(), buffer.Length());

    Monitor monitor("MP4Metadata::HasCompleteMetadata");
    MonitorAutoLock mon(monitor);
    MoofParser parser(stream, 0, false, &monitor);
    EXPECT_EQ(0u, parser.mOffset);
    EXPECT_FALSE(parser.ReachedEnd());

    nsTArray<MediaByteRange> byteRanges;
    byteRanges.AppendElement(MediaByteRange(0, 0));
    EXPECT_FALSE(parser.RebuildFragmentedIndex(byteRanges));

    EXPECT_TRUE(parser.GetCompositionRange(byteRanges).IsNull());
    EXPECT_TRUE(parser.mInitRange.IsNull());
    EXPECT_EQ(0u, parser.mOffset);
    EXPECT_FALSE(parser.ReachedEnd());
    EXPECT_TRUE(parser.HasMetadata());
    nsRefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
    EXPECT_TRUE(metadataBuffer);
    EXPECT_TRUE(parser.FirstCompleteMediaSegment().IsNull());
    EXPECT_TRUE(parser.FirstCompleteMediaHeader().IsNull());
  }
}

TEST(stagefright_MoofParser, test_case_mp4_skimming)
{
  const size_t step = 1u;
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> buffer = ReadTestFile(testFiles[test]);
    ASSERT_FALSE(buffer.IsEmpty());
    ASSERT_LE(step, buffer.Length());
    Monitor monitor("MP4Metadata::HasCompleteMetadata");
    MonitorAutoLock mon(monitor);
    // Just exercizing the parser starting at different points through the file,
    // making sure it doesn't crash.
    // No checks because results would differ for each position.
    for (size_t offset = 0; offset < buffer.Length() - step; offset += step) {
      size_t size = buffer.Length() - offset;
      while (size > 0) {
        nsRefPtr<TestStream> stream =
          new TestStream(buffer.Elements() + offset, size);

        MoofParser parser(stream, 0, false, &monitor);
        nsTArray<MediaByteRange> byteRanges;
        byteRanges.AppendElement(MediaByteRange(0, 0));
        EXPECT_FALSE(parser.RebuildFragmentedIndex(byteRanges));
        parser.GetCompositionRange(byteRanges);
        parser.HasMetadata();
        nsRefPtr<MediaByteBuffer> metadataBuffer = parser.Metadata();
        parser.FirstCompleteMediaSegment();
        parser.FirstCompleteMediaHeader();

        if (stream->mHighestSuccessfulEndOffset <= 0) {
          // No successful reads -> Cutting down the size won't change anything.
          break;
        }
        if (stream->mHighestSuccessfulEndOffset < size) {
          // Read up to a point before the end -> Resize down to that point.
          size = stream->mHighestSuccessfulEndOffset;
        } else {
          // Read up to the end (or after?!) -> Just cut 1 byte.
          size -= 1;
        }
      }
    }
  }
}

uint8_t media_libstagefright_gtest_video_init_mp4[] = {
  0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x69, 0x73, 0x6f, 0x6d,
  0x00, 0x00, 0x00, 0x01, 0x69, 0x73, 0x6f, 0x6d, 0x61, 0x76, 0x63, 0x31,
  0x00, 0x00, 0x02, 0xd1, 0x6d, 0x6f, 0x6f, 0x76, 0x00, 0x00, 0x00, 0x6c,
  0x6d, 0x76, 0x68, 0x64, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x49, 0x73, 0xf8,
  0xc8, 0x4a, 0xc5, 0x7a, 0x00, 0x00, 0x02, 0x58, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x18,
  0x69, 0x6f, 0x64, 0x73, 0x00, 0x00, 0x00, 0x00, 0x10, 0x80, 0x80, 0x80,
  0x07, 0x00, 0x4f, 0xff, 0xff, 0x29, 0x15, 0xff, 0x00, 0x00, 0x02, 0x0d,
  0x74, 0x72, 0x61, 0x6b, 0x00, 0x00, 0x00, 0x5c, 0x74, 0x6b, 0x68, 0x64,
  0x00, 0x00, 0x00, 0x01, 0xc8, 0x49, 0x73, 0xf8, 0xc8, 0x49, 0x73, 0xf9,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x40, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x01, 0x68, 0x00, 0x00,
  0x00, 0x00, 0x01, 0xa9, 0x6d, 0x64, 0x69, 0x61, 0x00, 0x00, 0x00, 0x20,
  0x6d, 0x64, 0x68, 0x64, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x49, 0x73, 0xf8,
  0xc8, 0x49, 0x73, 0xf9, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x55, 0xc4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x68, 0x64, 0x6c, 0x72,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x65,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x47, 0x50, 0x41, 0x43, 0x20, 0x49, 0x53, 0x4f, 0x20, 0x56, 0x69, 0x64,
  0x65, 0x6f, 0x20, 0x48, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x72, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x49, 0x6d, 0x69, 0x6e, 0x66, 0x00, 0x00, 0x00, 0x14,
  0x76, 0x6d, 0x68, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x64, 0x69, 0x6e, 0x66,
  0x00, 0x00, 0x00, 0x1c, 0x64, 0x72, 0x65, 0x66, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x75, 0x72, 0x6c, 0x20,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x73, 0x74, 0x62, 0x6c,
  0x00, 0x00, 0x00, 0xad, 0x73, 0x74, 0x73, 0x64, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x9d, 0x61, 0x76, 0x63, 0x31,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x02, 0x80, 0x01, 0x68, 0x00, 0x48, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x18, 0xff, 0xff, 0x00, 0x00, 0x00, 0x33, 0x61, 0x76,
  0x63, 0x43, 0x01, 0x64, 0x00, 0x1f, 0xff, 0xe1, 0x00, 0x1b, 0x67, 0x64,
  0x00, 0x1f, 0xac, 0x2c, 0xc5, 0x02, 0x80, 0xbf, 0xe5, 0xc0, 0x44, 0x00,
  0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xf2, 0x3c, 0x60, 0xc6,
  0x58, 0x01, 0x00, 0x05, 0x68, 0xe9, 0x2b, 0x2c, 0x8b, 0x00, 0x00, 0x00,
  0x14, 0x62, 0x74, 0x72, 0x74, 0x00, 0x01, 0x5a, 0xc2, 0x00, 0x24, 0x74,
  0x38, 0x00, 0x09, 0x22, 0x00, 0x00, 0x00, 0x00, 0x10, 0x73, 0x74, 0x74,
  0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x63, 0x74, 0x74, 0x73, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x10, 0x73, 0x74, 0x73, 0x63, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x73, 0x74, 0x73,
  0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x10, 0x73, 0x74, 0x63, 0x6f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x6d, 0x76, 0x65,
  0x78, 0x00, 0x00, 0x00, 0x10, 0x6d, 0x65, 0x68, 0x64, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x05, 0x76, 0x18, 0x00, 0x00, 0x00, 0x20, 0x74, 0x72, 0x65,
  0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x03, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00
};

const uint32_t media_libstagefright_gtest_video_init_mp4_len = 745;

TEST(stagefright_MP4Metadata, EmptyCTTS)
{
  nsRefPtr<MediaByteBuffer> buffer = new MediaByteBuffer(media_libstagefright_gtest_video_init_mp4_len);
  buffer->AppendElements(media_libstagefright_gtest_video_init_mp4, media_libstagefright_gtest_video_init_mp4_len);
  nsRefPtr<BufferStream> stream = new BufferStream(buffer);

  EXPECT_TRUE(MP4Metadata::HasCompleteMetadata(stream));
  nsRefPtr<MediaByteBuffer> metadataBuffer = MP4Metadata::Metadata(stream);
  EXPECT_TRUE(metadataBuffer);

  MP4Metadata metadata(stream);

  EXPECT_EQ(1u, metadata.GetNumberTracks(TrackInfo::kVideoTrack));
  mozilla::UniquePtr<mozilla::TrackInfo> track =
    metadata.GetTrackInfo(TrackInfo::kVideoTrack, 0);
  EXPECT_TRUE(track != nullptr);
  // We can seek anywhere in any MPEG4.
  EXPECT_TRUE(metadata.CanSeek());
  EXPECT_FALSE(metadata.Crypto().valid);
}

