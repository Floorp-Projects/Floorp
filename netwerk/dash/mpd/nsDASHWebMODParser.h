/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * nsDASHWebMODParser.h
 *****************************************************************************
 * Copyrigh(C) 2010 - 2012 Klagenfurt University
 *
 * Created on: May 1, 2012
 * Based on IsoffMainParser.h by:
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
 * Media Presentation Description (MPD) Parser for WebM On Demand Profile.
 *
 * Parses DOM built from MPD XML to verify data and populate MPD classes.
 * MPD classes used as a metadata source by WebM On Demand Manager.
 *
 *
 * General:
 * -- Ignore all elements with @|xlink|.
 * -- @|startWithSAP| XXX examine later - defines "Segments starting with
 *    media stream access points".
 * -- @|mimeType| for elements should be 'video/webm' or 'audio/webm'.
 *
 * |MPD|:
 * -- May have |BaseURL|.
 * -- @|type| = 'static' only.
 *
 * |Period|:
 * -- no |SegmentList| or |SegmentTemplate|; not just ignored, they shouldn't be
 *    present.
 *
 * |AdaptationSet|:
 * -- ignore this element if it has a |SegmentList| or |SegmentTemplate|.
 * -- ignore this element if @|subsegmentAlignment| is not present or is set
 *    to false.
 * To enable stream switching:
 *    -- @|subsegmentStartsWithSAP| = 1.
 *    -- @|segmentAlignment| = true.
 *    -- @|bitstreamSwitching| = true.
 * -- Can have @|width| and @|height|.
 * -- Can have @|id|.
 * -- Can have @|codecs|, but this should match appropriately with @|mimeType|.
 *
 * |Representation|:
 * -- should contain only one |Segment|, which should be 'Self-Initializing
 *    Indexed Media Segment'. Note: |Segment|s are generated from |BaseURL|
 *    elements within a |Representation|.
 * -- ignore if @|profiles| is not the same as MPD's @|profiles|.
 * -- ignore this element if it has a |SegmentList| or |SegmentTemplate|.
 * -- ignore if it does NOT have a |BaseURL|.
 * -- ignore if @|subsegmentStartsWithSAP| != 1 and has sibling |Representation|s
 *    in |AdaptationSet|.
 * -- no |IndexSegment|.
 *
 * |BaseURL|:
 * -- no specifics.
 *
 *
 */

#ifndef DASHWEBMODPARSER_H_
#define DASHWEBMODPARSER_H_

#include "nsCOMPtr.h"
#include "IMPDParser.h"
#include "MPD.h"
#include "Period.h"
#include "AdaptationSet.h"
#include "Representation.h"
#include "SegmentBase.h"

class nsIDOMElement;

namespace mozilla {
namespace net {

class nsDASHWebMODParser : public IMPDParser
{
public:
  nsDASHWebMODParser(nsIDOMElement* aRoot);
  virtual ~nsDASHWebMODParser();

  // Inherited from IMPDParser.
  MPD* Parse();

private:
  // Parses MPD tag attributes from the DOM.
  nsresult    VerifyMPDAttributes();

  // Parses the |BaseURL| to be used for all other URLs.
  nsresult    SetMPDBaseUrls(MPD* aMpd);

  // Parses all |Period|s in the DOM of the media presentation.
  nsresult    SetPeriods(MPD* aMpd);

  // Parses |AdaptationSet|s of media in the DOM node for the given |Period|.
  nsresult    SetAdaptationSets(nsIDOMElement* aPeriodNode,
                                Period* aPeriod,
                                bool &bIgnoreThisRep);

  // Validates the attributes for the given |AdaptationSet| of media.
  nsresult    ValidateAdaptationSetAttributes(nsIDOMElement* aChild,
                                              bool &bAttributesValid);

  // Parses |Representation|s in the DOM for the given |AdaptationSet|.
  nsresult    SetRepresentations(nsIDOMElement* aAdaptationSetNode,
                                 AdaptationSet* aAdaptationSet,
                                 bool &bIgnoreThisRep);

  // Parses |BaseURLs| for the given media |Representation|.
  // Note: in practise, this is combined with the |BaseURL| for the |MPD|.
  nsresult    SetRepresentationBaseUrls(nsIDOMElement* aRepNode,
                                        Representation* aRep,
                                        bool &bIgnoreThisRep);

  // Parses the |SegmentBase| for the media |Representation|.
  // For WebM OD, there is one |SegmentBase| and one |Segment| per media
  // |Representation|.
  nsresult    SetRepSegmentBase(nsIDOMElement* aRepElem,
                                Representation* aRep,
                                bool &bIgnoreThisRep);
  // Parses the |InitSegment| for the |SegmentBase|.
  // For WebM OD, the |InitSegment| is a range of bytes for each |SegmentBase|.
  nsresult    SetSegmentBaseInit(nsIDOMElement* aSegBaseElem,
                                 SegmentBase* aSegBase,
                                 bool &bIgnoreThisSegBase);

  // Gets an attribute's value from a DOM element.
  nsresult    GetAttribute(nsIDOMElement* aElem,
                           const nsAString& aAttribute,
                           nsAString& aValue);

  // Converts a subtring "<time>" in a string of format "PT<time>S" to a double.
  nsresult    GetTime(nsAString& aTimeStr, double& aTime);

  // The root DOM element of the MPD; populated after reading the XML file; used
  // to populate the MPD class structure.
  nsCOMPtr<nsIDOMElement>  mRoot;
};

}//namespace net
}//namespace mozilla

#endif /* DASHWEBMODPARSER_H_ */
