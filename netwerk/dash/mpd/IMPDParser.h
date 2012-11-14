/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * IMPDParser.h
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
 * Media Presentation Description (MPD) Parser.
 *
 * Interface to MPD parser. Derived classes parse MPD XML manifest and populate
 * MPD classes for use by MPD Manager.
 * Interface aims to be an abstracted API for all DASH MPD Profiles
 *     (e.g. DASH WebM On Demand Profile).
 */

#ifndef IMPDPARSER_H_
#define IMPDPARSER_H_

#include "MPD.h"

namespace mozilla {
namespace net {

class IMPDParser
{
public:
  // Parses XML file to create and populate MPD classes.
  // Called by MPD Manager.
  virtual MPD* Parse() = 0;
};

}// namespace net
}// namespace mozilla

#endif /* IMPDPARSER_H_ */
