/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

// needed to get vsnprintf with glibc
#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "prenv.h"

#include "mozilla/FunctionTimer.h"

#ifdef _MSC_VER
#define vsnprintf _vsnprintf
#include <windows.h>
#include <mmsystem.h>
#endif

using namespace mozilla;

// This /must/ come before the call to InitTimers below,
// or its constructor will be called after we've already
// assigned the Now() value to it.
static TimeStamp sAppStart;

nsAutoPtr<FunctionTimerLog> FunctionTimer::sLog;
char *FunctionTimer::sBuf1 = nsnull;
char *FunctionTimer::sBuf2 = nsnull;
int FunctionTimer::sBufSize = FunctionTimer::InitTimers();
unsigned FunctionTimer::sDepth = 0;

int
FunctionTimer::InitTimers()
{
    if (PR_GetEnv("MOZ_FT") == NULL)
        return 0;

    // ensure that this is initialized before us
    TimeStamp::Startup();

    sLog = new FunctionTimerLog(PR_GetEnv("MOZ_FT"));
    sBuf1 = (char *) malloc(BUF_LOG_LENGTH);
    sBuf2 = (char *) malloc(BUF_LOG_LENGTH);
    sAppStart = TimeStamp::Now();

    return BUF_LOG_LENGTH;
}

FunctionTimerLog::FunctionTimerLog(const char *fname)
    : mLatest(sAppStart)
{
    if (strcmp(fname, "stdout") == 0) {
        mFile = stdout;
    } else if (strcmp(fname, "stderr") == 0) {
        mFile = stderr;
    } else {
        FILE *fp = fopen(fname, "wb");
        if (!fp) {
            NS_WARNING("FunctionTimerLog: Failed to open file specified, logging disabled!");
        }
        mFile = fp;
    }

#ifdef _MSC_VER
    // Get 1ms resolution on Windows
    timeBeginPeriod(1);
#endif
}

FunctionTimerLog::~FunctionTimerLog()
{
    if (mFile && mFile != stdout && mFile != stderr)
        fclose((FILE*)mFile);

#ifdef _MSC_VER
    timeEndPeriod(1);
#endif
}

void
FunctionTimerLog::LogString(const char *str)
{
    if (mFile) {
        mLatest = TimeStamp::Now();
        TimeDuration elapsed = mLatest - sAppStart;
        fprintf((FILE*)mFile, "[% 9.2f] %s\n", elapsed.ToSeconds() * 1000.0, str);
    }
}

TimeDuration
FunctionTimerLog::LatestSinceStartup() const
{
    return mLatest - sAppStart;
}

int
FunctionTimer::ft_vsnprintf(char *str, int maxlen, const char *fmt, va_list args)
{
    return vsnprintf(str, maxlen, fmt, args);
}

int
FunctionTimer::ft_snprintf(char *str, int maxlen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int rval = ft_vsnprintf(str, maxlen, fmt, ap);

    va_end(ap);

    return rval;
}
