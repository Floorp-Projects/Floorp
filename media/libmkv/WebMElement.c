// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "EbmlIDs.h"
#include "WebMElement.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define kVorbisPrivateMaxSize  4000
#define UInt64 uint64_t

void writeHeader(EbmlGlobal *glob) {
  EbmlLoc start;
  Ebml_StartSubElement(glob, &start, EBML);
  Ebml_SerializeUnsigned(glob, EBMLVersion, 1);
  Ebml_SerializeUnsigned(glob, EBMLReadVersion, 1); // EBML Read Version
  Ebml_SerializeUnsigned(glob, EBMLMaxIDLength, 4); // EBML Max ID Length
  Ebml_SerializeUnsigned(glob, EBMLMaxSizeLength, 8); // EBML Max Size Length
  Ebml_SerializeString(glob, DocType, "webm"); // Doc Type
  Ebml_SerializeUnsigned(glob, DocTypeVersion, 2); // Doc Type Version
  Ebml_SerializeUnsigned(glob, DocTypeReadVersion, 2); // Doc Type Read Version
  Ebml_EndSubElement(glob, &start);
}

void writeSimpleBlock(EbmlGlobal *glob, unsigned char trackNumber, short timeCode,
                      int isKeyframe, unsigned char lacingFlag, int discardable,
                      unsigned char *data, unsigned long dataLength) {
  unsigned long blockLength = 4 + dataLength;
  unsigned char flags = 0x00 | (isKeyframe ? 0x80 : 0x00) | (lacingFlag << 1) | discardable;
  Ebml_WriteID(glob, SimpleBlock);
  blockLength |= 0x10000000; // TODO check length < 0x0FFFFFFFF
  Ebml_Serialize(glob, &blockLength, sizeof(blockLength), 4);
  trackNumber |= 0x80;  // TODO check track nubmer < 128
  Ebml_Write(glob, &trackNumber, 1);
  // Ebml_WriteSigned16(glob, timeCode,2); //this is 3 bytes
  Ebml_Serialize(glob, &timeCode, sizeof(timeCode), 2);
  flags = 0x00 | (isKeyframe ? 0x80 : 0x00) | (lacingFlag << 1) | discardable;
  Ebml_Write(glob, &flags, 1);
  Ebml_Write(glob, data, dataLength);
}

static UInt64 generateTrackID(unsigned int trackNumber) {
  UInt64 t = time(NULL) * trackNumber;
  UInt64 r = rand();
  r = r << 32;
  r +=  rand();
//  UInt64 rval = t ^ r;
  return t ^ r;
}

void writeVideoTrack(EbmlGlobal *glob, unsigned int trackNumber, int flagLacing,
                     const char *codecId, unsigned int pixelWidth, unsigned int pixelHeight,
                     unsigned int displayWidth, unsigned int displayHeight,
                     double frameRate) {
  EbmlLoc start;
  UInt64 trackID;
  Ebml_StartSubElement(glob, &start, TrackEntry);
  Ebml_SerializeUnsigned(glob, TrackNumber, trackNumber);
  trackID = generateTrackID(trackNumber);
  Ebml_SerializeUnsigned(glob, TrackUID, trackID);
  Ebml_SerializeString(glob, CodecName, "VP8");  // TODO shouldn't be fixed

  Ebml_SerializeUnsigned(glob, TrackType, 1); // video is always 1
  Ebml_SerializeString(glob, CodecID, codecId);
  {
    EbmlLoc videoStart;
    Ebml_StartSubElement(glob, &videoStart, Video);
    Ebml_SerializeUnsigned(glob, PixelWidth, pixelWidth);
    Ebml_SerializeUnsigned(glob, PixelHeight, pixelHeight);
    if (pixelWidth != displayWidth) {
      Ebml_SerializeUnsigned(glob, DisplayWidth, displayWidth);
    }
    if (pixelHeight != displayHeight) {
      Ebml_SerializeUnsigned(glob, DisplayHeight, displayHeight);
    }
    Ebml_SerializeFloat(glob, FrameRate, frameRate);
    Ebml_EndSubElement(glob, &videoStart); // Video
  }
  Ebml_EndSubElement(glob, &start); // Track Entry
}
void writeAudioTrack(EbmlGlobal *glob, unsigned int trackNumber, int flagLacing,
                     const char *codecId, double samplingFrequency, unsigned int channels,
                     unsigned char *private, unsigned long privateSize) {
  EbmlLoc start;
  UInt64 trackID;
  Ebml_StartSubElement(glob, &start, TrackEntry);
  Ebml_SerializeUnsigned(glob, TrackNumber, trackNumber);
  trackID = generateTrackID(trackNumber);
  Ebml_SerializeUnsigned(glob, TrackUID, trackID);
  Ebml_SerializeUnsigned(glob, TrackType, 2); // audio is always 2
  // I am using defaults for thesed required fields
  /*  Ebml_SerializeUnsigned(glob, FlagEnabled, 1);
      Ebml_SerializeUnsigned(glob, FlagDefault, 1);
      Ebml_SerializeUnsigned(glob, FlagForced, 1);
      Ebml_SerializeUnsigned(glob, FlagLacing, flagLacing);*/
  Ebml_SerializeString(glob, CodecID, codecId);
  Ebml_SerializeData(glob, CodecPrivate, private, privateSize);

  Ebml_SerializeString(glob, CodecName, "VORBIS");  // fixed for now
  {
    EbmlLoc AudioStart;
    Ebml_StartSubElement(glob, &AudioStart, Audio);
    Ebml_SerializeFloat(glob, SamplingFrequency, samplingFrequency);
    Ebml_SerializeUnsigned(glob, Channels, channels);
    Ebml_EndSubElement(glob, &AudioStart);
  }
  Ebml_EndSubElement(glob, &start);
}
void writeSegmentInformation(EbmlGlobal *ebml, EbmlLoc *startInfo, unsigned long timeCodeScale, double duration) {
  Ebml_StartSubElement(ebml, startInfo, Info);
  Ebml_SerializeUnsigned(ebml, TimecodeScale, timeCodeScale);
  Ebml_SerializeFloat(ebml, Segment_Duration, duration * 1000.0); // Currently fixed to using milliseconds
  Ebml_SerializeString(ebml, 0x4D80, "QTmuxingAppLibWebM-0.0.1");
  Ebml_SerializeString(ebml, 0x5741, "QTwritingAppLibWebM-0.0.1");
  Ebml_EndSubElement(ebml, startInfo);
}

/*
void Mkv_InitializeSegment(Ebml& ebml_out, EbmlLoc& ebmlLoc)
{
    Ebml_StartSubElement(ebml_out, ebmlLoc, 0x18538067);
}

void Mkv_InitializeSeek(Ebml& ebml_out, EbmlLoc& ebmlLoc)
{
    Ebml_StartSubElement(ebml_out, ebmlLoc, 0x114d9b74);
}
void Mkv_WriteSeekInformation(Ebml& ebml_out, SeekStruct& seekInformation)
{
    EbmlLoc ebmlLoc;
    Ebml_StartSubElement(ebml_out, ebmlLoc, 0x4dbb);
    Ebml_SerializeString(ebml_out, 0x53ab, seekInformation.SeekID);
    Ebml_SerializeUnsigned(ebml_out, 0x53ac, seekInformation.SeekPosition);
    Ebml_EndSubElement(ebml_out, ebmlLoc);
}

void Mkv_WriteSegmentInformation(Ebml& ebml_out, SegmentInformationStruct& segmentInformation)
{
    Ebml_SerializeUnsigned(ebml_out, 0x73a4, segmentInformation.segmentUID);
    if (segmentInformation.filename != 0)
        Ebml_SerializeString(ebml_out, 0x7384, segmentInformation.filename);
    Ebml_SerializeUnsigned(ebml_out, 0x2AD7B1, segmentInformation.TimecodeScale);
    Ebml_SerializeUnsigned(ebml_out, 0x4489, segmentInformation.Duration);
    // TODO date
    Ebml_SerializeWString(ebml_out, 0x4D80, L"MKVMUX");
    Ebml_SerializeWString(ebml_out, 0x5741, segmentInformation.WritingApp);
}

void Mkv_InitializeTrack(Ebml& ebml_out, EbmlLoc& ebmlLoc)
{
    Ebml_StartSubElement(ebml_out, ebmlLoc, 0x1654AE6B);
}

static void Mkv_WriteGenericTrackData(Ebml& ebml_out, TrackStruct& track)
{
    Ebml_SerializeUnsigned(ebml_out, 0xD7, track.TrackNumber);
    Ebml_SerializeUnsigned(ebml_out, 0x73C5, track.TrackUID);
    Ebml_SerializeUnsigned(ebml_out, 0x83, track.TrackType);
    Ebml_SerializeUnsigned(ebml_out, 0xB9, track.FlagEnabled ? 1 :0);
    Ebml_SerializeUnsigned(ebml_out, 0x88, track.FlagDefault ? 1 :0);
    Ebml_SerializeUnsigned(ebml_out, 0x55AA, track.FlagForced ? 1 :0);
    if (track.Language != 0)
        Ebml_SerializeString(ebml_out, 0x22B59C, track.Language);
    if (track.CodecID != 0)
        Ebml_SerializeString(ebml_out, 0x86, track.CodecID);
    if (track.CodecPrivate != 0)
        Ebml_SerializeData(ebml_out, 0x63A2, track.CodecPrivate, track.CodecPrivateLength);
    if (track.CodecName != 0)
        Ebml_SerializeWString(ebml_out, 0x258688, track.CodecName);
}

void Mkv_WriteVideoTrack(Ebml& ebml_out, TrackStruct & track, VideoTrackStruct& video)
{
    EbmlLoc trackHeadLoc, videoHeadLoc;
    Ebml_StartSubElement(ebml_out, trackHeadLoc, 0xAE);  // start Track
    Mkv_WriteGenericTrackData(ebml_out, track);
    Ebml_StartSubElement(ebml_out, videoHeadLoc, 0xE0);  // start Video
    Ebml_SerializeUnsigned(ebml_out, 0x9A, video.FlagInterlaced ? 1 :0);
    Ebml_SerializeUnsigned(ebml_out, 0xB0, video.PixelWidth);
    Ebml_SerializeUnsigned(ebml_out, 0xBA, video.PixelHeight);
    Ebml_SerializeUnsigned(ebml_out, 0x54B0, video.PixelDisplayWidth);
    Ebml_SerializeUnsigned(ebml_out, 0x54BA, video.PixelDisplayHeight);
    Ebml_SerializeUnsigned(ebml_out, 0x54B2, video.displayUnit);
    Ebml_SerializeFloat(ebml_out, 0x2383E3, video.FrameRate);
    Ebml_EndSubElement(ebml_out, videoHeadLoc);
    Ebml_EndSubElement(ebml_out, trackHeadLoc);

}

void Mkv_WriteAudioTrack(Ebml& ebml_out, TrackStruct & track, AudioTrackStruct& video)
{
    EbmlLoc trackHeadLoc, audioHeadLoc;
    Ebml_StartSubElement(ebml_out, trackHeadLoc, 0xAE);
    Mkv_WriteGenericTrackData(ebml_out, track);
    Ebml_StartSubElement(ebml_out, audioHeadLoc, 0xE0);  // start Audio
    Ebml_SerializeFloat(ebml_out, 0xB5, video.SamplingFrequency);
    Ebml_SerializeUnsigned(ebml_out, 0x9F, video.Channels);
    Ebml_SerializeUnsigned(ebml_out, 0x6264, video.BitDepth);
    Ebml_EndSubElement(ebml_out, audioHeadLoc); // end audio
    Ebml_EndSubElement(ebml_out, trackHeadLoc);
}

void Mkv_WriteEbmlClusterHead(Ebml& ebml_out,  EbmlLoc& ebmlLoc, ClusterHeadStruct & clusterHead)
{
    Ebml_StartSubElement(ebml_out, ebmlLoc, 0x1F43B675);
    Ebml_SerializeUnsigned(ebml_out, 0x6264, clusterHead.TimeCode);
}

void Mkv_WriteSimpleBlockHead(Ebml& ebml_out,  EbmlLoc& ebmlLoc, SimpleBlockStruct& block)
{
    Ebml_StartSubElement(ebml_out, ebmlLoc, 0xA3);
    Ebml_Write1UInt(ebml_out, block.TrackNumber);
    Ebml_WriteSigned16(ebml_out,block.TimeCode);
    unsigned char flags = 0x00 | (block.iskey ? 0x80:0x00) | (block.lacing << 1) | block.discardable;
    Ebml_Write1UInt(ebml_out, flags);  // TODO this may be the wrong function
    Ebml_Serialize(ebml_out, block.data, block.dataLength);
    Ebml_EndSubElement(ebml_out,ebmlLoc);
}
*/
