/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * Period.h
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
#ifndef PERIOD_H_
#define PERIOD_H_

#include "nsTArray.h"
#include "AdaptationSet.h"
#include "Representation.h"

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

namespace mozilla {
namespace net {

class Period
{
public:
  Period()
  {
    MOZ_COUNT_CTOR(Period);
  }
  virtual ~Period() {
    MOZ_COUNT_DTOR(Period);
  }

  // Gets/Adds |AdaptationSet|s of media for this media content |Period|.
  AdaptationSet const * GetAdaptationSet(uint32_t aIndex) const;
  // |Period| takes ownership of |AdaptationSet| here and will manage deletion.
  void                  AddAdaptationSet(AdaptationSet* aAdaptationSet);

  // Returns the num. of |AdaptationSet|s in this media content |Period|.
  uint16_t const        GetNumAdaptationSets() const;

  // Gets/Sets the start time of this media content |Period| in seconds.
  double const GetStart() const;
  void SetStart(double const aStart);

  // Gets/Sets the duration of this media content |Period| in seconds.
  double const GetDuration() const;
  void SetDuration(double const aDuration);

private:
  // List of |AdaptationSet|s of media in this |Period|.
  nsTArray<nsAutoPtr<AdaptationSet> > mAdaptationSets;

  // Start time in seconds for this |Period|.
  double mStart;

  // Duration in seconds for this |Period|.
  double mDuration;
};

}//namespace net
}//namespace mozilla


#endif /* PERIOD_H_ */
