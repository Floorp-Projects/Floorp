/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * MPD.cpp
 *****************************************************************************
 * Copyright(C) 2010 - 2011 Klagenfurt University
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
 * |MPD| - Media Presentation Description
 *
 * Describes the media presentation. Top of the hierarchy in an MPD file.
 * Contains one or a series of contiguous |Period|s, which contain 1+ available
 * pieces of media, available during that time period, e.g. audio in various
 * languages, a video component.
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
#include "MPD.h"


namespace mozilla {
namespace net {

void
MPD::AddPeriod(Period* aPeriod)
{
  NS_ENSURE_TRUE_VOID(aPeriod);
  // Only add |Period| if it's not in the array already.
  if (!mPeriods.Contains(aPeriod)) {
    mPeriods.AppendElement(aPeriod);
  }
}

Period const *
MPD::GetPeriod(uint32_t aIndex) const
{
  NS_ENSURE_TRUE(aIndex < mPeriods.Length(), nullptr);
  return mPeriods[aIndex];
}

uint32_t const
MPD::GetNumPeriods() const
{
  return mPeriods.Length();
}

void
MPD::AddBaseUrl(nsAString const& aUrl)
{
  NS_ENSURE_FALSE_VOID(aUrl.IsEmpty());
  // Only add |BaseUrl| string if it's not in the array already.
  if (!mBaseUrls.Contains(aUrl)) {
    mBaseUrls.AppendElement(aUrl);
  }
}

nsAString const&
MPD::GetBaseUrl(uint32_t aIndex) const
{
  NS_ENSURE_TRUE(aIndex < mBaseUrls.Length(), NS_LITERAL_STRING(""));
  return mBaseUrls[aIndex];
}

}//namespace net
}//namespace mozilla
