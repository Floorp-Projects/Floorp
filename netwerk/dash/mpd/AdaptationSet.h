/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * AdaptationSet.h
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

#ifndef ADAPTATIONSET_H_
#define ADAPTATIONSET_H_

#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "Representation.h"

namespace mozilla {
namespace net {

class AdaptationSet
{
public:
  AdaptationSet() :
    mWidth(0),
    mHeight(0),
    mIsBitstreamSwitching(false)
  {
    MOZ_COUNT_CTOR(AdaptationSet);
  }
  virtual ~AdaptationSet() {
    MOZ_COUNT_DTOR(AdaptationSet);
  }

  // Setters and getters for @width, @height and @mimetype.
  int32_t  GetWidth() const;
  void     SetWidth(int32_t const aWidth);
  int32_t  GetHeight() const;
  void     SetHeight(int32_t const aHeight);
  void     GetMIMEType(nsAString& aMIMEType) const;
  void     SetMIMEType(nsAString const &aMIMEType);

  // Gets a list of media |Representation| objects for this |AdaptationSet|.
  Representation const * GetRepresentation(uint32_t) const;

  // Adds a media |Representation| to this |AdaptationSet|. Takes ownership to
  // manage deletion.
  void     AddRepresentation(Representation* aRep);

  // Returns the number of media |Representations|.
  uint16_t GetNumRepresentations() const;

  // En/Dis-ables switching between media |Representation|s.
  void EnableBitstreamSwitching(bool const aEnable);
  bool IsBitstreamSwitchingEnabled() const;

private:
  // Array of media |Representations| to switch between.
  // Ordered list, ascending in order of bitrates.
  nsTArray<nsAutoPtr<Representation> >  mRepresentations;

  // @width, height and @mimetype of this media stream.
  int32_t                    mWidth;
  int32_t                    mHeight;
  nsString                   mMIMEType;

  // If true, switching between media |Representation|s is allowed.
  bool                       mIsBitstreamSwitching;
};
}
}

#endif /* ADAPTATIONSET_H_ */
