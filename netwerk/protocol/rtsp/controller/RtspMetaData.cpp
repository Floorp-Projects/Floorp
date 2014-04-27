/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspMetaData.h"
#include "prlog.h"

using namespace mozilla;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(RtspMetaData, nsIStreamingProtocolMetaData)

RtspMetaData::RtspMetaData()
 : mIndex(0)
 , mWidth(0)
 , mHeight(0)
 , mDuration(0)
 , mSampleRate(0)
 , mCurrentTimeStamp(0)
 , mChannelCount(0)
{
  mMimeType.AssignLiteral("NONE");
}

RtspMetaData::~RtspMetaData()
{

}

nsresult
RtspMetaData::DeserializeRtspMetaData(const InfallibleTArray<RtspMetadataParam>& metaArray)
{
  nsresult rv;

  // Deserialize meta data.
  for (uint32_t i = 0; i < metaArray.Length(); i++) {
    const RtspMetaValue& value = metaArray[i].value();
    const nsCString& name = metaArray[i].name();

    if (name.EqualsLiteral("FRAMETYPE")) {
      rv = SetFrameType(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("TIMESTAMP")) {
      rv = SetTimeStamp(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("TRACKS")) {
      rv = SetTotalTracks(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if(name.EqualsLiteral("MIMETYPE")) {
      rv = SetMimeType(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("WIDTH")) {
      rv = SetWidth(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("HEIGHT")) {
      rv = SetHeight(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("SAMPLERATE")) {
      rv = SetSampleRate(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if(name.EqualsLiteral("DURATION")) {
      rv = SetDuration(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("CHANNELCOUNT")) {
      rv = SetChannelCount(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("ESDS")) {
      rv = SetEsdsData(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    } else if (name.EqualsLiteral("AVCC")) {
      rv = SetAvccData(value);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetFrameType(uint32_t *aFrameType)
{
  NS_ENSURE_ARG_POINTER(aFrameType);
  *aFrameType = mFrameType;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetFrameType(uint32_t aFrameType)
{
  mFrameType = aFrameType;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetTotalTracks(uint32_t *aTracks)
{
  NS_ENSURE_ARG_POINTER(aTracks);
  *aTracks = mTotalTracks;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetTotalTracks(uint32_t aTracks)
{
  mTotalTracks = aTracks;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetMimeType(nsACString & aMimeType)
{
  aMimeType.Assign(mMimeType);
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetMimeType(const nsACString & aMimeType)
{
  mMimeType.Assign(aMimeType);
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetWidth(uint32_t *aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = mWidth;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetWidth(uint32_t aWidth)
{
  mWidth = aWidth;
  return NS_OK;
}

/* attribute unsigned long height; */
NS_IMETHODIMP
RtspMetaData::GetHeight(uint32_t *aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetHeight(uint32_t aHeight)
{
  mHeight = aHeight;
  return NS_OK;
}

/* attribute unsigned long long duration; */
NS_IMETHODIMP
RtspMetaData::GetDuration(uint64_t *aDuration)
{
  NS_ENSURE_ARG_POINTER(aDuration);
  *aDuration = mDuration;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetDuration(uint64_t aDuration)
{
  mDuration = aDuration;
  return NS_OK;
}

/* attribute unsigned long sampleRate; */
NS_IMETHODIMP
RtspMetaData::GetSampleRate(uint32_t *aSampleRate)
{
  NS_ENSURE_ARG_POINTER(aSampleRate);
  *aSampleRate = mSampleRate;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetSampleRate(uint32_t aSampleRate)
{
  mSampleRate = aSampleRate;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetTimeStamp(uint64_t *aTimeStamp)
{
  NS_ENSURE_ARG_POINTER(aTimeStamp);
  *aTimeStamp = mCurrentTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetTimeStamp(uint64_t aTimeStamp)
{
  mCurrentTimeStamp = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetChannelCount(uint32_t *aChannelCount)
{
  NS_ENSURE_ARG_POINTER(aChannelCount);
  *aChannelCount = mChannelCount;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetChannelCount(uint32_t aChannelCount)
{
  mChannelCount = aChannelCount;
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetEsdsData(nsACString & aESDS)
{
  aESDS.Assign(mESDS);
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetEsdsData(const nsACString & aESDS)
{
  mESDS.Assign(aESDS);
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::GetAvccData(nsACString & aAVCC)
{
  aAVCC.Assign(mAVCC);
  return NS_OK;
}

NS_IMETHODIMP
RtspMetaData::SetAvccData(const nsACString & aAVCC)
{
  mAVCC.Assign(aAVCC);
  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla
