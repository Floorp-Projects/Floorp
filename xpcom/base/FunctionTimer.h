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

#ifndef mozilla_FunctionTimer_h
#define mozilla_FunctionTimer_h

#include <stdarg.h>

#include "mozilla/TimeStamp.h"
#include "nscore.h"

#if defined(NS_FORCE_FUNCTION_TIMER) && !defined(NS_FUNCTION_TIMER)
#define NS_FUNCTION_TIMER
#endif

/*
 * Shortcut macros
 */

#ifdef NS_FUNCTION_TIMER

// Add a timer for this function, from this declaration until the
// function returns.  The function name will be used as the
// log string, and both function entry and exit will be printed.
#define NS_TIME_FUNCTION                                                \
    mozilla::FunctionTimer ft__autogen("%s (line %d)", __FUNCTION__, __LINE__)

// Add a timer for this block, but print only a) if the exit time is
// greater than the given number of milliseconds; or b) if the
// mark-to-mark time is greater than the given number of milliseconds.
// No function entry will be printed.  If the value given is negative,
// no function entry or exit will ever be printed, but all marks will.
#define NS_TIME_FUNCTION_MIN(_ms)                                       \
    mozilla::FunctionTimer ft__autogen((_ms), "%s (line %d)", __FUNCTION__, __LINE__)

// Add a timer for this block, but print only marks, not function
// entry and exit.  The same as calling the above macro with a negative value.
#define NS_TIME_FUNCTION_MARK_ONLY                                    \
    mozilla::FunctionTimer ft__autogen((-1), "%s (line %d)", __FUNCTION__, __LINE__)

// Add a timer for this block, using the printf-style format.
// Both function entry and exit will be printed.
#define NS_TIME_FUNCTION_FMT(...)                                       \
    mozilla::FunctionTimer ft__autogen(__VA_ARGS__)

// Add a timer for this block, using the given minimum number of
// milliseconds and the given printf-style format.  No function entry
// will be printed.
#define NS_TIME_FUNCTION_MIN_FMT(_ms, ...)                              \
    mozilla::FunctionTimer ft__autogen((_ms), __VA_ARGS__)

// Add a midway mark for the current timer; an existing timer must
// have already been created using one of the above macros.  The
// string given by the printf-style format will be logged, as well as
// the total elapsed time since the creation of the timer, and the
// time since the last mark.
#define NS_TIME_FUNCTION_MARK(...)              \
    ft__autogen.Mark(__VA_ARGS__)

// A double value representing the time elapsed since NS_TIME_FUNCTION
// initialization, in ms.
#define NS_TIME_FUNCTION_ELAPSED              \
    ft__autogen.Elapsed()

// A double value representing the time elapsed since the last
// NS_TIME_FUNCTION_MARK, in ms.
#define NS_TIME_FUNCTION_ELAPSED_SINCE_MARK                             \
    ft__autogen.ElapsedSinceMark

#else

#define NS_TIME_FUNCTION do { } while (0)
#define NS_TIME_FUNCTION_MIN(_ms) do { } while (0)
#define NS_TIME_FUNCTION_MARK_ONLY do { } while (0)
#define NS_TIME_FUNCTION_FMT(...) do { } while (0)
#define NS_TIME_FUNCTION_MIN_FMT(_ms, ...) do { } while (0)
#define NS_TIME_FUNCTION_MARK(...) do { } while (0)
#define NS_TIME_FUNCTION_ELAPSED (0)
#define NS_TIME_FUNCTION_ELAPSED_SINCE_MARK (0)

#endif

namespace mozilla {

class NS_COM FunctionTimerLog
{
public:
    FunctionTimerLog(const char *fname);
    ~FunctionTimerLog();

    void LogString(const char *str);

private:
    void *mFile;
};

class NS_COM FunctionTimer
{
    static FunctionTimerLog* sLog;
    static char *sBuf1, *sBuf2;
    static int sBufSize;

    enum { BUF_LOG_LENGTH = 256 };

public:
    static int InitTimers();

    static int ft_vsnprintf(char *str, int maxlen, const char *fmt, va_list args);
    static int ft_snprintf(char *str, int maxlen, const char *fmt, ...);

public:
    inline void TimeInit() {
        mStart = TimeStamp::Now();
        mLastMark = mStart;
    }

    inline double Elapsed() {
        return (TimeStamp::Now() - mStart).ToSeconds() * 1000.0;
    }

    inline double ElapsedSinceMark() {
        return (TimeStamp::Now() - mLastMark).ToSeconds() * 1000.0;
    }

    FunctionTimer(double minms, const char *s, ...)
        : mMinMs(minms)
    {
        va_list ap;
        va_start(ap, s);

        if (sLog) {
            TimeInit();

            ft_vsnprintf(mString, BUF_LOG_LENGTH, s, ap);
        }

        va_end(ap);
    }

    FunctionTimer(const char *s, ...)
        : mMinMs(0.0)
    {
        va_list ap;
        va_start(ap, s);

        if (sLog) {
            TimeInit();

            ft_vsnprintf(mString, BUF_LOG_LENGTH, s, ap);

            ft_snprintf(sBuf1, sBufSize, "> %s", mString);
            sLog->LogString(sBuf1);
        }

        va_end(ap);
    }

    void Mark(const char *s, ...)
    {
        va_list ap;
        va_start(ap, s);

        ft_vsnprintf(sBuf1, sBufSize, s, ap);

        TimeStamp now(TimeStamp::Now());
        double ms = (now - mStart).ToSeconds() * 1000.0;
        double msl = (now - mLastMark).ToSeconds() * 1000.0;
        mLastMark = now;

        if (msl > mMinMs) {
            ft_snprintf(sBuf2, sBufSize, "%s- %5.2f ms (%5.2f ms total) - %s [%s]", mMinMs < 0.0 ? "" : "*", msl, ms, mString, sBuf1);
            sLog->LogString(sBuf2);
        }

        va_end(ap);
    }

    ~FunctionTimer() {
        TimeStamp now(TimeStamp::Now());
        double ms = (now - mStart).ToSeconds() * 1000.0;
        double msl = (now - mLastMark).ToSeconds() * 1000.0;

        if (mMinMs < 0.0 || (mMinMs >= 0.0 && msl > mMinMs)) {
            ft_snprintf(sBuf1, sBufSize, "%s %5.2f ms (%5.2f ms total) - %s", mMinMs < 0.0 ? "<" : "*", msl, ms, mString);
            sLog->LogString(sBuf1);
        }
    }

    TimeStamp mStart, mLastMark;
    char mString[BUF_LOG_LENGTH+1];
    double mMinMs;
};

} // namespace mozilla

#endif // mozilla_FunctionTimer_h


