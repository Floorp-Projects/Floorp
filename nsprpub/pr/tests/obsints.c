/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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
