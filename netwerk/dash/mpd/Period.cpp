/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * Period.cpp
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
 * |Period|
 *
 * Describes a period of time in the media presentation. Direct child of |MPD|.
 * Alone, or one of a series of contiguous |Period|s, which contain 1+ available
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

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "Period.h"

namespace mozilla {
namespace net {

AdaptationSet const *
Period::GetAdaptationSet(uint32_t aIndex) const
{
  NS_ENSURE_TRUE(aIndex < mAdaptationSets.Length(), nullptr);
  return mAdaptationSets[aIndex];
}

void
Period::AddAdaptationSet(AdaptationSet* aAdaptationSet)
{
  NS_ENSURE_TRUE_VOID(aAdaptationSet);
  // Only add |AdaptationSet| ptr if it's not in the array already.
  if (!mAdaptationSets.Contains(aAdaptationSet)) {
    mAdaptationSets.AppendElement(aAdaptationSet);
  }
}

uint16_t const
Period::GetNumAdaptationSets() const
{
  return mAdaptationSets.Length();
}

double const
Period::GetStart() const
{
  return mStart;
}

double const
Period::GetDuration() const
{
  return mDuration;
}

void
Period::SetStart(double const aStart)
{
  mStart = aStart;
}

void
Period::SetDuration(double const aDuration)
{
  mDuration = aDuration;
}

}//namespace net
}//namespace mozilla
