/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

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
