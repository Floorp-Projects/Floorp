/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
 * nsDASHWebMODParser.cpp
 *****************************************************************************
 * Copyrigh(C) 2010 - 2012 Klagenfurt University
 *
 * Created on: May 1, 2012
 * Based on IsoffMainParser.cpp by:
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
 */

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsMimeTypes.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "prlog.h"
#include "nsDASHWebMODParser.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo* gnsDASHWebMODParserLog = nullptr;
#define LOG(msg, ...) \
        PR_LOG(gnsDASHWebMODParserLog, PR_LOG_DEBUG, \
               ("%p [nsDASHWebMODParser] " msg, this, __VA_ARGS__))
#define LOG1(msg) \
        PR_LOG(gnsDASHWebMODParserLog, PR_LOG_DEBUG, \
               ("%p [nsDASHWebMODParser] " msg, this))
#else
#define LOG(msg, ...)
#define LOG1(msg)
#endif

namespace mozilla {
namespace net {

nsDASHWebMODParser::nsDASHWebMODParser(nsIDOMElement* aRoot) :
  mRoot(aRoot)
{
  MOZ_COUNT_CTOR(nsDASHWebMODParser);
#if defined(PR_LOGGING)
  if(!gnsDASHWebMODParserLog)
    gnsDASHWebMODParserLog = PR_NewLogModule("nsDASHWebMODParser");
#endif
  LOG1("Created nsDASHWebMODParser");
}

nsDASHWebMODParser::~nsDASHWebMODParser()
{
  MOZ_COUNT_DTOR(nsDASHWebMODParser);
}

MPD*
nsDASHWebMODParser::Parse()
{
  LOG1("Parsing DOM into MPD objects");
  nsAutoPtr<MPD> mpd(new MPD());

  nsresult rv = VerifyMPDAttributes();
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = SetMPDBaseUrls(mpd);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = SetPeriods(mpd);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return mpd.forget();
}

nsresult
nsDASHWebMODParser::VerifyMPDAttributes()
{
  NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

  // @|type| should be "static".
  nsAutoString type;
  nsresult rv = GetAttribute(mRoot, NS_LITERAL_STRING("type"), type);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(type.EqualsLiteral("static"), NS_ERROR_ILLEGAL_VALUE);

  // Note: No attributes to be set in MPD object for DASH-WebM OD. This
  // function used for attribute verification only.
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetMPDBaseUrls(MPD* aMpd)
{
  NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = mRoot->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  int i = 0;
#endif
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (tagName.EqualsLiteral("BaseURL")) {
      nsAutoString baseUrlStr;
      rv = child->GetTextContent(baseUrlStr);
      NS_ENSURE_SUCCESS(rv, rv);

      aMpd->AddBaseUrl(baseUrlStr);
      LOG("MPD BaseURL #%d: \"%s\"",
          i++, NS_ConvertUTF16toUTF8(baseUrlStr).get());
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::GetTime(nsAString& aTimeStr, double& aTime)
{
  NS_ENSURE_FALSE(aTimeStr.IsEmpty(), NS_ERROR_NOT_INITIALIZED);
  // Fail if time string is not of the format "PT<time>S".
  NS_NAMED_LITERAL_STRING(prefix, "PT");
  NS_NAMED_LITERAL_STRING(suffix, "S");
  nsAString::const_iterator start, end, prefixStart, prefixEnd,
                            suffixStart, suffixEnd;

  // Search for "PT" at the start.
  aTimeStr.BeginReading(start);
  aTimeStr.EndReading(end);
  prefixStart = start;
  prefixEnd = end;
  NS_ENSURE_TRUE(FindInReadable(prefix, prefixStart, prefixEnd),
                 NS_ERROR_ILLEGAL_VALUE);
  NS_ENSURE_TRUE(prefixStart == start, NS_ERROR_ILLEGAL_VALUE);

  // Search for "S" after "PT".
  suffixStart = prefixEnd;
  suffixEnd = end;
  NS_ENSURE_TRUE(FindInReadable(suffix, suffixStart, suffixEnd),
                 NS_ERROR_ILLEGAL_VALUE);
  NS_ENSURE_TRUE(suffixStart != prefixEnd, NS_ERROR_ILLEGAL_VALUE);
  NS_ENSURE_TRUE(suffixEnd == end, NS_ERROR_ILLEGAL_VALUE);

  // Parse inner substring for time.
  const nsAutoString timeSubString(Substring(prefixEnd, suffixStart));
  LOG("Parsing substring \"%s\" in \"%s\"",
      NS_ConvertUTF16toUTF8(timeSubString).get(),
      NS_ConvertUTF16toUTF8(aTimeStr).get());
  NS_ENSURE_FALSE(timeSubString.IsEmpty(), NS_ERROR_ILLEGAL_VALUE);
  nsresult rv;
  aTime = timeSubString.ToDouble(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetPeriods(MPD* aMpd)
{
  NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = mRoot->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

#ifdef PR_LOGGING
  int i = 0;
#endif
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (tagName.EqualsLiteral("Period")) {
      nsAutoPtr<Period> period(new Period());

      // Get start time and duration
      nsAutoString value;
      rv = GetAttribute(child, NS_LITERAL_STRING("start"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!value.IsEmpty()) {
        double startTime = -1;
        rv = GetTime(value, startTime);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(0 <= startTime, NS_ERROR_ILLEGAL_VALUE);
        period->SetStart(startTime);
      }

      rv = GetAttribute(child, NS_LITERAL_STRING("duration"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!value.IsEmpty()) {
        double duration = -1;
        rv = GetTime(value, duration);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(0 <= duration, NS_ERROR_ILLEGAL_VALUE);
        period->SetDuration(duration);
      }

      bool bIgnoreThisPeriod;
      rv = SetAdaptationSets(child, period, bIgnoreThisPeriod);
      NS_ENSURE_SUCCESS(rv, rv);

      // |Period| should be ignored if its child elems are invalid
      if (bIgnoreThisPeriod) {
        LOG1("Ignoring period");
      } else {
        aMpd->AddPeriod(period.forget());
        LOG("Period #%d: added to MPD", i++);
      }
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, rv);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::ValidateAdaptationSetAttributes(nsIDOMElement* aChild,
                                                    bool &bAttributesValid)
{
  // Check common attributes first.
  nsAutoString value;
  nsresult rv = GetAttribute(aChild, NS_LITERAL_STRING("subsegmentStartsWithSAP"),
                           value);
  NS_ENSURE_SUCCESS(rv, rv);
  bAttributesValid = (!value.IsEmpty() && value.EqualsLiteral("1"));

  // Now check for video and audio specific attributes.
  nsAutoString mimeType;
  if (bAttributesValid) {
    rv = GetAttribute(aChild, NS_LITERAL_STRING("mimeType"), mimeType);
    NS_ENSURE_SUCCESS(rv, rv);
    bAttributesValid = !mimeType.IsEmpty();
    if (!bAttributesValid)
      LOG1("mimeType not present!");
  }
  // Validate attributes for video.
  if (bAttributesValid && mimeType.EqualsLiteral(VIDEO_WEBM)) {
    // @|segmentAlignment| is optional
    if (bAttributesValid) {
      rv = GetAttribute(aChild, NS_LITERAL_STRING("segmentAlignment"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      bAttributesValid = (value.IsEmpty() || value.EqualsLiteral("true"));
      if (!bAttributesValid)
        LOG1("segmentAlignment not present or invalid!");
    }
    if (bAttributesValid) {
      rv = GetAttribute(aChild, NS_LITERAL_STRING("subsegmentAlignment"),
                        value);
      NS_ENSURE_SUCCESS(rv, rv);
      bAttributesValid = (!value.IsEmpty() && value.EqualsLiteral("true"));
      if (!bAttributesValid)
        LOG1("subsegmentAlignment not present or invalid!");
    }
    if (bAttributesValid) {
      rv = GetAttribute(aChild, NS_LITERAL_STRING("bitstreamSwitching"),
                        value);
      NS_ENSURE_SUCCESS(rv, rv);
      bAttributesValid = (!value.IsEmpty() && value.EqualsLiteral("true"));
      if (!bAttributesValid)
        LOG1("bitstreamSwitching not present or invalid!");
    }
  } else if (bAttributesValid && mimeType.EqualsLiteral(AUDIO_WEBM)) {
  // Validate attributes for audio.
  } else if (bAttributesValid) {
    // attributes are not valid since mimeType is wrong
    bAttributesValid = false;
    LOG("mimeType is invalid: %s", NS_ConvertUTF16toUTF8(mimeType).get());
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetAdaptationSets(nsIDOMElement* aPeriodElem,
                                      Period* aPeriod,
                                      bool &bIgnoreThisPeriod)
{
  NS_ENSURE_ARG(aPeriodElem);
  NS_ENSURE_ARG(aPeriod);

  // Assume ok until an error is found.
  bIgnoreThisPeriod = false;

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = aPeriodElem->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  int i = 0;
#endif
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);

    // If child is invalid, return and ignore this |Period|
    if (tagName.EqualsLiteral("SegmentList")
        || tagName.EqualsLiteral("SegmentTemplate")) {
      bIgnoreThisPeriod = true;
      return NS_OK;
    }

    if (tagName.EqualsLiteral("AdaptationSet")) {
      // Ignore this |AdaptationSet| if an attribute is inavlid
      bool bAttributesValid = false;
      rv = ValidateAdaptationSetAttributes(child, bAttributesValid);
      NS_ENSURE_SUCCESS(rv, rv);

      // Add |AdaptationSet| to |Period| if attributes and child elems are ok
      if (bAttributesValid) {
        nsAutoPtr<AdaptationSet> adaptationSet(new AdaptationSet());

        // Attribute check already done; set correct value for this profile
        adaptationSet->EnableBitstreamSwitching(true);

        nsAutoString mimeType;
        rv = GetAttribute(child, NS_LITERAL_STRING("mimeType"), mimeType);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!mimeType.IsEmpty()) {
          adaptationSet->SetMIMEType(mimeType);
        }

        // TODO width

        // TODO height

        // TODO codecs

        bool bIgnoreThisAdaptSet = false;
        rv = SetRepresentations(child, adaptationSet, bIgnoreThisAdaptSet);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!bIgnoreThisAdaptSet) {
          LOG("AdaptationSet #%d: mimeType:%s width:%d height:%d codecs:%s",
              i, NS_ConvertUTF16toUTF8(mimeType).get(),
              adaptationSet->GetWidth(), adaptationSet->GetHeight(), "");
          aPeriod->AddAdaptationSet(adaptationSet.forget());
          LOG("AdaptationSet #%d: added to Period", i++);
        }
      }
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, rv);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetRepresentations(nsIDOMElement* aAdaptSetElem,
                                       AdaptationSet* aAdaptationSet,
                                       bool &bIgnoreThisAdaptSet)
{
  NS_ENSURE_ARG(aAdaptSetElem);
  NS_ENSURE_ARG(aAdaptationSet);

  // Assume ok until an error is found.
  bIgnoreThisAdaptSet = false;

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = aAdaptSetElem->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  int i = 0;
#endif
  bIgnoreThisAdaptSet = false;
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);

    // If child is invalid, return and ignore this |Period|
    if (tagName.EqualsLiteral("SegmentList")
        || tagName.EqualsLiteral("SegmentTemplate")) {
      bIgnoreThisAdaptSet = true;
      return NS_OK;
    }

    if (tagName.EqualsLiteral("Representation")) {

      nsAutoPtr<Representation> representation(new Representation());

      nsAutoString value;
      rv = GetAttribute(child, NS_LITERAL_STRING("width"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if(!value.IsEmpty()) {
        representation->SetWidth(value.ToInteger(&rv));
      }

      rv = GetAttribute(child, NS_LITERAL_STRING("height"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if(!value.IsEmpty()) {
        representation->SetHeight(value.ToInteger(&rv));
      }

      rv = GetAttribute(child, NS_LITERAL_STRING("bandwidth"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if(!value.IsEmpty()) {
        representation->SetBitrate(value.ToInteger(&rv));
      }

      LOG("Representation #%d: width:%d height:%d bitrate:%d",
          i, representation->GetWidth(),
          representation->GetHeight(),
          representation->GetBitrate());

      // Get |BaseURL| elements
      bool bIgnoreThisRep;
      SetRepresentationBaseUrls(child, representation, bIgnoreThisRep);

      // Get |SegmentBase| elements
      if (!bIgnoreThisRep)
        SetRepSegmentBase(child, representation, bIgnoreThisRep);

      if (!bIgnoreThisRep) {
        aAdaptationSet->AddRepresentation(representation.forget());
        LOG("Representation #%d: added to AdaptationSet", i++);
      }
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, rv);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetRepresentationBaseUrls(nsIDOMElement* aRepElem,
                                              Representation* aRep,
                                              bool &bIgnoreThisRep)
{
  NS_ENSURE_ARG(aRepElem);
  NS_ENSURE_ARG(aRep);

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = aRepElem->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  int i = 0;
#endif
  // Ignore if no |BaseURL| elems; assume none exist until discovered.
  bIgnoreThisRep = true;
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (tagName.EqualsLiteral("BaseURL")) {
      bIgnoreThisRep = false;
      nsAutoString baseUrlStr;
      rv = child->GetTextContent(baseUrlStr);
      NS_ENSURE_SUCCESS(rv, rv);

      aRep->AddBaseUrl(baseUrlStr);
      LOG("BaseURL #%d: \"%s\" added to Representation",
          i++, NS_ConvertUTF16toUTF8(baseUrlStr).get());
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, rv);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetRepSegmentBase(nsIDOMElement* aRepElem,
                                      Representation* aRep,
                                      bool &bIgnoreThisRep)
{
  NS_ENSURE_ARG(aRepElem);
  NS_ENSURE_ARG(aRep);

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = aRepElem->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  int i = 0;
#endif
  // Ignore if no |SegmentBase| elems; assume none exist until discovered.
  bIgnoreThisRep = true;
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (tagName.EqualsLiteral("SegmentBase")) {
      bIgnoreThisRep = false;
      bool bIgnoreThisSegBase = false;

      nsAutoPtr<SegmentBase> segmentBase(new SegmentBase());

      nsAutoString value;
      rv = GetAttribute(child, NS_LITERAL_STRING("indexRange"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if(!value.IsEmpty()) {
        segmentBase->SetIndexRange(value);
      } else {
        bIgnoreThisRep = true;
        bIgnoreThisSegBase = true;
      }

      if (!bIgnoreThisSegBase) {
        SetSegmentBaseInit(child, segmentBase, bIgnoreThisSegBase);
      }

      if (!bIgnoreThisSegBase) {
        aRep->SetSegmentBase(segmentBase.forget());
        LOG("SegmentBase #%d: added to Representation", i++);
      }
      break;
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, rv);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::SetSegmentBaseInit(nsIDOMElement* aSegBaseElem,
                                       SegmentBase* aSegBase,
                                       bool &bIgnoreThisSegBase)
{
  NS_ENSURE_ARG(aSegBaseElem);
  NS_ENSURE_ARG(aSegBase);

  nsCOMPtr<nsIDOMElement> child, nextChild;
  nsresult rv = aSegBaseElem->GetFirstElementChild(getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  int i = 0;
#endif
  // Ignore if no |Initialisation| elems; assume none exist until discovered.
  bIgnoreThisSegBase = true;
  while (child) {
    nsAutoString tagName;
    rv = child->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);
    // XXX Spec spells with 's'; WebM Project defn spells with 'z': accept both
    if (tagName.EqualsLiteral("Initialisation")
        || tagName.EqualsLiteral("Initialization")) {
      bIgnoreThisSegBase = false;

      nsAutoString value;
      rv = GetAttribute(child, NS_LITERAL_STRING("range"), value);
      NS_ENSURE_SUCCESS(rv, rv);
      if(!value.IsEmpty()) {
        aSegBase->SetInitRange(value);
        LOG("Initialisation #%d: added to SegmentBase", i++);
      } else {
        bIgnoreThisSegBase = true;
      }
      break;
    }
    rv = child->GetNextElementSibling(getter_AddRefs(nextChild));
    NS_ENSURE_SUCCESS(rv, rv);
    child = nextChild;
  }
  return NS_OK;
}

nsresult
nsDASHWebMODParser::GetAttribute(nsIDOMElement* aElem,
                                 const nsAString& aAttribute,
                                 nsAString& aValue)
{
  bool bAttributePresent;
  nsresult rv = aElem->HasAttribute(aAttribute, &bAttributePresent);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!bAttributePresent)
    aValue.AssignLiteral("");
  else {
    rv = aElem->GetAttribute(aAttribute, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

}//namespace net
}//namespace mozilla
