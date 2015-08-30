/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPerformanceStats_h
#define nsPerformanceStats_h

#include "nsIObserver.h"

#include "nsIPerformanceStats.h"

class nsPerformanceStatsService : public nsIPerformanceStatsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCESTATSSERVICE

  nsPerformanceStatsService();

private:
  nsresult UpdateTelemetry();
  virtual ~nsPerformanceStatsService();

  const uint64_t mProcessId;
  uint64_t mProcessStayed;
  uint64_t mProcessMoved;
  uint32_t mProcessUpdateCounter;
protected:
};

#endif
