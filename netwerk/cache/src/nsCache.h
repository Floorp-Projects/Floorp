/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsCache.h, released March 18, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 *    Darin Fisher     <darin@netscape.com>
 */

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
                         char *           format,
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
extern nsresult  ClientKeyFromCacheKey(const nsACString& key, char ** result);


#endif // _nsCache_h
