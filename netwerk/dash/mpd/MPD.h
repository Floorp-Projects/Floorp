/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * MPD.h
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

#ifndef MPD_H_
#define MPD_H_

#include "nsTArray.h"
#include "nsString.h"
#include "Period.h"

namespace mozilla {
namespace net {

class MPD
{
public:
  MPD()
  {
    MOZ_COUNT_CTOR(MPD);
  }
  virtual ~MPD() {
    MOZ_COUNT_DTOR(MPD);
  }

  // At least one media content |Period|s per Media Presentation. The |MPD|
  // contains 1+ available pieces of media, available during that time period
  // e.g. audio in various languages, a video component.
  // |MPD| takes ownership of |Period| in |AddPeriod| and will manage deletion.
  void           AddPeriod(Period* aPeriod);
  Period const * GetPeriod(uint32_t aIndex) const;
  uint32_t const GetNumPeriods() const;

  // Adds/Gets an absolute/relative |BaseURL| for the whole document.
  // Note: A relative |BaseURL| for the whole document will use the URL for the
  // MPD file itself as base.
  void             AddBaseUrl(nsAString const& aUrl);
  nsAString const& GetBaseUrl(uint32_t aIndex) const;
  bool             HasBaseUrls() { return !mBaseUrls.IsEmpty(); }

private:
  // List of media content |Period|s in this media presentation.
  nsTArray<nsAutoPtr<Period> > mPeriods;

  // List of |BaseURL|s which can be used to access the media.
  nsTArray<nsString> mBaseUrls;
};

}//namespace net
}//namespace mozilla

#endif /* MPD_H_ */
