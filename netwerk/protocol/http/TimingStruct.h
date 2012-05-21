/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef TimingStruct_h_
#define TimingStruct_h_

#include "mozilla/TimeStamp.h"

struct TimingStruct {
  mozilla::TimeStamp domainLookupStart;
  mozilla::TimeStamp domainLookupEnd;
  mozilla::TimeStamp connectStart;
  mozilla::TimeStamp connectEnd;
  mozilla::TimeStamp requestStart;
  mozilla::TimeStamp responseStart;
  mozilla::TimeStamp responseEnd;
};

#endif
