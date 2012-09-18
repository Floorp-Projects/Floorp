/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * SegmentBase.h
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

#ifndef SEGMENTBASE_H_
#define SEGMENTBASE_H_

#include "nsString.h"

namespace mozilla {
namespace net {

class SegmentBase
{
public:
  SegmentBase() :
    mInitRangeStart(0),
    mInitRangeEnd(0),
    mIndexRangeStart(0),
    mIndexRangeEnd(0)
  {
    MOZ_COUNT_CTOR(SegmentBase);
  }
  virtual ~SegmentBase()
  {
    MOZ_COUNT_DTOR(SegmentBase);
  }

  bool operator==(SegmentBase const & other) const {
    return (mInitRangeStart == other.mInitRangeStart
            && mInitRangeEnd == other.mInitRangeEnd
            && mIndexRangeStart == other.mIndexRangeStart
            && mIndexRangeEnd == other.mIndexRangeEnd);
  }
  bool operator!=(SegmentBase const & other) const {
    return !(*this == other);
  }

  // Get/Set the byte range for the initialization bytes.
  void GetInitRange(int64_t* aStartBytes, int64_t* aEndBytes) const;
  void SetInitRange(nsAString const &aRangeStr);

  // Get/Set the byte range for the index bytes.
  void GetIndexRange(int64_t* aStartBytes, int64_t* aEndBytes) const;
  void SetIndexRange(nsAString const &aRangeStr);

private:
  // Parses the string to get a start and end value.
  void SetRange(nsAString const &aRangeStr, int64_t &aStart, int64_t &aEnd);

  // Start and end values for the init byte range.
  int64_t mInitRangeStart;
  int64_t mInitRangeEnd;

  // Start and end values for the index byte range.
  int64_t mIndexRangeStart;
  int64_t mIndexRangeEnd;
};


}//namespace net
}//namespace mozilla

#endif /* SEGMENTBASE_H_ */
