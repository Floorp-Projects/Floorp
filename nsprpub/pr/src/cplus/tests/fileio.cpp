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

