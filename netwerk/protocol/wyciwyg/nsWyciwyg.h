/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michal Novotny <michal.novotny@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWyciwyg_h__
#define nsWyciwyg_h__

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

// e10s mess: IPDL-generatd headers include chromium which both #includes
// prlog.h, and #defines LOG in conflict with this file.
// Solution: (as described in bug 545995)
// 1) ensure that this file is #included before any IPDL-generated files and
//    anything else that #includes prlog.h, so that we can make sure prlog.h
//    sees FORCE_PR_LOG if needed.
// 2) #include IPDL boilerplate, and then undef LOG so our LOG wins.
// 3) nsNetModule.cpp does its own crazy stuff with #including prlog.h
//    multiple times; allow it to define ALLOW_LATE_NSHTTP_H_INCLUDE to bypass
//    check. 
#if defined(PR_LOG) && !defined(ALLOW_LATE_NSHTTP_H_INCLUDE)
#error "If nsWyciwyg.h #included it must come before any IPDL-generated files or other files that #include prlog.h"
#endif
#include "mozilla/net/NeckoChild.h"
#undef LOG

#include "plstr.h"
#include "prlog.h"
#include "prtime.h"

#if defined(PR_LOGGING)
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
#endif

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
