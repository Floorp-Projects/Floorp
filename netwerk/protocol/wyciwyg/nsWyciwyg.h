/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwyg_h__
#define nsWyciwyg_h__

#include "mozilla/net/NeckoChild.h"

// Get rid of chromium's LOG.
#undef LOG

#include "mozilla/Logging.h"

//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsWyciwyg:5
//    set NSPR_LOG_FILE=wyciwyg.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file wyciwyg.log
//
extern PRLogModuleInfo *gWyciwygLog;

// http logging
#define LOG1(args) MOZ_LOG(gWyciwygLog, PR_LOG_ERROR, args)
#define LOG2(args) MOZ_LOG(gWyciwygLog, PR_LOG_WARNING, args)
#define LOG3(args) MOZ_LOG(gWyciwygLog, PR_LOG_INFO, args)
#define LOG4(args) MOZ_LOG(gWyciwygLog, PR_LOG_DEBUG, args)
#define LOG(args) LOG4(args)

#define LOG1_ENABLED() MOZ_LOG_TEST(gWyciwygLog, PR_LOG_ERROR)
#define LOG2_ENABLED() MOZ_LOG_TEST(gWyciwygLog, PR_LOG_WARNING)
#define LOG3_ENABLED() MOZ_LOG_TEST(gWyciwygLog, PR_LOG_INFO)
#define LOG4_ENABLED() MOZ_LOG_TEST(gWyciwygLog, PR_LOG_DEBUG)
#define LOG_ENABLED() LOG4_ENABLED()

#define WYCIWYG_TYPE "text/html"

#endif // nsWyciwyg_h__
