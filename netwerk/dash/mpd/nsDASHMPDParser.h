/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * nsDASHMPDParser.h
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

/* DASH - Dynamic Adaptive Streaming over HTTP.
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * see DASHDecoder.cpp for info on DASH interaction with the media engine.
 */

#ifndef __DASHMPDPARSER_H__
#define __DASHMPDPARSER_H__

#include "nsAutoPtr.h"
#include "nsNetUtil.h"
#include "nsIPrincipal.h"
#include "nsIDOMElement.h"
#include "IMPDManager.h"

namespace mozilla {
namespace net {

class nsDASHMPDParser
{
public:
  // Constructor takes pointer to MPD byte stream and length, as well as the
  // current principal and URI from which the MPD download took place.
  // Ownership of |aMPDData| should be transferred to this object.
  nsDASHMPDParser(char*         aMPDData,
                  uint32_t      aDataLength,
                  nsIPrincipal* aPrincipal,
                  nsIURI*       aURI);
 
  ~nsDASHMPDParser();

  // Parses the MPD byte stream passed in the constructor.
  // Returns a pointer to the MPDManager and the MPD profile type.
  nsresult  Parse(IMPDManager**   aMPDManager,
                  DASHMPDProfile* aProfile);
private:
  // Returns the MPD profile type given the DOM node for the root.
  nsresult  GetProfile(nsIDOMElement* aRoot,
                       DASHMPDProfile &profile);
  // Debug: Prints the DOM elements.
  void      PrintDOMElements(nsIDOMElement* aRoot);
  // Debug: Prints a single DOM element.
  void      PrintDOMElement(nsIDOMElement* aElem, int32_t aOffset);

  // Pointer to the MPD byte stream.
  nsAutoPtr<char const>   mData;
  // Length in bytes of the MPD stream.
  uint32_t                mDataLength;
  // Principal of the document.
  nsCOMPtr<nsIPrincipal>  mPrincipal;
  // URI of the MPD Document downloaded.
  nsCOMPtr<nsIURI>        mURI;
};

}//namespace net
}//namespace mozilla

#endif /* __DASHMPDPARSER_H__ */
