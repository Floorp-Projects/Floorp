/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Cache Service Utility Functions
 */

#ifndef _nsCache_h_
#define _nsCache_h_

#include "mozilla/Logging.h"
#include "nsISupports.h"
#include "nsIFile.h"
#include "nsAString.h"
#include "prtime.h"
#include "nsError.h"

// PR_LOG args = "format string", arg, arg, ...
extern PRLogModuleInfo * gCacheLog;
void   CacheLogInit();
void   CacheLogPrintPath(mozilla::LogLevel level,
                         const char *     format,
                         nsIFile *        item);
#define CACHE_LOG_INIT()        CacheLogInit()
#define CACHE_LOG_INFO(args)  MOZ_LOG(gCacheLog, mozilla::LogLevel::Info, args)
#define CACHE_LOG_ERROR(args)   MOZ_LOG(gCacheLog, mozilla::LogLevel::Error, args)
#define CACHE_LOG_WARNING(args) MOZ_LOG(gCacheLog, mozilla::LogLevel::Warning, args)
#define CACHE_LOG_DEBUG(args)   MOZ_LOG(gCacheLog, mozilla::LogLevel::Debug, args)
#define CACHE_LOG_PATH(level, format, item) \
                                CacheLogPrintPath(level, format, item)


extern uint32_t  SecondsFromPRTime(PRTime prTime);
extern PRTime    PRTimeFromSeconds(uint32_t seconds);


extern nsresult  ClientIDFromCacheKey(const nsACString&  key, char ** result);
extern nsresult  ClientKeyFromCacheKey(const nsCString& key, nsACString &result);


#endif // _nsCache_h
