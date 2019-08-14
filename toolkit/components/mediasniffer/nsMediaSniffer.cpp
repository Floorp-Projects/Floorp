/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ADTSDemuxer.h"
#include "FlacDemuxer.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mp3sniff.h"
#include "nestegg/nestegg.h"
#include "nsIClassInfoImpl.h"
#include "nsIHttpChannel.h"
#include "nsMediaSniffer.h"
#include "nsMimeTypes.h"
#include "nsString.h"

#include <algorithm>

// The minimum number of bytes that are needed to attempt to sniff an mp4 file.
static const unsigned MP4_MIN_BYTES_COUNT = 12;
// The maximum number of bytes to consider when attempting to sniff a file.
static const uint32_t MAX_BYTES_SNIFFED = 512;
// The maximum number of bytes to consider when attempting to sniff for a mp3
// bitstream.
// This is 320kbps * 144 / 32kHz + 1 padding byte + 4 bytes of capture pattern.
static const uint32_t MAX_BYTES_SNIFFED_MP3 = 320 * 144 / 32 + 1 + 4;

NS_IMPL_ISUPPORTS(nsMediaSniffer, nsIContentSniffer)

nsMediaSnifferEntry nsMediaSniffer::sSnifferEntries[] = {
    // The string OggS, followed by the null byte.
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF\xFF", "OggS", APPLICATION_OGG),
    // The string RIFF, followed by four bytes, followed by the string WAVE
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF",
                  "RIFF\x00\x00\x00\x00WAVE", AUDIO_WAV),
    // mp3 with ID3 tags, the string "ID3".
    PATTERN_ENTRY("\xFF\xFF\xFF", "ID3", AUDIO_MP3),
    // FLAC with standard header
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "fLaC", AUDIO_FLAC)};

// For a complete list of file types, see http://www.ftyps.com/index.html
nsMediaSnifferEntry sFtypEntries[] = {
    PATTERN_ENTRY("\xFF\xFF\xFF", "mp4", VIDEO_MP4),  // Could be mp41 or mp42.
    PATTERN_ENTRY("\xFF\xFF\xFF", "avc",
                  VIDEO_MP4),  // Could be avc1, avc2, ...
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "3gp4",
                  VIDEO_MP4),  // 3gp4 is based on MP4
    PATTERN_ENTRY("\xFF\xFF\xFF", "3gp",
                  VIDEO_3GPP),  // Could be 3gp5, ...
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "M4V ", VIDEO_MP4),
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "M4A ", AUDIO_MP4),
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "M4P ", AUDIO_MP4),
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "qt  ", VIDEO_QUICKTIME),
    PATTERN_ENTRY("\xFF\xFF\xFF", "iso", VIDEO_MP4),  // Could be isom or iso2.
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "mmp4", VIDEO_MP4),
};

static bool MatchesBrands(const uint8_t aData[4], nsACString& aSniffedType) {
  for (size_t i = 0; i < mozilla::ArrayLength(sFtypEntries); ++i) {
    const auto& currentEntry = sFtypEntries[i];
    bool matched = true;
    MOZ_ASSERT(currentEntry.mLength <= 4,
               "Pattern is too large to match brand strings.");
    for (uint32_t j = 0; j < currentEntry.mLength; ++j) {
      if ((currentEntry.mMask[j] & aData[j]) != currentEntry.mPattern[j]) {
        matched = false;
        break;
      }
    }
    if (matched) {
      aSniffedType.AssignASCII(currentEntry.mContentType);
      return true;
    }
  }

  return false;
}

// This function implements sniffing algorithm for MP4 family file types,
// including MP4 (described at
// http://mimesniff.spec.whatwg.org/#signature-for-mp4), M4A (Apple iTunes
// audio), and 3GPP.
static bool MatchesMP4(const uint8_t* aData, const uint32_t aLength,
                       nsACString& aSniffedType) {
  if (aLength <= MP4_MIN_BYTES_COUNT) {
    return false;
  }
  // Conversion from big endian to host byte order.
  uint32_t boxSize =
      (uint32_t)(aData[3] | aData[2] << 8 | aData[1] << 16 | aData[0] << 24);

  // Boxsize should be evenly divisible by 4.
  if (boxSize % 4 || aLength < boxSize) {
    return false;
  }
  // The string "ftyp".
  if (aData[4] != 0x66 || aData[5] != 0x74 || aData[6] != 0x79 ||
      aData[7] != 0x70) {
    return false;
  }
  if (MatchesBrands(&aData[8], aSniffedType)) {
    return true;
  }
  // Skip minor_version (bytes 12-15).
  uint32_t bytesRead = 16;
  while (bytesRead < boxSize) {
    if (MatchesBrands(&aData[bytesRead], aSniffedType)) {
      return true;
    }
    bytesRead += 4;
  }

  return false;
}

static bool MatchesWebM(const uint8_t* aData, const uint32_t aLength) {
  return nestegg_sniff((uint8_t*)aData, aLength) ? true : false;
}

// This function implements mp3 sniffing based on parsing
// packet headers and looking for expected boundaries.
static bool MatchesMP3(const uint8_t* aData, const uint32_t aLength) {
  return mp3_sniff(aData, (long)aLength);
}

static bool MatchesFLAC(const uint8_t* aData, const uint32_t aLength) {
  return mozilla::FlacDemuxer::FlacSniffer(aData, aLength);
}

static bool MatchesADTS(const uint8_t* aData, const uint32_t aLength) {
  return mozilla::ADTSDemuxer::ADTSSniffer(aData, aLength);
}

NS_IMETHODIMP
nsMediaSniffer::GetMIMETypeFromContent(nsIRequest* aRequest,
                                       const uint8_t* aData,
                                       const uint32_t aLength,
                                       nsACString& aSniffedType) {
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    if (loadInfo->GetSkipContentSniffing()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    nsLoadFlags loadFlags = 0;
    channel->GetLoadFlags(&loadFlags);
    if (!(loadFlags & nsIChannel::LOAD_MEDIA_SNIFFER_OVERRIDES_CONTENT_TYPE)) {
      // For media, we want to sniff only if the Content-Type is unknown, or if
      // it is application/octet-stream.
      nsAutoCString contentType;
      nsresult rv = channel->GetContentType(contentType);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!contentType.IsEmpty() &&
          !contentType.EqualsLiteral(APPLICATION_OCTET_STREAM) &&
          !contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    }
  }

  const uint32_t clampedLength = std::min(aLength, MAX_BYTES_SNIFFED);

  for (size_t i = 0; i < mozilla::ArrayLength(sSnifferEntries); ++i) {
    const nsMediaSnifferEntry& currentEntry = sSnifferEntries[i];
    if (clampedLength < currentEntry.mLength || currentEntry.mLength == 0) {
      continue;
    }
    bool matched = true;
    for (uint32_t j = 0; j < currentEntry.mLength; ++j) {
      if ((currentEntry.mMask[j] & aData[j]) != currentEntry.mPattern[j]) {
        matched = false;
        break;
      }
    }
    if (matched) {
      aSniffedType.AssignASCII(currentEntry.mContentType);
      return NS_OK;
    }
  }

  if (MatchesMP4(aData, clampedLength, aSniffedType)) {
    return NS_OK;
  }

  if (MatchesWebM(aData, clampedLength)) {
    aSniffedType.AssignLiteral(VIDEO_WEBM);
    return NS_OK;
  }

  // Bug 950023: 512 bytes are often not enough to sniff for mp3.
  if (MatchesMP3(aData, std::min(aLength, MAX_BYTES_SNIFFED_MP3))) {
    aSniffedType.AssignLiteral(AUDIO_MP3);
    return NS_OK;
  }

  // Flac frames are generally big, often in excess of 24kB.
  // Using a size of MAX_BYTES_SNIFFED effectively means that we will only
  // recognize flac content if it starts with a frame.
  if (MatchesFLAC(aData, clampedLength)) {
    aSniffedType.AssignLiteral(AUDIO_FLAC);
    return NS_OK;
  }

  if (MatchesADTS(aData, clampedLength)) {
    aSniffedType.AssignLiteral(AUDIO_AAC);
    return NS_OK;
  }

  // Could not sniff the media type, we are required to set it to
  // application/octet-stream.
  aSniffedType.AssignLiteral(APPLICATION_OCTET_STREAM);
  return NS_ERROR_NOT_AVAILABLE;
}
