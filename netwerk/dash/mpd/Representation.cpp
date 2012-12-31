/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * Representation.cpp
 *****************************************************************************
 * Copyrigh(C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
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
 * |Representation|
 *
 * Describes a particular version of a piece of media described in an
 * |AdaptationSet|, a common example being a particular bitrate encoding for an
 * audio or video stream. Direct child of |AdaptationSet|, which contains 1+
 * available |Representation|s of the media.
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

#include "nsTArray.h"
#include "Representation.h"

namespace mozilla {
namespace net {

int64_t const
Representation::GetBitrate() const
{
  return mBitrate;
}

void
Representation::SetBitrate(int64_t aBitrate)
{
  mBitrate = aBitrate;
}

void
Representation::SetWidth(int32_t const aWidth)
{
  mWidth = aWidth;
}

int32_t const
Representation::GetWidth() const
{
  return mWidth;
}

void
Representation::SetHeight(int32_t aHeight)
{
  mHeight = aHeight;
}

int32_t const
Representation::GetHeight() const
{
  return mHeight;
}

void
Representation::AddBaseUrl(nsAString const& aUrl)
{
  NS_ENSURE_FALSE_VOID(aUrl.IsEmpty());
  // Only add if it's not already in the array.
  if (!mBaseUrls.Contains(aUrl)) {
    mBaseUrls.AppendElement(aUrl);
  }
}

nsAString const &
Representation::GetBaseUrl(uint32_t aIndex) const
{
  NS_ENSURE_TRUE(aIndex < mBaseUrls.Length(), NS_LITERAL_STRING(""));
  return mBaseUrls[aIndex];
}

SegmentBase const*
Representation::GetSegmentBase() const
{
  return mSegmentBase;
}

void
Representation::SetSegmentBase(SegmentBase* aBase)
{
  NS_ENSURE_TRUE_VOID(aBase);
  // Don't reassign if the ptrs or contents are equal.
  if (mSegmentBase != aBase
      || (mSegmentBase && (*mSegmentBase != *aBase))) {
    mSegmentBase = aBase;
  }
}

}//namespace net
}//namespace mozilla
