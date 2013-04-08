/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_locks.h"
#include "cpr_win_locks.h" /* Should really have this included from cpr_locks.h */
#include "cpr_debug.h"
#include "cpr_memory.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include <windows.h>

/* Defined in cpr_init.c */
extern CRITICAL_SECTION criticalSection;

/**
 * cprDisableSwap
 *
 * Windows forces you to create a Critical region semaphore and have threads
 * attempt to get ownership of that semaphore. The calling thread
 * will block until ownership of the semaphore is granted.
 *
 * Parameters: None
 *
 * Return Value: Nothing
 */
void
cprDisableSwap (void)
{
    EnterCriticalSection(&criticalSection);
}


/**
 * cprEnableSwap
 *
 * Releases ownership of the critical section semaphore.
 *
 * Parameters: None
 *
 * Return Value: Nothing
 */
void
cprEnableSwap (void)
{
    LeaveCriticalSection(&criticalSection);
}
