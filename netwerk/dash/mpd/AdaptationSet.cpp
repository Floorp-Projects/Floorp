/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * AdaptationSet.cpp
 *****************************************************************************
 * Copyright(C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Jan 27, 2012
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 * Contributors:
 *          Steve Workman <sworkman@mozilla.com>
 *
 * This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *****************************************************************************/

/* DASH - Dynamic Adaptive Streaming over HTTP
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * |AdaptationSet|
 *
 * Describes a type of media in a |Period| of time in the media presentation,
 * e.g. an audio or video stream. Direct child of |Period|, which contains 1+
 * available pieces of media, available during that time period.
 * |AdaptationSet| itself contains one or more |Representations| which describe
 * different versions of the media, most commonly different bitrate encodings.
 *
 * Common class used by all DASH Profiles.
 * Populated by implementation of MPD Parser.
 * Used as data source by implementation of MPD Manager.
 *
 * |MPD|
 *  --> |Period|s of time.
 *       --> |AdaptationSet|s for each type or group of media content.
 *            --> |Representation|s of media, encoded with different bitrates.
 *                 --> |Segment|s of media, identified by URL (+optional byte
 *                     range.
 */

#include "AdaptationSet.h"

namespace mozilla {
namespace net {

int32_t
AdaptationSet::GetWidth() const
{
  return mWidth;
}

void
AdaptationSet::SetWidth(int32_t const aWidth)
{
  mWidth = aWidth;
}

int32_t
AdaptationSet::GetHeight() const
{
  return mHeight;
}

void
AdaptationSet::SetHeight(int32_t const aHeight)
{
  mHeight = aHeight;
}

void
AdaptationSet::GetMIMEType(nsAString& aMIMEType) const
{
  aMIMEType = mMIMEType;
}

void
AdaptationSet::SetMIMEType(nsAString const &aMIMEType)
{
  NS_ENSURE_FALSE_VOID(aMIMEType.IsEmpty());
  mMIMEType = aMIMEType;
}

Representation const *
AdaptationSet::GetRepresentation(uint32_t aIndex) const
{
  NS_ENSURE_TRUE(aIndex < mRepresentations.Length(), nullptr);
  return mRepresentations[aIndex];
}

void
AdaptationSet::AddRepresentation(Representation* aRep)
{
  NS_ENSURE_TRUE_VOID(aRep);
  // Only add if it's not already in the array.
  if (!mRepresentations.Contains(aRep)) {
    mRepresentations.InsertElementSorted(aRep, CompareRepresentationBitrates());
  }
}

uint16_t
AdaptationSet::GetNumRepresentations() const
{
  return mRepresentations.Length();
}

void
AdaptationSet::EnableBitstreamSwitching(bool aEnable)
{
  mIsBitstreamSwitching = aEnable;
}

bool
AdaptationSet::IsBitstreamSwitchingEnabled() const
{
  return mIsBitstreamSwitching;
}

}//namespace net
}//namespace mozilla
