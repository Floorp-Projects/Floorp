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

#include "nsISupports.h"
#include "nsIFile.h"
#include "nsAString.h"
#include "prtime.h"
#include "nsError.h"
#include "prlog.h"

// PR_LOG args = "format string", arg, arg, ...
#if defined(PR_LOGGING)
extern PRLogModuleInfo * gCacheLog;
void   CacheLogInit();
void   CacheLogPrintPath(PRLogModuleLevel level,
                         const char *     format,
                         nsIFile *        item);
#define CACHE_LOG_INIT()        CacheLogInit()
#define CACHE_LOG_ALWAYS(args)  PR_LOG(gCacheLog, PR_LOG_ALWAYS, args)
#define CACHE_LOG_ERROR(args)   PR_LOG(gCacheLog, PR_LOG_ERROR, args)
#define CACHE_LOG_WARNING(args) PR_LOG(gCacheLog, PR_LOG_WARNING, args)
#define CACHE_LOG_DEBUG(args)   PR_LOG(gCacheLog, PR_LOG_DEBUG, args)
#define CACHE_LOG_PATH(level, format, item) \
                                CacheLogPrintPath(level, format, item)
#else
#define CACHE_LOG_INIT()        {}
#define CACHE_LOG_ALWAYS(args)  {}
#define CACHE_LOG_ERROR(args)   {}
#define CACHE_LOG_WARNING(args) {}
#define CACHE_LOG_DEBUG(args)   {}
#define CACHE_LOG_PATH(level, format, item)  {}
#endif


extern PRUint32  SecondsFromPRTime(PRTime prTime);
extern PRTime    PRTimeFromSeconds(PRUint32 seconds);


extern nsresult  ClientIDFromCacheKey(const nsACString&  key, char ** result);
extern nsresult  ClientKeyFromCacheKey(const nsCString& key, nsACString &result);


#endif // _nsCache_h
