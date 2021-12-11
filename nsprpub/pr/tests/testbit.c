/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        lazyinit.c
** Description: Test the functions and macros declared in prbit.h
**
*/

#include "nspr.h"

#define ErrorReport(x) { printf((x)); failed = 1; }

prbitmap_t myMap[512/32] = { 0 };

PRInt32 rc;
PRInt32 i;
PRIntn  failed = 0;

int main(int argc, char **argv)
{
    /*
    ** Test bitmap things.
    */
    if ( PR_TEST_BIT( myMap, 0 )) {
        ErrorReport("Test 0.0: Failed\n");
    }

    if ( PR_TEST_BIT( myMap, 31 )) {
        ErrorReport("Test 0.1: Failed\n");
    }

    if ( PR_TEST_BIT( myMap, 128 )) {
        ErrorReport("Test 0.2: Failed\n");
    }

    if ( PR_TEST_BIT( myMap, 129 )) {
        ErrorReport("Test 0.3: Failed\n");
    }


    PR_SET_BIT( myMap, 0 );
    if ( !PR_TEST_BIT( myMap, 0 )) {
        ErrorReport("Test 1.0: Failed\n");
    }

    PR_CLEAR_BIT( myMap, 0 );
    if ( PR_TEST_BIT( myMap, 0 )) {
        ErrorReport("Test 1.0.1: Failed\n");
    }

    PR_SET_BIT( myMap, 31 );
    if ( !PR_TEST_BIT( myMap, 31 )) {
        ErrorReport("Test 1.1: Failed\n");
    }

    PR_CLEAR_BIT( myMap, 31 );
    if ( PR_TEST_BIT( myMap, 31 )) {
        ErrorReport("Test 1.1.1: Failed\n");
    }

    PR_SET_BIT( myMap, 128 );
    if ( !PR_TEST_BIT( myMap, 128 )) {
        ErrorReport("Test 1.2: Failed\n");
    }

    PR_CLEAR_BIT( myMap, 128 );
    if ( PR_TEST_BIT( myMap, 128 )) {
        ErrorReport("Test 1.2.1: Failed\n");
    }

    PR_SET_BIT( myMap, 129 );
    if ( !PR_TEST_BIT( myMap, 129 )) {
        ErrorReport("Test 1.3: Failed\n");
    }

    PR_CLEAR_BIT( myMap, 129 );
    if ( PR_TEST_BIT( myMap, 129 )) {
        ErrorReport("Test 1.3.1: Failed\n");
    }


    /*
    ** Test Ceiling and Floor functions and macros
    */
    if ((rc = PR_CeilingLog2(32)) != 5 ) {
        ErrorReport("Test 10.0: Failed\n");
    }

    if ((rc = PR_FloorLog2(32)) != 5 ) {
        ErrorReport("Test 10.1: Failed\n");
    }


    /*
    ** Evaluate results and exit
    */
    if (failed)
    {
        printf("FAILED\n");
        return(1);
    }
    else
    {
        printf("PASSED\n");
        return(0);
    }
}  /* end main() */
/* end testbit.c */
