/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * IMPDManager.cpp
 *****************************************************************************
 * Copyrigh(C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Apr 20, 2011
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 * Contributors:
 *          Steve Workman <sworkman@mozilla.com>
 *
 * This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *****************************************************************************/

#include "nsIDOMElement.h"
#include "IMPDManager.h"
#include "nsDASHWebMODManager.h"
#include "nsDASHWebMODParser.h"


namespace mozilla {
namespace net {

/* static */
IMPDManager*
IMPDManager::Create(DASHMPDProfile aProfile, nsIDOMElement* aRoot)
{
  switch(aProfile)
  {
    case WebMOnDemand:
      return CreateWebMOnDemandManager(aRoot);
    default:
      return nullptr;
  }
}

/* static */
IMPDManager*
IMPDManager::CreateWebMOnDemandManager(nsIDOMElement* aRoot)
{
  // Parse DOM elements into MPD objects.
  nsDASHWebMODParser parser(aRoot);

  return new nsDASHWebMODManager(parser.Parse());
}



}//namespace net
}//namespace mozilla
