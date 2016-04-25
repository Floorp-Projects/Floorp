/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BRNameMatchingPolicy.h"

#include "mozilla/Assertions.h"

using namespace mozilla::psm;
using namespace mozilla::pkix;

Result
BRNameMatchingPolicy::FallBackToCommonName(
  Time notBefore,
  /*out*/ FallBackToSearchWithinSubject& fallBackToCommonName)
{
  // (new Date("2015-08-23T00:00:00Z")).getTime() / 1000
  static const Time AUGUST_23_2015 = TimeFromEpochInSeconds(1440288000);
  // (new Date("2016-08-23T00:00:00Z")).getTime() / 1000
  static const Time AUGUST_23_2016 = TimeFromEpochInSeconds(1471910400);
  switch (mMode)
  {
    case Mode::Enforce:
      fallBackToCommonName = FallBackToSearchWithinSubject::No;
      break;
    case Mode::EnforceAfter23August2015:
      fallBackToCommonName = notBefore > AUGUST_23_2015
                           ? FallBackToSearchWithinSubject::No
                           : FallBackToSearchWithinSubject::Yes;
      break;
    case Mode::EnforceAfter23August2016:
      fallBackToCommonName = notBefore > AUGUST_23_2016
                           ? FallBackToSearchWithinSubject::No
                           : FallBackToSearchWithinSubject::Yes;
      break;
    case Mode::DoNotEnforce:
      fallBackToCommonName = FallBackToSearchWithinSubject::Yes;
      break;
    default:
      MOZ_CRASH("Unexpected Mode");
  }
  return Success;
}
