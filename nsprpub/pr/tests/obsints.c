/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
int main(int argc, char **argv)
{
    printf("PASS\n");
    return 0;
}

#else /* NO_NSPR_10_SUPPORT */

#include "prtypes.h"  /* which includes protypes.h */

int main(int argc, char **argv)
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
