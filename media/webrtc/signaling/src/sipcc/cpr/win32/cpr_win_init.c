/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"

extern cpr_status_e cprSocketInit(void);
extern cpr_status_e cprSocketCleanup(void);

#ifdef WIN32_7960
extern void win32_stdio_init(void);
#endif

/* Boolean to check that cprPreInit been called */
static boolean pre_init_called = FALSE;

/**
 * cprInit
 *
 * Initialization routine to call before any application level
 * code initializes.
 *
 * Parameters: None
 *
 * Return Value: Success or Failure Indication
 */
CRITICAL_SECTION criticalSection;

cprRC_t
cprPreInit (void)
{
    static const int8_t fname[] = "cprPreInit";

    /*
     * Make function reentreant
     */
    if (pre_init_called equals TRUE) {
        return CPR_SUCCESS;
    }
    pre_init_called = TRUE;

    /* Initialize critical section semaphore used for task swapping */
    InitializeCriticalSection(&criticalSection);

    /* Initialize socket library */
    if (cprSocketInit() == CPR_FAILURE) {
        CPR_ERROR("%s: socket init failed\n", fname);
        return CPR_FAILURE;
    }

#ifdef WIN32_7960
    win32_stdio_init();
#endif
    return (CPR_SUCCESS);
}


/**
 * cprPostInit
 *
 * Initialization routine to call when all applications are ready to run.
 *
 * Parameters: None
 *
 * Return Value: Always returns success
 */
cprRC_t
cprPostInit (void)
{
    /*
     * There is no work to be done in the Windows
     * environment at this time.
     */
    return (CPR_SUCCESS);

}

