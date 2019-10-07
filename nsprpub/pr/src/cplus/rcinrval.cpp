/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** C++ interval times (ref: prinrval.h)
**
**  An interval is a period of time. The start of the interval (epoch)
**  must be defined by the application. The unit of time of an interval
**  is platform dependent, therefore so is the maximum interval that is
**  representable. However, that interval is never less that ~12 hours.
*/

#include "rcinrval.h"

RCInterval::~RCInterval() { }

RCInterval::RCInterval(RCInterval::RCReservedInterval special): RCBase()
{
    switch (special)
    {
        case RCInterval::now:
            interval = PR_IntervalNow();
            break;
        case RCInterval::no_timeout:
            interval = PR_INTERVAL_NO_TIMEOUT;
            break;
        case RCInterval::no_wait:
            interval = PR_INTERVAL_NO_WAIT;
            break;
        default:
            break;
    }
}  /* RCInterval::RCInterval */

/* rcinrval.cpp */
