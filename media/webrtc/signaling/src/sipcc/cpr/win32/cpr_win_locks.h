/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_LOCKS_H_
#define _CPR_WIN_LOCKS_H_


/*typedef int cprSignal_t;*/

typedef struct {
	/* Number of waiting thread */
	int noWaiters;

	/* Serialise access to the waiting thread count */
	CRITICAL_SECTION noWaitersLock;

	/* Semaphore used to queue up threads waiting for the condition to be signalled */
	HANDLE sema;

} cpr_condition_t;

#endif

