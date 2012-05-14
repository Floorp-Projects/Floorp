/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <kernel/OS.h>

static bigtime_t start;

PRTime
_MD_now (void)
{
    return (PRTime)real_time_clock_usecs();
}

void
_MD_interval_init (void)
{
    /* grab the base interval time */
    start = real_time_clock_usecs();
}

PRIntervalTime
_MD_get_interval (void)
{
    return( (PRIntervalTime) real_time_clock_usecs() / 10 );

#if 0
    /* return the number of tens of microseconds that have elapsed since
       we were initialized */
    bigtime_t now = real_time_clock_usecs();
    now -= start;
    now /= 10;
    return (PRIntervalTime)now;
#endif
}

PRIntervalTime
_MD_interval_per_sec (void)
{
    return 100000L;
}
