/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

PR_EXTERN(void) _PR_MD_INIT_CPUS();
PR_EXTERN(void) _PR_MD_WAKEUP_CPUS();
PR_EXTERN(void) _PR_MD_START_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_STOP_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_DISABLE_CLOCK_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_BLOCK_CLOCK_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_UNBLOCK_CLOCK_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_CLOCK_INTERRUPT(void);
PR_EXTERN(void) _PR_MD_INIT_STACK(PRThreadStack *ts, PRIntn redzone);
PR_EXTERN(void) _PR_MD_CLEAR_STACK(PRThreadStack* ts);
PR_EXTERN(PRInt32) _PR_MD_GET_INTSOFF(void);
PR_EXTERN(void) _PR_MD_SET_INTSOFF(PRInt32 _val);
PR_EXTERN(_PRCPU*) _PR_MD_CURRENT_CPU(void);
PR_EXTERN(void) _PR_MD_SET_CURRENT_CPU(_PRCPU *cpu);
PR_EXTERN(void) _PR_MD_INIT_RUNNING_CPU(_PRCPU *cpu);
PR_EXTERN(PRInt32) _PR_MD_PAUSE_CPU(PRIntervalTime timeout);
