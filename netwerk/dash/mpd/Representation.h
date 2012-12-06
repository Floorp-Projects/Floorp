/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * Representation.h
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

#ifndef REPRESENTATION_H_
#define REPRESENTATION_H_

#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "SegmentBase.h"

namespace mozilla {
namespace net {

class Representation
{
public:
  Representation() :
    mBitrate(0),
    mWidth(0),
    mHeight(0),
    mSegmentBase(nullptr)
  {
    MOZ_COUNT_CTOR(Representation);
  }
  virtual ~Representation() {
    MOZ_COUNT_DTOR(Representation);
  }

  bool operator<(const Representation &other) const {
    return this->mBitrate < other.mBitrate;
  }

  // Gets/Sets @bitrate in kbps.
  int64_t const    GetBitrate() const;
  void             SetBitrate(int64_t const aBitrate);

  // Gets/Sets @width and @height for the media if it's video.
  void             SetWidth(int32_t const aWidth);
  int32_t const    GetWidth() const;
  void             SetHeight(int32_t const aHeight);
  int32_t const    GetHeight() const;

  // Gets/Adds a |BaseURL| for the media files.
  void             AddBaseUrl(nsAString const& aUrl);
  nsAString const& GetBaseUrl(uint32_t aIndex) const;
  bool             HasBaseUrls() const { return !mBaseUrls.IsEmpty(); }

  // Gets/Sets a base |Segment| for the |Representation|.
  SegmentBase const* GetSegmentBase() const;
  // Takes ownership of |SegmentBase| to manage deletion.
  void               SetSegmentBase(SegmentBase* aBase);

private:
  // Bitrate of the media in kbps.
  int64_t mBitrate;

  // Width and height of the media if video.
  int32_t mWidth;
  int32_t mHeight;

  // List of absolute/relative |BaseURL|s which may be used to access the media.
  nsTArray<nsString> mBaseUrls;

  // The base |Segment| for the |Representation|.
  nsAutoPtr<SegmentBase> mSegmentBase;
};

// Comparator allows comparing |Representation|s based on media stream bitrate.
class CompareRepresentationBitrates
{
public:
  // Returns true if the elements are equals; false otherwise.
  // Note: |Representation| is stored as an array of |nsAutoPtr| in
  // |AdaptationSet|, but needs to be compared to regular pointers.
  // Hence the left hand side of the function being an
  // |nsAutoPtr| and the right being a regular pointer.
  bool Equals(const nsAutoPtr<Representation>& a,
              const Representation *b) const {
    return a == b;
  }

  // Returns true if (a < b); false otherwise.
  // Note: |Representation| is stored as an array of |nsAutoPtr| in
  // |AdaptationSet|, but needs to be compared to regular pointers.
  // Hence the left hand side of the function being an
  // |nsAutoPtr| and the right being a regular pointer.
  bool LessThan(const nsAutoPtr<Representation>& a,
                const Representation *b) const {
    return *a < *b;
  }
};

}//namespace net
}//namespace mozilla


#endif /* REPRESENTATION_H_ */
