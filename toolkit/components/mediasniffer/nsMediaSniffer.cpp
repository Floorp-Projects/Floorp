/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ADTSDemuxer.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Telemetry.h"
#include "mp3sniff.h"
#include "nestegg/nestegg.h"
#include "nsHttpChannel.h"
#include "nsIClassInfoImpl.h"
#include "nsIChannel.h"
#include "nsMediaSniffer.h"
#include "nsMimeTypes.h"
#include "nsQueryObject.h"
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
// Multi-channel low sample-rate AAC packets can be huge, have a higher maximum
// size.
static const uint32_t MAX_BYTES_SNIFFED_ADTS = 8096;

NS_IMPL_ISUPPORTS(nsMediaSniffer, nsIContentSniffer)

nsMediaSnifferEntry nsMediaSniffer::sSnifferEntries[] = {
    // The string OggS, followed by the null byte.
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF\xFF", "OggS", APPLICATION_OGG),
    // The string RIFF, followed by four bytes, followed by the string WAVE,
    // followed by 8 bytes, followed by 0x0055, the codec identifier for mp3 in
    // a RIFF container. This entry MUST be before the next one, which is
    // assumed to be a WAV file containing PCM data.
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF"
                  "\x00\x00\x00\x00"
                  "\xFF\xFF\xFF\xFF"
                  "\x00\x00\x00\x00"
                  "\x00\x00\x00\x00"
                  "\xFF\xFF",
                  "RIFF"
                  "\x00\x00\x00\x00"
                  "WAVE"
                  "\x00\x00\x00\x00"
                  "\x00\x00\x00\x00"
                  "\x55\x00",
                  AUDIO_MP3),
    // The string RIFF, followed by four bytes, followed by the string WAVE
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF",
                  "RIFF\x00\x00\x00\x00WAVE", AUDIO_WAV),
    // mp3 with ID3 tags, the string "ID3".
    PATTERN_ENTRY("\xFF\xFF\xFF", "ID3", AUDIO_MP3),
    // FLAC with standard header
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "fLaC", AUDIO_FLAC),
    PATTERN_ENTRY("\xFF\xFF\xFF\xFF\xFF\xFF\xFF", "#EXTM3U",
                  APPLICATION_MPEGURL)};

using PatternLabel = mozilla::Telemetry::LABELS_MEDIA_SNIFFER_MP4_BRAND_PATTERN;

struct nsMediaSnifferFtypEntry : nsMediaSnifferEntry {
  nsMediaSnifferFtypEntry(nsMediaSnifferEntry aBase, const PatternLabel aLabel)
      : nsMediaSnifferEntry(aBase), mLabel(aLabel) {}
  PatternLabel mLabel;
};

// For a complete list of file types, see http://www.ftyps.com/index.html
nsMediaSnifferFtypEntry sFtypEntries[] = {
    {PATTERN_ENTRY("\xFF\xFF\xFF", "mp4", VIDEO_MP4),
     PatternLabel::ftyp_mp4},  // Could be mp41 or mp42.
    {PATTERN_ENTRY("\xFF\xFF\xFF", "avc", VIDEO_MP4),
     PatternLabel::ftyp_avc},  // Could be avc1, avc2, ...
    {PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "3gp4", VIDEO_MP4),
     PatternLabel::ftyp_3gp4},  // 3gp4 is based on MP4
    {PATTERN_ENTRY("\xFF\xFF\xFF", "3gp", VIDEO_3GPP),
     PatternLabel::ftyp_3gp},  // Could be 3gp5, ...
    {PATTERN_ENTRY("\xFF\xFF\xFF", "M4V", VIDEO_MP4), PatternLabel::ftyp_M4V},
    {PATTERN_ENTRY("\xFF\xFF\xFF", "M4A", AUDIO_MP4), PatternLabel::ftyp_M4A},
    {PATTERN_ENTRY("\xFF\xFF\xFF", "M4P", AUDIO_MP4), PatternLabel::ftyp_M4P},
    {PATTERN_ENTRY("\xFF\xFF", "qt", VIDEO_QUICKTIME), PatternLabel::ftyp_qt},
    {PATTERN_ENTRY("\xFF\xFF\xFF", "crx", APPLICATION_OCTET_STREAM),
     PatternLabel::ftyp_crx},
    {PATTERN_ENTRY("\xFF\xFF\xFF", "iso", VIDEO_MP4),
     PatternLabel::ftyp_iso},  // Could be isom or iso2.
    {PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "mmp4", VIDEO_MP4),
     PatternLabel::ftyp_mmp4},
    {PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "avif", IMAGE_AVIF),
     PatternLabel::ftyp_avif},
};

static bool MatchesBrands(const uint8_t aData[4], nsACString& aSniffedType) {
  for (const auto& currentEntry : sFtypEntries) {
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
      // If we eventually remove the "iso" pattern entry per bug 1725190,
      // this block should be removed and the bug1725190.cr3 test in
      // test_mediasniffer_ext.js will need to be updated
      if (!mozilla::StaticPrefs::media_mp4_sniff_iso_brand() &&
          currentEntry.mLabel == PatternLabel::ftyp_iso) {
        continue;
      }

      aSniffedType.AssignASCII(currentEntry.mContentType);
      AccumulateCategorical(currentEntry.mLabel);
      return true;
    }
  }

  return false;
}

// This function implements sniffing algorithm for MP4 family file types,
// including MP4 (described at
// http://mimesniff.spec.whatwg.org/#signature-for-mp4), M4A (Apple iTunes
// audio), and 3GPP.
bool MatchesMP4(const uint8_t* aData, const uint32_t aLength,
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
  return nestegg_sniff((uint8_t*)aData, aLength);
}

// This function implements mp3 sniffing based on parsing
// packet headers and looking for expected boundaries.
static bool MatchesMP3(const uint8_t* aData, const uint32_t aLength) {
  return mp3_sniff(aData, (long)aLength);
}

static bool MatchesADTS(const uint8_t* aData, const uint32_t aLength) {
  return mozilla::ADTSDemuxer::ADTSSniffer(aData, aLength);
}

NS_IMETHODIMP
nsMediaSniffer::GetMIMETypeFromContent(nsIRequest* aRequest,
                                       const uint8_t* aData,
                                       const uint32_t aLength,
                                       nsACString& aSniffedType) {
  const uint32_t clampedLength = std::min(aLength, MAX_BYTES_SNIFFED);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

  auto maybeUpdate = mozilla::MakeScopeExit([channel]() {
    if (channel && XRE_IsParentProcess()) {
      if (RefPtr<mozilla::net::nsHttpChannel> httpChannel =
              do_QueryObject(channel)) {
        // If the audio or video type pattern matching algorithm given bytes
        // does not return undefined, then disable the further check and allow
        // the response.
        httpChannel->DisableIsOpaqueResponseAllowedAfterSniffCheck(
            mozilla::net::nsHttpChannel::SnifferType::Media);
      }
    };
  });

  // Check if this is a toplevel document served as application/octet-stream
  // to disable sniffing and allow the file to download. See: Bug 1828441
  if (channel && XRE_IsParentProcess()) {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    nsAutoCString mimeType;
    channel->GetContentType(mimeType);
    if (mimeType.EqualsLiteral(APPLICATION_OCTET_STREAM) &&
        loadInfo->GetExternalContentPolicyType() ==
            ExtContentPolicy::TYPE_DOCUMENT) {
      aSniffedType.AssignLiteral(APPLICATION_OCTET_STREAM);
      maybeUpdate.release();
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  for (const auto& currentEntry : sSnifferEntries) {
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

  if (MatchesADTS(aData, std::min(aLength, MAX_BYTES_SNIFFED_ADTS))) {
    aSniffedType.AssignLiteral(AUDIO_AAC);
    return NS_OK;
  }

  maybeUpdate.release();

  // Could not sniff the media type, we are required to set it to
  // application/octet-stream.
  aSniffedType.AssignLiteral(APPLICATION_OCTET_STREAM);
  return NS_ERROR_NOT_AVAILABLE;
}
