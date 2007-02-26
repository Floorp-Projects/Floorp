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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
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
 * test_mem.c
 *
 * Tests Malloc, Realloc and Free
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void testMalloc(PKIX_UInt32 **array)
{
        PKIX_UInt32 i, arraySize = 10;
        PKIX_TEST_STD_VARS();

        /* Create an integer array of size 10 */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc(
                            (PKIX_UInt32)(arraySize*sizeof (unsigned int)),
                            (void **) array, plContext));

        /* Fill in some values */
        (void) printf ("Setting array[i] = i...\n");
        for (i = 0; i < arraySize; i++) {
                (*array)[i] = i;
                if ((*array)[i] != i)
                        testError("Array has incorrect contents");
        }

        /* Memory now reflects changes */
        (void) printf("\tArray: a[0] = %d, a[5] = %d, a[7] = %d.\n",
                    (*array[0]), (*array)[5], (*array)[7]);

cleanup:
        PKIX_TEST_RETURN();
}

void testRealloc(PKIX_UInt32 **array)
{
        PKIX_UInt32 i, arraySize = 20;
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Realloc(*array,
                            (PKIX_UInt32)(arraySize*sizeof (unsigned int)),
                            (void **) array, plContext));

        /* Fill in the new elements */
        (void) printf ("Setting new portion of array to a[i] = i...\n");
        for (i = arraySize/2; i < arraySize; i++) {
                (*array)[i] = i;
                if ((*array)[i] != i)
                        testError("Array has incorrect contents");
        }

        /* New elements should be reflected. The old should be the same */
        (void) printf("\tArray: a[0] = %d, a[15] = %d, a[17] = %d.\n",
                      (*array)[0], (*array)[15], (*array)[17]);

cleanup:
        PKIX_TEST_RETURN();
}

void testFree(PKIX_UInt32 *array)
{

        PKIX_TEST_STD_VARS();
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(array, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        unsigned int *array = NULL;
        int arraySize = 10;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("Memory Allocation");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_PL_Malloc");
        testMalloc(&array);

        subTest("PKIX_PL_Realloc");
        testRealloc(&array);

        subTest("PKIX_PL_Free");
        testFree(array);

        /* --Negative Test Cases------------------- */
        /* Create an integer array of size 10 */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc(
                            (PKIX_UInt32)(arraySize*sizeof (unsigned int)),
                            (void **) &array, plContext));

        (void) printf("Attempting to reallocate 0 sized memory...\n");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Realloc(array, 0, (void **) &array, plContext));

        (void) printf("Attempting to allocate to null pointer...\n");

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_Malloc(10, NULL, plContext));

        (void) printf("Attempting to reallocate to null pointer...\n");

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_Realloc(NULL, 10, NULL, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(array, plContext));

cleanup:

        PKIX_Shutdown(plContext);

        endTests("Memory Allocation");

        return (0);
}
