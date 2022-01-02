/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* time.cpp - a test program */

#include "rctime.h"

#include <prlog.h>
#include <prprf.h>

#define DEFAULT_ITERATIONS 100

PRIntn main(PRIntn argc, char **argv)
{
    RCTime unitialized;
    RCTime now(PR_Now());
    RCTime current(RCTime::now);
    PRTime time = current;

    unitialized = now;
    now.Now();

    return 0;
}  /* main */

/* time.cpp */

