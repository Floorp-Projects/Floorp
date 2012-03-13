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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

/* Timer expiration region/pool/buffer pointers */
cprRegion_t timerRegion;
cprPool_t timerPool;

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

    /*
     * Create buffer pool for timer expiration msgs
     */
    timerRegion = cprCreateRegion("CPR - Timers");
    if (timerRegion) {
        timerPool = cprCreatePool(timerRegion, "Timers - 32 bytes", 10, 32);
        if (timerPool == NULL) {
            CPR_ERROR("%s buffer pool creation failed.\n", fname);
            return (CPR_FAILURE);
        }
    } else {
        CPR_ERROR("%s buffer region creation failed.\n", fname);
        return (CPR_FAILURE);
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

