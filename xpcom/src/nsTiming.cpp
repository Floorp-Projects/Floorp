/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTiming.h"
#include "prprf.h"
#include "prlog.h"
#include "prtime.h"


static PRLogModuleInfo* gTimingLog   = NULL;
static const char* gTimingModuleName = "nsTiming";

static inline PRBool
EnsureLogModule(void)
{
    if (gTimingLog == NULL) {
        if ((gTimingLog = PR_NewLogModule(gTimingModuleName)) != NULL) {
            // Off to start with
            gTimingLog->level = PR_LOG_NONE;
        }
    }

    return (gTimingLog != NULL) ? PR_TRUE : PR_FALSE;
}

extern "C" NS_COM void
TimingWriteMessage(const char* fmtstr, ...)
{
    if (! EnsureLogModule())
        return;

    if (gTimingLog->level == PR_LOG_NONE)
        return;

    char line[256];
    va_list ap;
    va_start(ap, fmtstr);

    PRExplodedTime now;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);

    // Print out "YYYYMMDD.HHMMSS.UUUUUU: "
    PRUint32 nb = PR_snprintf(line, sizeof(line) - 1,
                              "%04d%02d%02d.%02d%02d%02d.%06d: ",
                              now.tm_year, now.tm_month + 1, now.tm_mday,
                              now.tm_hour, now.tm_min, now.tm_sec,
                              now.tm_usec);

    // ...followed by the "real" message
    nb += PR_vsnprintf(line + nb, sizeof(line) - nb - 1, fmtstr, ap);

    PR_LOG(gTimingLog, PR_LOG_NOTICE, (line));
}


extern "C" NS_COM void
TimingSetEnabled(PRBool enabled)
{
    if (! EnsureLogModule())
        return;

    if (enabled) {
        if (gTimingLog->level == PR_LOG_NONE) {
            gTimingLog->level = PR_LOG_NOTICE;
            TimingWriteMessage("(tracing enabled)");
        }
    } else {
        if (gTimingLog->level != PR_LOG_NONE) {
            TimingWriteMessage("(tracing disabled)");
            gTimingLog->level = PR_LOG_NONE;
        }
    }
}


extern "C" NS_COM PRBool
TimingIsEnabled(void)
{
    if (! EnsureLogModule())
        return PR_FALSE;

    return (gTimingLog->level == PR_LOG_NONE) ? PR_FALSE : PR_TRUE;
}

