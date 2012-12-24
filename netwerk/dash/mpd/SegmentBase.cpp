/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * SegmentBase.cpp
 *****************************************************************************
 * Copyrigh(C) 2010 - 2012 Klagenfurt University
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
 * |SegmentBase|
 *
 * Describes common initialization information for |Segment|s in a
 * |Representation|.
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

#include "nsString.h"
#include "SegmentBase.h"


namespace mozilla {
namespace net {

void
SegmentBase::GetIndexRange(int64_t* aStartBytes, int64_t* aEndBytes) const
{
  NS_ENSURE_TRUE_VOID(aStartBytes);
  NS_ENSURE_TRUE_VOID(aEndBytes);
  *aStartBytes = mIndexRangeStart;
  *aEndBytes = mIndexRangeEnd;
}

void
SegmentBase::GetInitRange(int64_t* aStartBytes, int64_t* aEndBytes) const
{
  NS_ENSURE_TRUE_VOID(aStartBytes);
  NS_ENSURE_TRUE_VOID(aEndBytes);
  *aStartBytes = mInitRangeStart;
  *aEndBytes = mInitRangeEnd;
}

void
SegmentBase::SetIndexRange(nsAString const &aRangeStr)
{
  SetRange(aRangeStr, mIndexRangeStart, mIndexRangeEnd);
}

void
SegmentBase::SetInitRange(nsAString const &aRangeStr)
{
  SetRange(aRangeStr, mInitRangeStart, mInitRangeEnd);
}

void
SegmentBase::SetRange(nsAString const &aRangeStr,
                      int64_t &aStart,
                      int64_t &aEnd)
{
  NS_ENSURE_TRUE_VOID(!aRangeStr.IsEmpty());

  nsAString::const_iterator start, end, dashStart, dashEnd;

  aRangeStr.BeginReading(start);
  aRangeStr.EndReading(end);
  dashStart = start;
  dashEnd = end;

  if (FindInReadable(NS_LITERAL_STRING("-"), dashStart, dashEnd)) {
    nsAutoString temp(Substring(start, dashStart));
    nsresult rv;
    aStart = temp.ToInteger64(&rv);
    NS_ENSURE_SUCCESS_VOID(rv);

    temp = Substring(dashEnd, end);
    aEnd = temp.ToInteger64(&rv);
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}

}//namespace net
}//namespace mozilla
