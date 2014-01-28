/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MKV_DEFS_HPP
#define MKV_DEFS_HPP 1

/* Commenting out values not available in webm, but available in matroska */

enum mkv {
  EBML = 0x1A45DFA3,
  EBMLVersion = 0x4286,
  EBMLReadVersion = 0x42F7,
  EBMLMaxIDLength = 0x42F2,
  EBMLMaxSizeLength = 0x42F3,
  DocType = 0x4282,
  DocTypeVersion = 0x4287,
  DocTypeReadVersion = 0x4285,
/* CRC_32 = 0xBF, */
  Void = 0xEC,
  SignatureSlot = 0x1B538667,
  SignatureAlgo = 0x7E8A,
  SignatureHash = 0x7E9A,
  SignaturePublicKey = 0x7EA5,
  Signature = 0x7EB5,
  SignatureElements = 0x7E5B,
  SignatureElementList = 0x7E7B,
  SignedElement = 0x6532,
  /* segment */
  Segment = 0x18538067,
  /* Meta Seek Information */
  SeekHead = 0x114D9B74,
  Seek = 0x4DBB,
  SeekID = 0x53AB,
  SeekPosition = 0x53AC,
  /* Segment Information */
  Info = 0x1549A966,
/* SegmentUID = 0x73A4, */
/* SegmentFilename = 0x7384, */
/* PrevUID = 0x3CB923, */
/* PrevFilename = 0x3C83AB, */
/* NextUID = 0x3EB923, */
/* NextFilename = 0x3E83BB, */
/* SegmentFamily = 0x4444, */
/* ChapterTranslate = 0x6924, */
/* ChapterTranslateEditionUID = 0x69FC, */
/* ChapterTranslateCodec = 0x69BF, */
/* ChapterTranslateID = 0x69A5, */
  TimecodeScale = 0x2AD7B1,
  Segment_Duration = 0x4489,
  DateUTC = 0x4461,
/* Title = 0x7BA9, */
  MuxingApp = 0x4D80,
  WritingApp = 0x5741,
  /* Cluster */
  Cluster = 0x1F43B675,
  Timecode = 0xE7,
/* SilentTracks = 0x5854, */
/* SilentTrackNumber = 0x58D7, */
/* Position = 0xA7, */
  PrevSize = 0xAB,
  BlockGroup = 0xA0,
  Block = 0xA1,
/* BlockVirtual = 0xA2, */
  BlockAdditions = 0x75A1,
  BlockMore = 0xA6,
  BlockAddID = 0xEE,
  BlockAdditional = 0xA5,
  BlockDuration = 0x9B,
/* ReferencePriority = 0xFA, */
  ReferenceBlock = 0xFB,
/* ReferenceVirtual = 0xFD, */
/* CodecState = 0xA4, */
/* Slices = 0x8E, */
/* TimeSlice = 0xE8, */
  LaceNumber = 0xCC,
/* FrameNumber = 0xCD, */
/* BlockAdditionID = 0xCB, */
/* MkvDelay = 0xCE, */
/* Cluster_Duration = 0xCF, */
  SimpleBlock = 0xA3,
/* EncryptedBlock = 0xAF, */
  /* Track */
  Tracks = 0x1654AE6B,
  TrackEntry = 0xAE,
  TrackNumber = 0xD7,
  TrackUID = 0x73C5,
  TrackType = 0x83,
  FlagEnabled = 0xB9,
  FlagDefault = 0x88,
  FlagForced = 0x55AA,
  FlagLacing = 0x9C,
/* MinCache = 0x6DE7, */
/* MaxCache = 0x6DF8, */
  DefaultDuration = 0x23E383,
/* TrackTimecodeScale = 0x23314F, */
/* TrackOffset = 0x537F, */
  MaxBlockAdditionID = 0x55EE,
  Name = 0x536E,
  Language = 0x22B59C,
  CodecID = 0x86,
  CodecPrivate = 0x63A2,
  CodecName = 0x258688,
/* AttachmentLink = 0x7446, */
/* CodecSettings = 0x3A9697, */
/* CodecInfoURL = 0x3B4040, */
/* CodecDownloadURL = 0x26B240, */
/* CodecDecodeAll = 0xAA, */
/* TrackOverlay = 0x6FAB, */
/* TrackTranslate = 0x6624, */
/* TrackTranslateEditionUID = 0x66FC, */
/* TrackTranslateCodec = 0x66BF, */
/* TrackTranslateTrackID = 0x66A5, */
  /* video */
  Video = 0xE0,
  FlagInterlaced = 0x9A,
  WEBM_StereoMode = 0x53B8,
  AlphaMode = 0x53C0,
  PixelWidth = 0xB0,
  PixelHeight = 0xBA,
  PixelCropBottom = 0x54AA,
  PixelCropTop = 0x54BB,
  PixelCropLeft = 0x54CC,
  PixelCropRight = 0x54DD,
  DisplayWidth = 0x54B0,
  DisplayHeight = 0x54BA,
  DisplayUnit = 0x54B2,
  AspectRatioType = 0x54B3,
/* ColourSpace = 0x2EB524, */
/* GammaValue = 0x2FB523, */
  FrameRate = 0x2383E3,
  /* end video */
  /* audio */
  Audio = 0xE1,
  SamplingFrequency = 0xB5,
  OutputSamplingFrequency = 0x78B5,
  Channels = 0x9F,
/* ChannelPositions = 0x7D7B, */
  BitDepth = 0x6264,
  /* end audio */
  /* content encoding */
/* ContentEncodings = 0x6d80, */
/* ContentEncoding = 0x6240, */
/* ContentEncodingOrder = 0x5031, */
/* ContentEncodingScope = 0x5032, */
/* ContentEncodingType = 0x5033, */
/* ContentCompression = 0x5034, */
/* ContentCompAlgo = 0x4254, */
/* ContentCompSettings = 0x4255, */
/* ContentEncryption = 0x5035, */
/* ContentEncAlgo = 0x47e1, */
/* ContentEncKeyID = 0x47e2, */
/* ContentSignature = 0x47e3, */
/* ContentSigKeyID = 0x47e4, */
/* ContentSigAlgo = 0x47e5, */
/* ContentSigHashAlgo = 0x47e6, */
  /* end content encoding */
  /* Cueing Data */
  Cues = 0x1C53BB6B,
  CuePoint = 0xBB,
  CueTime = 0xB3,
  CueTrackPositions = 0xB7,
  CueTrack = 0xF7,
  CueClusterPosition = 0xF1,
  CueBlockNumber = 0x5378
/* CueCodecState = 0xEA, */
/* CueReference = 0xDB, */
/* CueRefTime = 0x96, */
/* CueRefCluster = 0x97, */
/* CueRefNumber = 0x535F, */
/* CueRefCodecState = 0xEB, */
  /* Attachment */
/* Attachments = 0x1941A469, */
/* AttachedFile = 0x61A7, */
/* FileDescription = 0x467E, */
/* FileName = 0x466E, */
/* FileMimeType = 0x4660, */
/* FileData = 0x465C, */
/* FileUID = 0x46AE, */
/* FileReferral = 0x4675, */
  /* Chapters */
/* Chapters = 0x1043A770, */
/* EditionEntry = 0x45B9, */
/* EditionUID = 0x45BC, */
/* EditionFlagHidden = 0x45BD, */
/* EditionFlagDefault = 0x45DB, */
/* EditionFlagOrdered = 0x45DD, */
/* ChapterAtom = 0xB6, */
/* ChapterUID = 0x73C4, */
/* ChapterTimeStart = 0x91, */
/* ChapterTimeEnd = 0x92, */
/* ChapterFlagHidden = 0x98, */
/* ChapterFlagEnabled = 0x4598, */
/* ChapterSegmentUID = 0x6E67, */
/* ChapterSegmentEditionUID = 0x6EBC, */
/* ChapterPhysicalEquiv = 0x63C3, */
/* ChapterTrack = 0x8F, */
/* ChapterTrackNumber = 0x89, */
/* ChapterDisplay = 0x80, */
/* ChapString = 0x85, */
/* ChapLanguage = 0x437C, */
/* ChapCountry = 0x437E, */
/* ChapProcess = 0x6944, */
/* ChapProcessCodecID = 0x6955, */
/* ChapProcessPrivate = 0x450D, */
/* ChapProcessCommand = 0x6911, */
/* ChapProcessTime = 0x6922, */
/* ChapProcessData = 0x6933, */
  /* Tagging */
/* Tags = 0x1254C367, */
/* Tag = 0x7373, */
/* Targets = 0x63C0, */
/* TargetTypeValue = 0x68CA, */
/* TargetType = 0x63CA, */
/* Tagging_TrackUID = 0x63C5, */
/* Tagging_EditionUID = 0x63C9, */
/* Tagging_ChapterUID = 0x63C4, */
/* AttachmentUID = 0x63C6, */
/* SimpleTag = 0x67C8, */
/* TagName = 0x45A3, */
/* TagLanguage = 0x447A, */
/* TagDefault = 0x4484, */
/* TagString = 0x4487, */
/* TagBinary = 0x4485, */
};
#endif
