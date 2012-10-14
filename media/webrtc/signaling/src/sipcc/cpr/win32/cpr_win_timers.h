/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_TIMERS_H_
#define _CPR_WIN_TIMERS_H_


/* Start routines for the timer threads */
extern int32_t cprTimerTick(void*);

/* Initialize the windows CPR timer system */
void cprTimerSystemInit(void);

#endif
