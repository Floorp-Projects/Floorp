/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
