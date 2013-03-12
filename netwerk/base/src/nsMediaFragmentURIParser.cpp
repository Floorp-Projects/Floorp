/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsEscape.h"
#include <utility>

#include "nsMediaFragmentURIParser.h"

using std::pair;
using std::make_pair;

namespace mozilla { namespace net {

nsMediaFragmentURIParser::nsMediaFragmentURIParser(nsIURI* aURI)
  : mClipUnit(eClipUnit_Pixel)
{
  nsAutoCString ref;
  aURI->GetRef(ref);
  Parse(ref);
}

bool nsMediaFragmentURIParser::ParseNPT(nsDependentSubstring aString)
{
  nsDependentSubstring original(aString);
  if (aString.Length() > 4 &&
      aString[0] == 'n' && aString[1] == 'p' &&
      aString[2] == 't' && aString[3] == ':') {
    aString.Rebind(aString, 4);
  }

  if (aString.Length() == 0) {
    return false;
  }

  double start = -1.0;
  double end = -1.0;

  ParseNPTTime(aString, start);

  if (aString.Length() == 0) {
    mStart.construct(start);
    return true;
  }

  if (aString[0] != ',') {
    aString.Rebind(original, 0);
    return false;
  }

  aString.Rebind(aString, 1);

  if (aString.Length() == 0) {
    aString.Rebind(original, 0);
    return false;
  }

  ParseNPTTime(aString, end);

  if (end <= start || aString.Length() != 0) {
    aString.Rebind(original, 0);
    return false;
  }

  mStart.construct(start);
  mEnd.construct(end);
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTTime(nsDependentSubstring& aString, double& aTime)
{
  if (aString.Length() == 0) {
    return false;
  }

  return
    ParseNPTHHMMSS(aString, aTime) ||
    ParseNPTMMSS(aString, aTime) ||
    ParseNPTSec(aString, aTime);
}

// Return true if the given character is a numeric character
static bool IsDigit(nsDependentSubstring::char_type aChar)
{
  return (aChar >= '0' && aChar <= '9');
}

// Return the index of the first character in the string that is not
// a numerical digit, starting from 'aStart'.
static uint32_t FirstNonDigit(nsDependentSubstring& aString, uint32_t aStart)
{
   while (aStart < aString.Length() && IsDigit(aString[aStart])) {
    ++aStart;
  }
  return aStart;
}
 
bool nsMediaFragmentURIParser::ParseNPTSec(nsDependentSubstring& aString, double& aSec)
{
  nsDependentSubstring original(aString);
  if (aString.Length() == 0) {
    return false;
  }

  uint32_t index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return false;
  }

  nsDependentSubstring n(aString, 0, index);
  nsresult ec;
  int32_t s = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return false;
  }

  aString.Rebind(aString, index);
  double fraction = 0.0;
  if (!ParseNPTFraction(aString, fraction)) {
    aString.Rebind(original, 0);
    return false;
  }

  aSec = s + fraction;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTMMSS(nsDependentSubstring& aString, double& aTime)
{
  nsDependentSubstring original(aString);
  uint32_t mm = 0;
  uint32_t ss = 0;
  double fraction = 0.0;
  if (!ParseNPTMM(aString, mm)) {
    aString.Rebind(original, 0);
    return false;
  }

  if (aString.Length() < 2 || aString[0] != ':') {
    aString.Rebind(original, 0);
    return false;
  }

  aString.Rebind(aString, 1);
  if (!ParseNPTSS(aString, ss)) {
    aString.Rebind(original, 0);
    return false;
  }

  if (!ParseNPTFraction(aString, fraction)) {
    aString.Rebind(original, 0);
    return false;
  }
  aTime = mm * 60 + ss + fraction;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTFraction(nsDependentSubstring& aString, double& aFraction)
{
  double fraction = 0.0;

  if (aString.Length() > 0 && aString[0] == '.') {
    uint32_t index = FirstNonDigit(aString, 1);

    if (index > 1) {
      nsDependentSubstring number(aString, 0, index);
      nsresult ec;
      fraction = PromiseFlatString(number).ToDouble(&ec);
      if (NS_FAILED(ec)) {
        return false;
      }
    }
    aString.Rebind(aString, index);
  }

  aFraction = fraction;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTHHMMSS(nsDependentSubstring& aString, double& aTime)
{
  nsDependentSubstring original(aString);
  uint32_t hh = 0;
  double seconds = 0.0;
  if (!ParseNPTHH(aString, hh)) {
    return false;
  }

  if (aString.Length() < 2 || aString[0] != ':') {
    aString.Rebind(original, 0);
    return false;
  }

  aString.Rebind(aString, 1);
  if (!ParseNPTMMSS(aString, seconds)) {
    aString.Rebind(original, 0);
    return false;
  }

  aTime = hh * 3600 + seconds;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTHH(nsDependentSubstring& aString, uint32_t& aHour)
{
  if (aString.Length() == 0) {
    return false;
  }

  uint32_t index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return false;
  }

  nsDependentSubstring n(aString, 0, index);
  nsresult ec;
  int32_t u = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return false;
  }

  aString.Rebind(aString, index);
  aHour = u;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTMM(nsDependentSubstring& aString, uint32_t& aMinute)
{
  return ParseNPTSS(aString, aMinute);
}

bool nsMediaFragmentURIParser::ParseNPTSS(nsDependentSubstring& aString, uint32_t& aSecond)
{
  if (aString.Length() < 2) {
    return false;
  }

  if (IsDigit(aString[0]) && IsDigit(aString[1])) {
    nsDependentSubstring n(aString, 0, 2);
    nsresult ec;
    int32_t u = PromiseFlatString(n).ToInteger(&ec);
    if (NS_FAILED(ec)) {
      return false;
    }

    aString.Rebind(aString, 2);
    if (u >= 60)
      return false;

    aSecond = u;
    return true;
  }

  return false;
}

static bool ParseInteger(nsDependentSubstring& aString,
                         int32_t& aResult)
{
  uint32_t index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return false;
  }

  nsDependentSubstring n(aString, 0, index);
  nsresult ec;
  int32_t s = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return false;
  }

  aString.Rebind(aString, index);
  aResult = s;
  return true;
}

static bool ParseCommaSeparator(nsDependentSubstring& aString)
{
  if (aString.Length() > 1 && aString[0] == ',') {
    aString.Rebind(aString, 1);
    return true;
  }

  return false;
}

bool nsMediaFragmentURIParser::ParseXYWH(nsDependentSubstring aString)
{
  int32_t x, y, w, h;
  ClipUnit clipUnit;

  // Determine units.
  if (StringBeginsWith(aString, NS_LITERAL_STRING("pixel:"))) {
    clipUnit = eClipUnit_Pixel;
    aString.Rebind(aString, 6);
  } else if (StringBeginsWith(aString, NS_LITERAL_STRING("percent:"))) {
    clipUnit = eClipUnit_Percent;
    aString.Rebind(aString, 8);
  } else {
    clipUnit = eClipUnit_Pixel;
  }

  // Read and validate coordinates.
  if (ParseInteger(aString, x) && x >= 0 &&
      ParseCommaSeparator(aString)       &&
      ParseInteger(aString, y) && y >= 0 &&
      ParseCommaSeparator(aString)       &&
      ParseInteger(aString, w) && w > 0  &&
      ParseCommaSeparator(aString)       &&
      ParseInteger(aString, h) && h > 0  &&
      aString.Length() == 0) {

    // Reject invalid percentage coordinates.
    if (clipUnit == eClipUnit_Percent &&
        (x + w > 100 || y + h > 100)) {
      return false;
    }

    mClip.construct(x, y, w, h);
    mClipUnit = clipUnit;
    return true;
  }

  return false;
}

void nsMediaFragmentURIParser::Parse(nsACString& aRef)
{
  // Create an array of possibly-invalid media fragments.
  nsTArray< std::pair<nsCString, nsCString> > fragments;
  nsCCharSeparatedTokenizer tokenizer(aRef, '&');

  while (tokenizer.hasMoreTokens()) {
    const nsCSubstring& nv = tokenizer.nextToken();
    int32_t index = nv.FindChar('=');
    if (index >= 0) {
      nsAutoCString name;
      nsAutoCString value;
      NS_UnescapeURL(StringHead(nv, index), esc_Ref | esc_AlwaysCopy, name);
      NS_UnescapeURL(Substring(nv, index + 1, nv.Length()),
                     esc_Ref | esc_AlwaysCopy, value);
      fragments.AppendElement(make_pair(name, value));
    }
  }

  // Parse the media fragment values.
  bool gotTemporal = false, gotSpatial = false;
  for (int i = fragments.Length() - 1 ; i >= 0 ; --i) {
    if (gotTemporal && gotSpatial) {
      // We've got one of each possible type. No need to look at the rest.
      break;
    } else if (!gotTemporal && fragments[i].first.EqualsLiteral("t")) {
      nsAutoString value = NS_ConvertUTF8toUTF16(fragments[i].second);
      gotTemporal = ParseNPT(nsDependentSubstring(value, 0));
    } else if (!gotSpatial && fragments[i].first.EqualsLiteral("xywh")) {
      nsAutoString value = NS_ConvertUTF8toUTF16(fragments[i].second);
      gotSpatial = ParseXYWH(nsDependentSubstring(value, 0));
    }
  }
}

}} // namespace mozilla::net
