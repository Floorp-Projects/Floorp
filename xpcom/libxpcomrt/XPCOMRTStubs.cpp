/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "mozilla/TimeStamp.h"


GeckoProcessType
XRE_GetProcessType()
{
  return GeckoProcessType_Default;
}

bool
XRE_IsParentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

bool
XRE_IsContentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Content;
}

#define PRINT_CALLED fprintf(stderr, "!!! ERROR: function %s defined in file %s should not be called, needs to be correctly implemented.\n", __FUNCTION__, __FILE__)

class nsAString;
class nsCString;

namespace base {
  class Histogram;
} // namespace base

namespace mozilla {
namespace Telemetry {

#include "mozilla/TelemetryHistogramEnums.h"

void Accumulate(ID id, uint32_t sample) {}
void Accumulate(ID id, const nsCString& key, uint32_t sample) {}
void Accumulate(const char* name, uint32_t sample) {}
void AccumulateTimeDelta(ID id, TimeStamp start, TimeStamp end) {}

base::Histogram* GetHistogramById(ID id)
{
  return nullptr;
}

base::Histogram* GetKeyedHistogramById(ID id, const nsAString&)
{
  return nullptr;
}

} // namespace Telemetry
} // namespace mozilla
