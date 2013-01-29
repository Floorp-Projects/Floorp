/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts =2 et sw =2 tw =80: */
/*
 * IMPDManager.h
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

/* DASH - Dynamic Adaptive Streaming over HTTP.
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * (see DASHDecoder.cpp for info on DASH interaction with the media engine).
 *
 * Media Presentation Description (MPD) Manager.
 *
 * Interface to MPD classes, populated with data from MPD XML manifest.
 * Employs adaptation algorithm to determine best representation of media to
 * download.
 *   Adaptation algorithm is separate and passed into manager.
 *   Interface aims to be an abstracted API for all DASH MPD Profiles
 *     (e.g. DASH WebM On Demand Profile).
 */

#ifndef IMPDMANAGER_H_
#define IMPDMANAGER_H_

#include "nsTArray.h"
#include "nsIURI.h"
#include "nsIDOMElement.h"

namespace mozilla {
namespace net {

// DASH MPD Profile Type
enum DASHMPDProfile
{
  WebMOnDemand,
  NotValid
  // Extend this for other types.
};

class Period;
class Representation;
class Segment;

class IMPDManager
{
public:
  // Describes the media type (audio/video) of the |AdaptationSet|.
  enum AdaptationSetType {
    DASH_ASTYPE_INVALID = 0,
    DASH_VIDEO_STREAM,
    DASH_AUDIO_STREAM,
    DAHS_AUDIO_VIDEO_STREAM
  };
  IMPDManager()
  {
    MOZ_COUNT_CTOR(IMPDManager);
  }

  virtual ~IMPDManager()
  {
    MOZ_COUNT_DTOR(IMPDManager);
  }

  // Used to get the first |Period| in the presentation.
  virtual Period const * GetFirstPeriod() const = 0;

  // Gets the total number of |AdaptationSet|s in the first |Period|.
  // Usually, this should be 2 for audio and video.
  // XXX Future versions may require a |Period| index.
  // XXX Future versions may have multiple tracks for audio.
  virtual uint32_t GetNumAdaptationSets() const = 0;

  // Returns the media type for the given |AdaptationSet|, audio/video.
  virtual AdaptationSetType
          GetAdaptationSetType(uint32_t const aAdaptSetIdx) const = 0;

  // Gets the number of media |Representation|s for the given |AdaptationSet|.
  // e.g how many bitrate encodings are there of the audio stream?
  virtual uint32_t
          GetNumRepresentations(uint32_t const aAdaptSetIdx) const = 0;

  // Gets the specified |Representation| from the specified |AdaptationSet|,
  // e.g. get metadata about the 64Kbps encoding of the video stream.
  virtual Representation const *
          GetRepresentation(uint32_t const aAdaptSetIdx,
                            uint32_t const aRepIdx) const = 0;

  // Gets the URL of the first media |Segment| for the specific media
  // |Representation|, e.g. the url of the first 64Kbps video segment.
  virtual nsresult GetFirstSegmentUrl(uint32_t const aAdaptSetIdx,
                                      uint32_t const aRepIdx,
                                      nsAString &aUrl) const = 0;

  // Returns the start time of the presentation in seconds.
  virtual double GetStartTime() const = 0;

  // Returns the duration of the presentation in seconds.
  virtual double GetDuration() const = 0;

  // Gets index of the |Representation| with next highest bitrate to the
  // estimated bandwidth passed in. Returns true if there is at least one
  // |Representation| with a bitrate lower than |aBandwidth|; otherwise returns
  // false. Depends on |mRepresentations| being an ordered list.
  virtual bool GetBestRepForBandwidth(uint32_t aAdaptSetIdx,
                                      uint64_t aBandwidth,
                                      uint32_t &aRepIdx) const = 0;
public:
  // Factory method.
  static IMPDManager* Create(DASHMPDProfile Profile, nsIDOMElement* aRoot);

private:
  // Used by factory method.
  static IMPDManager* CreateWebMOnDemandManager(nsIDOMElement* aRoot);
};

}//namespace net
}//namespace mozilla

#endif /* IMPDMANAGER_H_ */
