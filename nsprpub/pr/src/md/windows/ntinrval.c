/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * NT interval timers
 *
 */

#include "primpl.h"

#ifdef WINCE
typedef DWORD (*IntervalFuncType)(void);
static IntervalFuncType intervalFunc;
#endif

void
_PR_MD_INTERVAL_INIT()
{
#ifdef WINCE
    HMODULE mmtimerlib = LoadLibraryW(L"mmtimer.dll");  /* XXX leaked! */
    if (mmtimerlib) {
        intervalFunc = (IntervalFuncType)GetProcAddress(mmtimerlib,
                                                        "timeGetTime");
    } else {
        intervalFunc = &GetTickCount;
    }
#endif
}

PRIntervalTime 
_PR_MD_GET_INTERVAL()
{
    /* milliseconds since system start */
#ifdef WINCE
    return (*intervalFunc)();
#else
    return timeGetTime();
#endif
}

PRIntervalTime 
_PR_MD_INTERVAL_PER_SEC()
{
    return 1000;
}
