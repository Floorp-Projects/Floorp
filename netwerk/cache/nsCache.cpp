/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCache.h"
#include "nsReadableUtils.h"
#include "nsDependentSubstring.h"
#include "nsString.h"
#include "mozilla/IntegerPrintfMacros.h"

/**
 * Cache Service Utility Functions
 */

mozilla::LazyLogModule gCacheLog("cache");

void CacheLogPrintPath(mozilla::LogLevel level, const char* format,
                       nsIFile* item) {
  MOZ_LOG(gCacheLog, level, (format, item->HumanReadablePath().get()));
}

uint32_t SecondsFromPRTime(PRTime prTime) {
  int64_t microSecondsPerSecond = PR_USEC_PER_SEC;
  return uint32_t(prTime / microSecondsPerSecond);
}

PRTime PRTimeFromSeconds(uint32_t seconds) {
  int64_t intermediateResult = seconds;
  PRTime prTime = intermediateResult * PR_USEC_PER_SEC;
  return prTime;
}

nsresult ClientIDFromCacheKey(const nsACString& key, nsACString& result) {
  nsReadingIterator<char> colon;
  key.BeginReading(colon);

  nsReadingIterator<char> start;
  key.BeginReading(start);

  nsReadingIterator<char> end;
  key.EndReading(end);

  if (FindCharInReadable(':', colon, end)) {
    result.Assign(Substring(start, colon));
    return NS_OK;
  }

  NS_ASSERTION(false, "FindCharInRead failed to find ':'");
  return NS_ERROR_UNEXPECTED;
}

nsresult ClientKeyFromCacheKey(const nsCString& key, nsACString& result) {
  nsReadingIterator<char> start;
  key.BeginReading(start);

  nsReadingIterator<char> end;
  key.EndReading(end);

  if (FindCharInReadable(':', start, end)) {
    ++start;  // advance past clientID ':' delimiter
    result.Assign(Substring(start, end));
    return NS_OK;
  }

  NS_ASSERTION(false, "FindCharInRead failed to find ':'");
  result.Truncate(0);
  return NS_ERROR_UNEXPECTED;
}
