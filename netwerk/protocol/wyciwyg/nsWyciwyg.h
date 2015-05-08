/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWyciwyg_h__
#define nsWyciwyg_h__

#include "mozilla/net/NeckoChild.h"

// Get rid of chromium's LOG.
#undef LOG

#include "prlog.h"

//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsWyciwyg:5
//    set NSPR_LOG_FILE=wyciwyg.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file wyciwyg.log
//
extern PRLogModuleInfo *gWyciwygLog;

// http logging
#define LOG1(args) PR_LOG(gWyciwygLog, 1, args)
#define LOG2(args) PR_LOG(gWyciwygLog, 2, args)
#define LOG3(args) PR_LOG(gWyciwygLog, 3, args)
#define LOG4(args) PR_LOG(gWyciwygLog, 4, args)
#define LOG(args) LOG4(args)

#define LOG1_ENABLED() PR_LOG_TEST(gWyciwygLog, 1)
#define LOG2_ENABLED() PR_LOG_TEST(gWyciwygLog, 2)
#define LOG3_ENABLED() PR_LOG_TEST(gWyciwygLog, 3)
#define LOG4_ENABLED() PR_LOG_TEST(gWyciwygLog, 4)
#define LOG_ENABLED() LOG4_ENABLED()

#define WYCIWYG_TYPE "text/html"

#endif // nsWyciwyg_h__
