/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsMediaFragmentURIParser_h__)
#define nsMediaFragmentURIParser_h__

#include "nsIURI.h"
#include "nsString.h"
#include "nsRect.h"

// Class to handle parsing of a W3C media fragment URI as per
// spec at: http://www.w3.org/TR/media-frags/
// Only the temporaral URI portion of the spec is implemented.
// To use:
// a) Construct an instance with the URI containing the fragment
// b) Check for the validity of the values you are interested in
//    using e.g. HasStartTime().
// c) If the values are valid, obtain them using e.g. GetStartTime().

namespace mozilla { namespace net {

enum ClipUnit
{
  eClipUnit_Pixel,
  eClipUnit_Percent,
};

class nsMediaFragmentURIParser
{
public:
  // Create a parser with the provided URI.
  nsMediaFragmentURIParser(nsIURI* aURI);

  // True if a valid temporal media fragment indicated a start time.
  bool HasStartTime() const { return !mStart.empty(); }

  // If a valid temporal media fragment indicated a start time, returns
  // it in units of seconds. If not, defaults to 0.
  double GetStartTime() const { return mStart.ref(); }

  // True if a valid temporal media fragment indicated an end time.
  bool HasEndTime() const { return !mEnd.empty(); }

  // If a valid temporal media fragment indicated an end time, returns
  // it in units of seconds. If not, defaults to -1.
  double GetEndTime() const { return mEnd.ref(); }

  // True if a valid spatial media fragment indicated a clipping region.
  bool HasClip() const { return !mClip.empty(); }

  // True if a valid spatial media fragment indicated a resolution.
  bool HasResolution() const { return !mResolution.empty(); }

  // True if a valid spatial media fragment indicated a resolution.
  nsIntSize GetResolution() const { return mResolution.ref(); }

  // If a valid spatial media fragment indicated a clipping region,
  // returns the region. If not, returns an empty region. The unit
  // used depends on the value returned by GetClipUnit().
  nsIntRect GetClip() const { return mClip.ref(); }

  // If a valid spatial media fragment indicated a clipping region,
  // returns the unit used.
  ClipUnit GetClipUnit() const { return mClipUnit; }

private:
  // Parse the URI ref provided, looking for media fragments. This is
  // the top-level parser the invokes the others below.
  void Parse(nsACString& aRef);

  // The following methods parse the fragment as per the media
  // fragments specification. 'aString' contains the remaining
  // fragment data to be parsed. The method returns true
  // if the parse was successful and leaves the remaining unparsed
  // data in 'aString'. If the parse fails then false is returned
  // and 'aString' is left as it was when called.
  bool ParseNPT(nsDependentSubstring aString);
  bool ParseNPTTime(nsDependentSubstring& aString, double& aTime);
  bool ParseNPTSec(nsDependentSubstring& aString, double& aSec);
  bool ParseNPTFraction(nsDependentSubstring& aString, double& aFraction);
  bool ParseNPTMMSS(nsDependentSubstring& aString, double& aTime);
  bool ParseNPTHHMMSS(nsDependentSubstring& aString, double& aTime);
  bool ParseNPTHH(nsDependentSubstring& aString, uint32_t& aHour);
  bool ParseNPTMM(nsDependentSubstring& aString, uint32_t& aMinute);
  bool ParseNPTSS(nsDependentSubstring& aString, uint32_t& aSecond);
  bool ParseXYWH(nsDependentSubstring aString);
  bool ParseMozResolution(nsDependentSubstring aString);

  // Media fragment information.
  Maybe<double>    mStart;
  Maybe<double>    mEnd;
  Maybe<nsIntRect> mClip;
  ClipUnit         mClipUnit;
  Maybe<nsIntSize> mResolution;
};

}} // namespace mozilla::net

#endif
