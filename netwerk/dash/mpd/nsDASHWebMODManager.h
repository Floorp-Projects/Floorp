/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * nsDASHWebMODManager.h
 *****************************************************************************
 *
 * Created on: May 1, 2012
 * Based on IsoffMainManager.h by:
 *          Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 * Author:
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
 *
 * Media Presentation Description (MPD) Manager for WebM On Demand Profile.
 *                                                  ======================
 *
 * Implements MPD Manager interface to use Adaptation Algorithm to determine
 * which stream to download from WebM On Demand-based MPD.
 *
 * WebM On Demand describes a static, on demand media presentation using WebM
 * encoded media files.
 * Notes:
 *    1 time |Period| per media presentation |MPD|.
 *    1 audio stream (1 |Representation| for the audio |AdaptationSet|).
 *    1 or multiple video streams for stream switching:
 *         (multiple |Representation|s for the video |AdaptationSet|).
 *    1 file for each encoding (1 |Segment| per |Representation|).
 *    Stream switching allowed between clusters (clusters match DASH
 *         subsegments).
 *
 * Note: Adaptation algorithm is separate and passed into manager.
 * XXX Adaptation not yet implemented.
 */

#ifndef _NSDASHWEBMODMANAGER_H_
#define _NSDASHWEBMODMANAGER_H_

#include "nsTArray.h"
#include "nsIURI.h"
#include "nsString.h"
#include "MPD.h"
#include "Period.h"
#include "AdaptationSet.h"
#include "Representation.h"
#include "IMPDManager.h"

namespace mozilla {
namespace net {

class nsDASHWebMODManager : public IMPDManager
{
public:
  nsDASHWebMODManager(MPD* mpd);
  ~nsDASHWebMODManager();

  // See IMPDManager.h for descriptions of following inherited functions.
  Period const * GetFirstPeriod() const;
  uint32_t GetNumAdaptationSets() const;
  AdaptationSetType
                 GetAdaptationSetType(uint32_t const aAdaptSetIdx) const;
  uint32_t GetNumRepresentations(uint32_t const aAdaptSetIdx) const;
  Representation const * GetRepresentation(uint32_t const aAdaptSetIdx,
                                           uint32_t const aRepIdx) const;
  nsresult GetFirstSegmentUrl(uint32_t const aAdaptSetIdx,
                              uint32_t const aRepIdx,
                              nsAString &aUrl) const;
  double GetStartTime() const;
  double GetDuration() const;

  // Gets index of the |Representation| with next highest bitrate to the
  // estimated bandwidth passed in. Returns true if there is at least one
  // |Representation| with a bitrate lower than |aBandwidth|; otherwise returns
  // false. Depends on |mRepresentations| being an ordered list.
  bool GetBestRepForBandwidth(uint32_t aAdaptSetIdx,
                              uint64_t aBandwidth,
                              uint32_t &aRepIdx) const MOZ_OVERRIDE;

private:
  // Internal helper functions.
  AdaptationSet const * GetAdaptationSet(uint32_t const aAdaptSetIdx) const;
  AdaptationSetType GetAdaptationSetType(nsAString const &mimeType) const;
  uint32_t GetNumSegments(Representation const* aRep) const;

  // Pointer to the MPD class structure; holds data parsed from the MPD file.
  nsAutoPtr<MPD> mMpd;
};

}//namespace net
}//namespace mozilla

#endif /* _NSDASHWEBMODMANAGER_H_ */
