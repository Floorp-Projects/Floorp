/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMediaSniffer.h"
#include "nsMemory.h"
#include "nsIHttpChannel.h"
#include "nsString.h"
#include "nsMimeTypes.h"
#include "mozilla/ModuleUtils.h"

#include "nsIClassInfoImpl.h"
#include <algorithm>

// The minimum number of bytes that are needed to attempt to sniff an mp4 file.
static const unsigned MP4_MIN_BYTES_COUNT = 12;
// The maximum number of bytes to consider when attempting to sniff a file.
static const uint32_t MAX_BYTES_SNIFFED = 512;

NS_IMPL_ISUPPORTS1(nsMediaSniffer, nsIContentSniffer)

nsMediaSniffer::nsMediaSnifferEntry nsMediaSniffer::sSnifferEntries[] = {
  // The string OggS, followed by the null byte.
  PATTERN_ENTRY("\xFF\xFF\xFF\xFF\xFF", "OggS", APPLICATION_OGG),
  // The string RIFF, followed by four bytes, followed by the string WAVE
  PATTERN_ENTRY("\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF", "RIFF\x00\x00\x00\x00WAVE", AUDIO_WAV),
  // WebM
  PATTERN_ENTRY("\xFF\xFF\xFF\xFF", "\x1A\x45\xDF\xA3", VIDEO_WEBM),
  // mp3 with ID3 tags, the string "ID3".
  PATTERN_ENTRY("\xFF\xFF\xFF", "ID3", AUDIO_MP3)
};

// This function implements mp4 sniffing algorithm, described at
// http://mimesniff.spec.whatwg.org/#signature-for-mp4
static bool MatchesMP4(const uint8_t* aData, const uint32_t aLength)
{
  if (aLength <= MP4_MIN_BYTES_COUNT) {
    return false;
  }
  // Conversion from big endian to host byte order.
  uint32_t boxSize = (uint32_t)(aData[3] | aData[2] << 8 | aData[1] << 16 | aData[0] << 24);

  // Boxsize should be evenly divisible by 4.
  if (boxSize % 4 || aLength < boxSize) {
    return false;
  }
  // The string "ftyp".
  if (aData[4] != 0x66 ||
      aData[5] != 0x74 ||
      aData[6] != 0x79 ||
      aData[7] != 0x70) {
    return false;
  }
  for (uint32_t i = 2; i <= boxSize / 4 - 1 ; i++) {
    if (i == 3) {
      continue;
    }
    // The string "mp4".
    if (aData[4*i]   == 0x6D &&
        aData[4*i+1] == 0x70 &&
        aData[4*i+2] == 0x34) {
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
nsMediaSniffer::GetMIMETypeFromContent(nsIRequest* aRequest,
                                       const uint8_t* aData,
                                       const uint32_t aLength,
                                       nsACString& aSniffedType)
{
  // For media, we want to sniff only if the Content-Type is unknown, or if it
  // is application/octet-stream.
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    nsAutoCString contentType;
    nsresult rv = channel->GetContentType(contentType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!contentType.IsEmpty() &&
        !contentType.EqualsLiteral(APPLICATION_OCTET_STREAM) &&
        !contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  const uint32_t clampedLength = std::min(aLength, MAX_BYTES_SNIFFED);

  for (uint32_t i = 0; i < NS_ARRAY_LENGTH(sSnifferEntries); ++i) {
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

  if (MatchesMP4(aData, clampedLength)) {
    aSniffedType.AssignLiteral(VIDEO_MP4);
    return NS_OK;
  }

  // Could not sniff the media type, we are required to set it to
  // application/octet-stream.
  aSniffedType.AssignLiteral(APPLICATION_OCTET_STREAM);
  return NS_ERROR_NOT_AVAILABLE;
}
