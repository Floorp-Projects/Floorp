/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* fileio.cpp - a test program */

#include "rcfileio.h"

#include <prlog.h>
#include <prprf.h>

#define DEFAULT_ITERATIONS 100

PRIntn main(PRIntn argc, char **argv)
{
    PRStatus rv;
    RCFileIO fd;
    RCFileInfo info;
    rv = fd.Open("filio.dat", PR_CREATE_FILE, 0666);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = fd.FileInfo(&info);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = fd.Delete("filio.dat");
    PR_ASSERT(PR_SUCCESS == rv);
    fd.Close();
    PR_ASSERT(PR_SUCCESS == rv);

    return 0;
}  /* main */

/* interval.cpp */

