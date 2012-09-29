/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * nsDASHWebMODManager.cpp
 *****************************************************************************
 * Copyrigh(C) 2010 - 2012 Klagenfurt University
 *
 * Created on: May 1, 2012
 * Based on IsoffMainManager.cpp by:
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
 * (see nsDASHDecoder.cpp for info on DASH interaction with the media engine).
 *
 * Media Presentation Description (MPD) Manager for WebM On Demand Profile.
 *
 * Implements MPD Manager interface to use Adaptation Algorithm to determine
 * which stream to download from WebM On Demand-based MPD.
 *
 *   Note: Adaptation algorithm is separate and passed into manager.
 */

#include "nsTArray.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "SegmentBase.h"
#include "nsDASHWebMODManager.h"

namespace mozilla {
namespace net {

#if defined(PR_LOGGING)
static PRLogModuleInfo* gnsDASHWebMODManagerLog = nullptr;
#define LOG(msg, ...) \
        PR_LOG(gnsDASHWebMODManagerLog, PR_LOG_DEBUG, \
               ("%p [nsDASHWebMODManager] " msg, this, __VA_ARGS__))
#define LOG1(msg) PR_LOG(gDASHMPDParserLog, PR_LOG_DEBUG, \
                         ("%p [nsDASHWebMODManager] " msg, this))
#else
#define LOG(msg, ...)
#define LOG1(msg)
#endif

nsDASHWebMODManager::nsDASHWebMODManager(MPD* aMpd)
{
  MOZ_COUNT_CTOR(nsDASHWebMODManager);
  NS_ENSURE_TRUE(aMpd,);
  mMpd = aMpd;
#if defined(PR_LOGGING)
  if(!gnsDASHWebMODManagerLog)
    gnsDASHWebMODManagerLog = PR_NewLogModule("nsDASHWebMODManager");
#endif
  LOG("Created nsDASHWebMODManager with mMpd(%p)", mMpd.get());
}

nsDASHWebMODManager::~nsDASHWebMODManager()
{
  MOZ_COUNT_DTOR(nsDASHWebMODManager);
}


Period const *
nsDASHWebMODManager::GetFirstPeriod() const
{
  NS_ENSURE_TRUE(mMpd, nullptr);
  NS_ENSURE_TRUE(0 < mMpd->GetNumPeriods(), nullptr);
  return mMpd->GetPeriod(0);
}

nsresult
nsDASHWebMODManager::GetFirstSegmentUrl(uint32_t const aAdaptSetIdx,
                                        uint32_t const aRepIdx,
                                        nsAString &aUrl) const
{
  NS_ENSURE_TRUE(mMpd, NS_ERROR_NULL_POINTER);
  // Append base URL for MPD.
  if (mMpd->HasBaseUrls()) {
    aUrl.Append(mMpd->GetBaseUrl(0));
    LOG("BaseUrl \"%s\"", NS_ConvertUTF16toUTF8(aUrl).get());
  }

  // Append base URL for Representation.
  AdaptationSet const * adaptSet = GetAdaptationSet(aAdaptSetIdx);
  NS_ENSURE_TRUE(adaptSet, NS_ERROR_NULL_POINTER);

  NS_ENSURE_TRUE(aRepIdx < adaptSet->GetNumRepresentations(),
                 NS_ERROR_ILLEGAL_VALUE);
  Representation const * rep = adaptSet->GetRepresentation(aRepIdx);
  NS_ENSURE_TRUE(rep, NS_ERROR_NULL_POINTER);

  if (rep->HasBaseUrls()) {
    aUrl.Append(rep->GetBaseUrl(0));
    LOG("Appending \"%s\"",
        NS_ConvertUTF16toUTF8(rep->GetBaseUrl(0)).get());
  }

  return NS_OK;
}

uint32_t
nsDASHWebMODManager::GetNumAdaptationSets() const
{
  Period const * current = GetFirstPeriod();
  return current ? current->GetNumAdaptationSets() : 0;
}

IMPDManager::AdaptationSetType
nsDASHWebMODManager::GetAdaptationSetType(uint32_t const aAdaptSetIdx) const
{
  AdaptationSet const * adaptSet = GetAdaptationSet(aAdaptSetIdx);
  NS_ENSURE_TRUE(adaptSet, DASH_ASTYPE_INVALID);

  nsAutoString mimeType;
  adaptSet->GetMIMEType(mimeType);
  NS_ENSURE_TRUE(!mimeType.IsEmpty(), DASH_ASTYPE_INVALID);

  return GetAdaptationSetType(mimeType);
}

IMPDManager::AdaptationSetType
nsDASHWebMODManager::GetAdaptationSetType(nsAString const & aMimeType) const
{
  NS_ENSURE_TRUE(!aMimeType.IsEmpty(), DASH_ASTYPE_INVALID);

  if (aMimeType == NS_LITERAL_STRING(VIDEO_WEBM)) {
    return DASH_VIDEO_STREAM;
  } else if (aMimeType == NS_LITERAL_STRING(AUDIO_WEBM)) {
    return DASH_AUDIO_STREAM;
  } else {
    return DASH_ASTYPE_INVALID;
  }
}

uint32_t
nsDASHWebMODManager::GetNumRepresentations(uint32_t const aAdaptSetIdx) const
{
  AdaptationSet const * adaptSet = GetAdaptationSet(aAdaptSetIdx);
  NS_ENSURE_TRUE(adaptSet, DASH_ASTYPE_INVALID);

  return adaptSet->GetNumRepresentations();
}

AdaptationSet const *
nsDASHWebMODManager::GetAdaptationSet(uint32_t const aAdaptSetIdx) const
{
  Period const * current = GetFirstPeriod();
  NS_ENSURE_TRUE(current, nullptr);

  NS_ENSURE_TRUE(0 < current->GetNumAdaptationSets(), nullptr);
  NS_ENSURE_TRUE(aAdaptSetIdx < current->GetNumAdaptationSets(), nullptr);
  AdaptationSet const * adaptSet = current->GetAdaptationSet(aAdaptSetIdx);
  NS_ENSURE_TRUE(adaptSet, nullptr);
  return adaptSet;
}

Representation const *
nsDASHWebMODManager::GetRepresentation(uint32_t const aAdaptSetIdx,
                                       uint32_t const aRepIdx) const
{
  AdaptationSet const * adaptSet = GetAdaptationSet(aAdaptSetIdx);
  NS_ENSURE_TRUE(adaptSet, nullptr);

  NS_ENSURE_TRUE(aRepIdx < adaptSet->GetNumRepresentations(), nullptr);
  Representation const * rep = adaptSet->GetRepresentation(aRepIdx);
  NS_ENSURE_TRUE(rep, nullptr);

  return rep;
}

double
nsDASHWebMODManager::GetStartTime() const
{
  Period const * current = GetFirstPeriod();
  NS_ENSURE_TRUE(current, -1);

  return current->GetStart();
}

double
nsDASHWebMODManager::GetDuration() const
{
  Period const * current = GetFirstPeriod();
  NS_ENSURE_TRUE(current, -1);

  return current->GetDuration();
}

}//namespace net
}//namespace mozilla

