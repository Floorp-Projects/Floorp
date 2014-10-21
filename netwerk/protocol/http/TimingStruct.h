/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef TimingStruct_h_
#define TimingStruct_h_

#include "mozilla/TimeStamp.h"

namespace mozilla { namespace net {

struct TimingStruct {
  TimeStamp domainLookupStart;
  TimeStamp domainLookupEnd;
  TimeStamp connectStart;
  TimeStamp connectEnd;
  TimeStamp requestStart;
  TimeStamp responseStart;
  TimeStamp responseEnd;
};

struct ResourceTimingStruct : TimingStruct {
  TimeStamp fetchStart;
  TimeStamp redirectStart;
  TimeStamp redirectEnd;
};

}} // namespace mozilla::net

#endif
