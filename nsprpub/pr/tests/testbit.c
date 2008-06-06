/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

PRIntn main(PRIntn argc, char **argv )
{
    /*
    ** Test bitmap things.
    */
    if ( PR_TEST_BIT( myMap, 0 ))
        ErrorReport("Test 0.0: Failed\n");

    if ( PR_TEST_BIT( myMap, 31 ))
        ErrorReport("Test 0.1: Failed\n");

    if ( PR_TEST_BIT( myMap, 128 ))
        ErrorReport("Test 0.2: Failed\n");

    if ( PR_TEST_BIT( myMap, 129 ))
        ErrorReport("Test 0.3: Failed\n");


    PR_SET_BIT( myMap, 0 );
    if ( !PR_TEST_BIT( myMap, 0 ))
        ErrorReport("Test 1.0: Failed\n");

    PR_CLEAR_BIT( myMap, 0 );
    if ( PR_TEST_BIT( myMap, 0 ))
        ErrorReport("Test 1.0.1: Failed\n");

    PR_SET_BIT( myMap, 31 );
    if ( !PR_TEST_BIT( myMap, 31 ))
        ErrorReport("Test 1.1: Failed\n");

    PR_CLEAR_BIT( myMap, 31 );
    if ( PR_TEST_BIT( myMap, 31 ))
        ErrorReport("Test 1.1.1: Failed\n");

    PR_SET_BIT( myMap, 128 );
    if ( !PR_TEST_BIT( myMap, 128 ))
        ErrorReport("Test 1.2: Failed\n");

    PR_CLEAR_BIT( myMap, 128 );
    if ( PR_TEST_BIT( myMap, 128 ))
        ErrorReport("Test 1.2.1: Failed\n");

    PR_SET_BIT( myMap, 129 );
    if ( !PR_TEST_BIT( myMap, 129 ))
        ErrorReport("Test 1.3: Failed\n");

    PR_CLEAR_BIT( myMap, 129 );
    if ( PR_TEST_BIT( myMap, 129 ))
        ErrorReport("Test 1.3.1: Failed\n");


    /*
    ** Test Ceiling and Floor functions and macros
    */
    if ((rc = PR_CeilingLog2(32)) != 5 )
        ErrorReport("Test 10.0: Failed\n");

    if ((rc = PR_FloorLog2(32)) != 5 )
        ErrorReport("Test 10.1: Failed\n");


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
