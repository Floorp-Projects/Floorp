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
 * Test: obsints.c 
 *
 * Description: make sure that protypes.h defines the obsolete integer
 * types intn, uintn, uint, int8, uint8, int16, uint16, int32, uint32,
 * int64, and uint64.
 */

#include <stdio.h>

#ifdef NO_NSPR_10_SUPPORT

/* nothing to do */
int main()
{
    printf("PASS\n");
    return 0;
}

#else /* NO_NSPR_10_SUPPORT */

#include "prtypes.h"  /* which includes protypes.h */

int main()
{
    /*
     * Compilation fails if any of these integer types are not
     * defined by protypes.h.
     */
    intn in;
    uintn uin;
    uint ui;
    int8 i8;
    uint8 ui8;
    int16 i16;
    uint16 ui16;
    int32 i32;
    uint32 ui32;
    int64 i64;
    uint64 ui64;

    printf("PASS\n");
    return 0;
}

#endif /* NO_NSPR_10_SUPPORT */
