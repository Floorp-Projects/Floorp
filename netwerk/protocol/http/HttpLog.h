/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpLog_h__
#define HttpLog_h__


/*******************************************************************************
 *  This file should ONLY be #included by source (.cpp) files in the /http
 *  directory, not headers (.h).  If you need to use LOG() in a .h file, call
 *  PR_LOG directly.
 *
 *  This file should also be the first #include in your file.
 *
 *  Yes, this is kludgy.
 *******************************************************************************/


// e10s mess: IPDL-generated headers include chromium files that both #include
// prlog.h, and #define LOG in conflict with this file.
// Solution: (as described in bug 545995)
// 1) ensure that this file is #included before any IPDL-generated files and
//    anything else that #includes prlog.h, so that we can make sure prlog.h
//    sees FORCE_PR_LOG
// 2) #include IPDL boilerplate, and then undef LOG so our LOG wins.
// 3) nsNetModule.cpp does its own crazy stuff with #including prlog.h
//    multiple times; allow it to define ALLOW_LATE_HTTPLOG_H_INCLUDE to bypass
//    check.
#if defined(PR_LOG) && !defined(ALLOW_LATE_HTTPLOG_H_INCLUDE)
#error "If HttpLog.h #included it must come before any IPDL-generated files or other files that #include prlog.h"
#endif

// NeckoChild.h will include chromium, which will include prlog.h so define
// PR_FORCE before we do that.
#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include "mozilla/net/NeckoChild.h"

// Get rid of Chromium's LOG definition
#undef LOG

#if defined(PR_LOGGING)
//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsHttp:5
//    set NSPR_LOG_FILE=http.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file http.log
//
extern PRLogModuleInfo *gHttpLog;
#endif

// http logging
#define LOG1(args) PR_LOG(gHttpLog, 1, args)
#define LOG2(args) PR_LOG(gHttpLog, 2, args)
#define LOG3(args) PR_LOG(gHttpLog, 3, args)
#define LOG4(args) PR_LOG(gHttpLog, 4, args)
#define LOG5(args) PR_LOG(gHttpLog, 5, args)
#define LOG(args) LOG4(args)

#define LOG1_ENABLED() PR_LOG_TEST(gHttpLog, 1)
#define LOG2_ENABLED() PR_LOG_TEST(gHttpLog, 2)
#define LOG3_ENABLED() PR_LOG_TEST(gHttpLog, 3)
#define LOG4_ENABLED() PR_LOG_TEST(gHttpLog, 4)
#define LOG5_ENABLED() PR_LOG_TEST(gHttpLog, 5)
#define LOG_ENABLED() LOG4_ENABLED()

#endif // HttpLog_h__
