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
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * A test to see if the macros PR_DELETE and PR_FREEIF are
 * properly defined.  (See Bugzilla bug #39110.)
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

static void Fail(void)
{
    printf("FAIL\n");
    exit(1);
}

int main()
{
    int foo = 1;
    char *ptr = NULL;

    if (foo)
        PR_FREEIF(ptr);
    else
        Fail();

    printf("PASS\n");
    return 0;
}
